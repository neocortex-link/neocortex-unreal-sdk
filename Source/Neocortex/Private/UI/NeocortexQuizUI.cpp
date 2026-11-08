#include "UI/NeocortexQuizUI.h"
#include "UI/NeocortexUIStyle.h"
#include "UI/NeocortexChatTranscript.h"
#include "UI/NeocortexTextInput.h"
#include "NeocortexQuizAgent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"

using namespace NeocortexUI;

static constexpr int32 MaxQuizOptions = 6;
static constexpr int32 MaxQuizPlayers = 4;

static UCanvasPanelSlot* QuizPlace(UCanvasPanel* Root, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Pivot, const FVector2D& Offset, const FVector2D& Size)
{
    UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Widget);
    CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    CanvasSlot->SetAlignment(Pivot);
    CanvasSlot->SetPosition(Offset);
    CanvasSlot->SetSize(Size);
    return CanvasSlot;
}

// A chip's look: the current player outlined in amber, a judged answer in its verdict colour.
static FButtonStyle ChipStyle(bool bCurrent, const FLinearColor& Verdict)
{
    FButtonStyle Style = PillButtonStyle();
    const bool bJudged = Verdict.A > 0.0f;
    const FLinearColor Edge = bJudged ? Verdict : (bCurrent ? Amber : Amber * FLinearColor(1, 1, 1, 0.5f));
    const FLinearColor Fill = bCurrent ? SurfaceHover : FLinearColor(FColor(0x21, 0x21, 0x21));
    Style.SetNormal(Rounded(Fill, 24.0f, Edge, bCurrent || bJudged ? 3.0f : 2.0f));
    Style.SetHovered(Rounded(SurfaceHover, 24.0f, Edge, 3.0f));
    Style.SetNormalPadding(FMargin(12.0f, 4.0f));
    Style.SetPressedPadding(FMargin(12.0f, 4.0f));
    return Style;
}

UNeocortexQuizUI::UNeocortexQuizUI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PlayerNames = { TEXT("Anna"), TEXT("Bence"), TEXT("Csilla"), TEXT("Dani") };
}

TSharedRef<SWidget> UNeocortexQuizUI::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UNeocortexQuizUI::BuildTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;

    Players.Reset();
    for (const FString& Name : PlayerNames)
    {
        if (Name.TrimStartAndEnd().IsEmpty() || Players.Num() >= MaxQuizPlayers) continue;
        FNeocortexQuizParticipant Player;
        Player.Id = FString::Printf(TEXT("p%d"), Players.Num() + 1);
        Player.DisplayName = Name.TrimStartAndEnd();
        Players.Add(Player);
    }
    Verdicts.Init(FLinearColor::Transparent, Players.Num());

    // The conversation in the middle, with the speakers' names.
    Transcript = WidgetTree->ConstructWidget<UNeocortexChatTranscript>(UNeocortexChatTranscript::StaticClass());
    Transcript->bShowSenderNames = true;
    Transcript->MaxTextWidth = 760.0f - 48.0f - 8.0f - 48.0f - 36.0f;
    QuizPlace(Root, Transcript, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 150.0f), FVector2D(760.0f, 690.0f));

    // Tapped choices under it.
    Options = WidgetTree->ConstructWidget<UWrapBox>();
    Options->SetInnerSlotPadding(FVector2D(12.0f, 8.0f));
    Options->SetHorizontalAlignment(HAlign_Center);
    QuizPlace(Root, Options, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 852.0f), FVector2D(760.0f, 48.0f));

    // Who answers next, with each player's score.
    UHorizontalBox* ReplyAs = WidgetTree->ConstructWidget<UHorizontalBox>();
    QuizPlace(Root, ReplyAs, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -112.0f), FVector2D(760.0f, 48.0f));
    USpacer* LeftSpace = WidgetTree->ConstructWidget<USpacer>();
    ReplyAs->AddChildToHorizontalBox(LeftSpace)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UTextBlock* Caption = MakeText(WidgetTree, TEXT("REPLY AS"), 18, Amber, true);
    Caption->SetAutoWrapText(false);
    ReplyAs->AddChildToHorizontalBox(Caption)->SetVerticalAlignment(VAlign_Center);
    static const FName ChipHandlers[MaxQuizPlayers] = {
        GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleChip0), GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleChip1),
        GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleChip2), GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleChip3),
    };
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        UButton* Chip = WidgetTree->ConstructWidget<UButton>();
        Chip->SetStyle(ChipStyle(i == 0, FLinearColor::Transparent));
        UHorizontalBox* Cells = WidgetTree->ConstructWidget<UHorizontalBox>();
        UTextBlock* Name = MakeText(WidgetTree, Players[i].DisplayName, 20, FLinearColor::White, true);
        Name->SetAutoWrapText(false);
        Cells->AddChildToHorizontalBox(Name)->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Score = MakeText(WidgetTree, TEXT("0"), 20, Amber, true);
        Score->SetAutoWrapText(false);
        UHorizontalBoxSlot* ScoreSlot = Cells->AddChildToHorizontalBox(Score);
        ScoreSlot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));
        ScoreSlot->SetVerticalAlignment(VAlign_Center);
        Chip->AddChild(Cells);
        FScriptDelegate Delegate;
        Delegate.BindUFunction(this, ChipHandlers[i]);
        Chip->OnClicked.Add(Delegate);
        USizeBox* ChipSize = WidgetTree->ConstructWidget<USizeBox>();
        ChipSize->SetMinDesiredWidth(140.0f);
        ChipSize->SetHeightOverride(48.0f);
        ChipSize->SetContent(Chip);
        ReplyAs->AddChildToHorizontalBox(ChipSize)->SetPadding(FMargin(i == 0 ? 12.0f : 10.0f, 0.0f, 0.0f, 0.0f));
        Chips.Add(Chip);
        Scores.Add(Score);
    }
    USpacer* RightSpace = WidgetTree->ConstructWidget<USpacer>();
    ReplyAs->AddChildToHorizontalBox(RightSpace)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // The answer field, bottom centre.
    Answer = WidgetTree->ConstructWidget<UNeocortexTextInput>(UNeocortexTextInput::StaticClass());
    Answer->SetHint(TEXT("Wait for the question..."));
    Answer->OnSubmitted.AddDynamic(this, &UNeocortexQuizUI::HandleTyped);
    QuizPlace(Root, Answer, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -32.0f), FVector2D(760.0f, 64.0f));

    // The question card, top right.
    UBorder* Card = MakeBox(WidgetTree, NeocortexUI::CardFill, 24.0f, FMargin(24.0f));
    QuizPlace(Root, Card, FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-64.0f, 150.0f), FVector2D(480.0f, 420.0f));
    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Column);
    UTextBlock* Heading = MakeText(WidgetTree, TEXT("QUESTION"), 22, Amber, true);
    Column->AddChildToVerticalBox(Heading);
    UBorder* Rule = MakeBox(WidgetTree, FLinearColor(1, 1, 1, 0.1f), 0.0f, FMargin(0.0f, 2.0f));
    Column->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.0f, 8.0f, 64.0f, 22.0f));
    auto Line = [this, Column](int32 Size, const FLinearColor& Color, bool bBold, float Gap)
    {
        UTextBlock* Text = MakeText(WidgetTree, FString(), Size, Color, bBold);
        Column->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Gap));
        return Text;
    };
    ProgressText = Line(20, FLinearColor(1, 1, 1, 0.59f), false, 12.0f);
    KeywordText = Line(22, Amber, true, 12.0f);
    QuestionText = Line(26, FLinearColor::White, true, 12.0f);
    Column->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UpcomingText = Line(20, FLinearColor(1, 1, 1, 0.59f), false, 12.0f);
    StatusText = Line(20, Muted, false, 0.0f);

    // Start, Abort and Resync, top right.
    UHorizontalBox* Run = WidgetTree->ConstructWidget<UHorizontalBox>();
    QuizPlace(Root, Run, FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-64.0f, 64.0f), FVector2D(600.0f, 48.0f));
    Run->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto RunButton = [this, Run](const FString& Label, float Gap)
    {
        UButton* Button = MakeButton(WidgetTree, Label);
        Run->AddChildToHorizontalBox(Button)->SetPadding(FMargin(Gap, 0.0f, 0.0f, 0.0f));
        return Button;
    };
    StartButton = RunButton(TEXT("Start"), 0.0f);
    AbortButton = RunButton(TEXT("Abort"), 16.0f);
    ResyncButton = RunButton(TEXT("Resync"), 16.0f);
    StartButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandleStart);
    AbortButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandleAbort);
    ResyncButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandleResync);

    // Typed Answers, above Hint, Pass and Pass Everyone at the bottom right.
    UWidget* TypedRow = nullptr;
    TypedToggle = MakeToggle(WidgetTree, TEXT("Typed Answers"), TypedRow);
    TypedToggle->SetIsChecked(true);
    QuizPlace(Root, TypedRow, FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-64.0f, -112.0f), FVector2D(320.0f, 48.0f));

    UHorizontalBox* Turn = WidgetTree->ConstructWidget<UHorizontalBox>();
    QuizPlace(Root, Turn, FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(-64.0f, -32.0f), FVector2D(480.0f, 64.0f));
    Turn->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>())->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto TurnButton = [this, Turn](const FString& Label, float Gap)
    {
        UButton* Button = MakeButton(WidgetTree, Label, 20);
        UHorizontalBoxSlot* ButtonSlot = Turn->AddChildToHorizontalBox(Button);
        ButtonSlot->SetPadding(FMargin(Gap, 0.0f, 0.0f, 0.0f));
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
        return Button;
    };
    HintButton = TurnButton(TEXT("Hint"), 0.0f);
    PassButton = TurnButton(TEXT("Pass"), 12.0f);
    PassRestButton = TurnButton(TEXT("Pass Everyone"), 12.0f);
    HintButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandleHint);
    PassButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandlePass);
    PassRestButton->OnClicked.AddDynamic(this, &UNeocortexQuizUI::HandlePassRest);

    Refresh();
}

void UNeocortexQuizUI::NativeDestruct()
{
    SetQuiz(nullptr);
    Super::NativeDestruct();
}

void UNeocortexQuizUI::SetQuiz(UNeocortexQuizAgent* InQuiz)
{
    if (Quiz)
    {
        Quiz->OnHostLine.RemoveDynamic(this, &UNeocortexQuizUI::HandleHostLine);
        Quiz->OnQuestionChanged.RemoveDynamic(this, &UNeocortexQuizUI::HandleQuestion);
        Quiz->OnUpcomingQuestion.RemoveDynamic(this, &UNeocortexQuizUI::HandleUpcoming);
        Quiz->OnPendingChanged.RemoveDynamic(this, &UNeocortexQuizUI::HandlePending);
        Quiz->OnResult.RemoveDynamic(this, &UNeocortexQuizUI::HandleResult);
        Quiz->OnLeaderboardChanged.RemoveDynamic(this, &UNeocortexQuizUI::HandleLeaderboard);
        Quiz->OnQuizFinished.RemoveDynamic(this, &UNeocortexQuizUI::HandleFinished);
        Quiz->OnExpectingChanged.RemoveDynamic(this, &UNeocortexQuizUI::HandleExpecting);
        Quiz->OnBusyChanged.RemoveDynamic(this, &UNeocortexQuizUI::HandleBusy);
        Quiz->OnInputDropped.RemoveDynamic(this, &UNeocortexQuizUI::HandleDropped);
        Quiz->OnRequestError.RemoveDynamic(this, &UNeocortexQuizUI::HandleError);
        Quiz->OnCreditsExhausted.RemoveDynamic(this, &UNeocortexQuizUI::HandleCredits);
    }
    Quiz = InQuiz;
    if (Quiz)
    {
        Quiz->OnHostLine.AddDynamic(this, &UNeocortexQuizUI::HandleHostLine);
        Quiz->OnQuestionChanged.AddDynamic(this, &UNeocortexQuizUI::HandleQuestion);
        Quiz->OnUpcomingQuestion.AddDynamic(this, &UNeocortexQuizUI::HandleUpcoming);
        Quiz->OnPendingChanged.AddDynamic(this, &UNeocortexQuizUI::HandlePending);
        Quiz->OnResult.AddDynamic(this, &UNeocortexQuizUI::HandleResult);
        Quiz->OnLeaderboardChanged.AddDynamic(this, &UNeocortexQuizUI::HandleLeaderboard);
        Quiz->OnQuizFinished.AddDynamic(this, &UNeocortexQuizUI::HandleFinished);
        Quiz->OnExpectingChanged.AddDynamic(this, &UNeocortexQuizUI::HandleExpecting);
        Quiz->OnBusyChanged.AddDynamic(this, &UNeocortexQuizUI::HandleBusy);
        Quiz->OnInputDropped.AddDynamic(this, &UNeocortexQuizUI::HandleDropped);
        Quiz->OnRequestError.AddDynamic(this, &UNeocortexQuizUI::HandleError);
        Quiz->OnCreditsExhausted.AddDynamic(this, &UNeocortexQuizUI::HandleCredits);
    }
    Refresh();
}

bool UNeocortexQuizUI::IsAccepting() const
{
    return Quiz && !Quiz->SessionId.IsEmpty() && !Quiz->bFinished && !Quiz->bBusy
        && Quiz->Expecting != ENeocortexQuizExpecting::Nothing;
}

// What the players can do follows what the host is waiting for.
void UNeocortexQuizUI::Refresh()
{
    if (!StartButton) return;
    const bool bOpen = IsAccepting();
    const bool bStarted = Quiz && !Quiz->SessionId.IsEmpty();
    const bool bRunning = bStarted && !Quiz->bFinished;

    Answer->SetInputEnabled(bOpen);
    Answer->SetHint(bOpen && Players.IsValidIndex(Current)
        ? FString::Printf(TEXT("Reply as %s..."), *Players[Current].DisplayName) : FString(TEXT("Wait for the question...")));
    HintButton->SetIsEnabled(bOpen && Quiz->bHasQuestion && Quiz->CurrentQuestion.bHasHint);
    PassButton->SetIsEnabled(bOpen);
    PassRestButton->SetIsEnabled(bOpen && Players.Num() > 1);
    AbortButton->SetIsEnabled(bRunning);
    ResyncButton->SetIsEnabled(bRunning);
    Options->SetIsEnabled(bOpen);
    SetButtonLabel(StartButton, bStarted ? TEXT("Restart") : TEXT("Start"));
    for (int32 i = 0; i < Chips.Num(); ++i) Chips[i]->SetStyle(ChipStyle(i == Current, Verdicts[i]));

    if (!ErrorText.IsEmpty()) Note(ErrorText, ErrorColor);
    else if (!bStarted) Note(TEXT("Press Start to begin the show."), Muted);
    else if (Quiz->bFinished) Note(TEXT("Show over. Press Restart to play again."), Muted);
    else if (Quiz->bBusy) Note(FString::Printf(TEXT("%s is thinking..."), *HostName), Muted);
    else if (Quiz->Expecting == ENeocortexQuizExpecting::Nothing) Note(FString::Printf(TEXT("%s is speaking."), *HostName), Muted);
    else if (Quiz->PendingParticipants.Num() > 1) Note(FString::Printf(TEXT("Waiting on %d players."), Quiz->PendingParticipants.Num()), Muted);
    else if (Players.IsValidIndex(Current)) Note(FString::Printf(TEXT("Waiting for %s."), *Players[Current].DisplayName), Muted);
}

void UNeocortexQuizUI::Note(const FString& Text, const FLinearColor& Color)
{
    StatusText->SetText(FText::FromString(Text));
    StatusText->SetColorAndOpacity(FSlateColor(Color));
}

int32 UNeocortexQuizUI::SeatOf(const FString& ParticipantId) const
{
    return Players.IndexOfByPredicate([&ParticipantId](const FNeocortexQuizParticipant& P) { return P.Id == ParticipantId; });
}

void UNeocortexQuizUI::Select(int32 Index)
{
    Current = FMath::Clamp(Index, 0, FMath::Max(0, Players.Num() - 1));
    Refresh();
}

// The next player the host still waits on.
void UNeocortexQuizUI::SelectNext()
{
    for (int32 Step = 1; Step <= Players.Num() && Quiz; ++Step)
    {
        const int32 Index = (Current + Step) % Players.Num();
        const FString& Id = Players[Index].Id;
        if (Quiz->PendingParticipants.Contains(Id) && !Quiz->AnsweredParticipants.Contains(Id))
        {
            Select(Index);
            return;
        }
    }
    Refresh();
}

void UNeocortexQuizUI::AnswerAsCurrent(const FString& Text, bool bTyped)
{
    if (!IsAccepting() || !Players.IsValidIndex(Current)) return;
    ErrorText.Reset();
    Transcript->AddMessage(Text, true, Players[Current].DisplayName);
    Quiz->SayAs(Players[Current].Id, Text, bTyped);
    SelectNext();
}

void UNeocortexQuizUI::HandleTyped(const FString& Text)
{
    AnswerAsCurrent(Text, TypedToggle && TypedToggle->IsChecked());
}

void UNeocortexQuizUI::ChooseOption(int32 Index)
{
    if (!IsAccepting() || !OptionIds.IsValidIndex(Index) || !Players.IsValidIndex(Current)) return;
    ErrorText.Reset();
    Transcript->AddMessage(OptionTexts[Index], true, Players[Current].DisplayName);
    Quiz->ChooseAs(Players[Current].Id, OptionIds[Index]);
    SelectNext();
}

void UNeocortexQuizUI::HandleHostLine(const FNeocortexQuizLine& Line)
{
    Transcript->AddMessage(Line.Text, false, HostName);
}

void UNeocortexQuizUI::HandleQuestion(const FNeocortexQuizQuestion& Question)
{
    KeywordText->SetText(FText::FromString(FString::Printf(TEXT("Scene: %s"), Question.Keyword.IsEmpty() ? TEXT("-") : *Question.Keyword)));
    QuestionText->SetText(FText::FromString(Question.QuestionText));
    if (Quiz) ProgressText->SetText(FText::FromString(FString::Printf(TEXT("Question %d of %d"), Quiz->Progress.Asked, Quiz->Progress.Total)));

    Options->ClearChildren();
    OptionIds.Reset();
    OptionTexts.Reset();
    static const FName Handlers[MaxQuizOptions] = {
        GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption0), GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption1),
        GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption2), GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption3),
        GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption4), GET_FUNCTION_NAME_CHECKED(UNeocortexQuizUI, HandleOption5),
    };
    if (Question.IsChoice())
    {
        for (int32 i = 0; i < Question.Options.Num() && i < MaxQuizOptions; ++i)
        {
            UButton* Button = MakeButton(WidgetTree, Question.Options[i].Text);
            FScriptDelegate Delegate;
            Delegate.BindUFunction(this, Handlers[i]);
            Button->OnClicked.Add(Delegate);
            USizeBox* Height = WidgetTree->ConstructWidget<USizeBox>();
            Height->SetHeightOverride(48.0f);
            Height->SetContent(Button);
            Options->AddChildToWrapBox(Height);
            OptionIds.Add(Question.Options[i].Id);
            OptionTexts.Add(Question.Options[i].Text);
        }
    }
    for (FLinearColor& Verdict : Verdicts) Verdict = FLinearColor::Transparent;
    Select(0);
}

void UNeocortexQuizUI::HandleUpcoming(const FNeocortexQuizUpcoming& Upcoming)
{
    UpcomingText->SetText(FText::FromString(FString::Printf(TEXT("Next scene: %s"), Upcoming.Keyword.IsEmpty() ? TEXT("-") : *Upcoming.Keyword)));
}

void UNeocortexQuizUI::HandlePending(const TArray<FString>& Pending)
{
    SelectNext();
}

void UNeocortexQuizUI::HandleResult(const TArray<FNeocortexQuizResult>& Results)
{
    for (const FNeocortexQuizResult& Result : Results)
    {
        const int32 Seat = SeatOf(Result.ParticipantId);
        if (!Verdicts.IsValidIndex(Seat)) continue;
        Verdicts[Seat] = Result.Verdict == ENeocortexQuizVerdict::Correct ? Good
            : Result.Verdict == ENeocortexQuizVerdict::Partial ? Half
            : Result.Verdict == ENeocortexQuizVerdict::Incorrect ? ErrorColor : Muted;
    }
    Refresh();
}

void UNeocortexQuizUI::HandleLeaderboard(const TArray<FNeocortexQuizLeaderboardEntry>& Leaderboard)
{
    for (const FNeocortexQuizLeaderboardEntry& Entry : Leaderboard)
    {
        const int32 Seat = SeatOf(Entry.ParticipantId);
        if (Scores.IsValidIndex(Seat)) Scores[Seat]->SetText(FText::AsNumber(Entry.TotalScore));
    }
}

void UNeocortexQuizUI::HandleFinished(const TArray<FNeocortexQuizLeaderboardEntry>& Leaderboard)
{
    FString Standings = TEXT("Final standings");
    for (const FNeocortexQuizLeaderboardEntry& Entry : Leaderboard)
    {
        const int32 Seat = SeatOf(Entry.ParticipantId);
        const FString Name = Players.IsValidIndex(Seat) ? Players[Seat].DisplayName : Entry.DisplayName;
        Standings += FString::Printf(TEXT("\n%d. %s  %d"), Entry.Rank, *Name, Entry.TotalScore);
    }
    QuestionText->SetText(FText::FromString(Standings));
    Options->ClearChildren();
    OptionIds.Reset();
    OptionTexts.Reset();
    HandleLeaderboard(Leaderboard);
    Refresh();
}

void UNeocortexQuizUI::HandleExpecting(ENeocortexQuizExpecting Expecting) { Refresh(); }
void UNeocortexQuizUI::HandleBusy(bool bBusy) { Refresh(); }

void UNeocortexQuizUI::HandleDropped(const FString& Text, ENeocortexQuizInputDropReason Reason)
{
    const FString Why = StaticEnum<ENeocortexQuizInputDropReason>()->GetNameStringByValue(static_cast<int64>(Reason)).ToLower();
    Transcript->AddNotice(FString::Printf(TEXT("Dropped \"%s\" (%s)."), *Text, *Why));
}

void UNeocortexQuizUI::HandleError(const FNeocortexRequestError& Error)
{
    ErrorText = Error.Message;
    Refresh();
}

void UNeocortexQuizUI::HandleCredits(const FString& Code)
{
    ErrorText = FString::Printf(TEXT("Out of credits (%s)."), *Code);
    Refresh();
}

void UNeocortexQuizUI::HandleStart()
{
    if (!Quiz) return;
    ErrorText.Reset();
    if (Players.Num() == 0 || Quiz->CharacterId.IsEmpty() || Quiz->QuestionSetId.IsEmpty())
    {
        ErrorText = TEXT("Set the quiz host's character and question set, and at least one player.");
        Refresh();
        return;
    }
    Transcript->ClearMessages();
    QuestionText->SetText(FText::GetEmpty());
    KeywordText->SetText(FText::GetEmpty());
    UpcomingText->SetText(FText::GetEmpty());
    ProgressText->SetText(FText::GetEmpty());
    Options->ClearChildren();
    for (int32 i = 0; i < Players.Num(); ++i)
    {
        Verdicts[i] = FLinearColor::Transparent;
        Scores[i]->SetText(FText::AsNumber(0));
    }
    Quiz->BeginWithPlayers(Players);
    Refresh();
}

void UNeocortexQuizUI::HandleAbort()
{
    if (Quiz) Quiz->Abort();
    Refresh();
}

void UNeocortexQuizUI::HandleResync()
{
    ErrorText.Reset();
    if (Quiz) Quiz->Resync();
}

void UNeocortexQuizUI::HandleHint()
{
    AnswerAsCurrent(TEXT("Can I have a hint?"), false);
}

void UNeocortexQuizUI::HandlePass()
{
    if (!IsAccepting() || !Players.IsValidIndex(Current)) return;
    ErrorText.Reset();
    Transcript->AddNotice(FString::Printf(TEXT("%s passes."), *Players[Current].DisplayName));
    Quiz->PassAs(Players[Current].Id);
    SelectNext();
}

void UNeocortexQuizUI::HandlePassRest()
{
    if (!Quiz) return;
    for (const FNeocortexQuizParticipant& Player : Players)
    {
        if (Quiz->PendingParticipants.Contains(Player.Id)) Quiz->PassAs(Player.Id);
    }
}

void UNeocortexQuizUI::HandleOption0() { ChooseOption(0); }
void UNeocortexQuizUI::HandleOption1() { ChooseOption(1); }
void UNeocortexQuizUI::HandleOption2() { ChooseOption(2); }
void UNeocortexQuizUI::HandleOption3() { ChooseOption(3); }
void UNeocortexQuizUI::HandleOption4() { ChooseOption(4); }
void UNeocortexQuizUI::HandleOption5() { ChooseOption(5); }
void UNeocortexQuizUI::HandleChip0() { Select(0); }
void UNeocortexQuizUI::HandleChip1() { Select(1); }
void UNeocortexQuizUI::HandleChip2() { Select(2); }
void UNeocortexQuizUI::HandleChip3() { Select(3); }
