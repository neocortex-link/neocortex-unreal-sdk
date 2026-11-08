#include "NeocortexSettings.h"

UNeocortexSettings::UNeocortexSettings()
{
}

FString UNeocortexSettings::GetBaseUrl() const
{
#if WITH_EDITOR
	FString Override = BaseUrlOverride.TrimStartAndEnd();
	Override.RemoveFromEnd(TEXT("/"));
	if (!Override.IsEmpty()) return Override;
#endif
	return TEXT("https://api.neocortex.link/v3");
}

#if WITH_EDITOR

void UNeocortexSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	(void)SettingsChanged.ExecuteIfBound(PropertyChangedEvent.GetPropertyName());
}
#endif