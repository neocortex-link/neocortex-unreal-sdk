#include "UI/NeocortexChatMessageWidget.h"
#include "Components/HorizontalBoxSlot.h"

void UNeocortexChatMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UNeocortexChatMessageWidget::SetMessage(const FString& Text, bool bIsUser, bool bIsLTR)
{
	if (!MessageText || !Background || !LayoutBox || !LeftPadding || !RightPadding)
		return;

	const FString FinalText = bIsLTR ? Text : CorrectRTL(Text);
	MessageText->SetText(FText::FromString(FinalText));
	MessageText->SetColorAndOpacity(bIsUser ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::Black));

	Background->SetBrushColor(bIsUser ? UserMessageColor : AgentMessageColor);

	const bool bAlignRight = bIsUser ? !bIsLTR : bIsLTR;
	const EHorizontalAlignment Alignment = bAlignRight ? HAlign_Right : HAlign_Left;

	// Update layout slot alignment
	if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(this->Slot))
	{
		HorizontalBoxSlot->SetHorizontalAlignment(Alignment);
	}

	// Adjust padding visibility
	LeftPadding->SetVisibility(bAlignRight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	RightPadding->SetVisibility(!bAlignRight ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

FString UNeocortexChatMessageWidget::CorrectRTL(const FString& Input) const
{
	FString Reversed;
	for (int32 i = Input.Len() - 1; i >= 0; --i)
	{
		Reversed.AppendChar(Input[i]);
	}
	return Reversed;
}
