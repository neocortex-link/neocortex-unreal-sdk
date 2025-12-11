#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enums/NeocortexEnumTypes.h"
#include "NeocortexChatPanelWidget.generated.h"

struct FNeocortexChatMessage;
class UNeocortexChatMessageWidget;

/**
 * Main chat panel widget for displaying conversation history with AI characters.
 * Manages message display, scrolling, and visual feedback like writing indicators.
 * Supports both left-to-right and right-to-left text rendering.
 */
UCLASS()
class NEOCORTEX_API UNeocortexChatPanelWidget : public UUserWidget
{
 GENERATED_BODY()

public:
 /**
  * Adds a single message to the chat panel.
  * @param Text The message content to display
  * @param bIsUser True if message is from user, false if from agent
  */
 UFUNCTION(BlueprintCallable, Category = "Neocortex/Chat")
 void AddMessage(const FString& Text, bool bIsUser);

 /**
  * Populates the chat panel with historical messages.
  * @param Messages Array of chat messages to display
  */
 UFUNCTION(BlueprintCallable, Category = "Neocortex/Chat")
 void AddChatHistory(const TArray<FNeocortexChatMessage>& Messages);

 /** Removes all messages from the chat panel. */
 UFUNCTION(BlueprintCallable, Category = "Neocortex/Chat")
 void ClearChatMessages();

 /** Text rendering direction for localization support (LTR/RTL). */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex/Chat")
 EWritingDirection WritingDirection = EWritingDirection::LeftToRight;

protected:
 /** Scrollable container for displaying chat messages. */
 UPROPERTY(meta = (BindWidget))
 class UScrollBox* ChatScrollBox;

 /** Widget class to instantiate for each chat message. */
 UPROPERTY(EditAnywhere, Category = "Neocortex/Chat")
 TSubclassOf<UNeocortexChatMessageWidget> MessageItemClass;

 /** Widget class to instantiate for the typing/writing indicator. */
 UPROPERTY(EditAnywhere, Category = "Neocortex/Chat")
 TSubclassOf<UNeocortexChatMessageWidget> WritingIndicatorClass;

 /** Active instance of the writing indicator widget. */
 UPROPERTY()
 UNeocortexChatMessageWidget* WritingIndicator;

 /** Creates and configures the writing indicator widget. */
 void CreateWritingIndicatorWidget();
 
 virtual void NativeConstruct() override;

private:
 /** Scrolls the chat view to show the most recent message. */
 void ScrollToBottom();
};
