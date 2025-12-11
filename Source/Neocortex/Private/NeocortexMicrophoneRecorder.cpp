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

static TSharedPtr<IVoiceCapture> CreateWorkingVoiceCapture(int32& SampleRate, int32& NumChannels)
{
    // Enforce mono input; most voice capture APIs deliver mono 16-bit PCM
    NumChannels = 1;
    SampleRate  = SanitizeSampleRate(SampleRate);

    TArray<Audio::FCaptureDeviceInfo> Devices;
    Audio::FAudioCapture Capture;

    if (Capture.GetCaptureDevicesAvailable(Devices))
    {
        for (const auto& Info : Devices)
        {
            const int32 DeviceRate = SanitizeSampleRate(Info.PreferredSampleRate);
            UE_LOG(LogNeocortex, Log, TEXT("Trying VoiceCapture device: %s (pref %d Hz, %d ch)"),
                   *Info.DeviceName, Info.PreferredSampleRate, Info.InputChannels);

            // Force mono and sanitized rate
            TSharedPtr<IVoiceCapture> VC = FVoiceModule::Get().CreateVoiceCapture(
                Info.DeviceName, DeviceRate, /*Channels*/ 1);

            if (!VC.IsValid())
            {
                UE_LOG(LogNeocortex, Warning, TEXT("CreateVoiceCapture failed: %s"), *Info.DeviceName);
                continue;
            }
            if (!VC->Start())
            {
                UE_LOG(LogNeocortex, Warning, TEXT("VoiceCapture->Start() failed: %s"), *Info.DeviceName);
                continue;
            }
            VC->Stop();
            SampleRate = DeviceRate; // lock to working rate
            return VC;
        }
    }

    UE_LOG(LogNeocortex, Log, TEXT("Trying VoiceCapture default device"));
    TSharedPtr<IVoiceCapture> DefaultVC = FVoiceModule::Get().CreateVoiceCapture(TEXT(""));
    if (DefaultVC.IsValid())
    {
        if (DefaultVC->Start())
        {
            DefaultVC->Stop();
            return DefaultVC;
        }
    }

    return nullptr;
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


// Optional helpers if you ever receive stereo or need 16k output for STT.
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
        const int16 M = static_cast<int16>((L + R) / 2);
        OutMono16.Add(M);
    }
}

static void Decimate48kTo16k(const TArray<int16>& InMono48k, TArray<int16>& OutMono16k)
{
    OutMono16k.Reset();
    OutMono16k.Reserve(InMono48k.Num() / 3);
    for (int32 i = 0; i < InMono48k.Num(); i += 3)
    {
        OutMono16k.Add(InMono48k[i]);
    }
}

FNeocortexMicrophoneRecorder::FNeocortexMicrophoneRecorder(int32 InSampleRate, int32 InNumChannels)
    : SampleRate(SanitizeSampleRate(InSampleRate)), NumChannels(1) // enforce mono
{
    ListInputDevices();
    VoiceCapture = CreateWorkingVoiceCapture(SampleRate, NumChannels);
}

FNeocortexMicrophoneRecorder::~FNeocortexMicrophoneRecorder()
{
    StopRecording();
}

bool FNeocortexMicrophoneRecorder::StartRecording()
{
    if (!VoiceCapture.IsValid())
    {
        VoiceCapture = CreateWorkingVoiceCapture(SampleRate, NumChannels);
        if (!VoiceCapture.IsValid())
        {
            UE_LOG(LogNeocortex, Error, TEXT("Failed to create VoiceCapture device"));
            return false;
        }
    }

    UE_LOG(LogNeocortex, Log, TEXT("Starting microphone recording at %d Hz, %d ch"), SampleRate, NumChannels);

    PcmBuffer.Reset();
    bIsRecording = VoiceCapture->Start();
    if (!bIsRecording)
    {
        UE_LOG(LogNeocortex, Error, TEXT("VoiceCapture start failed"));
        return false;
    }
    return true;
}

void FNeocortexMicrophoneRecorder::StopRecording()
{
    if (VoiceCapture.IsValid() && bIsRecording)
    {
        UE_LOG(LogNeocortex, Log, TEXT("Stopping microphone recording"));
        VoiceCapture->Stop();
        bIsRecording = false;
    }
}

void FNeocortexMicrophoneRecorder::Tick(float /*DeltaTime*/)
{
    if (!bIsRecording || !VoiceCapture.IsValid())
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
            PcmBuffer.Append(Temp.GetData(), ReadBytes);
        }
    }
}

TArray<uint8> FNeocortexMicrophoneRecorder::GetWavData() const
{
    // PcmBuffer is expected as 16-bit PCM mono at SampleRate
    return FNeocortexWavEncoder::EncodePcm16ToWav(PcmBuffer, SampleRate, /*Channels*/ 1);
}

// If you need 16k mono output for STT:
TArray<uint8> FNeocortexMicrophoneRecorder::GetWavData16kMono() const
{
    // Interpret PcmBuffer as int16 mono at SampleRate
    const int32 NumSamples = PcmBuffer.Num() / sizeof(int16);
    const int16* Src = reinterpret_cast<const int16*>(PcmBuffer.GetData());

    TArray<int16> MonoIn;
    MonoIn.SetNumUninitialized(NumSamples);
    FMemory::Memcpy(MonoIn.GetData(), Src, PcmBuffer.Num());

    TArray<int16> Mono16k;
    if (SampleRate == 48000)
    {
        Decimate48kTo16k(MonoIn, Mono16k);
    }
    else if (SampleRate == 16000)
    {
        Mono16k = MonoIn;
    }
    else
    {
        // Fallback: keep as-is; add proper resampler for other rates if needed
        Mono16k = MonoIn;
    }

    // Encode as 16k mono WAV
    return FNeocortexWavEncoder::EncodePcm16ToWav(
        TArray<uint8>((uint8*)Mono16k.GetData(), Mono16k.Num() * sizeof(int16)),
        16000, 1);
}

bool FNeocortexMicrophoneRecorder::Supports16kMono() const
{
    // Native 16k or 48k (exact decimation factor of 3) are supported
    return SampleRate == 16000 || SampleRate == 48000;
}