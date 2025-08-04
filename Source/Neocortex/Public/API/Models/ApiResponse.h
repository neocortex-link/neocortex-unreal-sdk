#pragma once
#include "CoreMinimal.h"
#include "ApiResponse.generated.h"

USTRUCT(BlueprintType)
struct Neocortex_API FApiResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "message"))
	FString Message;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "action"))
	FString Action;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "transcription"))
	FString Transcription;

	//TODO figure out if this type works as we want
	TSharedPtr<FJsonValue> Data;
};
