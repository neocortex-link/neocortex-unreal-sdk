#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexAudioChatInputWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAudioRecordingStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioRecordingComplete, const TArray<uint8>&, WavData);

/**
 * Audio chat input widget — drives a UNeocortexMicrophoneRecorderComponent and optionally
 * forwards the recording to a UNeocortexSmartAgent for the full voice-in / voice-out flow.
 *
 * Setup in Blueprint:
 *   1. Add a UNeocortexMicrophoneRecorderComponent to your actor.
 *   2. Assign it to the Recorder property on this widget.
 *   3. Optionally assign a SmartAgent and enable bAutoSendOnStop.
 *
 * Supports two modes (set bPushToTalk):
 *   - Push-to-talk: bind RecordButton; hold to record, release to stop.
 *   - Manual: call StartRecording / StopRecording from Blueprint or C++.
 */
UCLASS()
class NEOCORTEX_API UNeocortexAudioChatInputWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Microphone recorder component — must be assigned before recording. */
    UPROPERTY(BlueprintReadWrite, Category="Neocortex")
    TObjectPtr<UNeocortexMicrophoneRecorderComponent> Recorder;

    /** Optional SmartAgent. When set and bAutoSendOnStop is true, WAV is sent via SendAudioForAudio. */
    UPROPERTY(BlueprintReadWrite, Category="Neocortex")
    TObjectPtr<UNeocortexSmartAgent> SmartAgent;

    /** When true, StopRecording() automatically calls SmartAgent->SendAudioForAudio. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex")
    bool bAutoSendOnStop = true;

    /**
     * Push-to-talk mode: bind a RecordButton widget — press starts recording, release stops it.
     * When false, call StartRecording / StopRecording manually.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Neocortex")
    bool bPushToTalk = false;

    // Optional child widgets — add these to your Blueprint layout as needed.

    /** Progress bar driven by microphone amplitude (0–1 scaled). BindWidgetOptional. */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UProgressBar> AmplitudeBar;

    /** Button used in push-to-talk mode. BindWidgetOptional. */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    TObjectPtr<UButton> RecordButton;

    /** Fired when recording begins. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnAudioRecordingStarted OnRecordingStarted;

    /** Fired when recording ends, with the captured WAV bytes. */
    UPROPERTY(BlueprintAssignable, Category="Neocortex|Events")
    FOnAudioRecordingComplete OnRecordingComplete;

    /** Begin capturing audio. No-op if already recording or Recorder is null. */
    UFUNCTION(BlueprintCallable, Category="Neocortex")
    void StartRecording();

    /** Stop capturing audio. Fires OnRecordingComplete; optionally forwards to SmartAgent. */
    UFUNCTION(BlueprintCallable, Category="Neocortex")
    void StopRecording();

    /** Returns true if currently capturing audio. */
    UFUNCTION(BlueprintPure, Category="Neocortex")
    bool IsRecording() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleButtonPressed();

    UFUNCTION()
    void HandleButtonReleased();

    UFUNCTION()
    void HandleWavReady(const TArray<uint8>& WavData);
};
