#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NeocortexSettings.generated.h"

#if WITH_EDITOR
/** Delegate fired when editor settings are modified. */
DECLARE_DELEGATE_OneParam(FEditorSettingsChanged, const FName&);
#endif

/**
 * Project settings for the Neocortex plugin.
 * Stores API credentials and configuration accessible through Project Settings UI.
 */
UCLASS(config=Game, defaultconfig, meta = (DisplayName="Neocortex"))
class NEOCORTEX_API UNeocortexSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNeocortexSettings();

	/** API key for authenticating with the Neocortex API service. Find this in your Neocortex account settings. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (DisplayName = "API Key", ToolTip = "The API Key is used to authenticate your application with the NeoCortex API. You can find it in your NeoCortex account settings."))
	FString ApiKey;

	/** ISO 639-1 language code to lock transcription language (e.g. "en", "ar", "fr").
	 *  Leave empty to let the backend auto-detect the spoken language.
	 *  Can be overridden per-agent on UNeocortexSmartAgent. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (DisplayName = "Transcription Language", ToolTip = "ISO 639-1 language code sent with audio transcription requests. Leave empty for auto-detection. Can be overridden per-agent."))
	FString Language;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Delegate invoked when settings are modified in the editor. */
	FEditorSettingsChanged SettingsChanged;
#endif
};
