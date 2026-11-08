#include "UI/NeocortexVoiceInput.h"
#include "UI/NeocortexUIStyle.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"

using namespace NeocortexUI;

// The discs, in pixels at 1920 x 1080: 64 across, an inner disc 48 across, 16 apart.
static constexpr float DiscSize = 64.0f;
static constexpr float InnerSize = 48.0f;
static constexpr float DiscGap = 16.0f;

static UImage* VoiceDisc(UWidgetTree* Tree, const FLinearColor& Color, float Size)
{
    UImage* Circle = Tree->ConstructWidget<UImage>();
    if (UTexture2D* Texture = Icon(TEXT("circle_64"))) Circle->SetBrush(Image(Texture, FVector2D(Size, Size), Color));
    else
    {
        FSlateBrush Brush = Rounded(Color, Size * 0.5f);
        Brush.ImageSize = FVector2D(Size, Size);
        Circle->SetBrush(Brush);
    }
    Circle->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    Circle->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Circle;
}

static UOverlaySlot* VoiceCentre(UOverlaySlot* Slot)
{
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    return Slot;
}

TSharedRef<SWidget> UNeocortexVoiceInput::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UNeocortexVoiceInput::BuildTree()
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    WidgetTree->RootWidget = Row;
    auto Square = [this](UWidget* Content)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
        Box->SetWidthOverride(DiscSize);
        Box->SetHeightOverride(DiscSize);
        Box->SetContent(Content);
        return Box;
    };

    // The level: an amber rim, a darker disc, and the level swelling inside it.
    UOverlay* Level = WidgetTree->ConstructWidget<UOverlay>();
    VoiceCentre(Level->AddChildToOverlay(VoiceDisc(WidgetTree, AmberRim, DiscSize)));
    VoiceCentre(Level->AddChildToOverlay(VoiceDisc(WidgetTree, AmberDisc, InnerSize)));
    LevelBar = VoiceDisc(WidgetTree, AmberLevel, InnerSize);
    LevelBar->SetRenderScale(FVector2D(LevelScale, LevelScale));
    VoiceCentre(Level->AddChildToOverlay(LevelBar));
    Row->AddChildToHorizontalBox(Square(Level))->SetVerticalAlignment(VAlign_Center);

    // One slot for the state disc and the hold button, so the row keeps its width.
    UOverlay* Holder = WidgetTree->ConstructWidget<UOverlay>();
    UOverlay* State = WidgetTree->ConstructWidget<UOverlay>();
    VoiceCentre(State->AddChildToOverlay(VoiceDisc(WidgetTree, AmberRim, DiscSize)));
    VoiceCentre(State->AddChildToOverlay(VoiceDisc(WidgetTree, AmberDisc, InnerSize)));
    MicIcon = WidgetTree->ConstructWidget<UImage>();
    if (UTexture2D* Mic = Icon(TEXT("mic_icon"))) MicIcon->SetBrush(Image(Mic, FVector2D(27.0f, 38.0f), IconInk));
    MicIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    VoiceCentre(State->AddChildToOverlay(MicIcon));
    ClockIcon = WidgetTree->ConstructWidget<UImage>();
    if (UTexture2D* Clock = Icon(TEXT("clock_icon"))) ClockIcon->SetBrush(Image(Clock, FVector2D(44.0f, 44.0f), IconInk));
    ClockIcon->SetVisibility(ESlateVisibility::Collapsed);
    VoiceCentre(State->AddChildToOverlay(ClockIcon));
    StateDisc = State;
    UOverlaySlot* StateSlot = Holder->AddChildToOverlay(State);
    StateSlot->SetHorizontalAlignment(HAlign_Fill);
    StateSlot->SetVerticalAlignment(VAlign_Fill);

    HoldButton = MakeRoundButton(WidgetTree, TEXT("ptt_icon"), TEXT("Hold"), DiscSize, 44.0f);
    HoldButton->OnPressed.AddDynamic(this, &UNeocortexVoiceInput::HandlePressed);
    HoldButton->OnReleased.AddDynamic(this, &UNeocortexVoiceInput::HandleReleased);
    UOverlaySlot* HoldSlot = Holder->AddChildToOverlay(HoldButton);
    HoldSlot->SetHorizontalAlignment(HAlign_Fill);
    HoldSlot->SetVerticalAlignment(VAlign_Fill);
    Row->AddChildToHorizontalBox(Square(Holder))->SetPadding(FMargin(DiscGap, 0.0f, 0.0f, 0.0f));

    Apply();
}

void UNeocortexVoiceInput::SetMicrophone(UNeocortexMicrophoneRecorderComponent* InRecorder)
{
    Recorder = InRecorder;
}

void UNeocortexVoiceInput::SetPushToTalk(bool bEnabled)
{
    if (bHolding) HandleReleased();
    bPushToTalk = bEnabled;
    Apply();
}

void UNeocortexVoiceInput::SetBusy(bool bInBusy)
{
    if (bBusy == bInBusy) return;
    bBusy = bInBusy;
    Apply();
}

void UNeocortexVoiceInput::Apply()
{
    if (!StateDisc) return;
    StateDisc->SetVisibility(bPushToTalk ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    HoldButton->SetVisibility(bPushToTalk ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    HoldButton->SetIsEnabled(!bBusy || bHolding);
    MicIcon->SetVisibility(bBusy ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    ClockIcon->SetVisibility(bBusy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UNeocortexVoiceInput::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!Recorder || !LevelBar) return;
    // The level eases toward the voice; at rest it is a small dot.
    const float Target = Recorder->IsRecording() ? FMath::Clamp(Recorder->GetLevel(), 0.1f, 1.0f) : 0.1f;
    LevelScale = FMath::Lerp(LevelScale, Target, FMath::Clamp(InDeltaTime * 6.0f, 0.0f, 1.0f));
    LevelBar->SetRenderScale(FVector2D(LevelScale, LevelScale));
    WaitProgress = Recorder->IsUserSpeaking() ? 0.0f : Recorder->GetWaitProgress();
}

// The silence countdown: a ring around the state disc, filling clockwise from the top.
int32 UNeocortexVoiceInput::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    if (bPushToTalk || WaitProgress <= 0.001f || !StateDisc) return Layer;

    const FGeometry& DiscGeometry = StateDisc->GetCachedGeometry();
    const FVector2D Centre = AllottedGeometry.AbsoluteToLocal(DiscGeometry.LocalToAbsolute(DiscGeometry.GetLocalSize() * 0.5f));
    const float Radius = (DiscSize + InnerSize) * 0.25f;
    const int32 Segments = FMath::Max(2, FMath::CeilToInt(48 * WaitProgress));
    TArray<FVector2D> Points;
    for (int32 i = 0; i <= Segments; ++i)
    {
        const float Angle = -PI * 0.5f + 2.0f * PI * WaitProgress * i / Segments;
        Points.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
    FSlateDrawElement::MakeLines(OutDrawElements, Layer + 1, AllottedGeometry.ToPaintGeometry(), Points,
        ESlateDrawEffect::None, AmberWait, true, (DiscSize - InnerSize) * 0.5f);
    return Layer + 1;
}

void UNeocortexVoiceInput::NativeDestruct()
{
    if (bHolding) HandleReleased();
    Super::NativeDestruct();
}

void UNeocortexVoiceInput::HandlePressed()
{
    if (!Recorder || bHolding || bBusy) return;
    bHolding = Recorder->StartRecording();
}

void UNeocortexVoiceInput::HandleReleased()
{
    if (!Recorder || !bHolding) return;
    bHolding = false;
    Recorder->StopRecording(false);
    Apply();
}
