#pragma once

#include "CoreMinimal.h"
#include "Message.generated.h"

USTRUCT(BlueprintType)
struct Neocortex_API FMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Neocortex", meta = (DisplayName = "Content", ToolTip = "The content of the message.", JsonName = "content"))
	FString Content;

	UPROPERTY(BlueprintReadWrite, Category = "Neocortex", meta = (DisplayName = "Role", ToolTip = "The role of the message sender.", JsonName = "role"))
	FString Role;

	FMessage() = default;

	FMessage(const FString& InContent, const FString& InRole)
		: Content(InContent), Role(InRole)
	{
	}	
};
