#include "NeocortexMicrophoneRecorderComponent.h"
#include "Neocortex.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

UNeocortexMicrophoneRecorderComponent::UNeocortexMicrophoneRecorderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNeocortexMicrophoneRecorderComponent::BeginPlay()
{
	Super::BeginPlay();
	Recorder = MakeUnique<FNeocortexMicrophoneRecorder>(SampleRate, NumChannels);
	
	if (Recorder.IsValid())
	{
		Recorder->ListInputDevices();
	}
	
	if (SmartAgent) return;

	// Cache existing smart agent; do not create a new one.
	if (const AActor* Owner = GetOwner())
	{
		SmartAgent = Owner->FindComponentByClass<UNeocortexSmartAgent>();
	}
	
	if (!SmartAgent)
	{
		UE_LOG( LogNeocortex, Log, TEXT("No SmartAgent assigned or found on owner; transcription disabled"));
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
#if PLATFORM_ANDROID || PLATFORM_IOS
	return UAndroidPermissionFunctionLibrary::CheckPermission("android.permission.RECORD_AUDIO");
#else
	return true;
#endif
}

void UNeocortexMicrophoneRecorderComponent::RequestMicrophonePermission()
{
#if PLATFORM_ANDROID || PLATFORM_IOS
	TArray<FString> Permissions;
	Permissions.Add("android.permission.RECORD_AUDIO");
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
		if (Recorder.IsValid())
		{
			Recorder->ListInputDevices();
		}
	}

	LastConsumedBytes = 0;
	bool bSuccess = Recorder.IsValid() ? Recorder->StartRecording() : false;
    
	if (!bSuccess)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Failed to start recording. Check OS microphone permissions."));
	}
    
	return bSuccess;
}

void UNeocortexMicrophoneRecorderComponent::StopRecording(bool bAutoTranscribe)
{
	if (!Recorder.IsValid() || !Recorder->IsRecording())
		return;

	Recorder->StopRecording();

	// Use device-native WAV for debug/playback
	const TArray<uint8> Wav = Recorder->GetWavData();
	OnWavReady.Broadcast(Wav);
	LastWavBytes = Wav;


	// For STT, prefer 16k mono if available
	if (bAutoTranscribe && SmartAgent)
	{
		UE_LOG(LogNeocortex, Log, TEXT("Auto-transcribing recorded audio"));
		if (Recorder->Supports16kMono())
		{
			const TArray<uint8> Wav16kMono = Recorder->GetWavData16kMono();
			SmartAgent->TranscribeBytes(Wav16kMono);
		}
		else
		{
			SmartAgent->TranscribeBytes(Wav);
		}
	}
}

void UNeocortexMicrophoneRecorderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Recorder.IsValid() && Recorder->IsRecording())
	{
		Recorder->Tick(DeltaTime);
		EmitNewChunks();
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
