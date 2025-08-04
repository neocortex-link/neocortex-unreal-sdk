#pragma once

#include "CoreMinimal.h"
#include "Enums/EnumTypes.h"
#include "ApiPayload.generated.h"

USTRUCT(BlueprintType)
struct Neocortex_API FApiPayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "url"))
	FString Url;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "method"))
	FString Method = "GET";
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "data"))
	TArray<uint8> Data;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (JsonName = "dataType"))
	EApiResponseDataType DataType = EApiResponseDataType::Text;
	
	FApiPayload()
	{
	}
};
