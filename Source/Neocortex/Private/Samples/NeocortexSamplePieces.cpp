#include "Samples/NeocortexSamplePieces.h"
#include "Samples/NeocortexSampleBase.h"
#include "UI/NeocortexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

using namespace NeocortexUI;

// Chrome

TSharedRef<SWidget> UNeocortexSampleChrome::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
        WidgetTree->RootWidget = Canvas;
        UNeocortexSampleFrameUI::BuildChrome(WidgetTree, Canvas, Subtitle, bOpaque);
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    return Super::RebuildWidget();
}

// Brief

TSharedRef<SWidget> UNeocortexSampleBrief::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Area = WidgetTree->ConstructWidget<UCanvasPanel>();
        const float Inset = bCard ? 8.0f : 0.0f;
        if (bCard)
        {
            UBorder* CardBox = MakeBox(WidgetTree, CardFill, 24.0f, FMargin(0.0f));
            CardBox->SetContent(Area);
            WidgetTree->RootWidget = CardBox;
        }
        else
        {
            WidgetTree->RootWidget = Area;
        }
        // Stretched across the brief: a strip at Top of Height pixels, or the rest when Height is 0.
        auto Stretch = [Area](UWidget* Widget, float Left, float Right, float Top, float Height, float Bottom)
        {
            UCanvasPanelSlot* StretchSlot = Area->AddChildToCanvas(Widget);
            StretchSlot->SetAnchors(Height > 0.0f ? FAnchors(0.0f, 0.0f, 1.0f, 0.0f) : FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            StretchSlot->SetOffsets(FMargin(Left, Top, Right, Height > 0.0f ? Height : Bottom));
        };
        UTextBlock* Heading = MakeText(WidgetTree, TEXT("DEMO BRIEF"), 22, Amber, true);
        Heading->SetAutoWrapText(false);
        Stretch(Heading, Inset, Inset, Inset, 32.0f, 0.0f);
        UImage* Rule = WidgetTree->ConstructWidget<UImage>();
        Rule->SetColorAndOpacity(FLinearColor(1, 1, 1, 0.1f));
        Stretch(Rule, Inset, Inset + 64.0f, Inset + 36.0f, 4.0f, 0.0f);

        FString Markup;
        if (!Lead.IsEmpty()) Markup += FString::Printf(TEXT("<b>%s</b>\n"), *Lead);
        for (int32 i = 0; i < Steps.Num(); ++i)
        {
            Markup += FString::Printf(TEXT("\n<n>%02d</n> %s"), i + 1, *Steps[i]);
        }
        Stretch(MakeRichText(WidgetTree, Markup, FontSize, BodyText), Inset, Inset + 64.0f, Inset + 64.0f, 0.0f, Inset);
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    return Super::RebuildWidget();
}

// Log card

TSharedRef<SWidget> UNeocortexSampleLogCard::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UBorder* CardBox = MakeBox(WidgetTree, CardFill, 24.0f, FMargin(24.0f));
        WidgetTree->RootWidget = CardBox;
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        CardBox->SetContent(Column);
        Column->AddChildToVerticalBox(MakeText(WidgetTree, Heading, 22, Amber, true));
        UBorder* Rule = MakeBox(WidgetTree, FLinearColor(1, 1, 1, 0.1f), 0.0f, FMargin(0.0f, 2.0f));
        Column->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.0f, 8.0f, 64.0f, 22.0f));
        Lines = WidgetTree->ConstructWidget<UScrollBox>();
        Lines->SetScrollBarVisibility(ESlateVisibility::Collapsed);
        Column->AddChildToVerticalBox(Lines)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    return Super::RebuildWidget();
}

void UNeocortexSampleLogCard::AddLine(const FString& Line, const FLinearColor& Color)
{
    TakeWidget();
    if (!Lines) return;
    UTextBlock* Entry = MakeText(WidgetTree, Line, 18, Color);
    Cast<UScrollBoxSlot>(Lines->AddChild(Entry))->SetPadding(FMargin(0.0f, 3.0f));
    Lines->ScrollToEnd();
}

// Sample card

TSharedRef<SWidget> UNeocortexSampleCard::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UButton* Card = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle CardStyle;
        CardStyle.SetNormal(Rounded(CardFill, 24.0f, FLinearColor(1, 1, 1, 0.08f), 2.0f));
        CardStyle.SetHovered(Rounded(FLinearColor(FColor(0x14, 0x14, 0x14)), 24.0f, AmberRim, 2.0f));
        CardStyle.SetPressed(Rounded(SurfaceDark, 24.0f, Amber, 2.0f));
        CardStyle.SetNormalPadding(FMargin(32.0f, 28.0f, 32.0f, 24.0f));
        CardStyle.SetPressedPadding(FMargin(32.0f, 28.0f, 32.0f, 24.0f));
        Card->SetStyle(CardStyle);
        Card->OnClicked.AddDynamic(this, &UNeocortexSampleCard::HandleClicked);
        WidgetTree->RootWidget = Card;

        UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>();
        if (UButtonSlot* InnerSlot = Cast<UButtonSlot>(Card->AddChild(Inner)))
        {
            InnerSlot->SetHorizontalAlignment(HAlign_Fill);
            InnerSlot->SetVerticalAlignment(VAlign_Top);
        }
        UHorizontalBox* Heading = WidgetTree->ConstructWidget<UHorizontalBox>();
        Inner->AddChildToVerticalBox(Heading);
        UTextBlock* Index = MakeText(WidgetTree, FString::Printf(TEXT("%02d"), Number), 28, Amber, true);
        Index->SetAutoWrapText(false);
        Heading->AddChildToHorizontalBox(Index)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
        UTextBlock* Name = MakeText(WidgetTree, Title, 28, FLinearColor::White, true);
        Name->SetAutoWrapText(false);
        Heading->AddChildToHorizontalBox(Name);
        Inner->AddChildToVerticalBox(MakeText(WidgetTree, Description, 21, BodyText))->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
    }
    return Super::RebuildWidget();
}

void UNeocortexSampleCard::HandleClicked()
{
    OnClicked.Broadcast();
}
