#include "NeocortexMicrophoneRecorderComponent.h"
#include "Neocortex.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Misc/FileHelper.h"
#include "NeocortexWavEncoder.h"

// A loud sound shorter than this is a click or a knock, not speech.
static constexpr float ShortestSpeechSeconds = 0.15f;

UNeocortexMicrophoneRecorderComponent::UNeocortexMicrophoneRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNeocortexMicrophoneRecorderComponent::BeginPlay()
{
	Super::BeginPlay();
	Recorder = MakeUnique<FNeocortexMicrophoneRecorder>(SampleRate, NumChannels);

	if (SmartAgent)
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		SmartAgent = Owner->FindComponentByClass<UNeocortexSmartAgent>();
	}

	if (!SmartAgent)
	{
		UE_LOG(LogNeocortex, Log, TEXT("No SmartAgent assigned or found on owner; transcription disabled"));
	}
}


void UNeocortexMicrophoneRecorderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopRecording(false);
	Recorder.Reset();
	Super::EndPlay(EndPlayReason);
}

bool UNeocortexMicrophoneRecorderComponent::HasMicrophonePermission() const
{
#if PLATFORM_ANDROID
	return UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.RECORD_AUDIO"));
#else
	// iOS: permission is controlled by NSMicrophoneUsageDescription in plist; OS prompts automatically.
	// Desktop: always available.
	return true;
#endif
}

void UNeocortexMicrophoneRecorderComponent::RequestMicrophonePermission()
{
#if PLATFORM_ANDROID
	TArray<FString> Permissions;
	Permissions.Add(TEXT("android.permission.RECORD_AUDIO"));
	UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
#endif
}

bool UNeocortexMicrophoneRecorderComponent::StartRecording()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	if (!HasMicrophonePermission())
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Microphone permission not granted"));
		RequestMicrophonePermission();
		return false;
	}
#endif

	if (!Recorder.IsValid())
	{
		Recorder = MakeUnique<FNeocortexMicrophoneRecorder>(SampleRate, NumChannels);
	}

	// The engine's voice capture applies these to every frame it hands over.
	if (IConsoleVariable* Silence = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionThreshold")))
	{
		Silence->Set(SilenceThreshold, ECVF_SetByCode);
	}
	if (IConsoleVariable* Gate = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.MicNoiseGateThreshold")))
	{
		Gate->Set(SilenceThreshold, ECVF_SetByCode);
	}

	LastConsumedBytes = 0;
	bool bSuccess = Recorder.IsValid() ? Recorder->StartRecording() : false;
    
	if (!bSuccess)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Recording did not start; the message above says why."));
	}
    
	return bSuccess;
}

bool UNeocortexMicrophoneRecorderComponent::StartListening()
{
	if (bListening) return true;
	if (IsRecording()) StopRecording(false);
	if (!StartRecording()) return false;
	bListening = true;
	SpeechStartByte = INDEX_NONE;
	SpeechTime = QuietTime = 0.f;
	return true;
}

void UNeocortexMicrophoneRecorderComponent::StopListening()
{
	if (!bListening) return;
	bListening = false;
	SpeechStartByte = INDEX_NONE;
	if (Recorder.IsValid() && Recorder->IsRecording()) Recorder->StopRecording();
}

// Speech is anything above VoiceThreshold. It ends after MaxWaitTime of quiet; a burst too short
// to be a word is forgotten and listening goes on.
void UNeocortexMicrophoneRecorderComponent::TickListening(float DeltaTime)
{
	const bool bLoud = GetLevel() >= VoiceThreshold;
	if (SpeechStartByte == INDEX_NONE)
	{
		if (!bLoud) return;
		const int32 LeadBytes = FMath::FloorToInt(LeadIn * Recorder->GetSampleRate()) * 2;
		SpeechStartByte = FMath::Max(0, Recorder->GetPcmData().Num() - LeadBytes);
		SpeechTime = QuietTime = 0.f;
		OnSpeechStarted.Broadcast();
		return;
	}

	SpeechTime += DeltaTime;
	QuietTime = bLoud ? 0.f : QuietTime + DeltaTime;
	if (QuietTime < MaxWaitTime) return;
	if (SpeechTime - QuietTime < ShortestSpeechSeconds)
	{
		SpeechStartByte = INDEX_NONE;
		OnSpeechEnded.Broadcast();
		return;
	}
	FinishSpeech();
}

void UNeocortexMicrophoneRecorderComponent::FinishSpeech()
{
	const int32 From = SpeechStartByte;
	bListening = false;
	SpeechStartByte = INDEX_NONE;
	Recorder->StopRecording();
	OnSpeechEnded.Broadcast();

	const TArray<uint8>& Pcm = Recorder->GetPcmData();
	TArray<uint8> Speech;
	Speech.Append(Pcm.GetData() + From, Pcm.Num() - From);
	LastWavBytes = FNeocortexWavEncoder::EncodePcm16ToWav(Speech, Recorder->GetSampleRate(), 1);
	OnWavReady.Broadcast(LastWavBytes);
}

void UNeocortexMicrophoneRecorderComponent::StopRecording(bool bAutoTranscribe)
{
	// While listening nothing is sent until the player pauses, so stopping just stops.
	if (bListening)
	{
		StopListening();
		return;
	}
	if (!Recorder.IsValid() || !Recorder->IsRecording())
		return;

	Recorder->StopRecording();

	// Always encode at the device's native sample rate. Transcription handles any rate, and
	// native-rate audio avoids the quality loss of client-side resampling.
	const TArray<uint8> Wav = Recorder->GetWavData();
	// A WAV header alone (44 bytes) holds no speech; sending it would only fail.
	if (Wav.Num() <= 44)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Nothing was recorded. The log above names the microphone in use; pick another with SetPreferredDevice if it is the wrong one."));
		return;
	}
	OnWavReady.Broadcast(Wav);
	LastWavBytes = Wav;

	if (bAutoTranscribe && SmartAgent)
	{
		UE_LOG(LogNeocortex, Log, TEXT("Auto-transcribing recorded audio (%d bytes, %d Hz)"),
			Wav.Num(), Recorder->GetSampleRate());
		SmartAgent->TranscribeBytes(Wav);
	}
}

void UNeocortexMicrophoneRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Recorder.IsValid())
	{
		// Always tick so the VoiceCapture buffer is drained during prewarm.
		// Without this, stale audio from the prewarm period would bleed into
		// the start of the next recording.
		Recorder->Tick(DeltaTime);

		if (Recorder->IsRecording())
		{
			EmitNewChunks();
			if (bListening) TickListening(DeltaTime);
		}
	}
}

void UNeocortexMicrophoneRecorderComponent::EmitNewChunks()
{
	const TArray<uint8>& Full = Recorder->GetPcmData();
	const int32 NewBytes = Full.Num() - LastConsumedBytes;
	if (NewBytes > 0)
	{
		TArray<uint8> Slice;
		Slice.Append(Full.GetData() + LastConsumedBytes, NewBytes);
		LastConsumedBytes = Full.Num();
		OnPcmChunk.Broadcast(Slice);
	}
}

void UNeocortexMicrophoneRecorderComponent::DebugPlayLastRecording()
{
	if (LastWavBytes.Num() == 0)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("No cached WAV to play"));
		return;
	}

	FWaveModInfo WaveInfo;
	if (!WaveInfo.ReadWaveInfo(LastWavBytes.GetData(), LastWavBytes.Num()))
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Invalid WAV format"));
		return;
	}

	const int32 TargetSampleRate = *WaveInfo.pSamplesPerSec;
	const int32 TargetChannels   = *WaveInfo.pChannels;
	const int32 BitsPerSample    = *WaveInfo.pBitsPerSample;
	const uint8* PcmData         = WaveInfo.SampleDataStart;
	const int32 PcmSize          = WaveInfo.SampleDataSize;

	if (BitsPerSample != 16)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Only 16-bit PCM supported for debug playback"));
		return;
	}

	const float DurationSec = (float)PcmSize / (TargetChannels * (BitsPerSample / 8) * TargetSampleRate);
	UE_LOG(LogNeocortex, Log, TEXT("Debug WAV: %d Hz, %d ch, %d bits, %.2fs"), TargetSampleRate, TargetChannels, BitsPerSample, DurationSec);

	if (!DebugSoundWave)
	{
		DebugSoundWave = NewObject<USoundWaveProcedural>(this);
		DebugSoundWave->SoundGroup   = SOUNDGROUP_Voice;
		DebugSoundWave->bLooping     = false;
	}

	DebugSoundWave->ResetAudio();
	DebugSoundWave->SetSampleRate(TargetSampleRate);
	DebugSoundWave->NumChannels = TargetChannels;
	DebugSoundWave->Duration    = DurationSec;

	DebugSoundWave->QueueAudio(PcmData, PcmSize);

	if (AActor* Owner = GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(Owner, DebugSoundWave, Owner->GetActorLocation());
	}
}

void UNeocortexMicrophoneRecorderComponent::DebugSaveLastRecording()
{
	if (LastWavBytes.Num() == 0)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("No cached WAV to save"));
		return;
	}

	const FString Dir  = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Neocortex"));
	const FString Path = FPaths::Combine(Dir, TEXT("LastRecording.wav"));
	IFileManager::Get().MakeDirectory(*Dir, true);

	if (FFileHelper::SaveArrayToFile(LastWavBytes, *Path))
	{
		UE_LOG(LogNeocortex, Log, TEXT("Saved recording to %s"), *Path);
	}
	else
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Failed to save recording"));
	}
}
