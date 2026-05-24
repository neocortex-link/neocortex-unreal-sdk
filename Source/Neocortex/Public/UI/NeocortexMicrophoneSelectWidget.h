#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "NeocortexMicrophoneSelectWidget.generated.h"

/**
 * Dropdown widget for selecting the active microphone input device.
 * Populates itself from available capture devices and persists the selection to game config.
 *
 * Setup in Blueprint:
 *   1. Add a UComboBoxString named "DeviceDropdown" to your layout.
 *   2. Assign the Recorder property to your UNeocortexMicrophoneRecorderComponent.
 */
UCLASS()
class NEOCORTEX_API UNeocortexMicrophoneSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** The recorder component whose device will be changed on selection. */
    UPROPERTY(BlueprintReadWrite, Category="Neocortex")
    TObjectPtr<UNeocortexMicrophoneRecorderComponent> Recorder;

    /** Repopulates the dropdown from the current list of available input devices. */
    UFUNCTION(BlueprintCallable, Category="Neocortex")
    void RefreshDevices();

protected:
    UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
    TObjectPtr<UComboBoxString> DeviceDropdown;

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    static const FString ConfigSection;
    static const FString ConfigKey;

    UFUNCTION()
    void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    void LoadSavedSelection();
    void SaveSelection(const FString& DeviceName) const;
};
