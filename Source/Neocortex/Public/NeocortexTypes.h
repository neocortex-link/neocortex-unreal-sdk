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

USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexInteractable
{
    GENERATED_BODY()

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

/** Represents an error response from a Neocortex API request. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexRequestError {
    GENERATED_BODY()

    /** HTTP status code or custom error code. */
    UPROPERTY(BlueprintReadOnly)
    int32 Code = 0;

    /** Human-readable error description. */
    UPROPERTY(BlueprintReadOnly)
    FString Message;
};

/** Response data from a chat interaction. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatResponseData {
    GENERATED_BODY()

    /** Session ID for the conversation (new or existing). */
    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    /** Character's text response. */
    UPROPERTY(BlueprintReadOnly)
    FString Response;

    /** Optional action identifier associated with the response. */
    UPROPERTY(BlueprintReadOnly)
    FString Action;

    /** Conversation flow state string (opaque to the client). */
    UPROPERTY(BlueprintReadOnly)
    FString FlowState;

    /** Character's emotional state. */
    UPROPERTY(BlueprintReadOnly)
    EEmotions Emotion = EEmotions::Neutral;

    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexInteractable> Metadata;
};

/** Request payload for retrieving chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryRequest {
    GENERATED_BODY()

    /** Required session ID to retrieve history for. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SessionId;

    /** Optional maximum number of messages to retrieve (default 10). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Limit = 10;
};

/** Represents a single message in a chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatMessage {
    GENERATED_BODY()

    /** Message content text. */
    UPROPERTY(BlueprintReadOnly)
    FString Content;

    /** Sender identifier (user or character). */
    UPROPERTY(BlueprintReadOnly)
    FString Sender;

    /** ISO timestamp of message creation. */
    UPROPERTY(BlueprintReadOnly)
    FString CreatedAt;
};

/** Response data containing chat history. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexChatHistoryResponseData {
    GENERATED_BODY()

    /** Session ID the history belongs to. */
    UPROPERTY(BlueprintReadOnly)
    FString SessionId;

    /** Array of chat messages in chronological order. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FNeocortexChatMessage> Messages;
};

/** Response data from audio transcription. */
USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeocortexAudioTranscribeResponseData {
    GENERATED_BODY()

    /** Transcribed text from audio input. */
    UPROPERTY(BlueprintReadOnly)
    FString Response;
};
