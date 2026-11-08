#include "NeocortexGroupDirector.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSettings.h"
#include "NeocortexEventLogger.h"
#include "Neocortex.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

UNeocortexGroupDirector::UNeocortexGroupDirector()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNeocortexGroupDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ++TurnToken;
    if (UNeocortexService* Service = GetService()) Service->Abort(RequestToken);
    Super::EndPlay(EndPlayReason);
}

UNeocortexService* UNeocortexGroupDirector::GetService() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UNeocortexSubsystem* Sub = GI ? GI->GetSubsystem<UNeocortexSubsystem>() : nullptr;
    return Sub ? Sub->GetService() : nullptr;
}

TArray<FString> UNeocortexGroupDirector::CastIds() const
{
    TArray<FString> Ids;
    for (const UNeocortexSmartAgent* Agent : Agents)
    {
        if (Agent && !Agent->ProjectId.IsEmpty()) Ids.AddUnique(Agent->ProjectId);
    }
    return Ids;
}

UNeocortexSmartAgent* UNeocortexGroupDirector::FindAgent(const FString& CharacterId) const
{
    for (UNeocortexSmartAgent* Agent : Agents)
    {
        if (Agent && Agent->ProjectId == CharacterId) return Agent;
    }
    return nullptr;
}

void UNeocortexGroupDirector::AddAgent(UNeocortexSmartAgent* Agent)
{
    if (Agent) Agents.AddUnique(Agent);
}

void UNeocortexGroupDirector::RemoveAgent(UNeocortexSmartAgent* Agent)
{
    Agents.Remove(Agent);
}

void UNeocortexGroupDirector::Send(const FString& Message)
{
    RunTurn(Message, FString());
}

void UNeocortexGroupDirector::SendTo(UNeocortexSmartAgent* Agent, const FString& Message)
{
    if (Agent && !Agent->ProjectId.IsEmpty()) RunTurn(Message, Agent->ProjectId);
}

void UNeocortexGroupDirector::SendToCharacter(const FString& CharacterId, const FString& Message)
{
    if (!CharacterId.IsEmpty()) RunTurn(Message, CharacterId);
}

void UNeocortexGroupDirector::ContinueScene()
{
    RunTurn(FString(), FString());
}

void UNeocortexGroupDirector::SendAudio(const TArray<uint8>& WavData)
{
    if (WavData.Num() == 0) return;
    if (bBusy)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("A group turn is already in progress; ignoring input until it finishes."));
        return;
    }

    UNeocortexService* Service = GetService();
    const TArray<FString> Ids = CastIds();
    if (!Service || Ids.Num() == 0)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("The group director has no agents with a Project Id."));
        OnTurnFinished.Broadcast();
        return;
    }

    // Claim the turn before the transcription: a second trigger would otherwise bill it twice.
    bBusy = true;
    const int32 Token = ++TurnToken;
    RequestToken = MakeShared<FNeocortexCancelToken, ESPMode::ThreadSafe>();
    TWeakObjectPtr<UNeocortexGroupDirector> Weak(this);
    Service->AudioToText(Ids[0], WavData,
        FNeocortexTranscribeDelegate::CreateLambda([Weak, Token](const FNeocortexAudioTranscribeResponseData& Data)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            Weak->bBusy = false;
            // Nothing heard: end the turn anyway, or a UI that locked its mic waits forever.
            if (Data.Response.TrimStartAndEnd().IsEmpty())
            {
                Weak->OnTurnFinished.Broadcast();
                return;
            }
            Weak->OnPlayerSpeech.Broadcast(Data.Response);
            Weak->RunTurn(Data.Response, FString());
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak, Token](const FNeocortexRequestError& Error)
        {
            if (Weak.IsValid() && Token == Weak->TurnToken) Weak->Fail(Error);
        }),
        RequestToken);
}

void UNeocortexGroupDirector::RequestHistory(int32 Limit, const FString& Before)
{
    UNeocortexService* Service = GetService();
    if (!Service || SessionId.IsEmpty()) return;

    TWeakObjectPtr<UNeocortexGroupDirector> Weak(this);
    Service->GetChatHistory(SessionId, Limit, Before,
        FNeocortexChatHistoryDelegate::CreateLambda([Weak](const FNeocortexChatHistoryPage& Page)
        {
            if (Weak.IsValid()) Weak->OnHistory.Broadcast(Page.Messages, Page.NextCursor);
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak](const FNeocortexRequestError& Error)
        {
            if (!Weak.IsValid()) return;
            Weak->OnRequestError.Broadcast(Error);
            Weak->OnError.Broadcast(Error.Message);
        }));
}

void UNeocortexGroupDirector::Abort()
{
    ++TurnToken;
    if (UNeocortexService* Service = GetService()) Service->Abort(RequestToken);
    for (UNeocortexSmartAgent* Agent : Agents)
    {
        if (Agent) Agent->StopSpeaking();
    }
    if (bBusy) EndTurn();
}

void UNeocortexGroupDirector::RunTurn(const FString& Message, const FString& SpeakerCharacterId)
{
    if (bBusy)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("A group turn is already in progress; ignoring input until it finishes."));
        return;
    }

    UNeocortexService* Service = GetService();
    const TArray<FString> Ids = CastIds();
    if (!Service || Ids.Num() == 0)
    {
        UE_LOG(LogNeocortex, Warning, TEXT("The group director has no agents with a Project Id."));
        OnTurnFinished.Broadcast();
        return;
    }

    bBusy = true;
    const int32 Token = ++TurnToken;
    OnTurnStarted.Broadcast();

    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    const UNeocortexSubsystem* Sub = GI ? GI->GetSubsystem<UNeocortexSubsystem>() : nullptr;
    const FString Metadata = bIncludeAllInteractables && Sub ? Sub->CreateInteractablesMetadata() : FString();

    RequestToken = MakeShared<FNeocortexCancelToken, ESPMode::ThreadSafe>();
    TWeakObjectPtr<UNeocortexGroupDirector> Weak(this);
    Service->RequestChat(Ids, SessionId, Message, SpeakerCharacterId, MaxTurns, Metadata,
        UNeocortexEventLogger::ConsumeLogsJson(this),
        FNeocortexGroupChatDelegate::CreateLambda([Weak, Token](const FNeocortexGroupChatResponse& Response)
        {
            if (!Weak.IsValid() || Token != Weak->TurnToken) return;
            if (!Response.SessionId.IsEmpty()) Weak->SessionId = Response.SessionId;
            Weak->OnGroupResponse.Broadcast(Response);
            Weak->PlaySpeaker(Response.Messages, 0, Token);
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak, Token](const FNeocortexRequestError& Error)
        {
            if (Weak.IsValid() && Token == Weak->TurnToken) Weak->Fail(Error);
        }),
        RequestToken);
}

void UNeocortexGroupDirector::PlaySpeaker(TArray<FNeocortexGroupMessage> Messages, int32 Index, int32 Token)
{
    if (Token != TurnToken) return;
    if (!Messages.IsValidIndex(Index))
    {
        EndTurn();
        return;
    }

    // Speakers play strictly in order, each finishing before the next starts.
    const FNeocortexGroupMessage& Message = Messages[Index];
    OnSpeaker.Broadcast(Message);
    UNeocortexSmartAgent* Agent = FindAgent(Message.CharacterId);
    if (!Agent)
    {
        PlaySpeaker(MoveTemp(Messages), Index + 1, Token);
        return;
    }

    TWeakObjectPtr<UNeocortexGroupDirector> Weak(this);
    Agent->Speak(Message, [Weak, Messages, Index, Token]()
    {
        if (Weak.IsValid()) Weak->PlaySpeaker(Messages, Index + 1, Token);
    });
}

void UNeocortexGroupDirector::EndTurn()
{
    bBusy = false;
    OnTurnFinished.Broadcast();
}

void UNeocortexGroupDirector::Fail(const FNeocortexRequestError& Error)
{
    OnRequestError.Broadcast(Error);
    OnError.Broadcast(Error.Message);
    EndTurn();
}
