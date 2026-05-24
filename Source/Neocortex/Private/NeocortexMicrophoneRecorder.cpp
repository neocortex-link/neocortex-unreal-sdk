// c++
/* File: 'Plugins/Neocortex/Source/Neocortex/Private/MicrophoneRecorder.cpp' */
#include "NeocortexMicrophoneRecorder.h"

#include "AudioCaptureCore.h"
#include "AudioCaptureDeviceInterface.h"
#include "Neocortex.h"
#include "NeocortexWavEncoder.h"
#include "VoiceModule.h"

static int32 SanitizeSampleRate(int32 Rate)
{
    // Prefer 16000 for STT or 48000 for device-native
    if (Rate == 16000 || Rate == 48000) return Rate;
    if (Rate >= 44000 && Rate <= 48000) return 48000;
    if (Rate >= 16000 && Rate < 44000)  return 16000;
    return 16000;
}

TArray<FString> FNeocortexMicrophoneRecorder::GetAvailableDeviceNames()
{
    TArray<FString> Names;
    TArray<Audio::FCaptureDeviceInfo> Devices;
    Audio::FAudioCapture Capture;
    if (Capture.GetCaptureDevicesAvailable(Devices))
    {
        for (const Audio::FCaptureDeviceInfo& Info : Devices)
        {
            Names.Add(Info.DeviceName);
        }
    }
    return Names;
}

static TSharedPtr<IVoiceCapture> TryOpenDevice(const FString& DeviceName, int32& SampleRate)
{
    TArray<Audio::FCaptureDeviceInfo> Devices;
    Audio::FAudioCapture Capture;
    if (!Capture.GetCaptureDevicesAvailable(Devices)) return nullptr;

    for (const Audio::FCaptureDeviceInfo& Info : Devices)
    {
        if (!Info.DeviceName.Equals(DeviceName, ESearchCase::IgnoreCase)) continue;
        const int32 DeviceRate = SanitizeSampleRate(Info.PreferredSampleRate);
        TSharedPtr<IVoiceCapture> VC = FVoiceModule::Get().CreateVoiceCapture(Info.DeviceName, DeviceRate, 1);
        if (!VC.IsValid()) return nullptr;
        SampleRate = DeviceRate;
        return VC;
    }
    return nullptr;
}

static TSharedPtr<IVoiceCapture> CreateWorkingVoiceCapture(int32& SampleRate, int32& NumChannels, const FString& PreferredDeviceName)
{
    NumChannels = 1;
    SampleRate  = SanitizeSampleRate(SampleRate);

    // Try the user's preferred device first.
    if (!PreferredDeviceName.IsEmpty())
    {
        TSharedPtr<IVoiceCapture> VC = TryOpenDevice(PreferredDeviceName, SampleRate);
        if (VC.IsValid())
        {
            UE_LOG(LogNeocortex, Log, TEXT("Using preferred mic device: %s"), *PreferredDeviceName);
            return VC;
        }
        UE_LOG(LogNeocortex, Warning, TEXT("Preferred mic device '%s' unavailable, falling back"), *PreferredDeviceName);
    }

    // Fall back: iterate all devices and take the first working one.
    TArray<Audio::FCaptureDeviceInfo> Devices;
    Audio::FAudioCapture Capture;
    if (Capture.GetCaptureDevicesAvailable(Devices))
    {
        for (const Audio::FCaptureDeviceInfo& Info : Devices)
        {
            const int32 DeviceRate = SanitizeSampleRate(Info.PreferredSampleRate);
            TSharedPtr<IVoiceCapture> VC = FVoiceModule::Get().CreateVoiceCapture(Info.DeviceName, DeviceRate, 1);
            if (!VC.IsValid()) continue;
            SampleRate = DeviceRate;
            UE_LOG(LogNeocortex, Log, TEXT("Using mic device: %s at %d Hz"), *Info.DeviceName, DeviceRate);
            return VC;
        }
    }

    TSharedPtr<IVoiceCapture> DefaultVC = FVoiceModule::Get().CreateVoiceCapture(TEXT(""), SampleRate, 1);
    if (DefaultVC.IsValid())
    {
        return DefaultVC;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Resampling
// ---------------------------------------------------------------------------

// Downmix stereo PCM16 bytes to a mono int16 array.
static void DownmixStereoToMono16(const uint8* InStereoBytes, int32 NumBytes, TArray<int16>& OutMono16)
{
    const int32 NumSamples16 = NumBytes / sizeof(int16);
    const int16* Src = reinterpret_cast<const int16*>(InStereoBytes);
    OutMono16.Reset();
    OutMono16.Reserve(NumSamples16 / 2);
    for (int32 i = 0; i + 1 < NumSamples16; i += 2)
    {
        const int32 L = Src[i];
        const int32 R = Src[i + 1];
        OutMono16.Add(static_cast<int16>((L + R) / 2));
    }
}

// Linear-interpolation resampler. Works for any rate ratio.
// Not studio quality, but avoids the aliasing of naive decimation.
// For voice/speech this is more than sufficient.
static void ResampleMono(const TArray<int16>& In, int32 InRate, TArray<int16>& Out, int32 OutRate)
{
    if (InRate == OutRate)
    {
        Out = In;
        return;
    }
    if (In.IsEmpty())
    {
        Out.Empty();
        return;
    }

    const double Ratio    = static_cast<double>(InRate) / OutRate;
    const int32  OutCount = FMath::Max(1, static_cast<int32>(In.Num() / Ratio));
    Out.SetNumUninitialized(OutCount);

    for (int32 i = 0; i < OutCount; ++i)
    {
        const double SrcPos = i * Ratio;
        const int32  Idx0   = static_cast<int32>(SrcPos);
        const int32  Idx1   = FMath::Min(Idx0 + 1, In.Num() - 1);
        const double Frac   = SrcPos - Idx0;
        Out[i] = static_cast<int16>(In[Idx0] + static_cast<int32>((In[Idx1] - In[Idx0]) * Frac));
    }
}

// ---------------------------------------------------------------------------
// FNeocortexMicrophoneRecorder
// ---------------------------------------------------------------------------

FNeocortexMicrophoneRecorder::FNeocortexMicrophoneRecorder(int32 InSampleRate, int32 InNumChannels)
    : SampleRate(SanitizeSampleRate(InSampleRate)), NumChannels(1) // enforce mono
{
    VoiceCapture = CreateWorkingVoiceCapture(SampleRate, NumChannels, PreferredDeviceName);
    // Prewarm: start the device now so the hardware pipeline is ready before the
    // first StartRecording() call. Without this, the first ~300ms of speech is
    // lost while the OS microphone driver initialises.
    StartPrewarm();
}

FNeocortexMicrophoneRecorder::~FNeocortexMicrophoneRecorder()
{
    bIsRecording = false;
    StopPrewarm();
}

void FNeocortexMicrophoneRecorder::StartPrewarm()
{
    if (!VoiceCapture.IsValid() || bIsPrewarmed)
        return;

    if (VoiceCapture->Start())
    {
        bIsPrewarmed = true;
        UE_LOG(LogNeocortex, Log, TEXT("Mic prewarmed at %d Hz"), SampleRate);
    }
    else
    {
        UE_LOG(LogNeocortex, Warning, TEXT("Mic prewarm failed"));
    }
}

void FNeocortexMicrophoneRecorder::StopPrewarm()
{
    if (VoiceCapture.IsValid() && bIsPrewarmed)
    {
        VoiceCapture->Stop();
        bIsPrewarmed = false;
        UE_LOG(LogNeocortex, Log, TEXT("Mic prewarm stopped"));
    }
}

void FNeocortexMicrophoneRecorder::SetPreferredDevice(const FString& DeviceName)
{
    PreferredDeviceName = DeviceName;

    bIsRecording = false;
    StopPrewarm();

    VoiceCapture = CreateWorkingVoiceCapture(SampleRate, NumChannels, PreferredDeviceName);
    StartPrewarm();
}

void FNeocortexMicrophoneRecorder::ListInputDevices() const
{
    TArray<Audio::FCaptureDeviceInfo> Devices;
    Audio::FAudioCapture Capture;

    if (!Capture.GetCaptureDevicesAvailable(Devices))
    {
        UE_LOG(LogNeocortex, Warning, TEXT("No audio capture devices found"));
        return;
    }

    UE_LOG(LogNeocortex, Log, TEXT("Audio capture devices: %d"), Devices.Num());
    for (int32 i = 0; i < Devices.Num(); ++i)
    {
        const auto& Info = Devices[i];
        UE_LOG(LogNeocortex, Log, TEXT("[%d] %s | PrefRate=%d Hz | InCh=%d"),
               i, *Info.DeviceName, Info.PreferredSampleRate, Info.InputChannels);
    }
}

bool FNeocortexMicrophoneRecorder::StartRecording()
{
    if (!VoiceCapture.IsValid())
    {
        VoiceCapture = CreateWorkingVoiceCapture(SampleRate, NumChannels, PreferredDeviceName);
        if (!VoiceCapture.IsValid())
        {
            UE_LOG(LogNeocortex, Error, TEXT("Failed to create VoiceCapture device"));
            return false;
        }
    }

    PcmBuffer.Reset();

    if (bIsPrewarmed)
    {
        // Device is already running — just start accumulating. No startup latency.
        bIsRecording = true;
        UE_LOG(LogNeocortex, Log, TEXT("Recording started (prewarmed) at %d Hz mono"), SampleRate);
        return true;
    }

    // Cold start fallback (prewarm failed earlier).
    UE_LOG(LogNeocortex, Log, TEXT("Recording cold-start at %d Hz mono"), SampleRate);
    bIsRecording = VoiceCapture->Start();
    if (!bIsRecording)
    {
        UE_LOG(LogNeocortex, Error, TEXT("VoiceCapture start failed"));
        return false;
    }
    bIsPrewarmed = true;
    return true;
}

void FNeocortexMicrophoneRecorder::StopRecording()
{
    if (!bIsRecording)
        return;

    UE_LOG(LogNeocortex, Log, TEXT("Recording stopped — %d bytes captured"), PcmBuffer.Num());
    bIsRecording = false;
    // Keep VoiceCapture running as prewarm so the next StartRecording() is instant.
}

void FNeocortexMicrophoneRecorder::Tick(float /*DeltaTime*/)
{
    // Always drain the VoiceCapture buffer, even when not recording, so the
    // internal ring buffer doesn't overflow and stale data never bleeds into
    // the next recording.
    if (!VoiceCapture.IsValid() || !bIsPrewarmed)
        return;

    uint32 BytesAvailable = 0;
    const EVoiceCaptureState::Type State = VoiceCapture->GetCaptureState(BytesAvailable);

    if (State == EVoiceCaptureState::Ok && BytesAvailable > 0)
    {
        TArray<uint8> Temp;
        Temp.SetNumUninitialized(BytesAvailable);
        uint32 ReadBytes = 0;
        VoiceCapture->GetVoiceData(Temp.GetData(), BytesAvailable, ReadBytes);
        if (ReadBytes > 0)
        {
            // Accumulate only during an active recording.
            if (bIsRecording)
            {
                PcmBuffer.Append(Temp.GetData(), ReadBytes);
            }

            // Always update amplitude (drives the UI bar even before recording starts).
            const int32 NumSamples = static_cast<int32>(ReadBytes) / sizeof(int16);
            const int16* Samples = reinterpret_cast<const int16*>(Temp.GetData());
            float SumSq = 0.f;
            for (int32 i = 0; i < NumSamples; ++i)
            {
                const float S = Samples[i] / 32768.f;
                SumSq += S * S;
            }
            Amplitude = NumSamples > 0 ? FMath::Sqrt(SumSq / NumSamples) : 0.f;
        }
    }
    else
    {
        Amplitude = FMath::Max(0.f, Amplitude - 0.05f);
    }
}

TArray<uint8> FNeocortexMicrophoneRecorder::GetWavData() const
{
    // PcmBuffer holds 16-bit PCM mono at SampleRate.
    return FNeocortexWavEncoder::EncodePcm16ToWav(PcmBuffer, SampleRate, /*Channels*/ 1);
}

TArray<uint8> FNeocortexMicrophoneRecorder::GetWavData16kMono() const
{
    if (SampleRate == 16000)
    {
        // Already at target rate.
        return FNeocortexWavEncoder::EncodePcm16ToWav(PcmBuffer, 16000, 1);
    }

    const int32 NumSamples = PcmBuffer.Num() / sizeof(int16);
    TArray<int16> MonoIn;
    MonoIn.SetNumUninitialized(NumSamples);
    FMemory::Memcpy(MonoIn.GetData(), PcmBuffer.GetData(), PcmBuffer.Num());

    TArray<int16> Mono16k;
    ResampleMono(MonoIn, SampleRate, Mono16k, 16000);

    return FNeocortexWavEncoder::EncodePcm16ToWav(
        TArray<uint8>(reinterpret_cast<const uint8*>(Mono16k.GetData()), Mono16k.Num() * sizeof(int16)),
        16000, 1);
}

bool FNeocortexMicrophoneRecorder::Supports16kMono() const
{
    // Linear interpolation handles any rate, but only flag rates we tested.
    return SampleRate == 16000 || SampleRate == 48000;
}
