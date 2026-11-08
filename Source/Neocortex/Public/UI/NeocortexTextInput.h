#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexTextInput.generated.h"

class UEditableTextBox;
class UButton;

/** Fired with what the player typed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexTextSubmitted, const FString&, Text);

/**
 * A pill text field and a round Send button, built in code. Enter or Send fires OnSubmitted with
 * the text and clears the field.
 */
UCLASS()
class NEOCORTEX_API UNeocortexTextInput : public UUserWidget
{
    GENERATED_BODY()
public:
    /** The grey hint in the empty field. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetHint(const FString& InHint);

    /** Locks or unlocks the field and the button. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetInputEnabled(bool bEnabled);

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|UI")
    FOnNeocortexTextSubmitted OnSubmitted;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY() TObjectPtr<UEditableTextBox> Field;
    UPROPERTY() TObjectPtr<UButton> SendButton;
    FString Hint = TEXT("Say something...");
    bool bEnabled = true;

    UFUNCTION() void HandleSend();
    UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type Method);
};
