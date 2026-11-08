#include "UI/NeocortexChatMessageWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Misc/EngineVersionComparison.h"

void UNeocortexChatMessageWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UNeocortexChatMessageWidget::SetMessage(const FString& Text, bool bIsUser, bool bIsLTR)
{
	if (!MessageText || !Background || !LayoutBox || !LeftPadding || !RightPadding)
		return;

	const bool bRTL = !bIsLTR;

	// Let Unreal's text shaping (HarfBuzz) and bidi handle Arabic/Hebrew joining and ordering; set
	// the flow-direction preference and justification instead of manually reversing the string.
	MessageText->SetText(FText::FromString(Text));
	const EFlowDirectionPreference Flow = bRTL ? EFlowDirectionPreference::RightToLeft : EFlowDirectionPreference::LeftToRight;
#if UE_VERSION_OLDER_THAN(5, 1, 0)
	MessageText->FlowDirectionPreference = Flow;
	if (const TSharedPtr<SWidget> Cached = MessageText->GetCachedWidget())
	{
		Cached->SetFlowDirectionPreference(Flow);
	}
#else
	MessageText->SetFlowDirectionPreference(Flow);
#endif
	MessageText->SetJustification(bRTL ? ETextJustify::Right : ETextJustify::Left);
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

void UNeocortexChatMessageWidget::SetFontOverride(const FSlateFontInfo& Font)
{
	if (MessageText)
	{
		MessageText->SetFont(Font);
	}
}
