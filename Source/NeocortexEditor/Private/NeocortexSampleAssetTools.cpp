#include "NeocortexSampleAssetTools.h"
#include "Samples/NeocortexSampleBase.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

UWidgetBlueprint* UNeocortexSampleAssetTools::BuildSampleScreen(const FString& AssetPath, TSubclassOf<ANeocortexSampleBase> SampleClass)
{
    if (!SampleClass) return nullptr;
    const ANeocortexSampleBase* Sample = SampleClass->GetDefaultObject<ANeocortexSampleBase>();
    const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);

    UPackage* Package = CreatePackage(*PackageName);
    Package->FullyLoad();
    UWidgetBlueprint* Screen = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        UNeocortexSampleFrameUI::StaticClass(), Package, FName(*AssetName), BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    if (!Screen) return nullptr;
    FAssetRegistryModule::AssetCreated(Screen);

    // The same layout the sample builds in code, as real widgets of the blueprint.
    UNeocortexSampleFrameUI::BuildLayout(Screen->WidgetTree, Sample);
#if !UE_VERSION_OLDER_THAN(5, 1, 0)
    Screen->WidgetTree->ForEachWidget([Screen](UWidget* Widget) { Screen->OnVariableAdded(Widget->GetFName()); });
#endif
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Screen);
    FKismetEditorUtilities::CompileBlueprint(Screen);

    Package->MarkPackageDirty();
    const FString File = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(Package, Screen, *File, Args);
    return Screen;
}
