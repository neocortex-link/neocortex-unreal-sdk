#include "UI/NeocortexChatTranscript.h"
#include "UI/NeocortexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "TimerManager.h"

using namespace NeocortexUI;

// The bubble's measurements, in pixels at 1920 x 1080.
static constexpr float AvatarSize = 48.0f;
static constexpr float AvatarGap = 8.0f;
static constexpr float OuterPadding = 48.0f;
static constexpr float RowSpacing = 16.0f;

UNeocortexChatTranscript::UNeocortexChatTranscript(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerBackground = Amber;
    PlayerText = SurfaceDark;
    CharacterBackground = SurfaceDark;
    CharacterText = Amber;
}

TSharedRef<SWidget> UNeocortexChatTranscript::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UNeocortexChatTranscript::BuildTree()
{
    // The list rests on the bottom edge and grows upward until it scrolls.
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
    WidgetTree->RootWidget = Root;
    List = WidgetTree->ConstructWidget<UScrollBox>();
    List->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    UOverlaySlot* ListSlot = Root->AddChildToOverlay(List);
    ListSlot->SetHorizontalAlignment(HAlign_Fill);
    ListSlot->SetVerticalAlignment(VAlign_Bottom);

    // The bubble of dots: a character bubble with an amber outline.
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(128.0f);
    Size->SetHeightOverride(48.0f);
    UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>();
    Bubble->SetBrush(Rounded(CharacterBackground, 24.0f, AmberRim, 2.0f));
    Bubble->SetHorizontalAlignment(HAlign_Center);
    Bubble->SetVerticalAlignment(VAlign_Center);
    Size->SetContent(Bubble);
    UHorizontalBox* DotRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    Bubble->SetContent(DotRow);
    UTexture2D* Circle = Icon(TEXT("circle_64"));
    for (int32 i = 0; i < 3; ++i)
    {
        UImage* Dot = WidgetTree->ConstructWidget<UImage>();
        if (Circle) Dot->SetBrush(Image(Circle, FVector2D(26.0f, 26.0f), CharacterText));
        else
        {
            FSlateBrush Brush = Rounded(CharacterText, 13.0f);
            Brush.ImageSize = FVector2D(26.0f, 26.0f);
            Dot->SetBrush(Brush);
        }
        DotRow->AddChildToHorizontalBox(Dot)->SetPadding(FMargin(i == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f));
        Dots.Add(Dot);
    }
    Row->AddChildToHorizontalBox(Size);
    TypingRow = Row;
    TypingRow->SetVisibility(ESlateVisibility::Collapsed);
}

void UNeocortexChatTranscript::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!List) return;

    if (bTypingShown)
    {
        TypingClock += InDeltaTime;
        for (int32 i = 0; i < Dots.Num(); ++i)
        {
            Dots[i]->SetRenderOpacity(0.3f + 0.7f * FMath::Max(0.0f, FMath::Sin(TypingClock * 5.0f - i * 0.9f)));
        }
    }

    // Each row fades by where its middle sits: clear at the top edge, opaque from Fade down.
    const float Height = MyGeometry.GetLocalSize().Y;
    if (Fade <= 0.0f || Height <= 0.0f) return;
    for (UWidget* Child : List->GetAllChildren())
    {
        if (!Child || !Child->IsVisible()) continue;
        const FGeometry& RowGeometry = Child->GetCachedGeometry();
        const FVector2D Middle = MyGeometry.AbsoluteToLocal(RowGeometry.LocalToAbsolute(RowGeometry.GetLocalSize() * 0.5f));
        Child->SetRenderOpacity(FMath::Clamp(Middle.Y / (Height * Fade), 0.0f, 1.0f));
    }
}

void UNeocortexChatTranscript::AddMessage(const FString& Text, bool bIsUser, const FString& Sender)
{
    if (!WidgetTree || Text.TrimStartAndEnd().IsEmpty()) return;
    TakeWidget();
    const FLinearColor Background = bIsUser ? PlayerBackground : CharacterBackground;
    const FLinearColor Foreground = bIsUser ? PlayerText : CharacterText;

    // The bubble: the sender's name when shown, then the text.
    UBorder* Bubble = WidgetTree->ConstructWidget<UBorder>();
    Bubble->SetBrush(Rounded(Background, 24.0f));
    Bubble->SetPadding(FMargin(18.0f, 10.0f));
    Bubble->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Bubble->SetContent(Column);
    if (bShowSenderNames && !Sender.IsEmpty())
    {
        UTextBlock* Name = MakeText(WidgetTree, Sender, 15, Foreground * FLinearColor(1, 1, 1, 0.7f));
        Name->SetAutoWrapText(false);
        Name->SetJustification(bIsUser ? ETextJustify::Right : ETextJustify::Left);
        Column->AddChildToVerticalBox(Name);
    }
    // As wide as the text, up to MaxTextWidth, then it wraps.
    UTextBlock* Body = MakeText(WidgetTree, Text, 22, Foreground);
    USizeBox* MaxWidth = WidgetTree->ConstructWidget<USizeBox>();
    MaxWidth->SetMaxDesiredWidth(MaxTextWidth);
    MaxWidth->SetContent(Body);
    Column->AddChildToVerticalBox(MaxWidth);
    USizeBox* MinHeight = WidgetTree->ConstructWidget<USizeBox>();
    MinHeight->SetMinDesiredHeight(48.0f);
    MinHeight->SetContent(Bubble);

    // The avatar: the sender's initial in a ring of the text colour.
    UWidget* Avatar = nullptr;
    if (bShowAvatars && !Sender.IsEmpty())
    {
        USizeBox* AvatarBox = WidgetTree->ConstructWidget<USizeBox>();
        AvatarBox->SetWidthOverride(AvatarSize);
        AvatarBox->SetHeightOverride(AvatarSize);
        UBorder* Ring = WidgetTree->ConstructWidget<UBorder>();
        Ring->SetBrush(Rounded(Background, AvatarSize * 0.5f, Foreground * FLinearColor(1, 1, 1, 0.5f), 2.0f));
        Ring->SetHorizontalAlignment(HAlign_Center);
        Ring->SetVerticalAlignment(VAlign_Center);
        Ring->SetPadding(FMargin(0.0f));
        UTextBlock* Initial = MakeText(WidgetTree, Sender.Left(1).ToUpper(), 26, Foreground);
        Initial->SetAutoWrapText(false);
        Ring->SetContent(Initial);
        AvatarBox->SetContent(Ring);
        Avatar = AvatarBox;
    }

    // The player's bubbles sit on the right, the avatar always on the outer edge.
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    auto AddSpace = [this, Row](float Width, bool bFill)
    {
        USpacer* Space = WidgetTree->ConstructWidget<USpacer>();
        Space->SetSize(FVector2D(Width, 1.0f));
        UHorizontalBoxSlot* SpaceSlot = Row->AddChildToHorizontalBox(Space);
        if (bFill) SpaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    };
    auto AddAvatar = [Row, Avatar](float Left, float Right)
    {
        if (!Avatar) return;
        UHorizontalBoxSlot* AvatarSlot = Row->AddChildToHorizontalBox(Avatar);
        AvatarSlot->SetVerticalAlignment(VAlign_Top);
        AvatarSlot->SetPadding(FMargin(Left, 0.0f, Right, 0.0f));
    };
    if (bIsUser)
    {
        AddSpace(OuterPadding, true);
        Row->AddChildToHorizontalBox(MinHeight)->SetVerticalAlignment(VAlign_Top);
        AddAvatar(AvatarGap, 0.0f);
    }
    else
    {
        AddAvatar(0.0f, AvatarGap);
        Row->AddChildToHorizontalBox(MinHeight)->SetVerticalAlignment(VAlign_Top);
        AddSpace(OuterPadding, true);
    }
    AddRow(Row);
}

void UNeocortexChatTranscript::AddNotice(const FString& Text, bool bIsError)
{
    if (!WidgetTree || Text.IsEmpty()) return;
    TakeWidget();
    UTextBlock* Line = MakeText(WidgetTree, Text, 18, bIsError ? ErrorColor : Muted);
    Line->SetJustification(ETextJustify::Center);
    AddRow(Line);
}

// New rows go above the typing bubble, which stays last while shown.
void UNeocortexChatTranscript::AddRow(UWidget* Row)
{
    if (bTypingShown) List->RemoveChild(TypingRow);
    UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(List->AddChild(Row));
    RowSlot->SetHorizontalAlignment(HAlign_Fill);
    RowSlot->SetPadding(FMargin(0.0f, RowSpacing * 0.5f));
    if (bTypingShown)
    {
        UScrollBoxSlot* TypingSlot = Cast<UScrollBoxSlot>(List->AddChild(TypingRow));
        TypingSlot->SetPadding(FMargin(0.0f, RowSpacing * 0.5f));
    }
    ScrollToEnd();
}

void UNeocortexChatTranscript::SetTyping(bool bTyping)
{
    TakeWidget();
    if (!List || bTyping == bTypingShown) return;
    bTypingShown = bTyping;
    List->RemoveChild(TypingRow);
    TypingRow->SetVisibility(bTyping ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bTyping) return;
    TypingClock = 0.0f;
    UScrollBoxSlot* TypingSlot = Cast<UScrollBoxSlot>(List->AddChild(TypingRow));
    TypingSlot->SetPadding(FMargin(0.0f, RowSpacing * 0.5f));
    ScrollToEnd();
}

void UNeocortexChatTranscript::ClearMessages()
{
    if (!List) return;
    List->ClearChildren();
    bTypingShown = false;
    if (TypingRow) TypingRow->SetVisibility(ESlateVisibility::Collapsed);
}

// After layout, so the height of the new row is known.
void UNeocortexChatTranscript::ScrollToEnd()
{
    if (UWorld* World = GetWorld())
    {
        TWeakObjectPtr<UNeocortexChatTranscript> Weak(this);
        World->GetTimerManager().SetTimerForNextTick([Weak]()
        {
            if (Weak.IsValid() && Weak->List) Weak->List->ScrollToEnd();
        });
    }
}
