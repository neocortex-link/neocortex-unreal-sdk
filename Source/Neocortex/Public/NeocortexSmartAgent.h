#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexSmartAgent.generated.h"

class USoundWaveProcedural;

/** Fired when a text chat response is received from the agent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmartAgentChat, const FNeocortexChatResponseData&, ChatResponseData);

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString ProjectId;

    /** Whether to automatically include all registered interactables in API requests. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables")
    bool bIncludeAllInteractables = false;

    /** If true, only include interactables within SearchRadius of this agent's owner. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables", meta = (EditCondition = "bIncludeAllInteractables"))
    bool bUseRadiusFilter = false;

    /** Radius in world units to search for nearby interactables when bUseRadiusFilter is true. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Interactables", meta = (EditCondition = "bIncludeAllInteractables && bUseRadiusFilter", ClampMin = "0.0"))
    float SearchRadius = 2000.0f;

    /** Fired when a text chat response is received. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentChat OnChat;

    /** Fired when an audio response is received. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentAudio OnAudio;

    /** Fired when audio transcription completes. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentTranscribe OnTranscribed;

    /** Fired when chat history is retrieved. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentChatHistory OnChatHistory;

    /** Fired when an error occurs during any operation. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnSmartAgentError OnError;

    /**
     * Sends a text message to the agent and receives a text response.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendMessage(const FString& Message);

    /**
     * Sends a text message to the agent and receives both text and audio responses.
     * @param Message The message to send
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendMessageForAudio(const FString& Message);
    
    /**
     * Transcribes audio data to text.
     * @param AudioData Audio data in WAV format
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void TranscribeBytes(const TArray<uint8>& Data);

    /**
     * Retrieves the chat history for this agent's character.
     * @param Limit Maximum number of messages to retrieve (default 10)
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void GetChatHistory(int32 Limit = 10);

    /** Clears the session ID to start a new conversation. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ClearSessionId();

protected:
    virtual void BeginPlay() override;

private:
    /** Weak reference to the Neocortex service instance. */
    TWeakObjectPtr<class UNeocortexService> Service;

    /** Weak reference to the Neocortex subsystem for interactables access. */
    TWeakObjectPtr<class UNeocortexSubsystem> Subsystem;

    /** Resolves the service instance from the subsystem. */
    void ResolveService();

    /**
     * Gets metadata JSON based on current settings.
     * @return JSON string of interactables metadata, or empty string if disabled
     */
    FString GetMetadata() const;

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
