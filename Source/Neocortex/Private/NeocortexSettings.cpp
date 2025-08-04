#include "NeocortexSettings.h"

UNeocortexSettings::UNeocortexSettings()
{
}

#if WITH_EDITOR

void UNeocortexSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	(void)SettingsChanged.ExecuteIfBound(PropertyChangedEvent.GetPropertyName());
}
#endif