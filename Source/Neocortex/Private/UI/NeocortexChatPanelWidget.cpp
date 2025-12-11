#include "UI/NeocortexChatPanelWidget.h"

#include "NeocortexTypes.h"
#include "Components/ScrollBox.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UI/NeocortexChatMessageWidget.h"

void UNeocortexChatPanelWidget::CreateWritingIndicatorWidget()
{
	WritingIndicator = CreateWidget<UNeocortexChatMessageWidget>(this, WritingIndicatorClass);
	ChatScrollBox->AddChild(WritingIndicator);
	WritingIndicator->SetVisibility(ESlateVisibility::Collapsed);
}

void UNeocortexChatPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Instantiate the typing indicator once
	if (WritingIndicatorClass && ChatScrollBox)
	{
		CreateWritingIndicatorWidget();
	}
}

void UNeocortexChatPanelWidget::AddMessage(const FString& Text, bool bIsUser)
{
	if (!MessageItemClass || !ChatScrollBox || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}


	const bool bIsLTR = WritingDirection == EWritingDirection::LeftToRight;

	UNeocortexChatMessageWidget* Message = CreateWidget<UNeocortexChatMessageWidget>(this, MessageItemClass);
	if (!Message) return;
	Message->SetMessage(Text, bIsUser, bIsLTR);
	ChatScrollBox->AddChild(Message);
	

	// Toggle the writing indicator visibility
	if (WritingIndicator)
	{
		WritingIndicator->SetVisibility(ESlateVisibility::Visible);
		WritingIndicator->SetMessage(TEXT(""), !bIsUser, bIsLTR);

		// Move it to the end
		ChatScrollBox->RemoveChild(WritingIndicator);
		ChatScrollBox->AddChild(WritingIndicator);
	}

	// Scroll after layout pass
	FTimerHandle ScrollTimer;
	GetWorld()->GetTimerManager().SetTimer(ScrollTimer, this, &UNeocortexChatPanelWidget::ScrollToBottom, 0.01f, false);
}

void UNeocortexChatPanelWidget::AddChatHistory(const TArray<FNeocortexChatMessage>& Messages)
{
	if (!Messages.IsEmpty()) 
	{
		for (const FNeocortexChatMessage& Message : Messages)
		{
			AddMessage(Message.Content, Message.Sender == TEXT("USER"));
		}
	}
	
}

void UNeocortexChatPanelWidget::ClearChatMessages()
{
	if (ChatScrollBox)
	{
		ChatScrollBox->ClearChildren();

		// Re-add the writing indicator
		if (WritingIndicatorClass && ChatScrollBox)
		{
			CreateWritingIndicatorWidget();
		}
	}
}

void UNeocortexChatPanelWidget::ScrollToBottom()
{
	if (ChatScrollBox)
	{
		ChatScrollBox->ScrollToEnd();
	}
}
