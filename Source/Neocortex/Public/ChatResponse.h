#pragma once

#include "CoreMinimal.h"
#include "ChatResponse.generated.h"

USTRUCT(BlueprintType)
struct Neocortex_API FChatResponse
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "message"))
	FString Message;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "action"))
	FString Action; 

	FChatResponse() = default;

	FChatResponse(const FString& InMessage, const FString& InAction)
		: Message(InMessage), Action(InAction)
	{
	}	
};
