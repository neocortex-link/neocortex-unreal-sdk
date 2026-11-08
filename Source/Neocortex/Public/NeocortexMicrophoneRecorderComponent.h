#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexMicrophoneRecorder.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexMicrophoneRecorderComponent.generated.h"

class USoundWaveProcedural;

/** Fired when new PCM audio data is captured during recording. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoicePcmChunk, const TArray<uint8>&, PcmChunk);

/** Fired when recording stops and complete WAV file is ready. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVoiceWavReady, const TArray<uint8>&, WavData);

/** Fired while listening, when the player starts or stops speaking. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVoiceActivity);

/**
 * Blueprint-accessible component for capturing microphone input and optionally transcribing speech.
 * Wraps FNeocortexMicrophoneRecorder and provides event-driven audio capture.
 * Can automatically send recordings to a SmartAgent for transcription.
 */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexMicrophoneRecorderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNeocortexMicrophoneRecorderComponent();

    /** Fired periodically during recording as new audio chunks are captured. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnVoicePcmChunk OnPcmChunk;

    /** Fired once when recording stops with the complete WAV file. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnVoiceWavReady OnWavReady;

    /** Optional SmartAgent reference for automatic transcription when recording stops. */
    UPROPERTY(BlueprintReadWrite, Category="Neocortex")
    TObjectPtr<UNeocortexSmartAgent> SmartAgent;

    /** Target sample rate in Hz (default 16000 for speech recognition). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Recording")
    int32 SampleRate = 16000;

    /** Number of audio channels (1=mono, 2=stereo, default 1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Recording")
    int32 NumChannels = 1;

    /**
     * Unreal's voice capture drops audio quieter than this level (0 to 1) while recording. The
     * engine's own default, 0.08, cuts off quiet or distant voices, so recording lowers it.
     * Raise it to ignore more background noise.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Recording", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SilenceThreshold = 0.02f;

    /** While listening, a level (GetLevel) above this counts as speech. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Voice Detection", meta=(ClampMin="0.0", ClampMax="1.0"))
    float VoiceThreshold = 0.15f;

    /** While listening, seconds of quiet after speech before the recording is finished and sent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Voice Detection", meta=(ClampMin="0.1"))
    float MaxWaitTime = 1.0f;

    /** While listening, seconds kept from before the voice crossed the threshold, so a soft first sound is not cut. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Voice Detection", meta=(ClampMin="0.0", ClampMax="1.0"))
    float LeadIn = 0.3f;

    /** While listening, fired when the player starts speaking. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnVoiceActivity OnSpeechStarted;

    /** While listening, fired when the player stops speaking. A recording follows through OnWavReady unless the sound was too short to be speech. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnVoiceActivity OnSpeechEnded;

    /**
     * Opens the microphone and waits for the player to speak. Speech starts a recording; once the
     * player has been quiet for MaxWaitTime it is sent through OnWavReady and listening stops.
     * Call it again for the next answer, for example when the character has finished replying.
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Voice Detection")
    bool StartListening();

    /** Stops listening without sending anything. */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Voice Detection")
    void StopListening();

    /** True while waiting for, or recording, the player's voice. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Voice Detection")
    bool IsListening() const { return bListening; }

    /** True while listening and the player is speaking. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Voice Detection")
    bool IsUserSpeaking() const { return bListening && SpeechStartByte != INDEX_NONE; }

    /** How far the quiet after speech has run towards MaxWaitTime, 0 to 1. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Voice Detection")
    float GetWaitProgress() const { return IsUserSpeaking() ? FMath::Clamp(QuietTime / FMath::Max(MaxWaitTime, 0.1f), 0.f, 1.f) : 0.f; }

    /**
     * Begins capturing audio from the default microphone device.
     * @return True if recording started successfully, false if permission denied or device unavailable
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Recording")
    bool StartRecording();

    /**
     * Stops capturing audio and optionally sends result to SmartAgent for transcription.
     * @param bAutoTranscribe If true and SmartAgent is set, automatically transcribe the recording
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Recording")
    void StopRecording(bool bAutoTranscribe = true);

    /** Returns true if currently capturing audio. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Recording")
    bool IsRecording() const { return Recorder.IsValid() && Recorder->IsRecording(); }

    /** Returns the RMS amplitude of the most recent audio chunk (0 to 1). Useful for driving amplitude UI. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Recording")
    float GetAmplitude() const { return Recorder.IsValid() ? Recorder->GetAmplitude() : 0.f; }

    /** The microphone level on a 0 to 1 scale that suits meters and VoiceThreshold. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Recording")
    float GetLevel() const { return FMath::Clamp(GetAmplitude() * 10.f, 0.f, 1.f); }

    /** Returns display names of all available audio input devices. */
    UFUNCTION(BlueprintPure, Category="Neocortex|Recording")
    static TArray<FString> GetAvailableDeviceNames() { return FNeocortexMicrophoneRecorder::GetAvailableDeviceNames(); }

    /**
     * Sets the preferred input device by name (must match a name from GetAvailableDeviceNames).
     * Takes effect immediately: stops and restarts the capture device if recording.
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Recording")
    void SetPreferredDevice(const FString& DeviceName) { if (Recorder.IsValid()) Recorder->SetPreferredDevice(DeviceName); }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Checks if the application has microphone access permission. */
    bool HasMicrophonePermission() const;

    /** Requests microphone permission from the operating system (mobile platforms). */
    void RequestMicrophonePermission();

    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** Low-level microphone recorder instance. */
    TUniquePtr<FNeocortexMicrophoneRecorder> Recorder;

    /** Tracks how many bytes have been consumed for chunk emission. */
    int32 LastConsumedBytes = 0;

    bool bListening = false;
    /** Where in the recording speech began, or INDEX_NONE while waiting for it. */
    int32 SpeechStartByte = INDEX_NONE;
    float SpeechTime = 0.f;
    float QuietTime = 0.f;

    void TickListening(float DeltaTime);
    void FinishSpeech();

    /** Broadcasts new PCM chunks via OnPcmChunk delegate. */
    void EmitNewChunks();

    /** Debug utility: plays back the last recorded audio. */
    void DebugPlayLastRecording();

    /** Debug utility: saves the last recording to disk. */
    void DebugSaveLastRecording();

    /** Procedural sound wave used for debug playback. */
    UPROPERTY()
    USoundWaveProcedural* DebugSoundWave = nullptr;

    /** Cached WAV data from last recording for debug purposes. */
    TArray<uint8> LastWavBytes;
};
