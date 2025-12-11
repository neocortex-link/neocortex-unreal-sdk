#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "NeocortexChatMessageWidget.generated.h"

/**
 * Widget for displaying individual chat messages in the Neocortex UI.
 * Handles message styling, layout direction (LTR/RTL), and visual differentiation between user and agent messages.
 */
UCLASS()
class NEOCORTEX_API UNeocortexChatMessageWidget : public UUserWidget
{
 GENERATED_BODY()

public:
 /**
  * Configures the message display with text, sender type, and reading direction.
  * @param Text The message content to display
  * @param bIsUser True if message is from user, false if from agent
  * @param bIsLTR True for left-to-right text, false for right-to-left (Arabic, Hebrew, etc.)
  */
 UFUNCTION(BlueprintCallable, Category = "Chat")
 void SetMessage(const FString& Text, bool bIsUser, bool bIsLTR = false);

protected:
 /** Text block displaying the message content */
 UPROPERTY(meta = (BindWidget))
 UTextBlock* MessageText;

 /** Border providing background color and styling for the message */
 UPROPERTY(meta = (BindWidget))
 UBorder* Background;

 /** Container for managing horizontal layout and padding */
 UPROPERTY(meta = (BindWidget))
 UHorizontalBox* LayoutBox;

 /** Left padding spacer for message alignment */
 UPROPERTY(meta = (BindWidget))
 UWidget* LeftPadding;

 /** Right padding spacer for message alignment */
 UPROPERTY(meta = (BindWidget))
 UWidget* RightPadding;

 virtual void NativeConstruct() override;

private:
 /** Background color for user messages (blue tone) */
 const FLinearColor UserMessageColor = FLinearColor(0.0f, 0.5f, 0.9f, 1.0f);

 /** Background color for agent messages (gray tone) */
 const FLinearColor AgentMessageColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

 /**
  * Corrects text rendering for right-to-left languages.
  * @param Input The original text string
  * @return Text with proper RTL formatting applied
  */
 FString CorrectRTL(const FString& Input) const;
};
