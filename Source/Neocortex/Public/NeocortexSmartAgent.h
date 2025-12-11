#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexSmartAgent.generated.h"

class USoundWaveProcedural;

/** Fired when a text chat response is received from the agent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentChat, const FString&, Response);

/** Fired when an audio response is received from the agent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentAudio, USoundWave*, SoundWave);

/** Fired when audio transcription completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentTranscribe, const FString&, Text);

/** Fired when chat history is retrieved. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentChatHistory, const TArray<FNeocortexChatMessage>&, Messages);

/** Fired when an error occurs during any agent operation. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentError, const FString&, Error);

/**
 * Blueprint-accessible component for AI character interactions.
 * Provides simplified interface for text chat, audio synthesis, speech transcription, and chat history.
 * Automatically manages session state and service connections.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent), EditInlineNew, DefaultToInstanced)
class NEOCORTEX_API UNeocortexSmartAgent : public UActorComponent
{
    GENERATED_BODY()
public:
    /** Unique identifier for the character this agent represents. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ProjectId;

    /** Fired when a text chat response is received. */
    UPROPERTY(BlueprintAssignable)
    FOnSmartAgentChat OnChat;

    /** Fired when an audio response is received. */
    UPROPERTY(BlueprintAssignable)
    FOnSmartAgentAudio OnAudio;

    /** Fired when audio transcription completes. */
    UPROPERTY(BlueprintAssignable)
    FOnSmartAgentTranscribe OnTranscribed;

    /** Fired when chat history is retrieved. */
    UPROPERTY(BlueprintAssignable)
    FOnSmartAgentChatHistory OnChatHistory;

    /** Fired when an error occurs during any operation. */
    UPROPERTY(BlueprintAssignable)
    FOnSmartAgentError OnError;

    /**
     * Sends a text message to the agent and receives a text response.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable)
    void SendMessage(const FString& Message);

    /**
     * Sends a text message to the agent and receives both text and audio responses.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable)
    void SendMessageForAudio(const FString& Message);

    /**
     * Transcribes audio data to text.
     * @param AudioData Audio data in WAV format
     */
    UFUNCTION(BlueprintCallable)
    void TranscribeBytes(const TArray<uint8>& Data);

    /**
     * Retrieves the chat history for this agent's character.
     * @param Limit Maximum number of messages to retrieve (default 10)
     */
    UFUNCTION(BlueprintCallable)
    void GetChatHistory(int32 Limit = 10);

    /** Clears the session ID to start a new conversation. */
    UFUNCTION(BlueprintCallable)
    void ClearSessionId();

protected:
    virtual void BeginPlay() override;

private:
    /** Weak reference to the Neocortex service instance. */
    TWeakObjectPtr<class UNeocortexService> Service;

    /** Resolves the service instance from the subsystem. */
    void ResolveService();

    /** Handler for text chat responses. */
    void OnChatResponse(const FNeocortexChatResponseData& ChatResponse) const;

    /** Handler for chat request failures. */
    void OnChatFail(const FNeocortexRequestError& RequestError) const;

    /** Handler for audio responses. */
    void OnAudioResponse(const TArray<uint8>& Bytes);

    /** Handler for transcription responses. */
    void OnTranscribeResponse(const FNeocortexAudioTranscribeResponseData& TranscribeResponse) const;

    /** Handler for chat history responses. */
    void OnChatHistoryResponse(const TArray<FNeocortexChatMessage>& ChatMessages) const;

    /** Handler for service-level failures. */
    void OnServiceFail(const FNeocortexRequestError& RequestError) const;

    /** Buffer for decoded audio samples. */
    TArray<int16> AudioDataCache;

    /** Cached procedural sound wave for audio playback. */
    UPROPERTY()
    USoundWaveProcedural* LastSoundWave = nullptr;
};
