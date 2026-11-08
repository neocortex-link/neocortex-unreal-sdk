#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexQuizTypes.h"
#include "NeocortexService.h"
#include "NeocortexQuizAgent.generated.h"

class UAudioComponent;
class FNeocortexSpeechJob;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizHostLine, const FNeocortexQuizLine&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizEmotion, EEmotions, Emotion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnQuizHostFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizQuestion, const FNeocortexQuizQuestion&, Question);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizUpcoming, const FNeocortexQuizUpcoming&, Upcoming);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizExpecting, ENeocortexQuizExpecting, Expecting);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizResults, const TArray<FNeocortexQuizResult>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizLeaderboard, const TArray<FNeocortexQuizLeaderboardEntry>&, Leaderboard);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizTurn, const FNeocortexQuizTurn&, Turn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizBusy, bool, bBusy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizPending, const TArray<FString>&, Pending);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuizInputDropped, const FString&, Text, ENeocortexQuizInputDropReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizRequestError, const FNeocortexRequestError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuizCreditsExhausted, const FString&, Code);

/**
 * Runs a quiz, a game show or a spoken test hosted by one of your characters.
 *
 * Point it at a character and a question set and call Begin. After that the only thing a game
 * does is hand over what the player said, with Say or Choose. Question order, retries, hints,
 * scoring and when the run ends are decided on the server from the question set.
 *
 * Listen to OnQuestionChanged to build the scene and OnUpcomingQuestion to prepare the next one
 * while the host is still talking.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexQuizAgent : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeocortexQuizAgent();

    /** The character that hosts the quiz. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString CharacterId;

    /** The question set to play. Create one under Question Sets in the dashboard. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString QuestionSetId;

    /** Questions in one run. 0 uses the question set's count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (ClampMin = "0", ClampMax = "100"))
    int32 QuestionCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    ENeocortexQuizQuestionOrder QuestionOrder = ENeocortexQuizQuestionOrder::SetDefault;

    /** Your own id for this player. Empty uses the device's player id. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString PlayerId;

    /** Everyone playing, in a game with more than one player. Empty for one player. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TArray<FNeocortexQuizParticipant> Participants;

    /** Start the quiz as soon as play begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bBeginOnStart = false;

    /** Speak the host's lines out loud in the character's voice. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Voice")
    bool bSpeakHostLines = true;

    /** Play each line as its voice arrives. Falls back to whole clips where streaming is not available. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Voice")
    bool bStreamHostSpeech = true;

    /** Where the host's voice plays. When left empty, one on the owner is used or created. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Voice")
    TObjectPtr<UAudioComponent> AudioComponent = nullptr;

    /** Hold what the player says while the host is busy, and send it once it is ready. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Input")
    bool bQueueInputWhileBusy = true;

    /** How long a held answer stays worth sending, in seconds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Input", meta = (ClampMin = "0.0"))
    float MaxPendingInputAge = 4.0f;

    /** Each line the host says, as it starts, with the emotion to play it in. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizHostLine OnHostLine;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizEmotion OnEmotionChanged;

    /** The host has finished speaking this turn. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizHostFinished OnHostFinished;

    /** Fired as the host starts asking, so the scene is set on the right line. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizQuestion OnQuestionChanged;

    /** The question after this one, so its scene can be loaded early. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizUpcoming OnUpcomingQuestion;

    /** What the game should collect next. Open the microphone on anything but Nothing. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizExpecting OnExpectingChanged;

    /** A turn judged an answer. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizResults OnResult;

    /** The standings changed. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizLeaderboard OnLeaderboardChanged;

    /** The whole turn, to read it yourself. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizTurn OnTurnReceived;

    /** The host has signed off, with the final standings. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizLeaderboard OnQuizFinished;

    /** True while a turn is in flight. Drive a spinner here. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizBusy OnBusyChanged;

    /** The players the round is still waiting on, whenever that changes. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizPending OnPendingChanged;

    /** Something the player said that never reached the host, and why. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizInputDropped OnInputDropped;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizError OnError;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizRequestError OnRequestError;

    /** Credits ran out (the error code). Input stays closed until Resync. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnQuizCreditsExhausted OnCreditsExhausted;

    /** The run in progress, or empty before Begin. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    FString SessionId;

    /** True while a question is on the table; CurrentQuestion then holds it. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    bool bHasQuestion = false;

    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    FNeocortexQuizQuestion CurrentQuestion;

    /** Which question is on the table and how many there are. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    FNeocortexQuizProgress Progress;

    /** What the game should be collecting right now. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    ENeocortexQuizExpecting Expecting = ENeocortexQuizExpecting::Nothing;

    /** True while a turn is in flight. Ignore player input until it clears. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    bool bBusy = false;

    /** True once the host has signed off. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    bool bFinished = false;

    /** Players the round is still waiting on, in roster order. Empty with one player. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    TArray<FString> PendingParticipants;

    /** Players who have answered this round. Empty with one player. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    TArray<FString> AnsweredParticipants;

    /** Which attempt at the open question this is, from 1. One player only. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    int32 Attempt = 0;

    /** How many more goes the open question allows. One player only. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    int32 AttemptsLeft = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    bool bHintGiven = false;

    /** How the host read the last thing said. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|State")
    ENeocortexQuizIntent LastIntent = ENeocortexQuizIntent::Unclear;

    /** Starts the quiz. The host welcomes the players and asks the first question. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Begin();

    /** Starts the quiz with this roster in place of Participants. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void BeginWithPlayers(const TArray<FNeocortexQuizParticipant>& Players);

    /** Abandons whatever is in flight and silences the host, without ending the run. Resync picks it up again. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Abort();

    /**
     * Hands the host what the player said, from speech to text or a text box. An empty string is
     * a deliberate pass; do not send an empty transcription this way. A typed answer is judged as
     * written: a misspelling is not a mishearing.
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Say(const FString& Text, bool bTyped = false);

    /** Hands the host a tapped choice, for a multiple choice or true or false question. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Choose(const FString& OptionId);

    /** Say, naming which player spoke in a game with more than one player. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SayAs(const FString& ParticipantId, const FString& Text, bool bTyped = false);

    /** Choose, naming which player tapped in a game with more than one player. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ChooseAs(const FString& ParticipantId, const FString& OptionId);

    /** The player gives no answer: time ran out, or they skipped. Your game owns the timer. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void PassTurn() { Say(FString()); }

    /** PassTurn for one player of a game with more than one player. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void PassAs(const FString& ParticipantId) { SayAs(ParticipantId, FString()); }

    /**
     * Reads the run back from the server and puts this agent in step with it, after a turn whose
     * response never arrived. Whatever was said on the lost turn is not sent again.
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Resync();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FHeldInput
    {
        bool bHasText = false;
        FString Text;
        FString OptionId;
        FString ParticipantId;
        bool bTyped = false;
        double At = 0.0;

        FString Spoken() const { return bHasText ? Text : OptionId; }
    };

    /** Every callback that resumes later checks this, so an abandoned turn stops there. */
    int32 TurnToken = 0;
    FNeocortexCancelTokenPtr RequestToken;
    TArray<FHeldInput> Held;
    EEmotions LastEmotion = EEmotions::Neutral;
    bool bEmotionRaised = false;

    // The lines being spoken.
    TArray<FNeocortexQuizLine> SpokenLines;
    int32 SpeakIndex = 0;
    bool bQuestionPending = false;
    FNeocortexQuizQuestion PendingQuestion;
    TFunction<void()> OnLinesSpoken;
    TSharedPtr<FNeocortexSpeechJob> CurrentSpeech;
    TSharedPtr<FNeocortexSpeechJob> NextSpeech;

    // The turn after the welcome, asked for while the welcome plays.
    bool bUpNextDone = false;
    bool bUpNextFailed = false;
    FNeocortexQuizTurn UpNext;
    TFunction<void()> OnUpNextReady;

    class UNeocortexService* GetService() const;
    void Send(FHeldInput Input);
    void RunTurn(TFunction<void(FNeocortexQuizTurnDelegate, FNeocortexErrorDelegate)> Request);
    void ApplyTurn(const FNeocortexQuizTurn& Turn, int32 Token, bool bResumedFromWelcome, TFunction<void()> OnApplied);
    void FlushHeld();
    void DropHeld(ENeocortexQuizInputDropReason Reason);
    void SetRound(const TArray<FString>& WaitingOn, const TArray<FString>& Answered, int32 InAttempt, int32 Left, bool bHint);
    void SetBusy(bool bNext);
    void SetExpecting(ENeocortexQuizExpecting Next);
    void RaiseEmotion(EEmotions Emotion);
    void Report(const FNeocortexRequestError& Error);

    void SpeakLines(const TArray<FNeocortexQuizLine>& Lines, bool bWithQuestion, const FNeocortexQuizQuestion& Question,
                    int32 Token, TFunction<void()> OnDone);
    void SpeakNextLine(int32 Token);
    void BeginHostLine(int32 Index, int32 Token);
    void RevealQuestion();
    TSharedPtr<FNeocortexSpeechJob> MakeSpeech(const FNeocortexQuizLine& Line);
    bool IsVoicing() const;
    void StopSpeaking();
};
