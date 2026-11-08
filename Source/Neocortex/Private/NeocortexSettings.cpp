#include "NeocortexSettings.h"
#include "NeocortexHttpClient.h"

UNeocortexSettings::UNeocortexSettings()
{
}

FString UNeocortexSettings::GetBaseUrl() const
{
	FString Override = UNeocortexHttpClient::BaseUrlOverride.TrimStartAndEnd();
	Override.RemoveFromEnd(TEXT("/"));
	return Override.IsEmpty() ? FString(TEXT("https://api.neocortex.link/v3")) : Override;
}

#if WITH_EDITOR

void UNeocortexSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	(void)SettingsChanged.ExecuteIfBound(PropertyChangedEvent.GetPropertyName());
}
#endif