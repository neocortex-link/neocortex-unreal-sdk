#pragma once
#include "CoreMinimal.h"
#include "Enums/NeocortexEnumTypes.h"
#include "NeocortexTypes.generated.h"

/** Represents an error response from a Neocortex API request. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexRequestError {
    GENERATED_BODY()
    
    /** HTTP status code or custom error code. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="code"))
    int32 Code = 0;
    
    /** Human-readable error description. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="message"))
    FString Message;
};

/** Request payload for text-to-text chat interactions. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatRequest {
    GENERATED_BODY()
    
    /** Optional session ID to continue an existing conversation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(JsonKey="sessionId"))
    FString SessionId;
    
    /** Required unique identifier for the character. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(JsonKey="characterId"))
    FString CharacterId;
    
    /** Required user message text. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(JsonKey="message"))
    FString Message;
};

/** Response data from a chat interaction. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatResponseData {
    GENERATED_BODY()
    
    /** Session ID for the conversation (new or existing). */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="sessionId"))
    FString SessionId;
    
    /** Character's text response. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="response"))
    FString Response;
    
    /** Optional action identifier associated with the response. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="action"))
    FString Action;
    
    /** Character's emotional state. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="emotion"))
    EEmotions Emotions;
};

/** Request payload for retrieving chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryRequest {
    GENERATED_BODY()
    
    /** Required session ID to retrieve history for. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(JsonKey="sessionId"))
    FString SessionId;
    
    /** Optional maximum number of messages to retrieve (default 10). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(JsonKey="limit"))
    int32 Limit = 10;
};

/** Represents a single message in a chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatMessage {
    GENERATED_BODY()
    
    /** Message content text. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="content"))
    FString Content;
    
    /** Sender identifier (user or character). */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="sender"))
    FString Sender;
    
    /** ISO timestamp of message creation. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="createdAt"))
    FString CreatedAt;
};

/** Response data containing chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryResponseData {
    GENERATED_BODY()
    
    /** Session ID the history belongs to. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="sessionId"))
    FString SessionId;
    
    /** Array of chat messages in chronological order. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="messages"))
    TArray<FNeocortexChatMessage> Messages;
};

/** Response data from audio transcription. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexAudioTranscribeResponseData {
    GENERATED_BODY()
    
    /** Transcribed text from audio input. */
    UPROPERTY(BlueprintReadOnly, meta=(JsonKey="response"))
    FString Response;
};
