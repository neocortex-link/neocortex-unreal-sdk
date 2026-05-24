#include "UI/NeocortexChatPanelWidget.h"
#include "NeocortexTypes.h"
#include "Components/ScrollBox.h"
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
    if (WritingIndicatorClass && ChatScrollBox)
    {
        CreateWritingIndicatorWidget();
    }
}

void UNeocortexChatPanelWidget::AppendMessage(const FString& Text, bool bIsUser)
{
    if (!MessageItemClass || !ChatScrollBox || !GetWorld() || GetWorld()->bIsTearingDown) return;

    const bool bIsLTR = WritingDirection == EWritingDirection::LeftToRight;
    UNeocortexChatMessageWidget* Message = CreateWidget<UNeocortexChatMessageWidget>(this, MessageItemClass);
    if (!Message) return;
    Message->SetMessage(Text, bIsUser, bIsLTR);
    ChatScrollBox->AddChild(Message);

    FTimerHandle ScrollTimer;
    GetWorld()->GetTimerManager().SetTimer(ScrollTimer, this, &UNeocortexChatPanelWidget::ScrollToBottom, 0.01f, false);
}

void UNeocortexChatPanelWidget::AddMessage(const FString& Text, bool bIsUser)
{
    AppendMessage(Text, bIsUser);

    if (bIsUser)
        ShowWritingIndicator();
    else
        HideWritingIndicator();
}

void UNeocortexChatPanelWidget::AddChatHistory(const TArray<FNeocortexChatMessage>& Messages)
{
    for (const FNeocortexChatMessage& Message : Messages)
    {
        // API returns "user" and "ai" as sender strings (lowercase).
        AppendMessage(Message.Content, Message.Sender == TEXT("user"));
    }
    HideWritingIndicator();
}

void UNeocortexChatPanelWidget::ClearChatMessages()
{
    if (!ChatScrollBox) return;
    ChatScrollBox->ClearChildren();
    if (WritingIndicatorClass)
    {
        CreateWritingIndicatorWidget();
    }
}

void UNeocortexChatPanelWidget::ShowWritingIndicator()
{
    if (!WritingIndicator || !ChatScrollBox) return;
    const bool bIsLTR = WritingDirection == EWritingDirection::LeftToRight;
    WritingIndicator->SetMessage(TEXT(""), false, bIsLTR); // always shown as AI side
    ChatScrollBox->RemoveChild(WritingIndicator);
    ChatScrollBox->AddChild(WritingIndicator);
    WritingIndicator->SetVisibility(ESlateVisibility::Visible);
}

void UNeocortexChatPanelWidget::HideWritingIndicator()
{
    if (WritingIndicator)
    {
        WritingIndicator->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UNeocortexChatPanelWidget::ScrollToBottom()
{
    if (ChatScrollBox)
    {
        ChatScrollBox->ScrollToEnd();
    }
}
