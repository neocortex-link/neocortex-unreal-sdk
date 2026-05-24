#include "UI/NeocortexAudioChatInputWidget.h"
#include "Neocortex.h"

void UNeocortexAudioChatInputWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (RecordButton && bPushToTalk)
    {
        RecordButton->OnPressed.AddDynamic(this, &UNeocortexAudioChatInputWidget::HandleButtonPressed);
        RecordButton->OnReleased.AddDynamic(this, &UNeocortexAudioChatInputWidget::HandleButtonReleased);
    }

    if (Recorder)
    {
        Recorder->OnWavReady.AddDynamic(this, &UNeocortexAudioChatInputWidget::HandleWavReady);
    }
}

void UNeocortexAudioChatInputWidget::NativeDestruct()
{
    if (RecordButton)
    {
        RecordButton->OnPressed.RemoveDynamic(this, &UNeocortexAudioChatInputWidget::HandleButtonPressed);
        RecordButton->OnReleased.RemoveDynamic(this, &UNeocortexAudioChatInputWidget::HandleButtonReleased);
    }

    if (Recorder)
    {
        Recorder->OnWavReady.RemoveDynamic(this, &UNeocortexAudioChatInputWidget::HandleWavReady);
    }

    Super::NativeDestruct();
}

void UNeocortexAudioChatInputWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!AmplitudeBar || !Recorder || !Recorder->IsRecording()) return;

    // Scale amplitude for visibility (matches Unity's x5 multiplier), clamped to 0–1.
    const float Amp = FMath::Clamp(Recorder->GetAmplitude() * 5.f, 0.f, 1.f);
    AmplitudeBar->SetPercent(Amp);
}

void UNeocortexAudioChatInputWidget::StartRecording()
{
    if (!Recorder)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("UNeocortexAudioChatInputWidget: Recorder is not set"));
        return;
    }
    if (IsRecording()) return;

    if (Recorder->StartRecording())
    {
        OnRecordingStarted.Broadcast();
    }
}

void UNeocortexAudioChatInputWidget::StopRecording()
{
    if (!Recorder || !IsRecording()) return;
    // bAutoTranscribe=false — we handle forwarding ourselves in HandleWavReady.
    Recorder->StopRecording(false);
}

bool UNeocortexAudioChatInputWidget::IsRecording() const
{
    return Recorder && Recorder->IsRecording();
}

void UNeocortexAudioChatInputWidget::HandleButtonPressed()
{
    StartRecording();
}

void UNeocortexAudioChatInputWidget::HandleButtonReleased()
{
    StopRecording();
}

void UNeocortexAudioChatInputWidget::HandleWavReady(const TArray<uint8>& WavData)
{
    OnRecordingComplete.Broadcast(WavData);

    if (bAutoSendOnStop && SmartAgent)
    {
        SmartAgent->SendAudioForAudio(WavData);
    }
}
