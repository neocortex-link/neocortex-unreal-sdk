#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexVoiceInput.generated.h"

class UNeocortexMicrophoneRecorderComponent;
class UButton;
class UImage;
class UTexture2D;

/**
 * Voice controls built in code, three amber discs in a row: one that swells with the microphone
 * level, one that shows the state (a microphone while listening, a clock while the character
 * answers) ringed by the silence countdown, and in push to talk a button to hold while speaking,
 * in place of the state disc. Recording follows the button; listening on its own is left to the
 * owner (see UNeocortexChatUI).
 */
UCLASS()
class NEOCORTEX_API UNeocortexVoiceInput : public UUserWidget
{
    GENERATED_BODY()
public:
    /** The microphone the discs follow and the hold button records with. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetMicrophone(UNeocortexMicrophoneRecorderComponent* InRecorder);

    /** Shows the hold button instead of the state disc. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetPushToTalk(bool bEnabled);

    /** True while the character answers: the clock shows and the hold button rests. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetBusy(bool bInBusy);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY() TObjectPtr<UNeocortexMicrophoneRecorderComponent> Recorder;
    UPROPERTY() TObjectPtr<UImage> LevelBar;
    UPROPERTY() TObjectPtr<UWidget> StateDisc;
    UPROPERTY() TObjectPtr<UImage> MicIcon;
    UPROPERTY() TObjectPtr<UImage> ClockIcon;
    UPROPERTY() TObjectPtr<UButton> HoldButton;

    bool bPushToTalk = false;
    bool bBusy = false;
    bool bHolding = false;
    float LevelScale = 0.1f;
    float WaitProgress = 0.0f;

    void BuildTree();
    void Apply();
    UFUNCTION() void HandlePressed();
    UFUNCTION() void HandleReleased();
};
