#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexHttpClient.h"
#include "Enums/NeocortexEnumTypes.h"
#include "NeocortexSmartAgent.generated.h"

class USoundWaveProcedural;
class UAudioComponent;
class FNeocortexSpeechJob;
class FNeocortexChatStream;

/** Fired when a text chat response is received from the agent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentChat, const FNeocortexChatResponseData&, ChatResponseData);

/** Fired when an audio response is received from the agent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentAudio, USoundWave*, SoundWave);

/** Fired when audio transcription completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentTranscribe, const FString&, Text);

/** Fired when a page of chat history is retrieved. Pass NextCursor to RequestChatHistory to load older messages. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSmartAgentChatHistory, const TArray<FNeocortexChatHistoryEntry>&, Messages, const FString&, NextCursor);

/** Fired when an error occurs during any agent operation. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentError, const FString&, Error);

/** Fired with the classified error when any agent operation fails. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentRequestError, const FNeocortexRequestError&, Error);

/** Fired when a chat line drops in. See UNeocortexSmartAgent::ChatLinesMode. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentChatLine, const FNeocortexChatLine&, Line);

/** Fired with each line's emotion as it drops in. Drive character animation here. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentEmotion, EEmotions, Emotion);

/** Fired during the pause before the next line in Text mode, for a typing indicator. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSmartAgentComposingNextLine);

/** Fired once the whole reply has finished playing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSmartAgentReplyFinished);

/** Fired when all actions of a reply have been run. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSmartAgentActionsCompleted);

/** Fired when an action arrives that has no handler registered. It is skipped. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentUnhandledAction, const FNeocortexChatAction&, Action);

/** Fired with each piece of text as a streamed reply is written. See UNeocortexSmartAgent::bStreamReplies. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSmartAgentTextDelta, const FString&, Text, int32, LineIndex);

/** Blueprint handler for an action keyword. The next action starts when it returns. */
DECLARE_DYNAMIC_DELEGATE_OneParam(FNeocortexActionHandler, const FNeocortexChatAction&, Action);

/**
 * A Neocortex character: sends messages, speaks the reply as chat lines, and runs the actions
 * it triggers.
 *
 * Set ProjectId, then call SendMessage, SendMessageForAudio or SendAudioForAudio.
 * ChatLinesMode decides how a reply is delivered:
 *   PerLineAudio  each line is voiced separately (about 1 audio credit per line)
 *   SingleAudio   lines drop in while one clip voices the whole reply (1 credit)
 *   Text          lines drop in with an emotion each, no audio
 *   Off           one reply, delivered whole, nothing played for you
 * In every mode but Off, input sent while the character is still speaking is queued and sent
 * once the reply ends.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent), EditInlineNew, DefaultToInstanced)
class NEOCORTEX_API UNeocortexSmartAgent : public UActorComponent
{
    GENERATED_BODY()
public:
    /** Unique identifier for the character this agent represents. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString ProjectId;

    /** Fetch and replay this character's stored conversation (OnChatHistory) when play begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bLoadHistoryOnStart = false;

    /**
     * Controls how replies are delivered. PerLineAudio costs about 1 audio credit PER LINE;
     * SingleAudio costs 1 per reply. Both voiced modes check your credits first: a low balance
     * voices the reply with one clip, an empty one shows text only.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Chat Lines")
    EChatLinesMode ChatLinesMode = EChatLinesMode::PerLineAudio;

    /** Where line audio plays. When left empty, one on the owner is used or created for a voiced mode. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Chat Lines")
    TObjectPtr<UAudioComponent> AudioComponent = nullptr;

    /**
     * Play speech as it is synthesized instead of waiting for whole clips, so the voice starts
     * sooner. Falls back to whole clips where streaming is not available.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Chat Lines")
    bool bStreamSpeech = true;

    /**
     * EXPERIMENTAL: stream the reply itself, so the first line starts while the rest is still
     * being written. Raises OnTextDelta as text arrives. Falls back to whole replies where
     * streaming is not available.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Chat Lines")
    bool bStreamReplies = false;

    /** When a reply's actions run. The two speech triggers need a mode other than Off; Off runs them on arrival. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Actions")
    EActionTrigger ActionTrigger = EActionTrigger::WhenResponseReceived;

    /** Seconds to wait after one action finishes before the next one starts, when a reply carries several. 0 runs them back to back. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Actions", meta = (ClampMin = "0.0", Units = "s"))
    float DelayBetweenActions = 0.f;

    /** Send what this character perceives (every UNeocortexInteractableComponent in the world) with each message. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables")
    bool bIncludeAllInteractables = true;

    /** If true, only include interactables within SearchRadius of this agent's owner. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables", meta = (EditCondition = "bIncludeAllInteractables"))
    bool bUseRadiusFilter = false;

    /** Radius in world units to search for nearby interactables when bUseRadiusFilter is true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables", meta = (EditCondition = "bIncludeAllInteractables && bUseRadiusFilter", ClampMin = "0.0"))
    float SearchRadius = 2000.0f;

    /** Ceiling on how many interactables one message carries. The nearest win. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables", meta = (EditCondition = "bIncludeAllInteractables", ClampMin = "1"))
    int32 MaxPerceivedEntities = 10;

    /** Fired when a text chat response is received. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentChat OnChat;

    /**
     * Fired with each clip of speech: the whole reply in Off mode, each voiced line or the
     * reply clip otherwise. In a voiced mode the clip is already playing on AudioComponent.
     */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentAudio OnAudio;

    /** Fired when audio transcription completes. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentTranscribe OnTranscribed;

    /** Fired when a page of chat history is retrieved. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentChatHistory OnChatHistory;

    /** Fired with a short message when an operation fails. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentError OnError;

    /** Fired with the classified error (status, code, IsRetryable) when an operation fails. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentRequestError OnRequestError;

    /** Fired when a chat line drops in. Add it to your chat panel here. Not fired in Off mode. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Chat Lines")
    FOnSmartAgentChatLine OnChatLineStarted;

    /** Fired with each line's emotion as it drops in. Not fired in Off mode. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Chat Lines")
    FOnSmartAgentEmotion OnEmotionChanged;

    /** Fired in Text mode during the pause before the next line. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Chat Lines")
    FOnSmartAgentComposingNextLine OnComposingNextLine;

    /** Fired once the whole reply has finished. Not fired in Off mode. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Chat Lines")
    FOnSmartAgentReplyFinished OnReplyFinished;

    /** Fired with each piece of text while a streamed reply is written (bStreamReplies). */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Chat Lines")
    FOnSmartAgentTextDelta OnTextDelta;

    /** Fired when all actions of a reply have been run. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Actions")
    FOnSmartAgentActionsCompleted OnActionsCompleted;

    /** Fired when an action arrives that has no handler registered. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Actions")
    FOnSmartAgentUnhandledAction OnUnhandledAction;

    /**
     * Sends a text message to the agent and receives a text response.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendMessage(const FString& Message);

    /**
     * Sends a text message and receives the reply with speech. In a chat lines mode the agent
     * voices the reply itself, so this is the same as SendMessage.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendMessageForAudio(const FString& Message);

    /**
     * Transcribes audio data to text, without chatting.
     * @param Data Audio data in WAV format
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void TranscribeBytes(const TArray<uint8>& Data);

    /**
     * Transcribes audio then sends the result as a message, receiving both text and audio responses.
     * This is the full voice-in / voice-out flow.
     * @param WavData Audio data in WAV format
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendAudioForAudio(const TArray<uint8>& WavData);

    /**
     * Fetches a page of this character's conversation, oldest turn first (OnChatHistory).
     * @param Limit Maximum number of messages to retrieve
     * @param Before The NextCursor of the previous page, to load older messages; empty for the latest
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void RequestChatHistory(int32 Limit = 10, const FString& Before = TEXT(""));

    /** Fetches the latest messages of this character's conversation (OnChatHistory). */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void GetChatHistory(int32 Limit = 10) { RequestChatHistory(Limit); }

    /** The current conversation's session id, or empty before the first message. */
    UFUNCTION(BlueprintPure, Category = "Neocortex")
    FString GetSessionId() const;

    /** Clears the session ID to start a new conversation. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ClearSessionId();

    /**
     * Assigns what performs an action keyword, as authored on the character. The handler runs
     * when the action comes up and the next action starts when it returns. Action.TargetId
     * names the interactable it applies to.
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Actions", meta = (DisplayName = "Register Action"))
    void RegisterActionEvent(const FString& Keyword, FNeocortexActionHandler Handler);

    /** Assigns an instant C++ handler for an action keyword. */
    void RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&)> Handler);

    /**
     * Assigns a C++ handler that takes time. Call Finish when the action is done; the next
     * action waits for it.
     *   Agent->RegisterAction(TEXT("DANCE"), [this](const FNeocortexChatAction& Action, TFunction<void()> Finish) { ... });
     */
    void RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&, TFunction<void()> /*Finish*/)> Handler);

    /** Removes the handler for an action keyword. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Actions")
    void UnregisterAction(const FString& Keyword);

    /**
     * Speaks a reply produced elsewhere (a UNeocortexGroupDirector turn) through this character.
     * OnDone runs once the reply has been played, after a short pause before the next speaker.
     */
    void Speak(const FNeocortexGroupMessage& Message, TFunction<void()> OnDone);

    /** Stops whatever the character is saying and drops queued input. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void StopSpeaking();

    /** Cancels the request in flight (a whole voice turn included) and stops speaking. Nothing reports an error for it. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Abort();

    /** Returns true while a request is in flight or a reply is still playing. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex")
    bool IsRequestPending() const { return bRequestPending || State != EReplyState::Idle; }

    /** True while a reply is pending or its chat lines are still being played. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex|Chat Lines")
    bool IsSpeaking() const { return State != EReplyState::Idle; }

    /** True while a reply is being waited for and nothing is playing yet. Drive a thinking pose here. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex|Chat Lines")
    bool IsThinking() const { return State == EReplyState::WaitingForReply || (ChatLinesMode == EChatLinesMode::Off && bRequestPending); }

    /** True while the reply's actions are being run. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex|Actions")
    bool IsRunningActions() const { return bActionsRunning; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Playback state for the chat lines modes. */
    enum class EReplyState : uint8 { Idle, WaitingForReply, Playing };
    EReplyState State = EReplyState::Idle;

    /** Inputs received while a reply was in flight, submitted in order when it finishes. */
    TArray<TFunction<void()>> PendingInputs;

    /** Incremented to cancel any in-flight playback (destroy, new reply, error). */
    int32 PlaybackToken = 0;

    /** Groups the requests of the current send, so Abort can cancel them. */
    FNeocortexCancelTokenPtr RequestToken;

    /** Weak reference to the Neocortex service instance. */
    TWeakObjectPtr<class UNeocortexService> Service;

    /** Weak reference to the Neocortex subsystem for interactables access. */
    TWeakObjectPtr<class UNeocortexSubsystem> Subsystem;

    /** Resolves the service instance from the subsystem. */
    void ResolveService();

    /** Finds or creates the audio component a voiced mode plays on. */
    void EnsureAudioComponent();

    /** True when the agent can send; reports why not otherwise. */
    bool CheckReady(const TCHAR* Caller);

    /** Starts a new group of requests for one send. */
    FNeocortexCancelTokenPtr NewRequestToken();

    /** What this character perceives, as JSON, or empty when perception is off. */
    FString GetMetadata() const;

    void OnChatResponse(const FNeocortexChatResponseData& ChatResponse);
    void OnAudioResponse(const TArray<uint8>& Bytes);
    void OnTranscribeResponse(const FNeocortexAudioTranscribeResponseData& TranscribeResponse);
    void OnServiceFail(const FNeocortexRequestError& RequestError);

    /** Broadcasts an error without touching the reply state. */
    void ReportError(const FNeocortexRequestError& RequestError);

    // Chat lines playback (only active when ChatLinesMode != Off).
    /** Queues the send if the character is still speaking; else runs it now. */
    void Dispatch(TFunction<void()> Send);
    /** Pulls the next queued input (if any) after a reply finishes or aborts. */
    void DispatchNext();
    /** Sends a chat turn, streamed when bStreamReplies is on and streaming is available. */
    void SendChat(const FString& Message);
    void SendStreamedChat(const FString& Message);
    /** Starts playback of a whole reply in the current mode. */
    void PlayReply(const FNeocortexChatResponseData& Response);
    /** Picks how ReplyLines are delivered (credit aware) and starts them. */
    void StartPlayback(int32 Token);
    /** Plays the line at LineIndex, voiced when bVoicePerLine, then moves on. */
    void PlayNextLine(int32 Token);
    void ScheduleNextLine(float Delay, int32 Token);
    /** Called once every line has been delivered. */
    void OnLinesDone(int32 Token);
    /** Speaks the whole reply as one clip while the lines drop in on top of it. */
    void PlayOneClipAndDrop(int32 Token);
    void DropLineForSingleClip(int32 Index, int32 Token);
    void FinishSingleClipIfDone(int32 Token);
    /** Creates the speech for one line or the whole reply. */
    TSharedPtr<FNeocortexSpeechJob> MakeSpeechJob(const FString& Text, const FString& SpokenText, EEmotions Emotion);
    void StopAudio();
    /** Emits OnChatLineStarted and OnEmotionChanged for a line. */
    void BeginLine(const FNeocortexChatLine& Line);
    /** Ends the reply, fires OnReplyFinished, and dispatches any queued input. */
    void FinishReply(int32 Token);

    // Actions.
    void CaptureActions(const TArray<FNeocortexChatAction>& Actions);
    void RunPendingActions();
    void ProcessNextAction();
    /** Starts the next action, after DelayBetweenActions when there is one waiting. */
    void ContinueActions();

    using FActionHandler = TFunction<void(const FNeocortexChatAction&, TFunction<void()>)>;
    TMap<FString, FActionHandler> ActionHandlers;
    TArray<FNeocortexChatAction> PendingActions;
    bool bActionsRunning = false;
    bool bWarnedOffModeTrigger = false;

    /** Cached procedural sound wave for audio playback. */
    UPROPERTY()
    USoundWaveProcedural* LastSoundWave = nullptr;

    /** The reply being delivered. More lines may still arrive while bLinesOpen (streamed replies). */
    TArray<FNeocortexChatLine> ReplyLines;
    int32 LineIndex = 0;
    bool bLinesOpen = false;
    bool bWaitingForLines = false;
    bool bVoicePerLine = false;
    bool bPlaybackStarted = false;

    /** The line being voiced, and the next one, fetched while the current one is heard. */
    TSharedPtr<FNeocortexSpeechJob> CurrentSpeech;
    TSharedPtr<FNeocortexSpeechJob> NextSpeech;
    int32 NextSpeechIndex = INDEX_NONE;

    /** The streamed reply in flight. */
    TSharedPtr<FNeocortexChatStream> ChatStream;

    /** Single clip state: the reply ends once the clip has played and every line has dropped in. */
    bool bSingleClipMode = false;
    bool bSingleClipActive = false;
    bool bSingleLinesDone = false;

    /** Runs once a reply given to Speak has been played. */
    TFunction<void()> SpeakDone;

    FTimerHandle LineTimer;
    FTimerHandle SpeakGapTimer;
    FTimerHandle ActionDelayTimer;

    /** True while a conversational request is in flight in Off mode. Prevents concurrent sends. */
    bool bRequestPending = false;

    /** True when the active Off mode flow ends with an audio response. */
    bool bExpectingAudio = false;
};
