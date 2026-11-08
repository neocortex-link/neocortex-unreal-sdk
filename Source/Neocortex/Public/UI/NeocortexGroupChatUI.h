#pragma once
#include "CoreMinimal.h"
#include "UI/NeocortexChatUI.h"
#include "NeocortexGroupChatUI.generated.h"

class UNeocortexGroupDirector;

/**
 * The chat for a group scene: on the left over a fading backdrop, each speaker's turn a bubble
 * with its name and initial, a text field and voice controls under it. Whatever the player types
 * or says goes to the whole group.
 *
 *   UNeocortexGroupChatUI* Chat = CreateWidget<UNeocortexGroupChatUI>(PlayerController, UNeocortexGroupChatUI::StaticClass());
 *   Chat->SetDirector(Director);
 *   Chat->SetMicrophone(Microphone);
 *   Chat->AddToViewport();
 */
UCLASS()
class NEOCORTEX_API UNeocortexGroupChatUI : public UNeocortexChatUI
{
    GENERATED_BODY()
public:
    UNeocortexGroupChatUI(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetDirector(UNeocortexGroupDirector* InDirector);

protected:
    virtual void NativeDestruct() override;
    virtual void Submit(const FString& Text) override;
    virtual void SubmitAudio(const TArray<uint8>& Wav) override;
    virtual bool IsBusy() const override;
    virtual bool CanTalk() const override;

    UPROPERTY() TObjectPtr<UNeocortexGroupDirector> Director;

private:
    UFUNCTION() void HandleSpeaker(const FNeocortexGroupMessage& Message);
    UFUNCTION() void HandleTurnStarted();
    UFUNCTION() void HandleTurnFinished();
    UFUNCTION() void HandlePlayerSpeech(const FString& Text);
    UFUNCTION() void HandleGroupError(const FNeocortexRequestError& Error);

    void BindDirector(bool bBind);
};
