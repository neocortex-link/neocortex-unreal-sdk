#include "Samples/NeocortexSamples.h"
#include "Samples/NeocortexSampleActors.h"
#include "Samples/NeocortexSamplePieces.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexGroupDirector.h"
#include "NeocortexQuizAgent.h"
#include "NeocortexUsageGate.h"
#include "NeocortexInteractableComponent.h"
#include "NeocortexSubsystem.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "UI/NeocortexChatUI.h"
#include "UI/NeocortexGroupChatUI.h"
#include "UI/NeocortexQuizUI.h"
#include "UI/NeocortexUIStyle.h"
#include "UI/NeocortexWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

using namespace NeocortexUI;
using FScreen = UNeocortexSampleFrameUI;

// The top right corner of the screen, for the pill buttons there.
static const FVector2D TopRight(1.0f, 0.0f);

// A chat that covers the screen; it lays its own parts out inside.
template<class T> static T* AddChat(UWidgetTree* Tree, UCanvasPanel* Root, const TCHAR* Name)
{
    T* Chat = FScreen::Make<T>(Tree, Name);
    UCanvasPanelSlot* ChatSlot = Root->AddChildToCanvas(Chat);
    ChatSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    ChatSlot->SetOffsets(FMargin(0.0f));
    return Chat;
}

static UNeocortexChatUI* AddChatUI(UWidgetTree* Tree, UCanvasPanel* Root, ENeocortexChatLayout Layout, float Height, bool bText, bool bVoice)
{
    UNeocortexChatUI* Chat = AddChat<UNeocortexChatUI>(Tree, Root, TEXT("Chat"));
    Chat->Layout = Layout;
    Chat->ConversationHeight = Height;
    Chat->bShowTextInput = bText;
    Chat->bShowVoiceInput = bVoice;
    Chat->InputHint = bVoice ? TEXT("Say something...") : TEXT("Say something to Cora...");
    return Chat;
}

static void AddPill(UWidgetTree* Tree, UCanvasPanel* Root, const TCHAR* Name, const FString& Label, const FVector2D& Offset)
{
    UNeocortexPillButton* Button = FScreen::Make<UNeocortexPillButton>(Tree, Name);
    Button->Label = Label;
    FScreen::PlaceOn(Root, Button, TopRight, TopRight, Offset, FVector2D(256.0f, 48.0f));
}

static UNeocortexMicrophoneRecorderComponent* AddMicrophone(AActor* Owner)
{
    UNeocortexMicrophoneRecorderComponent* Mic = NewObject<UNeocortexMicrophoneRecorderComponent>(Owner, TEXT("Microphone"));
    Mic->RegisterComponent();
    // The microphone picked in the launcher, when there is one.
    const FString Picked = FNeocortexSamplePicks::Load().Microphone;
    if (!Picked.IsEmpty()) Mic->SetPreferredDevice(Picked);
    return Mic;
}

// 1. Text Chat

void ANeocortexTextChatSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChatUI(Tree, Root, ENeocortexChatLayout::Bottom, 512.0f, true, false);
    AddPill(Tree, Root, TEXT("NewChatSession"), TEXT("New Chat Session"), FVector2D(-64.0f, 64.0f));
}

void ANeocortexTextChatSample::SetupSample()
{
    Chat = FindWidget<UNeocortexChatUI>(TEXT("Chat"));
    if (Chat) Chat->SetAgent(AgentOf(0));
    if (UNeocortexPillButton* Button = FindWidget<UNeocortexPillButton>(TEXT("NewChatSession"))) Button->OnClicked.AddDynamic(this, &ANeocortexTextChatSample::HandleNewSession);
}

// The next message starts a fresh session: Cora meets the player again.
void ANeocortexTextChatSample::HandleNewSession()
{
    if (UNeocortexSmartAgent* Agent = AgentOf(0)) Agent->ClearSessionId();
    if (Chat) Chat->ClearMessages();
}

// 2. Audio Chat

void ANeocortexAudioChatSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChatUI(Tree, Root, ENeocortexChatLayout::Bottom, 384.0f, false, true);
    UNeocortexDropdown* Devices = FScreen::Make<UNeocortexDropdown>(Tree, TEXT("MicrophonePicker"));
    Devices->Placeholder = TEXT("Default");
    FScreen::PlaceOn(Root, Devices, TopRight, TopRight, FVector2D(-336.0f, 64.0f), FVector2D(512.0f, 48.0f));
    UNeocortexToggle* PushToTalk = FScreen::Make<UNeocortexToggle>(Tree, TEXT("PushToTalk"));
    PushToTalk->Label = TEXT("Push-to-Talk");
    FScreen::PlaceOn(Root, PushToTalk, FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-64.0f, -64.0f), FVector2D(256.0f, 48.0f));
    AddPill(Tree, Root, TEXT("NewChatSession"), TEXT("New Chat Session"), FVector2D(-64.0f, 64.0f));
}

void ANeocortexAudioChatSample::SetupSample()
{
    Microphone = AddMicrophone(this);
    Chat = FindWidget<UNeocortexChatUI>(TEXT("Chat"));
    if (Chat)
    {
        Chat->SetAgent(AgentOf(0));
        Chat->SetMicrophone(Microphone);
    }

    // The microphones on this machine, the default one first.
    if (UNeocortexDropdown* Devices = FindWidget<UNeocortexDropdown>(TEXT("MicrophonePicker")))
    {
        Devices->ClearOptions();
        Devices->AddOption(TEXT("Default"));
        for (const FString& Name : UNeocortexMicrophoneRecorderComponent::GetAvailableDeviceNames()) Devices->AddOption(Name);
        const FString Picked = FNeocortexSamplePicks::Load().Microphone;
        Devices->SetSelectedOption(Picked.IsEmpty() || Devices->FindOptionIndex(Picked) == INDEX_NONE ? FString(TEXT("Default")) : Picked);
        Devices->OnPicked.AddDynamic(this, &ANeocortexAudioChatSample::HandleMicrophonePicked);
    }
    if (UNeocortexToggle* PushToTalk = FindWidget<UNeocortexToggle>(TEXT("PushToTalk"))) PushToTalk->OnToggled.AddDynamic(this, &ANeocortexAudioChatSample::HandlePushToTalk);
    if (UNeocortexPillButton* Button = FindWidget<UNeocortexPillButton>(TEXT("NewChatSession"))) Button->OnClicked.AddDynamic(this, &ANeocortexAudioChatSample::HandleNewSession);
}

void ANeocortexAudioChatSample::HandleMicrophonePicked(const FString& Item, bool bByPlayer)
{
    if (!bByPlayer || !Microphone) return;
    Microphone->SetPreferredDevice(Item == TEXT("Default") ? FString() : Item);
}

void ANeocortexAudioChatSample::HandlePushToTalk(bool bIsChecked)
{
    if (Chat) Chat->SetPushToTalk(bIsChecked);
}

void ANeocortexAudioChatSample::HandleNewSession()
{
    if (UNeocortexSmartAgent* Agent = AgentOf(0)) Agent->ClearSessionId();
    if (Chat) Chat->ClearMessages();
}

// 3. Actions

void ANeocortexActionsSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChatUI(Tree, Root, ENeocortexChatLayout::Bottom, 384.0f, true, true);
}

void ANeocortexActionsSample::SetupSample()
{
    UNeocortexSmartAgent* Agent = AgentOf(0);
    if (UNeocortexSampleMotion* Motion = MotionOf(Agent))
    {
        // Each handler gets Finish: the next action waits until the move is over.
        Agent->RegisterAction(TEXT("DANCE"), [Motion](const FNeocortexChatAction&, TFunction<void()> Finish)
        {
            Motion->Dance(MoveTemp(Finish));
        });
        Agent->RegisterAction(TEXT("JUMP"), [Motion](const FNeocortexChatAction&, TFunction<void()> Finish)
        {
            Motion->Jump(MoveTemp(Finish));
        });
    }
    Chat = FindWidget<UNeocortexChatUI>(TEXT("Chat"));
    if (Chat)
    {
        Chat->SetAgent(Agent);
        Chat->SetMicrophone(AddMicrophone(this));
    }
}

// 4. Interactables

FNeocortexBriefLayout ANeocortexInteractablesSample::GetBriefLayout() const
{
    FNeocortexBriefLayout Layout;
    Layout.bCard = true;
    Layout.Offset = FVector2D(-32.0f, 0.0f);
    return Layout;
}

void ANeocortexInteractablesSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChatUI(Tree, Root, ENeocortexChatLayout::Side, 512.0f, true, true);
}

void ANeocortexInteractablesSample::SetupSample()
{
    UNeocortexSmartAgent* Agent = AgentOf(0);
    Chat = FindWidget<UNeocortexChatUI>(TEXT("Chat"));
    if (Chat)
    {
        Chat->SetAgent(Agent);
        Chat->SetMicrophone(AddMicrophone(this));
    }
    UNeocortexSampleMotion* Motion = MotionOf(Agent);
    if (!Agent || !Motion) return;
    // Each run starts clean, so Cora perceives the scene afresh.
    Agent->ClearSessionId();

    // GO_TO_LOCATION names an interactable by id; Cora walks onto its centre.
    Motion->StopDistance = 0.0f;
    TWeakObjectPtr<ANeocortexInteractablesSample> Weak(this);
    Agent->RegisterAction(TEXT("GO_TO_LOCATION"), [Weak, Motion](const FNeocortexChatAction& Action, TFunction<void()> Finish)
    {
        const UGameInstance* GI = Weak.IsValid() ? Weak->GetGameInstance() : nullptr;
        const UNeocortexSubsystem* Sub = GI ? GI->GetSubsystem<UNeocortexSubsystem>() : nullptr;
        UNeocortexInteractableComponent* Target = nullptr;
        for (UNeocortexInteractableComponent* Candidate : Sub ? Sub->GetAllInteractables() : TArray<UNeocortexInteractableComponent*>())
        {
            if (Candidate->GetId() == Action.TargetId) Target = Candidate;
        }
        if (!Target || !Target->GetOwner())
        {
            if (Weak.IsValid() && Weak->Chat) Weak->Chat->AddNotice(FString::Printf(TEXT("GO_TO_LOCATION names %s, which is not in the scene."), *Action.TargetId), true);
            Finish();
            return;
        }
        Motion->WalkTo(Target->GetOwner()->GetActorLocation(), MoveTemp(Finish));
    });
}

// 5. Group Chat

FNeocortexBriefLayout ANeocortexGroupChatSample::GetBriefLayout() const
{
    FNeocortexBriefLayout Layout;
    Layout.bCard = true;
    Layout.Offset = FVector2D(-32.0f, 0.0f);
    Layout.Size = FVector2D(512.0f, 640.0f);
    return Layout;
}

void ANeocortexGroupChatSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    UNeocortexGroupChatUI* Group = AddChat<UNeocortexGroupChatUI>(Tree, Root, TEXT("Chat"));
    Group->ConversationHeight = 512.0f;
    Group->bShowVoiceInput = true;

    // A toggle per joining character, centred at the top.
    UHorizontalBox* Row = FScreen::Make<UHorizontalBox>(Tree, TEXT("CharacterToggles"));
    FScreen::PlaceOn(Root, Row, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 40.0f), FVector2D(640.0f, 48.0f));
    Row->AddChildToHorizontalBox(Tree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for (int32 i = 1; i <= 2; ++i)
    {
        UNeocortexToggle* Toggle = FScreen::Make<UNeocortexToggle>(Tree, i == 1 ? TEXT("Toggle1") : TEXT("Toggle2"));
        Toggle->Label = FString::Printf(TEXT("Character %d"), i);
        Toggle->bChecked = i == 1;
        Row->AddChildToHorizontalBox(Toggle)->SetPadding(FMargin(i == 1 ? 0.0f : 48.0f, 0.0f, 0.0f, 0.0f));
    }
    Row->AddChildToHorizontalBox(Tree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

void ANeocortexGroupChatSample::FaceCamera(AActor* Body) const
{
    UNeocortexSampleMotion* Motion = Body ? Body->FindComponentByClass<UNeocortexSampleMotion>() : nullptr;
    if (Motion && ViewCamera) Motion->Face(ViewCamera->GetActorLocation());
}

void ANeocortexGroupChatSample::SetupSample()
{
    const FNeocortexSamplePicks Picks = FNeocortexSamplePicks::Load();
    Director = NewObject<UNeocortexGroupDirector>(this, TEXT("Director"));
    Director->RegisterComponent();
    Director->OnTurnStarted.AddDynamic(this, &ANeocortexGroupChatSample::HandleTurnStarted);
    Director->OnTurnFinished.AddDynamic(this, &ANeocortexGroupChatSample::HandleTurnFinished);

    // The first character hosts for the whole scene; the next two join and leave with the toggles.
    if (UNeocortexSmartAgent* Host = AgentOf(0))
    {
        Director->AddAgent(Host);
        FaceCamera(Host->GetOwner());
    }
    static const FName Handlers[] = {
        GET_FUNCTION_NAME_CHECKED(ANeocortexGroupChatSample, HandleToggle0),
        GET_FUNCTION_NAME_CHECKED(ANeocortexGroupChatSample, HandleToggle1),
    };
    for (int32 i = 0; i < 2; ++i)
    {
        UNeocortexToggle* Toggle = FindWidget<UNeocortexToggle>(i == 0 ? TEXT("Toggle1") : TEXT("Toggle2"));
        ANeocortexSampleCharacter* Body = SceneCharacters.IsValidIndex(i + 1) ? SceneCharacters[i + 1].Get() : nullptr;
        const bool bHasCharacter = Body && Body->Agent && !Body->GetCharacterId().IsEmpty() && Seats.IsValidIndex(i) && Offstage.IsValidIndex(i);
        if (!bHasCharacter)
        {
            // Fewer characters on the key than seats: the seat stays empty.
            if (Body) Body->SetActorHiddenInGame(true);
            if (Toggle) Toggle->SetVisibility(ESlateVisibility::Collapsed);
            continue;
        }
        const bool bPresent = Toggle ? Toggle->bChecked : i == 0;
        Body->SetActorLocation((bPresent ? Seats[i] : Offstage[i])->GetActorLocation());
        FaceCamera(Body);
        if (bPresent) Director->AddAgent(Body->Agent);
        Members.Add(Body->Agent);
        Present.Add(bPresent);
        if (Toggle)
        {
            if (Picks.OtherCharacterNames.IsValidIndex(i)) Toggle->SetLabel(Picks.OtherCharacterNames[i]);
            FScriptDelegate Delegate;
            Delegate.BindUFunction(this, Handlers[i]);
            Toggle->OnToggled.Add(Delegate);
            Toggles.Add(Toggle);
        }
    }

    Chat = FindWidget<UNeocortexGroupChatUI>(TEXT("Chat"));
    if (Chat)
    {
        Chat->SetDirector(Director);
        Chat->SetMicrophone(AddMicrophone(this));
        if (Members.Num() == 0) Chat->AddNotice(TEXT("A group needs at least two characters on your key."), true);
    }
}

void ANeocortexGroupChatSample::HandleToggle0(bool bIsChecked) { Toggle(0, bIsChecked); }
void ANeocortexGroupChatSample::HandleToggle1(bool bIsChecked) { Toggle(1, bIsChecked); }

// Joining walks to a seat and leaving walks off; either way the next round starts.
void ANeocortexGroupChatSample::Toggle(int32 Index, bool bJoin)
{
    if (!Members.IsValidIndex(Index)) return;
    if (bMoving || Director->IsBusy() || bJoin == Present[Index])
    {
        if (Toggles.IsValidIndex(Index)) Toggles[Index]->SetChecked(Present[Index]);
        return;
    }
    UNeocortexSmartAgent* Member = Members[Index];
    UNeocortexSampleMotion* Motion = MotionOf(Member);
    if (!Motion) return;
    Present[Index] = bJoin;
    bMoving = true;
    SetTogglesEnabled(false);
    if (!bJoin) Director->RemoveAgent(Member);
    TWeakObjectPtr<ANeocortexGroupChatSample> Weak(this);
    Motion->StopDistance = 0.0f;
    Motion->WalkTo((bJoin ? Seats[Index] : Offstage[Index])->GetActorLocation(), [Weak, Member, bJoin]()
    {
        if (!Weak.IsValid()) return;
        Weak->FaceCamera(Member->GetOwner());
        if (bJoin) Weak->Director->AddAgent(Member);
        Weak->bMoving = false;
        Weak->SetTogglesEnabled(true);
        Weak->Director->ContinueScene();
    });
}

void ANeocortexGroupChatSample::SetTogglesEnabled(bool bEnabled)
{
    for (UNeocortexToggle* Toggle : Toggles) Toggle->SetEnabled(bEnabled);
}

void ANeocortexGroupChatSample::HandleTurnStarted() { SetTogglesEnabled(false); }
void ANeocortexGroupChatSample::HandleTurnFinished() { if (!bMoving) SetTogglesEnabled(true); }

// 6. Usage Gating

void ANeocortexUsageGatingSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChatUI(Tree, Root, ENeocortexChatLayout::Bottom, 512.0f, true, false);
    UNeocortexSampleLogCard* Log = FScreen::Make<UNeocortexSampleLogCard>(Tree, TEXT("UsageLog"));
    Log->Heading = TEXT("USAGE LOG");
    FScreen::PlaceOn(Root, Log, FVector2D(0.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(64.0f, 40.0f), FVector2D(480.0f, 560.0f));
    AddPill(Tree, Root, TEXT("RefreshLimits"), TEXT("Refresh Limits"), FVector2D(-336.0f, 64.0f));
    AddPill(Tree, Root, TEXT("NewChatSession"), TEXT("New Chat Session"), FVector2D(-64.0f, 64.0f));
}

void ANeocortexUsageGatingSample::SetupSample()
{
    Agent = AgentOf(0);
    UsageLog = FindWidget<UNeocortexSampleLogCard>(TEXT("UsageLog"));
    if (!Agent) return;
    Agent->OnChatLineStarted.AddDynamic(this, &ANeocortexUsageGatingSample::HandleLine);

    Gate = UNeocortexUsageGate::CreateUsageGate(this);
    Gate->OnUsageUpdated.AddDynamic(this, &ANeocortexUsageGatingSample::HandleUsage);
    Gate->OnAccountUpdated.AddDynamic(this, &ANeocortexUsageGatingSample::HandleAccount);
    Gate->OnLowCredits.AddDynamic(this, &ANeocortexUsageGatingSample::HandleLow);
    Gate->OnCreditsEmpty.AddDynamic(this, &ANeocortexUsageGatingSample::HandleEmpty);
    Gate->OnPlayerOverLimit.AddDynamic(this, &ANeocortexUsageGatingSample::HandlePlayerCap);
    Gate->OnCharacterOverLimit.AddDynamic(this, &ANeocortexUsageGatingSample::HandleCharacterCap);
    Gate->OnRequestFailed.AddDynamic(this, &ANeocortexUsageGatingSample::HandleGateError);
    HandleRefresh();

    // The chat does not send by itself: every message goes through the gate first.
    Chat = FindWidget<UNeocortexChatUI>(TEXT("Chat"));
    if (Chat)
    {
        Chat->SetAgent(Agent);
        Chat->bAutoSend = false;
        Chat->OnSubmitted.AddDynamic(this, &ANeocortexUsageGatingSample::HandleSubmitted);
    }
    if (UNeocortexPillButton* Refresh = FindWidget<UNeocortexPillButton>(TEXT("RefreshLimits"))) Refresh->OnClicked.AddDynamic(this, &ANeocortexUsageGatingSample::HandleRefresh);
    if (UNeocortexPillButton* Button = FindWidget<UNeocortexPillButton>(TEXT("NewChatSession"))) Button->OnClicked.AddDynamic(this, &ANeocortexUsageGatingSample::HandleNewSession);
}

void ANeocortexUsageGatingSample::Log(const FString& Line, const FLinearColor& Color)
{
    if (UsageLog) UsageLog->AddLine(Line, Color);
}

void ANeocortexUsageGatingSample::HandleRefresh()
{
    if (!Gate || !Agent) return;
    Gate->RefreshAccount();
    Gate->RefreshUsage(FString(), Agent->ProjectId);
}

void ANeocortexUsageGatingSample::HandleNewSession()
{
    if (Agent) Agent->ClearSessionId();
    if (Chat) Chat->ClearMessages();
}

void ANeocortexUsageGatingSample::HandleSubmitted(const FString& Text)
{
    if (!Gate->CanUseSmartNPC(FString(), Agent->ProjectId))
    {
        Log(TEXT("Blocked by the usage gate; the message was not sent."), Half);
        return;
    }
    Chat->SetTyping(true);
    Agent->SendMessage(Text);
    Gate->RefreshUsage(FString(), Agent->ProjectId);
}

void ANeocortexUsageGatingSample::HandleLine(const FNeocortexChatLine& Line)
{
    Log(FString::Printf(TEXT("Line: %s"), *Line.Text), BodyText);
}

static const TCHAR* StatusName(EUsageStatus Status)
{
    return Status == EUsageStatus::Empty ? TEXT("empty") : Status == EUsageStatus::Low ? TEXT("low") : TEXT("ok");
}

void ANeocortexUsageGatingSample::HandleUsage(const FNeocortexUsageResponse& Usage)
{
    Log(FString::Printf(TEXT("Usage updated: %s, %d credits left"), StatusName(Usage.Status), Usage.CreditsRemaining), BodyText);
}

void ANeocortexUsageGatingSample::HandleAccount(const FNeocortexAccountResponse& Account)
{
    Log(FString::Printf(TEXT("Account: %s tier, %d credits"), *Account.Tier, Account.CreditsRemaining), BodyText);
}

void ANeocortexUsageGatingSample::HandleLow(const FNeocortexUsageResponse& Usage) { Log(TEXT("Credits are running low."), Half); }
void ANeocortexUsageGatingSample::HandleEmpty(const FNeocortexUsageResponse& Usage) { Log(TEXT("Credits are empty. Smart features are off."), ErrorColor); }
void ANeocortexUsageGatingSample::HandlePlayerCap(const FNeocortexUsageResponse& Usage) { Log(TEXT("This player reached the cap set on the API key."), ErrorColor); }
void ANeocortexUsageGatingSample::HandleCharacterCap(const FNeocortexUsageResponse& Usage) { Log(TEXT("This character reached the cap set on the API key."), ErrorColor); }
void ANeocortexUsageGatingSample::HandleGateError(const FString& Error) { Log(FString::Printf(TEXT("Usage request failed: %s"), *Error), ErrorColor); }

// 7. Quiz Show

FNeocortexBriefLayout ANeocortexQuizShowSample::GetBriefLayout() const
{
    FNeocortexBriefLayout Layout;
    Layout.Anchor = FVector2D(1.0f, 0.0f);
    Layout.Pivot = FVector2D(1.0f, 0.0f);
    Layout.Offset = FVector2D(-64.0f, 600.0f);
    Layout.Size = FVector2D(480.0f, 300.0f);
    Layout.FontSize = 21;
    return Layout;
}

void ANeocortexQuizShowSample::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    AddChat<UNeocortexQuizUI>(Tree, Root, TEXT("Quiz"));
}

void ANeocortexQuizShowSample::SetupSample()
{
    const FNeocortexSamplePicks Picks = FNeocortexSamplePicks::Load();
    Quiz = SceneCharacters.IsValidIndex(0) && SceneCharacters[0] ? SceneCharacters[0]->QuizAgent.Get() : nullptr;
    if (UNeocortexQuizUI* Screen = FindWidget<UNeocortexQuizUI>(TEXT("Quiz")))
    {
        Screen->HostName = Picks.CharacterName.IsEmpty() ? TEXT("Cora") : Picks.CharacterName;
        Screen->SetQuiz(Quiz);
    }
}
