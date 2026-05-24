#include "UI/NeocortexMicrophoneSelectWidget.h"
#include "Neocortex.h"
#include "Misc/ConfigCacheIni.h"

const FString UNeocortexMicrophoneSelectWidget::ConfigSection = TEXT("NeocortexAudio");
const FString UNeocortexMicrophoneSelectWidget::ConfigKey    = TEXT("PreferredMicDevice");

void UNeocortexMicrophoneSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (DeviceDropdown)
    {
        DeviceDropdown->OnSelectionChanged.AddDynamic(this, &UNeocortexMicrophoneSelectWidget::HandleSelectionChanged);
        RefreshDevices();
    }
}

void UNeocortexMicrophoneSelectWidget::NativeDestruct()
{
    if (DeviceDropdown)
    {
        DeviceDropdown->OnSelectionChanged.RemoveDynamic(this, &UNeocortexMicrophoneSelectWidget::HandleSelectionChanged);
    }
    Super::NativeDestruct();
}

void UNeocortexMicrophoneSelectWidget::RefreshDevices()
{
    if (!DeviceDropdown) return;

    DeviceDropdown->ClearOptions();

    const TArray<FString> Devices = FNeocortexMicrophoneRecorder::GetAvailableDeviceNames();
    for (const FString& Name : Devices)
    {
        DeviceDropdown->AddOption(Name);
    }

    if (Devices.IsEmpty())
    {
        DeviceDropdown->AddOption(TEXT("No devices found"));
        DeviceDropdown->SetIsEnabled(false);
        return;
    }

    DeviceDropdown->SetIsEnabled(true);
    LoadSavedSelection();
}

void UNeocortexMicrophoneSelectWidget::HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    // Ignore programmatic selections (e.g. from LoadSavedSelection).
    if (SelectionType == ESelectInfo::Direct) return;

    SaveSelection(SelectedItem);

    if (Recorder)
    {
        Recorder->SetPreferredDevice(SelectedItem);
        UE_LOG(LogNeocortex, Log, TEXT("Mic device changed to: %s"), *SelectedItem);
    }
}

void UNeocortexMicrophoneSelectWidget::LoadSavedSelection()
{
    FString Saved;
    GConfig->GetString(*ConfigSection, *ConfigKey, Saved, GGameIni);

    if (!Saved.IsEmpty() && DeviceDropdown->FindOptionIndex(Saved) != -1)
    {
        DeviceDropdown->SetSelectedOption(Saved);
        if (Recorder) Recorder->SetPreferredDevice(Saved);
    }
    else if (DeviceDropdown->GetOptionCount() > 0)
    {
        // Default to the first available device.
        const FString First = DeviceDropdown->GetOptionAtIndex(0);
        DeviceDropdown->SetSelectedOption(First);
        if (Recorder) Recorder->SetPreferredDevice(First);
    }
}

void UNeocortexMicrophoneSelectWidget::SaveSelection(const FString& DeviceName) const
{
    GConfig->SetString(*ConfigSection, *ConfigKey, *DeviceName, GGameIni);
    GConfig->Flush(false, GGameIni);
}
