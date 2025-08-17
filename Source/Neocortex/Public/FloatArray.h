#pragma once

#include "CoreMinimal.h"
#include "FloatArray.generated.h"

USTRUCT(BlueprintType)
struct NEOCORTEX_API FFloatArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (DisplayName = "Data Buffer", ToolTip = "Buffer of float values.", JsonName = "buffer"))
	TArray<float> Buffer;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (DisplayName = "Values", ToolTip = "Array of float values.", JsonName = "written"))
	int32 Written;

	FFloatArray()
		: Written(0)
	{
	}
	FFloatArray(const TArray<float>& InBuffer)
		: Buffer(InBuffer), Written(InBuffer.Num())
	{
	}
	
};
