#include "NeocortexSmartAgent.h"
#include "Neocortex.h"
#include "NeocortexService.h"
#include "NeocortexSettings.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSessionManager.h"
#include "NeocortexEventLogger.h"
#include "NeocortexActionQueue.h"
#include "NeocortexFunctionLibrary.h"
#include "NeocortexSpeech.h"
#include "Engine/World.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

// Fixed pause between two characters in a group turn.
static constexpr float GroupSpeakerGap = 1.0f;

// Reading time before the next line drops in when no audio paces the lines.
static float LineDelay(const FString& Text)
{
    return FMath::Clamp(Text.Len() / 15.0f, 0.5f, 3.0f);
}

static bool NeedsAudio(EChatLinesMode Mode)
{
    return Mode == EChatLinesMode::SingleAudio || Mode == EChatLinesMode::PerLineAudio;
}

void UNeocortexSmartAgent::BeginPlay()
{
    Super::BeginPlay();
    ResolveService();
    if (NeedsAudio(ChatLinesMode)) EnsureAudioComponent();

    // Next tick, so listeners bound in other BeginPlay calls hear it. An empty session id would
    // be rejected by the server, so only load when one exists.
    if (bLoadHistoryOnStart && !GetSessionId().IsEmpty())
    {
        if (UWorld* W = GetWorld())
        {
            TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
            W->GetTimerManager().SetTimerForNextTick([Weak]()
            {
                if (Weak.IsValid()) Weak->RequestChatHistory();
            });
        }
    }
}

void UNeocortexSmartAgent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Service.IsValid()) Service->Abort(RequestToken);
    // Cancel any in-flight playback and timers.
    ++PlaybackToken;
    StopAudio();
    SpeakDone = nullptr;
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(LineTimer);
        W->GetTimerManager().ClearTimer(SpeakGapTimer);
        W->GetTimerManager().ClearTimer(ActionDelayTimer);
    }
    Super::EndPlay(EndPlayReason);
}

void UNeocortexSmartAgent::ResolveService()
{
    if (GetWorld())
    {
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            if (UNeocortexSubsystem* Sub = GI->GetSubsystem<UNeocortexSubsystem>())
            {
                Service = Sub->GetService();
                Subsystem = Sub;
            }
        }
    }
}

void UNeocortexSmartAgent::EnsureAudioComponent()
{
    AActor* Owner = GetOwner();
    if (AudioComponent || !Owner) return;

    AudioComponent = Owner->FindComponentByClass<UAudioComponent>();
    if (AudioComponent) return;

    AudioComponent = NewObject<UAudioComponent>(Owner, TEXT("NeocortexVoice"));
    AudioComponent->bAutoActivate = false;
    if (USceneComponent* Root = Owner->GetRootComponent())
    {
        AudioComponent->SetupAttachment(Root);
    }
    AudioComponent->RegisterComponent();
}

bool UNeocortexSmartAgent::CheckReady(const TCHAR* Caller)
{
    if (Service.IsValid() && !ProjectId.IsEmpty()) return true;

    const FString Message = ProjectId.IsEmpty()
        ? TEXT("Character ID is required. Set Project Id on the Smart Agent.")
        : TEXT("The Neocortex service is not available.");
    UE_LOG(LogNeocortex, Error, TEXT("%s: %s"), Caller, *Message);
    ReportError(FNeocortexRequestError::FromMessage(Message));
    return false;
}

FNeocortexCancelTokenPtr UNeocortexSmartAgent::NewRequestToken()
{
    RequestToken = MakeShared<FNeocortexCancelToken, ESPMode::ThreadSafe>();
    return RequestToken;
}

FString UNeocortexSmartAgent::GetMetadata() const
{
    if (!bIncludeAllInteractables || !Subsystem.IsValid())
    {
        return TEXT("");
    }

    const AActor* Owner = GetOwner();
    if (!Owner)
    {
        return Subsystem->CreateInteractablesMetadata();
    }

    return Subsystem->CreatePerceptionMetadata(Owner->GetActorLocation(),
        bUseRadiusFilter ? SearchRadius : 0.0f, MaxPerceivedEntities, ProjectId, Owner->GetName());
}

FString UNeocortexSmartAgent::GetSessionId() const
{
    return Subsystem.IsValid() ? Subsystem->GetSessionManager()->Get(ProjectId) : FString();
}

void UNeocortexSmartAgent::ClearSessionId()
{
    if (Subsystem.IsValid())
    {
        Subsystem->GetSessionManager()->Clear(ProjectId);
    }
}

void UNeocortexSmartAgent::SendMessage(const FString& Message)
{
    if (!CheckReady(TEXT("SendMessage"))) return;

    if (ChatLinesMode == EChatLinesMode::Off)
    {
        if (bRequestPending)
        {
            ReportError(FNeocortexRequestError::FromMessage(TEXT("A request is already in flight.")));
            return;
        }

        bRequestPending = true;
        bExpectingAudio = false;
        SendChat(Message);
        return;
    }

    // A chat lines mode: the chat request is text only; audio (if any) is produced during playback.
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    const FString Msg = Message;
    Dispatch([Weak, Msg]()
    {
        if (Weak.IsValid()) Weak->SendChat(Msg);
    });
}

void UNeocortexSmartAgent::SendChat(const FString& Message)
{
    Service->TextToText(
        ProjectId, Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
        GetMetadata(), UNeocortexEventLogger::ConsumeLogsJson(this), NewRequestToken());
}

void UNeocortexSmartAgent::SendMessageForAudio(const FString& Message)
{
    // In a chat lines mode the agent voices the reply itself, so only text is asked for rather
    // than also paying for a separate full reply clip.
    if (ChatLinesMode != EChatLinesMode::Off)
    {
        SendMessage(Message);
        return;
    }

    if (!CheckReady(TEXT("SendMessageForAudio"))) return;
    if (bRequestPending)
    {
        ReportError(FNeocortexRequestError::FromMessage(TEXT("A request is already in flight.")));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = true;
    Service->TextToAudio(
        ProjectId,
        Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexAudioDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnAudioResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
        GetMetadata(),
        UNeocortexEventLogger::ConsumeLogsJson(this),
        NewRequestToken());
}

void UNeocortexSmartAgent::TranscribeBytes(const TArray<uint8>& Data)
{
    if (!CheckReady(TEXT("TranscribeBytes"))) return;
    if (bRequestPending)
    {
        ReportError(FNeocortexRequestError::FromMessage(TEXT("A request is already in flight.")));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = false;
    Service->AudioToText(
        ProjectId,
        Data,
        FNeocortexTranscribeDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnTranscribeResponse),
        FNeocortexErrorDelegate::CreateWeakLambda(this, [this](const FNeocortexRequestError& Error)
        {
            bRequestPending = false;
            ReportError(Error);
        }),
        NewRequestToken());
}

void UNeocortexSmartAgent::SendAudioForAudio(const TArray<uint8>& WavData)
{
    if (!CheckReady(TEXT("SendAudioForAudio"))) return;

    if (ChatLinesMode == EChatLinesMode::Off)
    {
        if (bRequestPending)
        {
            ReportError(FNeocortexRequestError::FromMessage(TEXT("A request is already in flight.")));
            return;
        }

        bRequestPending = true;
        bExpectingAudio = true;
        Service->AudioToAudio(
            ProjectId,
            WavData,
            FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
            FNeocortexAudioDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnAudioResponse),
            FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
            GetMetadata(),
            UNeocortexEventLogger::ConsumeLogsJson(this),
            NewRequestToken());
        return;
    }

    // A chat lines mode: transcribe, then chat (audio is voiced during playback).
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    const TArray<uint8> Wav = WavData;
    Dispatch([Weak, Wav]()
    {
        if (!Weak.IsValid()) return;
        UNeocortexSmartAgent* A = Weak.Get();
        const FNeocortexCancelTokenPtr Token = A->NewRequestToken();
        A->Service->AudioToText(
            A->ProjectId, Wav,
            FNeocortexTranscribeDelegate::CreateLambda([Weak, Token](const FNeocortexAudioTranscribeResponseData& T)
            {
                if (!Weak.IsValid() || Token->IsCancelled()) return;
                Weak->OnTranscribed.Broadcast(T.Response);
                Weak->SendChat(T.Response);
            }),
            FNeocortexErrorDelegate::CreateUObject(A, &UNeocortexSmartAgent::OnServiceFail),
            Token);
    });
}

void UNeocortexSmartAgent::RequestChatHistory(int32 Limit, const FString& Before)
{
    if (!CheckReady(TEXT("RequestChatHistory"))) return;

    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    Service->GetChatHistory(GetSessionId(), Limit, Before,
        FNeocortexChatHistoryDelegate::CreateLambda([Weak](const FNeocortexChatHistoryPage& Page)
        {
            if (Weak.IsValid()) Weak->OnChatHistory.Broadcast(Page.Messages, Page.NextCursor);
        }),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::ReportError));
}

void UNeocortexSmartAgent::OnChatResponse(const FNeocortexChatResponseData& ChatResponse)
{
    OnChat.Broadcast(ChatResponse);
    CaptureActions(ChatResponse.Actions);

    if (ChatLinesMode != EChatLinesMode::Off)
    {
        PlayReply(ChatResponse);
        return;
    }

    // Only terminal for TextToText; TextToAudio and AudioToAudio still expect an audio response.
    if (!bExpectingAudio) bRequestPending = false;
}

void UNeocortexSmartAgent::OnAudioResponse(const TArray<uint8>& Bytes)
{
    bRequestPending = false;
    bExpectingAudio = false;

    // Empty bytes: the reply had nothing to speak.
    if (Bytes.Num() == 0) return;

    float Duration = 0.0f;
    USoundWaveProcedural* SW = NeocortexAudio::DecodeMp3(this, Bytes, Duration);
    if (!SW)
    {
        ReportError(FNeocortexRequestError::FromMessage(TEXT("The speech response could not be decoded.")));
        return;
    }

    LastSoundWave = SW;
    OnAudio.Broadcast(LastSoundWave);
}

void UNeocortexSmartAgent::OnTranscribeResponse(const FNeocortexAudioTranscribeResponseData& TranscribeResponse)
{
    bRequestPending = false;
    OnTranscribed.Broadcast(TranscribeResponse.Response);
}

void UNeocortexSmartAgent::ReportError(const FNeocortexRequestError& RequestError)
{
    OnRequestError.Broadcast(RequestError);
    OnError.Broadcast(RequestError.Message);
}

void UNeocortexSmartAgent::OnServiceFail(const FNeocortexRequestError& RequestError)
{
    ReportError(RequestError);

    if (ChatLinesMode != EChatLinesMode::Off)
    {
        // Only a failure before playback unblocks the queue; a line's audio failing during
        // playback is handled inline (the text still shows, playback advances).
        if (State == EReplyState::WaitingForReply)
        {
            State = EReplyState::Idle;
            ++PlaybackToken;
            DispatchNext();
        }
        return;
    }

    bRequestPending = false;
    bExpectingAudio = false;
}

void UNeocortexSmartAgent::Speak(const FNeocortexGroupMessage& Message, TFunction<void()> OnDone)
{
    FNeocortexChatResponseData Response = UNeocortexService::FlattenMessage(Message, FString(), TArray<FNeocortexInteractable>());
    if (Response.CharacterId.IsEmpty()) Response.CharacterId = ProjectId;
    OnChat.Broadcast(Response);
    CaptureActions(Response.Actions);

    if (ChatLinesMode == EChatLinesMode::Off || !Service.IsValid())
    {
        if (OnDone) OnDone();
        return;
    }

    PlayReply(Response);
    SpeakDone = MoveTemp(OnDone);
}

void UNeocortexSmartAgent::StopSpeaking()
{
    ++PlaybackToken;
    StopAudio();
    PendingInputs.Empty();
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(LineTimer);
    }
    bSingleClipActive = false;
    if (State != EReplyState::Idle)
    {
        State = EReplyState::Idle;
        OnReplyFinished.Broadcast();
    }
    if (SpeakDone)
    {
        TFunction<void()> Done = MoveTemp(SpeakDone);
        SpeakDone = nullptr;
        Done();
    }
}

void UNeocortexSmartAgent::Abort()
{
    if (Service.IsValid()) Service->Abort(RequestToken);
    RequestToken.Reset();
    bRequestPending = false;
    bExpectingAudio = false;
    StopSpeaking();
}

// Chat lines playback

void UNeocortexSmartAgent::Dispatch(TFunction<void()> Send)
{
    if (State != EReplyState::Idle)
    {
        PendingInputs.Add(MoveTemp(Send));
        UE_LOG(LogNeocortex, Verbose, TEXT("Input received while speaking, queued (pending: %d)."), PendingInputs.Num());
        return;
    }

    State = EReplyState::WaitingForReply;
    Send();
}

void UNeocortexSmartAgent::DispatchNext()
{
    if (PendingInputs.Num() == 0) return;

    TFunction<void()> Next = MoveTemp(PendingInputs[0]);
    PendingInputs.RemoveAt(0);
    State = EReplyState::WaitingForReply;
    Next();
}

void UNeocortexSmartAgent::PlayReply(const FNeocortexChatResponseData& Response)
{
    // A previous reply's speech must not outlive the reply that replaced it.
    StopAudio();

    ReplyLines = Response.ResolveLines();
    LineIndex = 0;
    bSingleClipMode = false;
    State = EReplyState::Playing;
    StartPlayback(++PlaybackToken);
}

void UNeocortexSmartAgent::StartPlayback(int32 Token)
{
    LineIndex = 0;
    bVoicePerLine = false;

    if (ChatLinesMode == EChatLinesMode::Text)
    {
        PlayNextLine(Token);
        return;
    }

    EnsureAudioComponent();
    if (!AudioComponent)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("A voiced chat lines mode needs an AudioComponent. Showing text only (no credits spent)."));
        PlayNextLine(Token);
        return;
    }

    // Credit aware: empty means text only; low means one clip for the whole reply.
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    const EChatLinesMode Mode = ChatLinesMode;
    auto Voice = [Weak, Token, Mode](bool bLow)
    {
        if (!Weak.IsValid() || Token != Weak->PlaybackToken) return;
        UNeocortexSmartAgent* A = Weak.Get();
        if (Mode == EChatLinesMode::SingleAudio || bLow)
        {
            if (Mode != EChatLinesMode::SingleAudio)
            {
                UE_LOG(LogNeocortex, Warning, TEXT("Credits low, voicing this reply with one clip instead of one per line."));
            }
            A->PlayOneClipAndDrop(Token);
            return;
        }
        A->bVoicePerLine = true;
        A->PlayNextLine(Token);
    };

    Service->GetUsage(FString(), ProjectId,
        FNeocortexUsageDelegate::CreateLambda([Weak, Token, Voice](const FNeocortexUsageResponse& Usage)
        {
            if (!Weak.IsValid() || Token != Weak->PlaybackToken) return;
            if (Usage.Status == EUsageStatus::Empty)
            {
                UE_LOG(LogNeocortex, Warning, TEXT("Credits empty, showing the reply as text only."));
                Weak->PlayNextLine(Token);
                return;
            }
            Voice(Usage.Status == EUsageStatus::Low);
        }),
        // A failed lookup never blocks a paid feature.
        FNeocortexErrorDelegate::CreateLambda([Voice](const FNeocortexRequestError&)
        {
            Voice(false);
        }));
}

TSharedPtr<FNeocortexSpeechJob> UNeocortexSmartAgent::MakeSpeechJob(const FString& Text, const FString& SpokenText, EEmotions Emotion)
{
    TSharedPtr<FNeocortexSpeechJob> Job = MakeShared<FNeocortexSpeechJob>(Service.Get(), AudioComponent, this,
        ProjectId, Text, SpokenText, Emotion, bStreamSpeech);
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    Job->OnClip.BindLambda([Weak](USoundWaveProcedural* Clip)
    {
        if (!Weak.IsValid()) return;
        Weak->LastSoundWave = Clip;
        Weak->OnAudio.Broadcast(Clip);
    });
    Job->OnError.BindLambda([Weak](const FNeocortexRequestError& Error)
    {
        if (Weak.IsValid()) Weak->ReportError(Error);
    });
    Job->Start();
    return Job;
}

void UNeocortexSmartAgent::PlayNextLine(int32 Token)
{
    if (Token != PlaybackToken) return;

    if (LineIndex >= ReplyLines.Num())
    {
        OnLinesDone(Token);
        return;
    }

    const int32 Index = LineIndex;
    const FNeocortexChatLine Line = ReplyLines[Index];

    if (!bVoicePerLine)
    {
        BeginLine(Line);
        LineIndex = Index + 1;
        if (LineIndex >= ReplyLines.Num())
        {
            OnLinesDone(Token);
            return;
        }
        OnComposingNextLine.Broadcast();
        ScheduleNextLine(LineDelay(Line.Text), Token);
        return;
    }

    TSharedPtr<FNeocortexSpeechJob> Speech = NextSpeech.IsValid() && NextSpeechIndex == Index
        ? NextSpeech
        : MakeSpeechJob(Line.Text, Line.SpokenText, Line.Emotion);
    NextSpeech.Reset();
    NextSpeechIndex = INDEX_NONE;
    CurrentSpeech = Speech;

    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    TSharedRef<bool> bAnnounced = MakeShared<bool>(false);
    // The line is announced as it becomes audible, so caption and voice are one event. The next
    // line is fetched from here, so it never competes with this one's first sound.
    Speech->OnStarted.BindLambda([Weak, Token, Index, bAnnounced]()
    {
        if (!Weak.IsValid() || Token != Weak->PlaybackToken) return;
        UNeocortexSmartAgent* A = Weak.Get();
        *bAnnounced = true;
        A->BeginLine(A->ReplyLines[Index]);
        if (A->ReplyLines.IsValidIndex(Index + 1))
        {
            const FNeocortexChatLine& Next = A->ReplyLines[Index + 1];
            A->NextSpeech = A->MakeSpeechJob(Next.Text, Next.SpokenText, Next.Emotion);
            A->NextSpeechIndex = Index + 1;
        }
    });
    Speech->OnFinished.BindLambda([Weak, Token, Index, bAnnounced](bool)
    {
        if (!Weak.IsValid() || Token != Weak->PlaybackToken) return;
        UNeocortexSmartAgent* A = Weak.Get();
        A->CurrentSpeech.Reset();
        A->LineIndex = Index + 1;
        if (!*bAnnounced)
        {
            // Nothing was heard (nothing to speak, or the speech failed): the text still shows.
            const FNeocortexChatLine& Line = A->ReplyLines[Index];
            A->BeginLine(Line);
            A->ScheduleNextLine(LineDelay(Line.Text), Token);
            return;
        }
        A->PlayNextLine(Token);
    });
    Speech->Play();
}

void UNeocortexSmartAgent::ScheduleNextLine(float Delay, int32 Token)
{
    UWorld* W = GetWorld();
    if (!W) return;
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    W->GetTimerManager().SetTimer(LineTimer, FTimerDelegate::CreateLambda([Weak, Token]()
    {
        if (Weak.IsValid()) Weak->PlayNextLine(Token);
    }), Delay, false);
}

void UNeocortexSmartAgent::OnLinesDone(int32 Token)
{
    if (Token != PlaybackToken) return;
    if (bSingleClipMode)
    {
        bSingleLinesDone = true;
        FinishSingleClipIfDone(Token);
        return;
    }
    FinishReply(Token);
}

void UNeocortexSmartAgent::PlayOneClipAndDrop(int32 Token)
{
    FString Text, Spoken;
    for (const FNeocortexChatLine& Line : ReplyLines)
    {
        Text += Line.Text;
        Spoken += Line.SpeechText();
    }
    const EEmotions FirstEmotion = ReplyLines.Num() > 0 ? ReplyLines[0].Emotion : EEmotions::Neutral;

    bSingleClipMode = true;
    bSingleClipActive = true;
    bSingleLinesDone = false;

    // Hold the speaking state until the clip ends so queued input does not cut in.
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    CurrentSpeech = MakeSpeechJob(Text, Spoken, FirstEmotion);
    CurrentSpeech->OnFinished.BindLambda([Weak, Token](bool)
    {
        if (!Weak.IsValid() || Token != Weak->PlaybackToken) return;
        Weak->CurrentSpeech.Reset();
        Weak->bSingleClipActive = false;
        Weak->FinishSingleClipIfDone(Token);
    });
    CurrentSpeech->Play();

    // The voice covers the gap, so no typing cue while the character speaks.
    DropLineForSingleClip(0, Token);
}

void UNeocortexSmartAgent::DropLineForSingleClip(int32 Index, int32 Token)
{
    if (Token != PlaybackToken) return;
    if (!ReplyLines.IsValidIndex(Index))
    {
        OnLinesDone(Token);
        return;
    }

    BeginLine(ReplyLines[Index]);
    if (!ReplyLines.IsValidIndex(Index + 1))
    {
        OnLinesDone(Token);
        return;
    }

    if (UWorld* W = GetWorld())
    {
        TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
        W->GetTimerManager().SetTimer(LineTimer, FTimerDelegate::CreateLambda([Weak, Index, Token]()
        {
            if (Weak.IsValid()) Weak->DropLineForSingleClip(Index + 1, Token);
        }), LineDelay(ReplyLines[Index].Text), false);
    }
}

void UNeocortexSmartAgent::FinishSingleClipIfDone(int32 Token)
{
    if (Token != PlaybackToken || bSingleClipActive || !bSingleLinesDone) return;
    bSingleClipMode = false;
    FinishReply(Token);
}

void UNeocortexSmartAgent::StopAudio()
{
    if (CurrentSpeech.IsValid()) CurrentSpeech->Stop();
    if (NextSpeech.IsValid()) NextSpeech->Stop();
    CurrentSpeech.Reset();
    NextSpeech.Reset();
    NextSpeechIndex = INDEX_NONE;
    if (AudioComponent && AudioComponent->IsPlaying())
    {
        AudioComponent->Stop();
    }
}

void UNeocortexSmartAgent::BeginLine(const FNeocortexChatLine& Line)
{
    OnChatLineStarted.Broadcast(Line);
    OnEmotionChanged.Broadcast(Line.Emotion);
    if (ActionTrigger == EActionTrigger::WhenSpeechStarts) RunPendingActions();
}

void UNeocortexSmartAgent::FinishReply(int32 Token)
{
    if (Token != PlaybackToken) return;

    State = EReplyState::Idle;
    OnReplyFinished.Broadcast();
    if (ActionTrigger == EActionTrigger::AfterReplySpoken) RunPendingActions();

    if (SpeakDone)
    {
        // A short pause before the next character speaks.
        TFunction<void()> Done = MoveTemp(SpeakDone);
        SpeakDone = nullptr;
        if (UWorld* W = GetWorld())
        {
            W->GetTimerManager().SetTimer(SpeakGapTimer, FTimerDelegate::CreateLambda([Done]() { Done(); }), GroupSpeakerGap, false);
        }
        else
        {
            Done();
        }
    }
    DispatchNext();
}

// Actions

void UNeocortexSmartAgent::RegisterActionEvent(const FString& Keyword, FNeocortexActionHandler Handler)
{
    ActionHandlers.Add(Keyword, [Handler](const FNeocortexChatAction& Action, TFunction<void()> Finish)
    {
        Handler.ExecuteIfBound(Action);
        Finish();
    });
}

void UNeocortexSmartAgent::RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&)> Handler)
{
    ActionHandlers.Add(Keyword, [Handler](const FNeocortexChatAction& Action, TFunction<void()> Finish)
    {
        Handler(Action);
        Finish();
    });
}

void UNeocortexSmartAgent::RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&, TFunction<void()>)> Handler)
{
    ActionHandlers.Add(Keyword, MoveTemp(Handler));
}

void UNeocortexSmartAgent::UnregisterAction(const FString& Keyword)
{
    ActionHandlers.Remove(Keyword);
}

void UNeocortexSmartAgent::CaptureActions(const TArray<FNeocortexChatAction>& Actions)
{
    for (const FNeocortexChatAction& Action : Actions)
    {
        if (!Action.Name.IsEmpty()) PendingActions.Add(Action);
    }

    bool bDeferred = ActionTrigger != EActionTrigger::WhenResponseReceived;
    // The deferred triggers wait for a playback event that Off mode never raises.
    if (bDeferred && ChatLinesMode == EChatLinesMode::Off)
    {
        if (!bWarnedOffModeTrigger)
        {
            bWarnedOffModeTrigger = true;
            UE_LOG(LogNeocortex, Warning, TEXT("ActionTrigger waits for speech, but ChatLinesMode is Off and plays none. Running actions on arrival instead."));
        }
        bDeferred = false;
    }
    if (!bDeferred) RunPendingActions();
}

void UNeocortexSmartAgent::RunPendingActions()
{
    // Safe to call more than once; the running and empty guards make it a no-op.
    if (!bActionsRunning && PendingActions.Num() > 0) ProcessNextAction();
}

void UNeocortexSmartAgent::ProcessNextAction()
{
    if (PendingActions.Num() == 0)
    {
        bActionsRunning = false;
        OnActionsCompleted.Broadcast();
        return;
    }

    bActionsRunning = true;
    const FNeocortexChatAction Action = PendingActions[0];
    PendingActions.RemoveAt(0);

    if (const FActionHandler* Handler = ActionHandlers.Find(Action.Name))
    {
        // Finish may be called once, from anywhere, at any later time.
        TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
        TSharedRef<bool> bFinished = MakeShared<bool>(false);
        const FActionHandler Run = *Handler;
        Run(Action, [Weak, bFinished]()
        {
            if (*bFinished || !Weak.IsValid()) return;
            *bFinished = true;
            Weak->ContinueActions();
        });
        return;
    }

    OnUnhandledAction.Broadcast(Action);
    // A sibling action queue component may be the one running this action.
    if (!GetOwner() || !GetOwner()->FindComponentByClass<UNeocortexActionQueue>())
    {
        UE_LOG(LogNeocortex, Warning, TEXT("%s asked for action '%s', which has no handler. Register one with RegisterAction."),
            *ProjectId, *Action.Name);
    }
    ContinueActions();
}

void UNeocortexSmartAgent::ContinueActions()
{
    UWorld* W = GetWorld();
    if (DelayBetweenActions <= 0.f || PendingActions.Num() == 0 || !W)
    {
        ProcessNextAction();
        return;
    }
    TWeakObjectPtr<UNeocortexSmartAgent> Weak(this);
    W->GetTimerManager().SetTimer(ActionDelayTimer, FTimerDelegate::CreateLambda([Weak]()
    {
        if (Weak.IsValid()) Weak->ProcessNextAction();
    }), DelayBetweenActions, false);
}
