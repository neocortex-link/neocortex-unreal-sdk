#include "NeocortexSmartAgent.h"
#include "Neocortex.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSessionManager.h"
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
            }
        }
    }
}

void UNeocortexSmartAgent::SendMessage(const FString& Message)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("SendMessage: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }

    Service->TextToText(
        ProjectId,
        Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatFail));
}

void UNeocortexSmartAgent::SendMessageForAudio(const FString& Message)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("invalid state"));
        UE_LOG(LogNeocortex, Error, TEXT("SendMessageForAudio: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    } 

    Service->TextToAudio(
        ProjectId,
        Message,
        FNeocortexChatDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnChatResponse),
        FNeocortexAudioDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnAudioResponse),        
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail));
}

void UNeocortexSmartAgent::TranscribeBytes(const TArray<uint8>& Data)
{    
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("Invalid state"));
        UE_LOG (LogNeocortex, Error, TEXT("TranscribeBytes: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
        return;
    }
    Service->AudioToText(
        ProjectId,
        Data,
        FNeocortexTranscribeDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnTranscribeResponse),
        FNeocortexErrorDelegate::CreateUObject(this, &UNeocortexSmartAgent::OnServiceFail));
}

void UNeocortexSmartAgent::GetChatHistory(int32 Limit)
{
    if (!Service.IsValid() || ProjectId.IsEmpty())
    {
        OnError.Broadcast(TEXT("Invalid state"));
        UE_LOG (LogNeocortex, Error, TEXT("GetChatHistory: invalid state (ProjectId='%s', Service valid=%s)"), *ProjectId, Service.IsValid() ? TEXT("true") : TEXT("false"));
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
    UObject* Outer = GetOuter();
    UE_LOG(LogNeocortex, Log, TEXT("ClearSessionId called, Outer=%s"), Outer ? *Outer->GetName() : TEXT("nullptr"));

    UNeocortexSubsystem* Sub = Cast<UNeocortexSubsystem>(Outer);
    if (!Sub)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("Outer is not a UNeocortexSubsystem, trying alternative method"));
        
        // Alternative: Get subsystem through world
        if (GetWorld())
        {
            if (UGameInstance* GI = GetWorld()->GetGameInstance())
            {
                Sub = GI->GetSubsystem<UNeocortexSubsystem>();
            }
        }
    }

    if (!Sub)
    {
        UE_LOG(LogNeocortex, Error, TEXT("Failed to get UNeocortexSubsystem"));
        return;
    }

    UNeocortexSessionManager* SessionMgr = Sub->GetSessionManager();
    if (!SessionMgr)
    {
        UE_LOG(LogNeocortex, Error, TEXT("SessionManager is null"));
        return;
    }

    SessionMgr->Clear(ProjectId);
    UE_LOG(LogNeocortex, Log, TEXT("Cleared session ID for ProjectId='%s'"), *ProjectId);
}


void UNeocortexSmartAgent::OnChatResponse(const FNeocortexChatResponseData& ChatResponse) const
{
    //TODO: handle empty response
    OnChat.Broadcast(ChatResponse.Response);
}

void UNeocortexSmartAgent::OnChatFail(const FNeocortexRequestError& RequestError) const
{
    //TODO: handle empty response
    OnError.Broadcast(RequestError.Message);
}

void UNeocortexSmartAgent::OnAudioResponse(const TArray<uint8>& Bytes)
{
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

    OnAudio.Broadcast(LastSoundWave);
}

void UNeocortexSmartAgent::OnTranscribeResponse(const FNeocortexAudioTranscribeResponseData& TranscribeResponse) const
{    
    OnTranscribed.Broadcast(TranscribeResponse.Response);
}

void UNeocortexSmartAgent::OnChatHistoryResponse(const TArray<FNeocortexChatMessage>& ChatMessages) const
{
    OnChatHistory.Broadcast(ChatMessages);
}

void UNeocortexSmartAgent::OnServiceFail(const FNeocortexRequestError& RequestError) const
{
    OnError.Broadcast(RequestError.Message);
}
