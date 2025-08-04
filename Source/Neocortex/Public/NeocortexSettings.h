#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NeocortexSettings.generated.h"

#if WITH_EDITOR
DECLARE_DELEGATE_OneParam(FEditorSettingsChanged, const FName&);
#endif

UCLASS(config=Game, defaultconfig, meta = (DisplayName="Neocortex"))
class Neocortex_API UNeocortexSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNeocortexSettings();

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (DisplayName = "API Key", ToolTip = "The API Key is used to authenticate your application with the Neocortex API. You can find it in your Neocortex account settings."))
	FString ApiKey;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FEditorSettingsChanged SettingsChanged;
#endif
	
};
