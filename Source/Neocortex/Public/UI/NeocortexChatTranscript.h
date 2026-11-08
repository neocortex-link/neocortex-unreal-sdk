#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexChatTranscript.generated.h"

class UScrollBox;
class UHorizontalBox;
class UBorder;
class UImage;

/**
 * A scrolling list of chat bubbles, built in code, newest at the bottom. Each bubble is as wide as
 * its text, with the sender's initial in a round avatar on the outer edge; the player's bubbles are
 * amber on the right, the characters' dark on the left. A bubble of pulsing dots shows while a
 * reply is on its way, and older messages fade out toward the top.
 */
UCLASS()
class NEOCORTEX_API UNeocortexChatTranscript : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Show the sender's initial in a round avatar beside each named bubble. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    bool bShowAvatars = true;

    /** Show the sender's name inside each bubble, useful when several characters talk. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    bool bShowSenderNames = false;

    /** Messages fade from clear at the top to opaque at this share of the height; 0 turns it off. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Fade = 0.6f;

    /** The widest a bubble's text gets before it wraps, in pixels at 1920 x 1080. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    float MaxTextWidth = 560.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FLinearColor PlayerBackground;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FLinearColor PlayerText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FLinearColor CharacterBackground;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FLinearColor CharacterText;

    UNeocortexChatTranscript(const FObjectInitializer& ObjectInitializer);

    /** Adds a bubble. Sender labels it and gives the avatar its initial. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void AddMessage(const FString& Text, bool bIsUser, const FString& Sender);

    /** Adds a centred line, for scene events and errors. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void AddNotice(const FString& Text, bool bIsError = false);

    /** Shows or hides the bubble of dots, for a reply on its way. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetTyping(bool bTyping);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void ClearMessages();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY() TObjectPtr<UScrollBox> List;
    UPROPERTY() TObjectPtr<UWidget> TypingRow;
    UPROPERTY() TArray<TObjectPtr<UImage>> Dots;
    float TypingClock = 0.0f;
    bool bTypingShown = false;

    void BuildTree();
    void AddRow(UWidget* Row);
    void ScrollToEnd();
};
