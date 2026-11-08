#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "NeocortexQuizTypes.h"
#include "NeocortexHttpClient.h"
#include "Dom/JsonObject.h"
#include "NeocortexService.generated.h"

class FNeocortexHttpStream;
class FNeocortexChatStream;

/** Delegate fired when a chat response is received. */
DECLARE_DELEGATE_OneParam(FNeocortexChatDelegate, const FNeocortexChatResponseData&);

/** Delegate fired when audio data is received. Empty bytes mean there was nothing to speak. */
DECLARE_DELEGATE_OneParam(FNeocortexAudioDelegate, const TArray<uint8>&);

/** Delegate fired when audio transcription completes. */
DECLARE_DELEGATE_OneParam(FNeocortexTranscribeDelegate, const FNeocortexAudioTranscribeResponseData&);

/** Delegate fired when a request fails with an error. */
DECLARE_DELEGATE_OneParam(FNeocortexErrorDelegate, const FNeocortexRequestError&);

/** Delegate fired when a page of chat history is retrieved. */
DECLARE_DELEGATE_OneParam(FNeocortexChatHistoryDelegate, const FNeocortexChatHistoryPage&);

/** Delegate fired when the developer account info is retrieved. */
DECLARE_DELEGATE_OneParam(FNeocortexAccountDelegate, const FNeocortexAccountResponse&);

/** Delegate fired when usage info is retrieved. */
DECLARE_DELEGATE_OneParam(FNeocortexUsageDelegate, const FNeocortexUsageResponse&);

/** Delegate fired with the key's characters. */
DECLARE_DELEGATE_OneParam(FNeocortexCharactersDelegate, const TArray<FNeocortexCharacterSummary>&);

/** Delegate fired with the key's question sets. */
DECLARE_DELEGATE_OneParam(FNeocortexQuestionSetsDelegate, const TArray<FNeocortexQuestionSetSummary>&);

/** Delegate fired with a whole chat turn, every speaker in order. */
DECLARE_DELEGATE_OneParam(FNeocortexGroupChatDelegate, const FNeocortexGroupChatResponse&);

/** Delegate fired with one quiz turn. */
DECLARE_DELEGATE_OneParam(FNeocortexQuizTurnDelegate, const FNeocortexQuizTurn&);

/** Delegate fired with a quiz run read back from the server. */
DECLARE_DELEGATE_OneParam(FNeocortexQuizStateDelegate, const FNeocortexQuizState&);

/**
 * Service layer for the Neocortex API.
 * Handles chat, speech synthesis, transcription, chat history, account and usage.
 *
 * Requests time out after 30 seconds for chat, 20 for audio and 15 for lookups. Lookups,
 * history and audio are sent once more after a retryable failure (see
 * FNeocortexRequestError::IsRetryable); a chat turn never is. Every call takes an optional
 * cancel token, so a whole voice turn can be cancelled with Abort.
 */
UCLASS()
class NEOCORTEX_API UNeocortexService : public UObject
{
    GENERATED_BODY()
public:
    static constexpr float ChatTimeoutSeconds = 30.0f;
    static constexpr float AudioTimeoutSeconds = 20.0f;
    static constexpr float LookupTimeoutSeconds = 15.0f;

    /**
     * Initializes the service with HTTP client and session manager.
     * @param Http HTTP client for making API requests
     * @param Session Session manager for tracking conversation state
     */
    void Init(UNeocortexHttpClient* Http, class UNeocortexSessionManager* Session);

    /**
     * Sends one chat turn to a character and receives its reply. Stores the session id for the
     * character.
     * @param CharacterId Unique identifier for the character
     * @param Message Player's text message
     * @param OnChatResponse Callback invoked with the character's response
     * @param OnFail Callback invoked if the request fails
     * @param Metadata Optional JSON array of what the character perceives
     * @param Events Optional JSON array of game events
     * @param Token Optional group to cancel the request with
     */
    void TextToText(const FString& CharacterId,
                    const FString& Message,
                    FNeocortexChatDelegate OnChatResponse,
                    FNeocortexErrorDelegate OnFail,
                    const FString& Metadata = TEXT(""),
                    const FString& Events = TEXT(""),
                    FNeocortexCancelTokenPtr Token = nullptr);

    /** Sends a chat turn, then generates speech for the whole reply. */
    void TextToAudio(const FString& CharacterId,
                     const FString& Message,
                     FNeocortexChatDelegate OnChatResponse,
                     FNeocortexAudioDelegate OnAudioResponse,
                     FNeocortexErrorDelegate OnFail,
                     const FString& Metadata = TEXT(""),
                     const FString& Events = TEXT(""),
                     FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * Generates speech for a piece of text, voiced in the given emotion.
     * IMPORTANT: every call costs 1 audio credit, so voicing each line multiplies the audio cost
     * of a reply by the number of lines. Text with nothing to speak (blank, punctuation, emoji)
     * sends no request and calls OnAudio with empty bytes.
     * @param CharacterId Unique identifier for the character
     * @param Message Text to synthesize
     * @param Emotion Emotion name (uppercase, e.g. "HAPPY")
     * @param OnAudio Callback invoked with the raw audio bytes
     * @param OnFail Callback invoked if the request fails
     * @param SpokenText Optional phonetic text, spoken instead of Message when it has something to say
     */
    void GenerateAudio(const FString& CharacterId,
                       const FString& Message,
                       const FString& Emotion,
                       FNeocortexAudioDelegate OnAudio,
                       FNeocortexErrorDelegate OnFail,
                       const FString& SpokenText = TEXT(""),
                       FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * Transcribes audio to text without chatting. Any character the key owns works; it is used
     * for metering.
     * @param CharacterId Unique identifier for the character
     * @param WavBytes Audio data in WAV format
     * @param OnTranscribeResponse Callback invoked with transcription result
     * @param OnFail Callback invoked if the request fails
     */
    void AudioToText(const FString& CharacterId,
                     const TArray<uint8>& WavBytes,
                     FNeocortexTranscribeDelegate OnTranscribeResponse,
                     FNeocortexErrorDelegate OnFail,
                     FNeocortexCancelTokenPtr Token = nullptr);

    /** Transcribes audio then sends the result as a chat message, returning both text and audio responses. */
    void AudioToAudio(const FString& CharacterId,
                      const TArray<uint8>& WavBytes,
                      FNeocortexChatDelegate OnChatResponse,
                      FNeocortexAudioDelegate OnAudioResponse,
                      FNeocortexErrorDelegate OnFail,
                      const FString& Metadata = TEXT(""),
                      const FString& Events = TEXT(""),
                      FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * Retrieves one page of a session's transcript, oldest turn first.
     * @param SessionId The session to read
     * @param Limit Maximum number of messages in the page
     * @param Before The previous page's NextCursor, to page back; empty for the latest page
     */
    void GetChatHistory(const FString& SessionId, int32 Limit, const FString& Before,
                        FNeocortexChatHistoryDelegate OnChatHistoryResponse, FNeocortexErrorDelegate OnFail,
                        FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * Retrieves the developer account info (tier, email, remaining credits, next refresh).
     * Read-only and unmetered; never costs a credit.
     */
    void GetAccount(FNeocortexAccountDelegate OnAccount, FNeocortexErrorDelegate OnFail);

    /**
     * Retrieves team credit status plus optional per-player / per-character usage.
     * Read-only and unmetered. An unknown playerId returns zero usage, not an error.
     * @param PlayerId External player id (empty to omit)
     * @param CharacterId Character/project id (empty to omit)
     */
    void GetUsage(const FString& PlayerId, const FString& CharacterId, FNeocortexUsageDelegate OnUsage, FNeocortexErrorDelegate OnFail);

    /**
     * Sends one turn to the unified chat endpoint for a whole cast and returns every speaker's
     * reply, in order. Pass 1 to 3 character ids. A cast of two or more needs a Pro or Team key.
     * @param SpeakerCharacterId Forces who answers; empty lets the cast decide
     * @param Message Empty for an ambient turn: the cast talks among themselves
     * @param MaxTurns Most characters that speak in this turn; 0 for the server's default
     */
    void RequestChat(const TArray<FString>& CharacterIds, const FString& SessionId, const FString& Message,
                     const FString& SpeakerCharacterId, int32 MaxTurns, const FString& Metadata, const FString& Events,
                     FNeocortexGroupChatDelegate OnResponse, FNeocortexErrorDelegate OnFail,
                     FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * EXPERIMENTAL. Starts a streamed chat turn for one character on /v3/chat/stream. Null on a
     * platform without streaming. Stores the session id for the character when it arrives.
     */
    TSharedPtr<FNeocortexChatStream> RequestChatStream(const FString& CharacterId, const FString& Message,
                     const FString& Metadata, const FString& Events);

    /** Opens a POST whose body is read as it arrives. Used for streamed speech and replies. */
    TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> OpenStream(const FString& Path, const FString& JsonBody,
                     const TCHAR* Accept, float TimeoutSeconds);

    /**
     * Starts a quiz and returns its first turn (the host's welcome).
     * @param QuestionCount 0 uses the question set's own count
     * @param SelectionMode "sequential", "random", or empty for the question set's setting
     */
    void BeginQuiz(const FString& CharacterId, const FString& QuestionSetId, const FString& PlayerId,
                   const TArray<FNeocortexQuizParticipant>& Participants, int32 QuestionCount, const FString& SelectionMode,
                   FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail, FNeocortexCancelTokenPtr Token = nullptr);

    /**
     * Sends what the player did and returns the next turn. A run that has already finished
     * replays its closing turn; a busy or moved run is retried.
     * @param bHasSay Whether Say is sent at all; an empty Say is a deliberate pass
     * @param OptionId A tapped choice, or empty
     * @param bTyped Typed words are judged as written; a misspelling is not a mishearing
     */
    void ContinueQuiz(const FString& SessionId, bool bHasSay, const FString& Say, const FString& OptionId,
                      const FString& ParticipantId, bool bTyped,
                      FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail, FNeocortexCancelTokenPtr Token = nullptr);

    /** Reads a quiz run back: standings, the open question and what the host waits for. Unmetered. */
    void GetQuizSession(const FString& SessionId, FNeocortexQuizStateDelegate OnState, FNeocortexErrorDelegate OnFail,
                        FNeocortexCancelTokenPtr Token = nullptr);

    /** Flattens one speaker of a chat turn: Response is the lines joined, Emotion the first line's. */
    static FNeocortexChatResponseData FlattenMessage(const FNeocortexGroupMessage& Message, const FString& SessionId,
                                                     const TArray<FNeocortexInteractable>& Metadata);

    /** Reads one speaker of a chat turn. */
    static FNeocortexGroupMessage ParseGroupMessage(const TSharedPtr<FJsonObject>& Obj);

    /** Reads a whole chat turn. */
    static FNeocortexGroupChatResponse ParseGroupChat(const TSharedPtr<FJsonObject>& Root);

    /** Reads one chat line. */
    static FNeocortexChatLine ParseChatLine(const TSharedPtr<FJsonObject>& Obj);

    /** Lists the characters this key can use (id and name). Read-only and unmetered. */
    void GetCharacters(FNeocortexCharactersDelegate OnList, FNeocortexErrorDelegate OnFail);

    /** Lists the question sets this key can use. Read-only and unmetered. */
    void GetQuestionSets(FNeocortexQuestionSetsDelegate OnList, FNeocortexErrorDelegate OnFail);

    /** Cancels every request in the token's group. Nothing in the group calls back afterwards. */
    void Abort(const FNeocortexCancelTokenPtr& Token);

    /** Stable per-device player id sent on chat/usage requests. */
    static const FString& GetDefaultPlayerId();

private:
    /** HTTP client reference for API communication. */
    UPROPERTY()
    UNeocortexHttpClient* HttpRef = nullptr;

    /** Session manager reference for conversation state tracking. */
    UPROPERTY()
    UNeocortexSessionManager* SessionRef = nullptr;

    /** Sends one quiz turn, retrying while the run is busy or has just moved on. */
    void SendQuizTurn(const FString& Body, const FNeocortexRequestOptions& Options, int32 Attempt,
                      FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail);
};
