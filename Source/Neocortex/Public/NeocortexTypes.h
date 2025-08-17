#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.generated.h"

USTRUCT(BlueprintType)
struct NEOCORTEX_API FNeoRequestError {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Code = 0;
	UPROPERTY(BlueprintReadOnly) FString Message;
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FChatRequest {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SessionId;   // optional
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CharacterId; // required
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Message;     // required
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FChatResponseData {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) FString Response;
	UPROPERTY(BlueprintReadOnly) FString Action;
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FChatHistoryRequest {
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SessionId; // required (server expects it)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Limit = 10;  // optional
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FChatHistoryMessage {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Content;
	UPROPERTY(BlueprintReadOnly) FString Sender;
	UPROPERTY(BlueprintReadOnly) FString CreatedAt; 
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FChatHistoryResponseData {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString SessionId;
	UPROPERTY(BlueprintReadOnly) TArray<FChatHistoryMessage> Messages;
};

USTRUCT(BlueprintType)
struct NEOCORTEX_API FAudioTranscribeResponseData {
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Response;
};
