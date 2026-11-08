#include "NeocortexQuizAgent.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSpeech.h"
#include "Neocortex.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UNeocortexQuizAgent::UNeocortexQuizAgent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNeocortexQuizAgent::BeginPlay()
{
    Super::BeginPlay();
    RequestToken = MakeShared<FNeocortexCancelToken, ESPMode::ThreadSafe>();

    AActor* Owner = GetOwner();
    if (bSpeakHostLines && !AudioComponent && Owner)
    {
        AudioComponent = Owner->FindComponentByClass<UAudioComponent>();
        if (!AudioComponent)
        {
            AudioComponent = NewObject<UAudioComponent>(Owner, TEXT("HostVoice"));
            AudioComponent->bAutoActivate = false;
            if (USceneComponent* Root = Owner->GetRootComponent()) AudioComponent->SetupAttachment(Root);
            AudioComponent->RegisterComponent();
        }
    }

    if (bBeginOnStart && GetWorld())
    {
        TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
        GetWorld()->GetTimerManager().SetTimerForNextTick([Weak]()
        {
            if (Weak.IsValid()) Weak->Begin();
        });
    }
}

void UNeocortexQuizAgent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ++TurnToken;
    if (UNeocortexService* Service = GetService()) Service->Abort(RequestToken);
    StopSpeaking();
    OnLinesSpoken = nullptr;
    OnUpNextReady = nullptr;
    Super::EndPlay(EndPlayReason);
}

UNeocortexService* UNeocortexQuizAgent::GetService() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UNeocortexSubsystem* Sub = GI ? GI->GetSubsystem<UNeocortexSubsystem>() : nullptr;
    return Sub ? Sub->GetService() : nullptr;
}

void UNeocortexQuizAgent::Report(const FNeocortexRequestError& Error)
{
    OnRequestError.Broadcast(Error);
    OnError.Broadcast(Error.Message);
}

void UNeocortexQuizAgent::BeginWithPlayers(const TArray<FNeocortexQuizParticipant>& Players)
{
    Participants = Players;
    Begin();
}

void UNeocortexQuizAgent::Begin()
{
    // Starting again is fair; two runs answering into one scene is not.
    Abort();

    bFinished = false;
    bHasQuestion = false;
    CurrentQuestion = FNeocortexQuizQuestion();
    Progress = FNeocortexQuizProgress();
    bEmotionRaised = false;
    SetRound({}, {}, 0, 0, false);

    UNeocortexService* Service = GetService();
    if (!Service)
    {
        Report(FNeocortexRequestError::FromMessage(TEXT("The Neocortex service is not available.")));
        return;
    }

    FString Mode;
    if (QuestionOrder == ENeocortexQuizQuestionOrder::InOrder) Mode = TEXT("sequential");
    else if (QuestionOrder == ENeocortexQuizQuestionOrder::Shuffled) Mode = TEXT("random");

    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    RunTurn([Weak, Mode](FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail)
    {
        UNeocortexQuizAgent* A = Weak.Get();
        A->GetService()->BeginQuiz(A->CharacterId, A->QuestionSetId, A->PlayerId, A->Participants, A->QuestionCount, Mode,
            OnTurn, OnFail, A->RequestToken);
    });
}

void UNeocortexQuizAgent::Abort()
{
    ++TurnToken;
    if (UNeocortexService* Service = GetService()) Service->Abort(RequestToken);
    RequestToken = MakeShared<FNeocortexCancelToken, ESPMode::ThreadSafe>();
    StopSpeaking();
    OnLinesSpoken = nullptr;
    OnUpNextReady = nullptr;
    DropHeld(ENeocortexQuizInputDropReason::Superseded);
    SetBusy(false);
    SetExpecting(ENeocortexQuizExpecting::Nothing);
}

void UNeocortexQuizAgent::Say(const FString& Text, bool bTyped)
{
    FHeldInput Input;
    Input.bHasText = true;
    Input.Text = Text;
    Input.bTyped = bTyped;
    Send(Input);
}

void UNeocortexQuizAgent::Choose(const FString& OptionId)
{
    FHeldInput Input;
    Input.OptionId = OptionId;
    Send(Input);
}

void UNeocortexQuizAgent::SayAs(const FString& ParticipantId, const FString& Text, bool bTyped)
{
    FHeldInput Input;
    Input.bHasText = true;
    Input.Text = Text;
    Input.bTyped = bTyped;
    Input.ParticipantId = ParticipantId;
    Send(Input);
}

void UNeocortexQuizAgent::ChooseAs(const FString& ParticipantId, const FString& OptionId)
{
    FHeldInput Input;
    Input.OptionId = OptionId;
    Input.ParticipantId = ParticipantId;
    Send(Input);
}

void UNeocortexQuizAgent::Resync()
{
    UNeocortexService* Service = GetService();
    if (SessionId.IsEmpty() || !Service) return;

    const int32 Token = ++TurnToken;
    SetBusy(true);

    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    Service->GetQuizSession(SessionId,
        FNeocortexQuizStateDelegate::CreateLambda([Weak, Token](const FNeocortexQuizState& State)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            UNeocortexQuizAgent* A = Weak.Get();
            A->bFinished = State.IsFinished();
            A->bHasQuestion = State.bHasActiveQuestion;
            A->CurrentQuestion = State.ActiveQuestion;
            A->SetRound(State.PendingParticipantIds, State.AnsweredParticipantIds, State.Attempt, State.AttemptsLeft, State.bHintGiven);
            A->Progress.Asked = FMath::Max(0, State.TotalQuestions - State.QuestionsRemaining);
            A->Progress.Total = State.TotalQuestions;
            if (State.Leaderboard.Num() > 0) A->OnLeaderboardChanged.Broadcast(State.Leaderboard);
            if (A->bHasQuestion) A->OnQuestionChanged.Broadcast(A->CurrentQuestion);
            if (A->bFinished) A->OnQuizFinished.Broadcast(State.Leaderboard);
            A->SetBusy(false);
            // Last, so a game reopening input on this event finds the agent settled.
            A->SetExpecting(State.Expecting);
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak, Token](const FNeocortexRequestError& Error)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            Weak->Report(Error);
            Weak->SetBusy(false);
            Weak->SetExpecting(ENeocortexQuizExpecting::Nothing);
        }),
        RequestToken);
}

// Turns

void UNeocortexQuizAgent::Send(FHeldInput Input)
{
    Input.At = FPlatformTime::Seconds();

    if (SessionId.IsEmpty())
    {
        OnError.Broadcast(TEXT("Call Begin before sending answers to the quiz."));
        OnInputDropped.Broadcast(Input.Spoken(), ENeocortexQuizInputDropReason::NoSession);
        return;
    }
    if (bFinished)
    {
        OnError.Broadcast(TEXT("This quiz has already finished."));
        OnInputDropped.Broadcast(Input.Spoken(), ENeocortexQuizInputDropReason::Finished);
        return;
    }

    if (bBusy)
    {
        if (!bQueueInputWhileBusy)
        {
            OnInputDropped.Broadcast(Input.Spoken(), ENeocortexQuizInputDropReason::Busy);
            return;
        }
        // Last one wins, per player: the freshest words aim at what the host has just asked.
        for (int32 i = 0; i < Held.Num(); ++i)
        {
            if (Held[i].ParticipantId == Input.ParticipantId)
            {
                OnInputDropped.Broadcast(Held[i].Spoken(), ENeocortexQuizInputDropReason::Superseded);
                Held.RemoveAt(i);
                break;
            }
        }
        Held.Add(Input);
        return;
    }

    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    RunTurn([Weak, Input](FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail)
    {
        UNeocortexQuizAgent* A = Weak.Get();
        A->GetService()->ContinueQuiz(A->SessionId, Input.bHasText, Input.Text, Input.OptionId, Input.ParticipantId, Input.bTyped,
            OnTurn, OnFail, A->RequestToken);
    });
}

void UNeocortexQuizAgent::RunTurn(TFunction<void(FNeocortexQuizTurnDelegate, FNeocortexErrorDelegate)> Request)
{
    if (!GetService())
    {
        Report(FNeocortexRequestError::FromMessage(TEXT("The Neocortex service is not available.")));
        return;
    }

    const int32 Token = ++TurnToken;
    SetBusy(true);
    // Nothing is collected while a turn is in the air.
    SetExpecting(ENeocortexQuizExpecting::Nothing);

    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    Request(
        FNeocortexQuizTurnDelegate::CreateLambda([Weak, Token](const FNeocortexQuizTurn& Turn)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            Weak->ApplyTurn(Turn, Token, false, [Weak, Token]()
            {
                if (!Weak.IsValid() || Token != Weak->TurnToken) return;
                Weak->SetBusy(false);
                Weak->FlushHeld();
            });
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak, Token](const FNeocortexRequestError& Error)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            UNeocortexQuizAgent* A = Weak.Get();
            A->Report(Error);
            A->SetBusy(false);
            A->DropHeld(ENeocortexQuizInputDropReason::Failed);
            // Resyncing would reopen input into the same refusal.
            if (Error.IsOutOfCredits())
            {
                A->SetExpecting(ENeocortexQuizExpecting::Nothing);
                A->OnCreditsExhausted.Broadcast(Error.Code);
                return;
            }
            A->Resync();
        }));
}

void UNeocortexQuizAgent::ApplyTurn(const FNeocortexQuizTurn& Turn, int32 Token, bool bResumedFromWelcome, TFunction<void()> OnApplied)
{
    if (!Turn.SessionId.IsEmpty()) SessionId = Turn.SessionId;
    LastIntent = Turn.Intent;
    SetRound(Turn.Pending, Turn.Answered, Turn.Attempt, Turn.AttemptsLeft, Turn.bHintGiven);
    OnTurnReceived.Broadcast(Turn);

    // A turn carries the question only when it changes; its arrival is the change.
    if (Turn.bHasQuestion)
    {
        bHasQuestion = true;
        CurrentQuestion = Turn.Question;
        if (Turn.bHasProgress) Progress = Turn.Progress;
    }
    else if (Turn.bDone)
    {
        bHasQuestion = false;
        CurrentQuestion = FNeocortexQuizQuestion();
        if (Turn.bHasProgress) Progress = Turn.Progress;
    }

    if (Turn.bHasUpcoming) OnUpcomingQuestion.Broadcast(Turn.Upcoming);
    if (Turn.Results.Num() > 0) OnResult.Broadcast(Turn.Results);
    if (Turn.Leaderboard.Num() > 0) OnLeaderboardChanged.Broadcast(Turn.Leaderboard);

    // Over the moment the turn says so, even while the sign off is still spoken.
    bFinished = Turn.bDone;

    // The welcome: the host greets the players and the first question comes next, asked for now
    // while the greeting plays. What marks it is that the player has nothing to do yet; a retry
    // offer or a clue also arrives without a question but expects an answer. bResumedFromWelcome
    // stops a server that keeps answering with no question from looping.
    const bool bWelcome = !Turn.bDone && !Turn.bHasQuestion
        && Turn.Expecting == ENeocortexQuizExpecting::Nothing && !bResumedFromWelcome;
    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    if (bWelcome)
    {
        bUpNextDone = false;
        bUpNextFailed = false;
        OnUpNextReady = nullptr;
        GetService()->ContinueQuiz(SessionId, false, FString(), FString(), FString(), false,
            FNeocortexQuizTurnDelegate::CreateLambda([Weak, Token](const FNeocortexQuizTurn& Next)
            {
                if (!Weak.IsValid() || Token != Weak->TurnToken) return;
                Weak->UpNext = Next;
                Weak->bUpNextDone = true;
                if (Weak->OnUpNextReady)
                {
                    TFunction<void()> Ready = MoveTemp(Weak->OnUpNextReady);
                    Weak->OnUpNextReady = nullptr;
                    Ready();
                }
            }),
            FNeocortexErrorDelegate::CreateLambda([Weak, Token](const FNeocortexRequestError& Error)
            {
                if (!Weak.IsValid() || Token != Weak->TurnToken) return;
                Weak->Report(Error);
                Weak->bUpNextFailed = true;
                Weak->bUpNextDone = true;
                if (Weak->OnUpNextReady)
                {
                    TFunction<void()> Ready = MoveTemp(Weak->OnUpNextReady);
                    Weak->OnUpNextReady = nullptr;
                    Ready();
                }
            }),
            RequestToken);
    }

    const bool bDone = Turn.bDone;
    const ENeocortexQuizExpecting NextExpecting = Turn.Expecting;
    const TArray<FNeocortexQuizLeaderboardEntry> Leaderboard = Turn.Leaderboard;
    SpeakLines(Turn.Lines, Turn.bHasQuestion, Turn.Question, Token,
        [Weak, Token, bWelcome, bDone, NextExpecting, Leaderboard, OnApplied]()
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            UNeocortexQuizAgent* A = Weak.Get();
            A->OnHostFinished.Broadcast();

            if (bWelcome)
            {
                auto Continue = [Weak, Token, OnApplied]()
                {
                    if (!Weak.IsValid() || Token != Weak->TurnToken) return;
                    // The greeting was spoken but the question never came: read the run back.
                    if (Weak->bUpNextFailed)
                    {
                        Weak->Resync();
                        return;
                    }
                    Weak->ApplyTurn(Weak->UpNext, Token, true, OnApplied);
                };
                if (A->bUpNextDone) Continue();
                else A->OnUpNextReady = Continue;
                return;
            }

            if (bDone)
            {
                A->OnQuizFinished.Broadcast(Leaderboard);
                OnApplied();
                return;
            }
            A->SetExpecting(NextExpecting);
            OnApplied();
        });
}

void UNeocortexQuizAgent::FlushHeld()
{
    if (Held.Num() == 0) return;
    const FHeldInput Next = Held[0];
    Held.RemoveAt(0);

    if (bFinished || Expecting == ENeocortexQuizExpecting::Nothing)
    {
        OnInputDropped.Broadcast(Next.Spoken(), ENeocortexQuizInputDropReason::Finished);
        return;
    }
    // An answer to a question the host has since moved past is worse than none.
    if (FPlatformTime::Seconds() - Next.At > MaxPendingInputAge)
    {
        OnInputDropped.Broadcast(Next.Spoken(), ENeocortexQuizInputDropReason::Expired);
        return;
    }
    Send(Next);
}

void UNeocortexQuizAgent::DropHeld(ENeocortexQuizInputDropReason Reason)
{
    const TArray<FHeldInput> Dropped = MoveTemp(Held);
    Held.Reset();
    for (const FHeldInput& Input : Dropped)
    {
        OnInputDropped.Broadcast(Input.Spoken(), Reason);
    }
}

void UNeocortexQuizAgent::SetRound(const TArray<FString>& WaitingOn, const TArray<FString>& Answered, int32 InAttempt, int32 Left, bool bHint)
{
    AnsweredParticipants = Answered;
    Attempt = InAttempt;
    AttemptsLeft = Left;
    bHintGiven = bHint;
    if (PendingParticipants == WaitingOn) return;
    PendingParticipants = WaitingOn;
    OnPendingChanged.Broadcast(WaitingOn);
}

void UNeocortexQuizAgent::SetBusy(bool bNext)
{
    if (bBusy == bNext) return;
    bBusy = bNext;
    OnBusyChanged.Broadcast(bNext);
}

void UNeocortexQuizAgent::SetExpecting(ENeocortexQuizExpecting Next)
{
    if (Expecting == Next) return;
    Expecting = Next;
    OnExpectingChanged.Broadcast(Next);
}

void UNeocortexQuizAgent::RaiseEmotion(EEmotions Emotion)
{
    if (bEmotionRaised && LastEmotion == Emotion) return;
    bEmotionRaised = true;
    LastEmotion = Emotion;
    OnEmotionChanged.Broadcast(Emotion);
}

// Host speech

bool UNeocortexQuizAgent::IsVoicing() const
{
    return bSpeakHostLines && AudioComponent && !CharacterId.IsEmpty() && GetService();
}

TSharedPtr<FNeocortexSpeechJob> UNeocortexQuizAgent::MakeSpeech(const FNeocortexQuizLine& Line)
{
    TSharedPtr<FNeocortexSpeechJob> Job = MakeShared<FNeocortexSpeechJob>(GetService(), AudioComponent, this,
        CharacterId, Line.Text, Line.SpokenText, Line.Emotion, bStreamHostSpeech);
    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    Job->OnError.BindLambda([Weak](const FNeocortexRequestError& Error)
    {
        if (Weak.IsValid()) Weak->Report(Error);
    });
    Job->Start();
    return Job;
}

// Says the lines in order, putting the new question on screen as the line that asks it begins.
// If no line claims to be the question, the scene is set before the first line rather than never.
// Each line's speech is requested while the line before it still plays, so the gap between two
// lines is a frame, not a round trip.
void UNeocortexQuizAgent::SpeakLines(const TArray<FNeocortexQuizLine>& Lines, bool bWithQuestion, const FNeocortexQuizQuestion& Question,
                                     int32 Token, TFunction<void()> OnDone)
{
    StopSpeaking();
    SpokenLines.Reset();
    bool bAnyAsks = false;
    for (const FNeocortexQuizLine& Line : Lines)
    {
        if (Line.Text.IsEmpty()) continue;
        bAnyAsks |= Line.IsQuestion();
        SpokenLines.Add(Line);
    }

    bQuestionPending = bWithQuestion;
    PendingQuestion = Question;
    OnLinesSpoken = MoveTemp(OnDone);
    SpeakIndex = 0;
    if (!bAnyAsks) RevealQuestion();

    if (IsVoicing() && SpokenLines.Num() > 0) NextSpeech = MakeSpeech(SpokenLines[0]);
    SpeakNextLine(Token);
}

void UNeocortexQuizAgent::SpeakNextLine(int32 Token)
{
    if (Token != TurnToken) return;

    if (SpeakIndex >= SpokenLines.Num())
    {
        RevealQuestion();
        TFunction<void()> Done = MoveTemp(OnLinesSpoken);
        OnLinesSpoken = nullptr;
        if (Done) Done();
        return;
    }

    const int32 Index = SpeakIndex;
    TSharedPtr<FNeocortexSpeechJob> Speech = NextSpeech;
    NextSpeech.Reset();
    CurrentSpeech = Speech;

    if (!Speech.IsValid())
    {
        BeginHostLine(Index, Token);
        SpeakIndex = Index + 1;
        SpeakNextLine(Token);
        return;
    }

    // The line is raised as it becomes audible, so caption and voice are one event.
    TWeakObjectPtr<UNeocortexQuizAgent> Weak(this);
    TSharedRef<bool> bAnnounced = MakeShared<bool>(false);
    Speech->OnStarted.BindLambda([Weak, Index, Token, bAnnounced]()
    {
        if (!Weak.IsValid() || Token != Weak->TurnToken) return;
        *bAnnounced = true;
        Weak->BeginHostLine(Index, Token);
    });
    Speech->OnFinished.BindLambda([Weak, Index, Token, bAnnounced](bool)
    {
        if (!Weak.IsValid() || Token != Weak->TurnToken) return;
        if (!*bAnnounced) Weak->BeginHostLine(Index, Token);
        Weak->CurrentSpeech.Reset();
        Weak->SpeakIndex = Index + 1;
        Weak->SpeakNextLine(Token);
    });
    Speech->Play();
}

void UNeocortexQuizAgent::BeginHostLine(int32 Index, int32 Token)
{
    if (!SpokenLines.IsValidIndex(Index)) return;
    // The next line is asked for here, so it never competes with this one's first sound.
    if (IsVoicing() && SpokenLines.IsValidIndex(Index + 1) && !NextSpeech.IsValid())
    {
        NextSpeech = MakeSpeech(SpokenLines[Index + 1]);
    }
    const FNeocortexQuizLine Line = SpokenLines[Index];
    if (Line.IsQuestion()) RevealQuestion();
    OnHostLine.Broadcast(Line);
    RaiseEmotion(Line.Emotion);
}

void UNeocortexQuizAgent::RevealQuestion()
{
    if (!bQuestionPending) return;
    bQuestionPending = false;
    OnQuestionChanged.Broadcast(PendingQuestion);
}

void UNeocortexQuizAgent::StopSpeaking()
{
    if (CurrentSpeech.IsValid()) CurrentSpeech->Stop();
    if (NextSpeech.IsValid()) NextSpeech->Stop();
    CurrentSpeech.Reset();
    NextSpeech.Reset();
    if (AudioComponent && AudioComponent->IsPlaying()) AudioComponent->Stop();
}
