#include "NeocortexSmartAgent.h"
#include "Neocortex.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSessionManager.h"
#include "NeocortexEventLogger.h"
#include "Engine/World.h"
#include "Sound/SoundWaveProcedural.h"
#include "NeocortexDrMp3.h"
#include "Audio.h"

void UNeocortexSmartAgent::BeginPlay()
{
    Super::BeginPlay();
    ResolveService();
}

void UNeocortexSmartAgent::ResolveService()
{
    if (GetWorld())
    {
        if (UGameInstance* GI = GetWorld()->GetGameInstance())
        {
            if (UNeocortexSubsystem* Sub = GI->GetSubsystem<UNeocortexSubsystem>())
            {
                Service = Sub->GetService();
                Subsystem = Sub;
            }
        }
    }
}

FString UNeocortexSmartAgent::GetMetadata() const
{
    if (!bIncludeAllInteractables || !Subsystem.IsValid())
    {
        return TEXT("");
    }

    if (bUseRadiusFilter && GetOwner())
    {
        const FVector Location = GetOwner()->GetActorLocation();
        return Subsystem->CreateInteractablesMetadataInRadius(Location, SearchRadius);
    }

    return Subsystem->CreateInteractablesMetadata();
}

void UNeocortexSmartAgent::SendMessage(const FString& Message)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("SendMessage: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }
    if (bRequestPending)
    {
        OnError.Broadcast(TEXT("request already in flight"));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = false;
    const FString Metadata = GetMetadata();
    const FString Events = UNeocortexEventLogger::ConsumeLogsJson(this);
    Service->TextToText(
        ProjectId,
        Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
        Metadata,
        Events);
}

void UNeocortexSmartAgent::SendMessageForAudio(const FString& Message)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("SendMessageForAudio: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }
    if (bRequestPending)
    {
        OnError.Broadcast(TEXT("request already in flight"));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = true;
    const FString Metadata = GetMetadata();
    const FString Events = UNeocortexEventLogger::ConsumeLogsJson(this);
    Service->TextToAudio(
        ProjectId,
        Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexAudioDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnAudioResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
        Metadata,
        Events);
}

void UNeocortexSmartAgent::TranscribeBytes(const TArray<uint8>& Data)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("TranscribeBytes: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }
    if (bRequestPending)
    {
        OnError.Broadcast(TEXT("request already in flight"));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = false;
    Service->AudioToText(
        ProjectId,
        Data,
        FNeocortexTranscribeDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnTranscribeResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail));
}

void UNeocortexSmartAgent::SendAudioForAudio(const TArray<uint8>& WavData)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        return;
    }
    if (bRequestPending)
    {
        OnError.Broadcast(TEXT("request already in flight"));
        return;
    }

    bRequestPending = true;
    bExpectingAudio = true;
    const FString Metadata = GetMetadata();
    const FString Events = UNeocortexEventLogger::ConsumeLogsJson(this);
    Service->AudioToAudio(
        ProjectId,
        WavData,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexAudioDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnAudioResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail),
        Metadata,
        Events);
}

void UNeocortexSmartAgent::GetChatHistory(int32 Limit)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("GetChatHistory: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }

    Service->GetChatHistory(
        ProjectId,
        Limit,
        FNeocortexChatHistoryDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatHistoryResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail));
}

void UNeocortexSmartAgent::ClearSessionId()
{
    if (Subsystem.IsValid())
    {
        Subsystem->GetSessionManager()->Clear(ProjectId);
    }
}


void UNeocortexSmartAgent::OnChatResponse(const FNeocortexChatResponseData& ChatResponse)
{
    // Only terminal for TextToText; TextToAudio and AudioToAudio still expect an audio response.
    if (!bExpectingAudio) bRequestPending = false;
    OnChat.Broadcast(ChatResponse);
}

void UNeocortexSmartAgent::OnAudioResponse(const TArray<uint8>& Bytes)
{
    bRequestPending = false;
    bExpectingAudio = false;

    if (Bytes.Num() == 0)
    {
        OnError.Broadcast(TEXT("Received empty audio data"));
        UE_LOG(LogNeocortex, Error, TEXT("OnAudioResponse: Received empty audio data"));
        return;
    }

    USoundWaveProcedural* SW = NewObject<USoundWaveProcedural>(this);
    LastSoundWave = SW;
    SW->bLooping = false;
    SW->bCanProcessAsync = true;

    AudioDataCache.Reset();
    int32 SampleRate = 0, Channels = 0;
    if (const bool bDecodeSuccess = FNeocortexDrMp3Decoder::DecodeAllPCM16(Bytes.GetData(), Bytes.Num(), AudioDataCache, SampleRate, Channels); !bDecodeSuccess)
    {
        OnError.Broadcast(TEXT("Failed to decode audio"));
        UE_LOG(LogNeocortex, Error, TEXT("OnAudioResponse: Failed to decode audio data"));
        return;
    }

    SW->SetSampleRate(SampleRate);
    SW->NumChannels = Channels;
    SW->QueueAudio(reinterpret_cast<const uint8*>(AudioDataCache.GetData()), AudioDataCache.Num() * sizeof(int16));
    AudioDataCache.Reset(); // QueueAudio copies internally; no need to keep this around

    OnAudio.Broadcast(LastSoundWave);
}

void UNeocortexSmartAgent::OnTranscribeResponse(const FNeocortexAudioTranscribeResponseData& TranscribeResponse)
{
    bRequestPending = false;
    OnTranscribed.Broadcast(TranscribeResponse.Response);
}

void UNeocortexSmartAgent::OnChatHistoryResponse(const TArray<FNeocortexChatMessage>& ChatMessages) const
{
    OnChatHistory.Broadcast(ChatMessages);
}

void UNeocortexSmartAgent::OnServiceFail(const FNeocortexRequestError& RequestError)
{
    bRequestPending = false;
    bExpectingAudio = false;
    OnError.Broadcast(RequestError.Message);
}
