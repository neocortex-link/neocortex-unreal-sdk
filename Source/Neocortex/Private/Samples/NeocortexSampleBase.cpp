#include "Samples/NeocortexSampleBase.h"
#include "Samples/NeocortexSampleActors.h"
#include "Samples/NeocortexSamplePieces.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexFaceAnimator.h"
#include "Samples/NeocortexSampleAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "NeocortexQuizAgent.h"
#include "NeocortexInteractableComponent.h"
#include "UI/NeocortexUIStyle.h"
#include "Neocortex.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"

using namespace NeocortexUI;

static const TCHAR* PicksSection = TEXT("NeocortexSamples");

const TCHAR* ANeocortexSampleBase::LauncherMap = TEXT("/Neocortex/Samples/Maps/NeocortexSamples");

FNeocortexSamplePicks FNeocortexSamplePicks::Load()
{
    FNeocortexSamplePicks Picks;
    GConfig->GetString(PicksSection, TEXT("CharacterId"), Picks.CharacterId, GGameUserSettingsIni);
    GConfig->GetString(PicksSection, TEXT("CharacterName"), Picks.CharacterName, GGameUserSettingsIni);
    GConfig->GetArray(PicksSection, TEXT("OtherCharacterIds"), Picks.OtherCharacterIds, GGameUserSettingsIni);
    GConfig->GetArray(PicksSection, TEXT("OtherCharacterNames"), Picks.OtherCharacterNames, GGameUserSettingsIni);
    GConfig->GetString(PicksSection, TEXT("QuestionSetId"), Picks.QuestionSetId, GGameUserSettingsIni);
    GConfig->GetString(PicksSection, TEXT("Microphone"), Picks.Microphone, GGameUserSettingsIni);
    return Picks;
}

void FNeocortexSamplePicks::Save() const
{
    GConfig->SetString(PicksSection, TEXT("CharacterId"), *CharacterId, GGameUserSettingsIni);
    GConfig->SetString(PicksSection, TEXT("CharacterName"), *CharacterName, GGameUserSettingsIni);
    GConfig->SetArray(PicksSection, TEXT("OtherCharacterIds"), OtherCharacterIds, GGameUserSettingsIni);
    GConfig->SetArray(PicksSection, TEXT("OtherCharacterNames"), OtherCharacterNames, GGameUserSettingsIni);
    GConfig->SetString(PicksSection, TEXT("QuestionSetId"), *QuestionSetId, GGameUserSettingsIni);
    GConfig->SetString(PicksSection, TEXT("Microphone"), *Microphone, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

// Motion

static const TCHAR* ClipsDir = TEXT("/Neocortex/Samples/Cora/Animations/");

// Seconds a clip takes to fade into the next; leaving talking is slower, so a reply settles.
static constexpr float ClipBlend = 0.25f;
static constexpr float TalkingBlendOut = 0.5f;

UNeocortexSampleMotion::UNeocortexSampleMotion()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UNeocortexSampleMotion::Setup(USkeletalMeshComponent* InSkin, UNeocortexSmartAgent* InAgent, UNeocortexQuizAgent* InQuizHost)
{
    Agent = InAgent;
    QuizHost = InQuizHost;
    if (!InSkin) return;

    for (const TCHAR* Name : { TEXT("Idle"), TEXT("Talking"), TEXT("Thinking"), TEXT("Walking"), TEXT("Dance"), TEXT("Jump") })
    {
        const FString Path = FString(ClipsDir) + Name + TEXT(".") + Name;
        if (UAnimSequence* Clip = LoadObject<UAnimSequence>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
        {
            Clips.Add(Name, Clip);
        }
    }
    if (!Clips.Contains(TEXT("Idle"))) return;

    InSkin->SetAnimInstanceClass(UNeocortexSampleAnimInstance::StaticClass());
    Anim = Cast<UNeocortexSampleAnimInstance>(InSkin->GetAnimInstance());
    PlayClip(TEXT("Idle"), true);
}

void UNeocortexSampleMotion::PlayClip(FName Name, bool bLoop)
{
    UAnimSequence* Clip = Clips.FindRef(Name);
    if (!Anim || !Clip) return;
    const float Blend = Playing == TEXT("Talking") ? TalkingBlendOut : ClipBlend;
    Playing = Name;
    Anim->Play(Clip, Blend, bLoop);
}

bool UNeocortexSampleMotion::IsTalking() const
{
    if (Agent.IsValid())
    {
        const UAudioComponent* Voice = Agent->AudioComponent;
        return (Voice && Voice->IsPlaying()) || (Agent->IsSpeaking() && !Agent->IsThinking());
    }
    return QuizHost.IsValid() && QuizHost->AudioComponent && QuizHost->AudioComponent->IsPlaying();
}

bool UNeocortexSampleMotion::IsThinking() const
{
    if (Agent.IsValid()) return Agent->IsThinking();
    return QuizHost.IsValid() && QuizHost->bBusy && !IsTalking();
}

void UNeocortexSampleMotion::Begin(EMove InMove, TFunction<void()> OnDone)
{
    End();
    Move = InMove;
    Done = MoveTemp(OnDone);
    Elapsed = 0.0f;
    Start = GetOwner()->GetActorLocation();
    StartYaw = GetOwner()->GetActorRotation().Yaw;
}

void UNeocortexSampleMotion::End()
{
    Move = EMove::None;
    bOneShot = false;
    TFunction<void()> Finished = MoveTemp(Done);
    Done = nullptr;
    if (Finished) Finished();
}

void UNeocortexSampleMotion::Jump(TFunction<void()> OnDone)
{
    Begin(EMove::Jump, MoveTemp(OnDone));
    if (Anim && Clips.Contains(TEXT("Jump")))
    {
        bOneShot = true;
        PlayClip(TEXT("Jump"), false);
    }
}

void UNeocortexSampleMotion::Dance(TFunction<void()> OnDone)
{
    Begin(EMove::Dance, MoveTemp(OnDone));
    if (Anim && Clips.Contains(TEXT("Dance")))
    {
        bOneShot = true;
        PlayClip(TEXT("Dance"), false);
    }
}

void UNeocortexSampleMotion::WalkTo(const FVector& Target, TFunction<void()> OnDone)
{
    Begin(EMove::Walk, MoveTemp(OnDone));
    Goal = FVector(Target.X, Target.Y, Start.Z);
}

void UNeocortexSampleMotion::Face(const FVector& Target)
{
    AActor* Owner = GetOwner();
    const FVector To = Target - Owner->GetActorLocation();
    if (To.SizeSquared2D() > 1.0f) Owner->SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
}

void UNeocortexSampleMotion::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AActor* Owner = GetOwner();
    Elapsed += DeltaTime;

    if (bOneShot)
    {
        // A clip played once ends the move when it reaches its last frame.
        if (Anim && Anim->IsFinished()) End();
    }
    else if (Move == EMove::Jump)
    {
        const float T = FMath::Min(Elapsed / 0.6f, 1.0f);
        Owner->SetActorLocation(Start + FVector(0.0f, 0.0f, FMath::Sin(PI * T) * 45.0f));
        if (T >= 1.0f) End();
    }
    else if (Move == EMove::Dance)
    {
        const float T = FMath::Min(Elapsed / 2.0f, 1.0f);
        Owner->SetActorRotation(FRotator(0.0f, StartYaw + 720.0f * T, 0.0f));
        Owner->SetActorLocation(Start + FVector(0.0f, 0.0f, FMath::Abs(FMath::Sin(Elapsed * 2.0f * PI)) * 12.0f));
        if (T >= 1.0f)
        {
            Owner->SetActorLocationAndRotation(Start, FRotator(0.0f, StartYaw, 0.0f));
            End();
        }
    }
    else if (Move == EMove::Walk)
    {
        const FVector To = Goal - Owner->GetActorLocation();
        const float Distance = To.Size2D();
        if (Distance <= FMath::Max(StopDistance, 0.1f))
        {
            End();
        }
        else
        {
            Face(Goal);
            Owner->SetActorLocation(Owner->GetActorLocation() + To.GetSafeNormal2D() * FMath::Min(WalkSpeed * DeltaTime, Distance));
        }
    }

    if (!Anim || bOneShot) return;
    const FName Wanted = Move == EMove::Walk ? FName(TEXT("Walking"))
        : IsTalking() ? FName(TEXT("Talking"))
        : IsThinking() ? FName(TEXT("Thinking"))
        : FName(TEXT("Idle"));
    if (Wanted != Playing) PlayClip(Wanted, true);
}

// Frame

UCanvasPanelSlot* UNeocortexSampleFrameUI::PlaceOn(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Pivot, const FVector2D& Offset, const FVector2D& Size)
{
    UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);
    CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    CanvasSlot->SetAlignment(Pivot);
    CanvasSlot->SetPosition(Offset);
    CanvasSlot->SetSize(Size);
    return CanvasSlot;
}

void UNeocortexSampleFrameUI::BuildChrome(UWidgetTree* Tree, UCanvasPanel* Canvas, const FString& Subtitle, bool bOpaque)
{
    auto Fill = [Canvas](UWidget* Widget)
    {
        UCanvasPanelSlot* FillSlot = Canvas->AddChildToCanvas(Widget);
        FillSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
        FillSlot->SetOffsets(FMargin(0.0f));
    };
    if (bOpaque)
    {
        UImage* Black = Tree->ConstructWidget<UImage>();
        Black->SetColorAndOpacity(FLinearColor::Black);
        Black->SetVisibility(ESlateVisibility::HitTestInvisible);
        Fill(Black);
    }

    // A faint grid of dots over the whole screen.
    if (UTexture2D* Dots = Icon(TEXT("dots")))
    {
        UImage* Grid = Tree->ConstructWidget<UImage>();
        FSlateBrush Brush = Image(Dots, FVector2D(Dots->GetSizeX(), Dots->GetSizeY()), FLinearColor(1, 1, 1, 0.1f));
        Brush.Tiling = ESlateBrushTileType::Both;
        Grid->SetBrush(Brush);
        Grid->SetVisibility(ESlateVisibility::HitTestInvisible);
        Fill(Grid);
    }

    // Amber brackets 32 in from each corner: one image, mirrored.
    if (UTexture2D* CornerTexture = Icon(TEXT("corner")))
    {
        for (int32 i = 0; i < 4; ++i)
        {
            const FVector2D Side((i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f);
            UImage* Bracket = Tree->ConstructWidget<UImage>();
            Bracket->SetBrush(Image(CornerTexture, FVector2D(64.0f, 64.0f), Corner));
            Bracket->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
            Bracket->SetRenderScale(FVector2D(Side.X > 0.0f ? -1.0f : 1.0f, Side.Y > 0.0f ? -1.0f : 1.0f));
            Bracket->SetVisibility(ESlateVisibility::HitTestInvisible);
            PlaceOn(Canvas, Bracket, Side, Side, FVector2D(32.0f - 64.0f * Side.X, 32.0f - 64.0f * Side.Y), FVector2D(64.0f, 64.0f));
        }
    }

    // The logo, "NEOCORTEX" and the sample's name at the top left.
    if (UTexture2D* Logo = Icon(TEXT("logo")))
    {
        UImage* Mark = Tree->ConstructWidget<UImage>();
        Mark->SetBrush(Image(Logo, FVector2D(64.0f, 64.0f)));
        Mark->SetVisibility(ESlateVisibility::HitTestInvisible);
        PlaceOn(Canvas, Mark, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(64.0f, 64.0f), FVector2D(64.0f, 64.0f));
    }
    UTextBlock* Name = MakeText(Tree, TEXT("NEOCORTEX"), 32, FLinearColor::White, true);
    Name->SetAutoWrapText(false);
    PlaceOn(Canvas, Name, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(144.0f, 60.0f), FVector2D(400.0f, 40.0f));
    UTextBlock* Sub = MakeText(Tree, Subtitle, 20, FLinearColor(1, 1, 1, 0.59f));
    Sub->SetAutoWrapText(false);
    PlaceOn(Canvas, Sub, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(144.0f, 100.0f), FVector2D(480.0f, 28.0f));
}

UCanvasPanel* UNeocortexSampleFrameUI::BuildLayout(UWidgetTree* Tree, const ANeocortexSampleBase* Sample)
{
    UCanvasPanel* Root = Make<UCanvasPanel>(Tree, TEXT("Screen"));
    Tree->RootWidget = Root;

    // The frame fills the screen.
    UNeocortexSampleChrome* Chrome = Make<UNeocortexSampleChrome>(Tree, TEXT("Chrome"));
    Chrome->Subtitle = Sample->GetSubtitle();
    Chrome->bOpaque = Sample->IsOpaque();
    UCanvasPanelSlot* ChromeSlot = Root->AddChildToCanvas(Chrome);
    ChromeSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    ChromeSlot->SetOffsets(FMargin(0.0f));

    // Back to Samples sits beside the header.
    if (Sample->HasBackButton())
    {
        UNeocortexPillButton* Back = Make<UNeocortexPillButton>(Tree, TEXT("BackButton"));
        Back->Label = TEXT("Back to Samples");
        Back->FontSize = 18;
        PlaceOn(Root, Back, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D(488.0f, 76.0f), FVector2D(190.0f, 40.0f));
    }

    // The demo brief.
    const FString Lead = Sample->GetLead();
    const TArray<FString> Steps = Sample->GetSteps();
    if (!Lead.IsEmpty() || Steps.Num() > 0)
    {
        const FNeocortexBriefLayout Layout = Sample->GetBriefLayout();
        UNeocortexSampleBrief* Brief = Make<UNeocortexSampleBrief>(Tree, TEXT("Brief"));
        Brief->Lead = Lead;
        Brief->Steps = Steps;
        Brief->bCard = Layout.bCard;
        Brief->FontSize = Layout.FontSize;
        PlaceOn(Root, Brief, Layout.Anchor, Layout.Pivot, Layout.Offset, Layout.Size);
    }

    Sample->BuildScreen(Tree, Root);
    return Root;
}

void UNeocortexSampleFrameUI::EnsureLayout(const ANeocortexSampleBase* Sample)
{
    if (WidgetTree && !WidgetTree->RootWidget && Sample) BuildLayout(WidgetTree, Sample);
    if (bConnected) return;
    bConnected = true;
    if (UNeocortexPillButton* Back = Find<UNeocortexPillButton>(TEXT("BackButton")))
    {
        Back->OnClicked.AddDynamic(this, &UNeocortexSampleFrameUI::HandleBack);
    }
}

void UNeocortexSampleFrameUI::HandleBack()
{
    UGameplayStatics::OpenLevel(this, FName(ANeocortexSampleBase::LauncherMap));
}

// Sample base

ANeocortexSampleBase::ANeocortexSampleBase()
{
    PrimaryActorTick.bCanEverTick = false;
}

FString ANeocortexSampleBase::GetSubtitle() const
{
    return FString::Printf(TEXT("Unreal SDK \u00B7 %s Demo"), *GetTitle());
}

APlayerController* ANeocortexSampleBase::GetPlayer() const
{
    return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

void ANeocortexSampleBase::BeginPlay()
{
    Super::BeginPlay();
    APlayerController* PC = GetPlayer();
    if (!PC) return;

    PC->SetIgnoreMoveInput(true);
    PC->SetIgnoreLookInput(true);
    // The default pawn has no part in a sample and must not stand in front of the camera.
    if (APawn* Pawn = PC->GetPawn())
    {
        Pawn->SetActorHiddenInGame(true);
        Pawn->SetActorEnableCollision(false);
    }

    // The scene's camera, with a 60 degree vertical field at any window shape.
    if (ViewCamera)
    {
        if (ULocalPlayer* Local = PC->GetLocalPlayer()) Local->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV;
        PC->bAutoManageActiveCameraTarget = false;
        PC->SetViewTarget(ViewCamera);
    }

    // The screen: its parts are connected to the scene before it shows.
    Frame = CreateWidget<UNeocortexSampleFrameUI>(PC, ScreenClass ? ScreenClass.Get() : UNeocortexSampleFrameUI::StaticClass());
    Frame->EnsureLayout(this);
    SetupSample();
    Frame->AddToViewport();
    PC->bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);

    EnableInput(PC);
    if (InputComponent && HasBackButton()) InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ANeocortexSampleBase::HandleEscape);
}

void ANeocortexSampleBase::HandleEscape()
{
    if (Frame) Frame->HandleBack();
}

UNeocortexSmartAgent* ANeocortexSampleBase::AgentOf(int32 Index) const
{
    return SceneCharacters.IsValidIndex(Index) && SceneCharacters[Index] ? SceneCharacters[Index]->Agent.Get() : nullptr;
}

UNeocortexSampleMotion* ANeocortexSampleBase::MotionOf(UNeocortexSmartAgent* Agent) const
{
    return Agent && Agent->GetOwner() ? Agent->GetOwner()->FindComponentByClass<UNeocortexSampleMotion>() : nullptr;
}
