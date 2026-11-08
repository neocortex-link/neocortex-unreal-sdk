#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeocortexSampleAssetTools.generated.h"

class ANeocortexSampleBase;
class UWidgetBlueprint;

/**
 * Editor only: writes a sample's screen into a Widget Blueprint, the same layout the sample builds
 * in code, so it can be opened and changed in the UMG designer. Scripts/build_samples.py calls it
 * for every sample map.
 */
UCLASS()
class NEOCORTEXEDITOR_API UNeocortexSampleAssetTools : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Creates the Widget Blueprint at AssetPath (for example /Neocortex/Samples/Maps/WBP_TextChat), replacing one already there. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Samples")
    static UWidgetBlueprint* BuildSampleScreen(const FString& AssetPath, TSubclassOf<ANeocortexSampleBase> SampleClass);
};
