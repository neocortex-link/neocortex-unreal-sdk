#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "WebImage.h"
#include "NeocortexService.generated.h"

/** Delegate fired when a chat response is received. */
DECLARE_DELEGATE_OneParam(FNeocortexChatDelegate, const FNeocortexChatResponseData&);

/** Delegate fired when audio data is received. */
DECLARE_DELEGATE_OneParam(FNeocortexAudioDelegate, const TArray<uint8>&);

/** Delegate fired when audio transcription completes. */
DECLARE_DELEGATE_OneParam(FNeocortexTranscribeDelegate, const FNeocortexAudioTranscribeResponseData&);

/** Delegate fired when a request fails with an error. */
DECLARE_DELEGATE_OneParam(FNeocortexErrorDelegate, const FNeocortexRequestError&);

/** Delegate fired when chat history is retrieved. */
DECLARE_DELEGATE_OneParam(FNeocortexChatHistoryDelegate, const TArray<FNeocortexChatMessage>&);

/**
 * Service layer for Neocortex API interactions.
 * Handles text-to-text chat, text-to-audio synthesis, audio transcription, and chat history retrieval.
 * Manages HTTP communication and session state for character-based conversations.
 */
UCLASS()
class NEOCORTEX_API UNeocortexService : public UObject
{
    GENERATED_BODY()
public:
    /**
     * Initializes the service with HTTP client and session manager.
     * @param Http HTTP client for making API requests
     * @param Session Session manager for tracking conversation state
     */
    void Init(class UNeocortexHttpClient* Http, class UNeocortexSessionManager* Session);

    /**
     * Sends a text message and receives a text response from a character.
     * @param CharacterId Unique identifier for the character
     * @param Message User's text message
     * @param OnChatResponse Callback invoked with the character's response
     * @param OnFail Callback invoked if the request fails
     */
    void TextToText(const FString& CharacterId,
                    const FString& Message,
                    FNeocortexChatDelegate OnChatResponse,
                    FNeocortexErrorDelegate OnFail);

    /**
     * Sends a text message and receives both text and audio responses from a character.
     * @param CharacterId Unique identifier for the character
     * @param Message User's text message
     * @param OnChatResponse Callback invoked with the character's text response
     * @param OnAudioResponse Callback invoked with the character's audio response
     * @param OnFail Callback invoked if the request fails
     */
    void TextToAudio(const FString& CharacterId,
                     const FString& Message,
                     FNeocortexChatDelegate OnChatResponse,
                     FNeocortexAudioDelegate OnAudioResponse,
                     FNeocortexErrorDelegate OnFail);

    /**
     * Transcribes audio input to text for a character conversation.
     * @param CharacterId Unique identifier for the character
     * @param WavBytes Audio data in WAV format
     * @param OnTranscribeResponse Callback invoked with transcription result
     * @param OnFail Callback invoked if the request fails
     */
    void AudioToText(const FString& CharacterId,
                     const TArray<uint8>& WavBytes,
                     FNeocortexTranscribeDelegate OnTranscribeResponse,
                     FNeocortexErrorDelegate OnFail);

    /**
     * Retrieves chat history for a character.
     * @param CharacterId Unique identifier for the character
     * @param Limit Maximum number of messages to retrieve
     * @param OnChatHistoryResponse Callback invoked with chat history
     * @param OnFail Callback invoked if the request fails
     */
    void GetChatHistory(const FString& CharacterId, int32 Limit, FNeocortexChatHistoryDelegate OnChatHistoryResponse, FNeocortexErrorDelegate OnFail);

private:
    /** HTTP client reference for API communication. */
    UPROPERTY()
    UNeocortexHttpClient* HttpRef = nullptr;

    /** Session manager reference for conversation state tracking. */
    UPROPERTY()
    UNeocortexSessionManager* SessionRef = nullptr;

    /**
     * Creates a standardized error object from an HTTP response.
     * @param Where Context string describing where the error occurred
     * @param Response HTTP response object (may be null)
     * @param IsSuccessful True if the request completed without network errors
     * @return Structured error object with diagnostic information
     */
    FNeocortexRequestError MakeError(const FString& Where, const FHttpResponsePtr& Response, bool IsSuccessful) const;
};
