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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex", meta=(UseComponentPicker))
    TObjectPtr<UNeocortexSmartAgent> SmartAgent;

    /** Target sample rate in Hz (default 16000 for speech recognition). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Recording")
    int32 SampleRate = 16000;

    /** Number of audio channels (1=mono, 2=stereo, default 1). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex|Recording")
    int32 NumChannels = 1;

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
