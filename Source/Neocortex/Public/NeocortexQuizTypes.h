#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "NeocortexQuizTypes.generated.h"

/** What the game should collect next. */
UENUM(BlueprintType)
enum class ENeocortexQuizExpecting : uint8
{
	/** The host is talking, or the quiz is over. Collect nothing. */
	Nothing,
	/** An attempt at the question on screen. */
	Answer,
	/** A yes or a no, usually to an offer of another try. */
	Confirmation,
};

/** How the host read what the player said. Only Answer is ever scored. */
UENUM(BlueprintType)
enum class ENeocortexQuizIntent : uint8
{
	Unclear,
	Answer,
	Affirm,
	Decline,
	Repeat,
	Hint,
	Stop,
};

/** What a host line is doing, so a scene can be timed to it rather than to the turn. */
UENUM(BlueprintType)
enum class ENeocortexQuizLineRole : uint8
{
	/** Responding to what the player said. Also used for any value this version does not know. */
	Reaction,
	Greeting,
	Question,
	Closing,
};

/** How one player's attempt was judged. */
UENUM(BlueprintType)
enum class ENeocortexQuizVerdict : uint8
{
	Unanswered,
	Correct,
	Partial,
	Incorrect,
};

/** Why something the player said never reached the quiz host. */
UENUM(BlueprintType)
enum class ENeocortexQuizInputDropReason : uint8
{
	/** They said something newer before the host was ready. The newer one is sent. */
	Superseded,
	/** It waited too long to still be an answer to what the host asked. */
	Expired,
	/** The run has already ended. */
	Finished,
	/** The quiz has not been started yet. */
	NoSession,
	/** The turn it would have been sent on failed. */
	Failed,
	/** The host was busy and the agent is set not to hold anything. */
	Busy,
};

/** The order questions are asked in. */
UENUM(BlueprintType)
enum class ENeocortexQuizQuestionOrder : uint8
{
	/** The question set's own setting. */
	SetDefault,
	InOrder,
	Shuffled,
};

/** A player taking part in a quiz. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizParticipant
{
	GENERATED_BODY()

	/** Your own id for this player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	FString DisplayName;

	/** Only "human" players are supported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	FString Type = TEXT("human");
};

/** One line the quiz host says, with what that line is doing. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString Text;

	/** The line as it should be spoken. Empty when the server sent none. */
	UPROPERTY(BlueprintReadOnly)
	FString SpokenText;

	UPROPERTY(BlueprintReadOnly)
	EEmotions Emotion = EEmotions::Neutral;

	UPROPERTY(BlueprintReadOnly)
	ENeocortexQuizLineRole Role = ENeocortexQuizLineRole::Reaction;

	/** True when this line is the one that puts the question to the player. */
	bool IsQuestion() const { return Role == ENeocortexQuizLineRole::Question; }
	const FString& SpeechText() const { return SpokenText.IsEmpty() ? Text : SpokenText; }
};

/** A choice on a multiple choice or true or false question. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizOption
{
	GENERATED_BODY()

	/** Pass this to UNeocortexQuizAgent::Choose. */
	UPROPERTY(BlueprintReadOnly)
	FString Id;

	UPROPERTY(BlueprintReadOnly)
	FString Text;
};

/**
 * The question on screen. Carries everything a game may show and nothing it may not: the
 * expected answer, the rubric and the hint text stay on the server until the run is over.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizQuestion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString QuestionId;

	/** The author's name for the scene this question needs, such as SHOW_RED_APPLE. */
	UPROPERTY(BlueprintReadOnly)
	FString Keyword;

	/** "free_text", "multiple_choice" or "true_false". */
	UPROPERTY(BlueprintReadOnly)
	FString Type;

	UPROPERTY(BlueprintReadOnly)
	FString QuestionText;

	/** The choices to show, empty for an open answer. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexQuizOption> Options;

	/** Whether the author wrote a clue the player can ask for. */
	UPROPERTY(BlueprintReadOnly)
	bool bHasHint = false;

	/** True when the player picks from Options rather than speaking freely. */
	bool IsChoice() const { return Type == TEXT("multiple_choice") || Type == TEXT("true_false"); }
};

/** The question after the one on screen, so its scene can be built early. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizUpcoming
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString QuestionId;

	UPROPERTY(BlueprintReadOnly)
	FString Keyword;
};

/** How far through the run the players are. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizProgress
{
	GENERATED_BODY()

	/** Which question is on the table, counting from one. Zero before the first. */
	UPROPERTY(BlueprintReadOnly)
	int32 Asked = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Total = 0;
};

/** How one player's attempt was judged, and what it was worth. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ParticipantId;

	UPROPERTY(BlueprintReadOnly)
	ENeocortexQuizVerdict Verdict = ENeocortexQuizVerdict::Unanswered;

	UPROPERTY(BlueprintReadOnly)
	int32 Score = 0;
};

/** One player's standing. Players on the same score share a rank. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString ParticipantId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalScore = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Rank = 0;
};

/**
 * One turn of a quiz: what the host says now, what the game should put on screen, and what to
 * collect next.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizTurn
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString SessionId;

	/** What the host says, in order. Time the scene to the line whose role is Question. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexQuizLine> Lines;

	/** Game actions the host triggered. TargetId is a participant id when it is about one player. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexChatAction> Actions;

	UPROPERTY(BlueprintReadOnly)
	ENeocortexQuizExpecting Expecting = ENeocortexQuizExpecting::Nothing;

	/** How the host read the last thing said. Unclear on the opening turn. */
	UPROPERTY(BlueprintReadOnly)
	ENeocortexQuizIntent Intent = ENeocortexQuizIntent::Unclear;

	/** True when this turn puts a new question on screen; Question then holds it. */
	UPROPERTY(BlueprintReadOnly)
	bool bHasQuestion = false;

	UPROPERTY(BlueprintReadOnly)
	FNeocortexQuizQuestion Question;

	/** True when the question after this one is known; Upcoming then holds it. */
	UPROPERTY(BlueprintReadOnly)
	bool bHasUpcoming = false;

	UPROPERTY(BlueprintReadOnly)
	FNeocortexQuizUpcoming Upcoming;

	UPROPERTY(BlueprintReadOnly)
	bool bHasProgress = false;

	UPROPERTY(BlueprintReadOnly)
	FNeocortexQuizProgress Progress;

	/** How the last answer was judged; empty on a turn that judged nothing. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexQuizResult> Results;

	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexQuizLeaderboardEntry> Leaderboard;

	/** Players the round is still waiting on, in a game with more than one player. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Pending;

	/** Players who have answered this round, in a game with more than one player. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Answered;

	/** Which attempt at the open question this is, from 1. One player only. */
	UPROPERTY(BlueprintReadOnly)
	int32 Attempt = 0;

	/** How many more goes the open question allows after this one. One player only. */
	UPROPERTY(BlueprintReadOnly)
	int32 AttemptsLeft = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHintGiven = false;

	/** True once the host has signed off. Stop sending turns. */
	UPROPERTY(BlueprintReadOnly)
	bool bDone = false;
};

/** A quiz run read back as it stands right now, so a client that lost a response can catch up. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuizState
{
	GENERATED_BODY()

	/** "created", "started", "collecting", "ready", "evaluated" or "finished". */
	UPROPERTY(BlueprintReadOnly)
	FString Phase;

	UPROPERTY(BlueprintReadOnly)
	int32 TotalQuestions = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 QuestionsRemaining = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHasActiveQuestion = false;

	UPROPERTY(BlueprintReadOnly)
	FNeocortexQuizQuestion ActiveQuestion;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> AnsweredParticipantIds;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> PendingParticipantIds;

	UPROPERTY(BlueprintReadOnly)
	int32 Attempt = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 AttemptsLeft = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bHintGiven = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<FNeocortexQuizLeaderboardEntry> Leaderboard;

	UPROPERTY(BlueprintReadOnly)
	ENeocortexQuizExpecting Expecting = ENeocortexQuizExpecting::Nothing;

	bool IsFinished() const { return Phase == TEXT("finished"); }
};
