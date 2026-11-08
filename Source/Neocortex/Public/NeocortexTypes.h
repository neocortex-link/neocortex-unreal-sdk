#pragma once
#include "CoreMinimal.h"
#include "Enums/NeocortexEnumTypes.h"
#include "NeocortexTypes.generated.h"

UENUM(BlueprintType)
enum EFNeocortexInteractableType
{
    Character        UMETA(DisplayName = "Character"),
    Object      UMETA(DisplayName = "OBJECT")
};

/** A key-value property pair for describing interactable characteristics. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexInteractableProperty
{
    GENERATED_BODY()

    /** Property name/key (e.g., "Color", "Health", "State"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    /** Property value (e.g., "Red", "100", "Open"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Value;
};

/** One thing a character can perceive: an object, another character, or the player. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexInteractable
{
    GENERATED_BODY()

    /** Stable unique id. Actions reference entities by this, never by name. */
    UPROPERTY(BlueprintReadOnly)
    FString Id;
    /** Set when this entity is a Neocortex character. */
    UPROPERTY(BlueprintReadOnly)
    FString CharacterId;
    UPROPERTY(BlueprintReadOnly)
    FString Type;
    UPROPERTY(BlueprintReadOnly)
    FString Name;
    UPROPERTY(BlueprintReadOnly)
    bool IsSubject = false;
    UPROPERTY(BlueprintReadOnly)
    FVector Position = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexInteractableProperty> Properties;

};

/**
 * A failed request, classified so a game can branch on what happened rather than on the
 * wording of a message.
 *
 *   401                         invalid or missing API key
 *   402 insufficient_credits    the team is out of credits
 *   403                         key inactive, plan required, not a team member
 *   429 rate_limit_exceeded,
 *       character_credit_limit,
 *       player_credit_limit     slow down, or a per-key cap was hit
 *   502 speech_failed           speech synthesis failed
 *   502 transcription_failed    speech recognition failed
 *   503 database_unavailable    the service is busy; retry in a few seconds
 *   504 upstream_timeout        the model did not answer in time; retry
 *
 * Status is 0 for a transport failure (no connection, DNS, TLS), for a local timeout and for
 * a failure raised before any request was sent; bTimedOut says which.
 * The checks are also available in Blueprints through UNeocortexFunctionLibrary.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexRequestError {
    GENERATED_BODY()

    /** HTTP status, or 0 when the request never got an answer. */
    UPROPERTY(BlueprintReadOnly)
    int32 Status = 0;

    /** The API's machine readable code (e.g. "insufficient_credits"), or empty when it sent none. */
    UPROPERTY(BlueprintReadOnly)
    FString Code;

    /** A short, human readable message, safe to show in a log or a debug label. */
    UPROPERTY(BlueprintReadOnly)
    FString Message;

    /** This side gave up waiting for an answer. */
    UPROPERTY(BlueprintReadOnly)
    bool bTimedOut = false;

    /** The URL that failed, for logs. */
    UPROPERTY(BlueprintReadOnly)
    FString Url;

    /** A failure raised before any request was sent (missing id, nothing to send). */
    static FNeocortexRequestError FromMessage(const FString& InMessage)
    {
        FNeocortexRequestError E;
        E.Message = InMessage;
        return E;
    }

    /** True when trying the same request again a moment later can succeed. */
    bool IsRetryable() const
    {
        return bTimedOut || Code == TEXT("database_unavailable") || Code == TEXT("upstream_timeout")
            || Status == 503 || Status == 504;
    }
    bool IsUnauthorized() const { return Status == 401; }
    bool IsForbidden() const { return Status == 403; }
    bool IsOutOfCredits() const { return Status == 402 || IsCreditCode(); }
    bool IsRateLimited() const { return Status == 429 && !IsCreditCode(); }
    bool IsSpeechFailure() const { return Code == TEXT("speech_failed"); }
    bool IsTranscriptionFailure() const { return Code == TEXT("transcription_failed"); }

private:
    bool IsCreditCode() const
    {
        return Code == TEXT("insufficient_credits") || Code == TEXT("character_credit_limit")
            || Code == TEXT("player_credit_limit");
    }
};

/**
 * One ordered piece of a reply with the emotion it should be delivered in. Concatenating the
 * text of all lines equals the full FNeocortexChatResponseData::Response.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatLine {
    GENERATED_BODY()

    /** This line's text. */
    UPROPERTY(BlueprintReadOnly)
    FString Text;

    /** The line as it should be spoken (numbers, dates and currencies spelled out). Empty when the server sent none. */
    UPROPERTY(BlueprintReadOnly)
    FString SpokenText;

    /** Emotion this line should be delivered in. */
    UPROPERTY(BlueprintReadOnly)
    EEmotions Emotion = EEmotions::Neutral;

    /** What to send to speech synthesis: the spoken text when there is one, else the text. */
    const FString& SpeechText() const { return SpokenText.IsEmpty() ? Text : SpokenText; }
};

/**
 * One action the character triggered, in the order it intends to perform it. TargetId is the
 * interactable it applies to (see UNeocortexInteractableComponent::Id), or empty when it needs
 * no object.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatAction {
    GENERATED_BODY()

    /** The action keyword, as authored on the character. */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** Id of the interactable the action applies to, or empty. */
    UPROPERTY(BlueprintReadOnly)
    FString TargetId;
};

/** One character's reply to a chat turn. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatResponseData {
    GENERATED_BODY()

    /** Session ID for the conversation (new or existing). */
    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    /** The character that produced this reply. */
    UPROPERTY(BlueprintReadOnly)
    FString CharacterId;

    /** The character's display name. */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** The whole reply: every line's text joined. */
    UPROPERTY(BlueprintReadOnly)
    FString Response;

    /** The reply as it should be spoken (numbers, dates and currencies spelled out). */
    UPROPERTY(BlueprintReadOnly)
    FString SpokenResponse;

    /** Name of the first of Actions, for code that reads a single action. */
    UPROPERTY(BlueprintReadOnly)
    FString Action;

    /** Every action the character triggered this turn, in order. Empty when none. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatAction> Actions;

    /** The Conversation Flow checkpoint this turn landed on. Empty when the character has no flow. */
    UPROPERTY(BlueprintReadOnly)
    FString FlowState;

    /** The first line's emotion. */
    UPROPERTY(BlueprintReadOnly)
    EEmotions Emotion = EEmotions::Neutral;

    /** The scene's interactables after the turn, as the server echoed them. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexInteractable> Metadata;

    /** Ordered per-emotion lines that make up the reply. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatLine> Lines;

    /** The action keywords in order. */
    TArray<FString> GetActionNames() const
    {
        TArray<FString> Out;
        for (const FNeocortexChatAction& A : Actions) Out.Add(A.Name);
        return Out;
    }

    /** The lines to play. A reply with no lines plays as one line. */
    TArray<FNeocortexChatLine> ResolveLines() const
    {
        if (Lines.Num() > 0) return Lines;
        FNeocortexChatLine Single;
        Single.Text = Response;
        Single.SpokenText = SpokenResponse;
        Single.Emotion = Emotion;
        return { Single };
    }
};

/**
 * One character's turn inside a chat reply: the lines it speaks (each with its own emotion),
 * the actions it triggered and its Conversation Flow state. A solo chat carries one of these.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexGroupMessage {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString CharacterId;

    /** The character's display name, for labelling the line in a shared transcript. */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatLine> Lines;

    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatAction> Actions;

    /** The Conversation Flow checkpoint this turn landed on. Empty when the character has no flow. */
    UPROPERTY(BlueprintReadOnly)
    FString FlowState;

    /** The whole turn as it should be spoken, when the server sent it. */
    UPROPERTY(BlueprintReadOnly)
    FString SpokenMessage;
};

/**
 * A whole chat turn: Messages are the speakers in the order they spoke (one for a solo cast),
 * SessionId the shared session, and Metadata the scene's interactables after the turn.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexGroupChatResponse {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexGroupMessage> Messages;

    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexInteractable> Metadata;
};

/** One of the key's characters, as listed by /characters. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexCharacterSummary {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Id;

    UPROPERTY(BlueprintReadOnly)
    FString Name;
};

/** One of the key's question sets, as listed by /question-sets. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexQuestionSetSummary {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Id;

    UPROPERTY(BlueprintReadOnly)
    FString Name;

    UPROPERTY(BlueprintReadOnly)
    int32 QuestionCount = 0;
};

/** Developer account info from the read-only /account endpoint. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexAccountResponse {
    GENERATED_BODY()

    /** Account tier: "FREE" / "PRO" / "TEAM". */
    UPROPERTY(BlueprintReadOnly)
    FString Tier;

    /** Owner email address. */
    UPROPERTY(BlueprintReadOnly)
    FString Email;

    /** Remaining team credits. */
    UPROPERTY(BlueprintReadOnly)
    int32 CreditsRemaining = 0;

    /** ISO timestamp of the next credit refresh (may be empty). */
    UPROPERTY(BlueprintReadOnly)
    FString NextRefresh;
};

/** Per-player usage, present when a playerId was passed to /usage. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexPlayerUsage {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 InteractionsToday = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 CreditsUsedAllTime = 0;

    /** True when the player exceeded a developer-configured cap. */
    UPROPERTY(BlueprintReadOnly)
    bool bOverLimit = false;
};

/** Per-character usage, present when a characterId was passed to /usage. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexCharacterUsage {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 CreditsUsedAllTime = 0;

    /** True when the character exceeded a developer-configured cap. */
    UPROPERTY(BlueprintReadOnly)
    bool bOverLimit = false;
};

/** Team credit status plus optional per-player / per-character usage from /usage. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexUsageResponse {
    GENERATED_BODY()

    /** Team credit status (Ok / Low / Empty). */
    UPROPERTY(BlueprintReadOnly)
    EUsageStatus Status = EUsageStatus::Ok;

    /** Remaining team credits. */
    UPROPERTY(BlueprintReadOnly)
    int32 CreditsRemaining = 0;

    /** Per-player usage; zeroed when no playerId was queried. */
    UPROPERTY(BlueprintReadOnly)
    FNeocortexPlayerUsage Player;

    /** Per-character usage; zeroed when no characterId was queried. */
    UPROPERTY(BlueprintReadOnly)
    FNeocortexCharacterUsage Character;
};

/**
 * One stored turn of a past conversation. Unlike a live chat line, this is a persisted record
 * of a whole turn from either side, with who said it and when.
 */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryEntry {
    GENERATED_BODY()

    /** Message content text. */
    UPROPERTY(BlueprintReadOnly)
    FString Content;

    /** "USER" (the player) or "ASSISTANT" (a character). Scene events carry their own value. */
    UPROPERTY(BlueprintReadOnly)
    FString Sender;

    /** In a group session, which character spoke this line; empty for USER lines. */
    UPROPERTY(BlueprintReadOnly)
    FString SpeakerCharacterId;

    /** Display name of the speaker; empty for USER lines. */
    UPROPERTY(BlueprintReadOnly)
    FString Name;

    /** Group sessions: who this line was directed at ("Player" or a character name). */
    UPROPERTY(BlueprintReadOnly)
    FString AddressedTo;

    /** The emotion a character line was delivered in; Neutral when not stored. */
    UPROPERTY(BlueprintReadOnly)
    EEmotions Emotion = EEmotions::Neutral;

    /** Action keywords this turn triggered; empty for USER lines. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> Actions;

    /** ISO timestamp of message creation. */
    UPROPERTY(BlueprintReadOnly)
    FString CreatedAt;

    /** True when the player said this line. */
    bool IsUser() const { return Sender.Equals(TEXT("USER"), ESearchCase::IgnoreCase); }
};

/** A page of a session's transcript, oldest turn first. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryPage {
    GENERATED_BODY()

    /** Session ID the history belongs to. */
    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    /** Stored turns, oldest first. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatHistoryEntry> Messages;

    /** Cursor to the next, older page; pass it back as Before. Empty at the start of history. */
    UPROPERTY(BlueprintReadOnly)
    FString NextCursor;
};

/** Response data from audio transcription. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexAudioTranscribeResponseData {
    GENERATED_BODY()

    /** Transcribed text from audio input. */
    UPROPERTY(BlueprintReadOnly)
    FString Response;
};
