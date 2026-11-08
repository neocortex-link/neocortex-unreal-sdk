#include "NeocortexActionQueue.h"
#include "Neocortex.h"
#include "NeocortexSmartAgent.h"
#include "GameFramework/Actor.h"

UNeocortexActionQueue::UNeocortexActionQueue()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNeocortexActionQueue::BeginPlay()
{
    Super::BeginPlay();

    if (!Agent && GetOwner())
    {
        Agent = GetOwner()->FindComponentByClass<UNeocortexSmartAgent>();
    }

    if (!Agent)
    {
        UE_LOG(LogNeocortex, Error, TEXT("NeocortexActionQueue has no agent assigned."));
        return;
    }

    Agent->OnChat.AddDynamic(this, &UNeocortexActionQueue::HandleChatResponse);
}

void UNeocortexActionQueue::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Agent)
    {
        Agent->OnChat.RemoveDynamic(this, &UNeocortexActionQueue::HandleChatResponse);
    }
    Super::EndPlay(EndPlayReason);
}

void UNeocortexActionQueue::RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&, const FNeocortexChatResponseData&)> Handler)
{
    Handlers.Add(Keyword, MoveTemp(Handler));
}

void UNeocortexActionQueue::UnregisterAction(const FString& Keyword)
{
    Handlers.Remove(Keyword);
}

void UNeocortexActionQueue::HandleChatResponse(const FNeocortexChatResponseData& Response)
{
    for (const FNeocortexChatAction& Action : Response.Actions)
    {
        if (!Action.Name.IsEmpty())
        {
            Queue.Emplace(Action, Response);
        }
    }

    // Kick processing only when idle; a running sequence picks up new items when it advances.
    if (!bRunning && !bAwaitingFinish && Queue.Num() > 0)
    {
        ProcessNext();
    }
}

void UNeocortexActionQueue::ProcessNext()
{
    if (Queue.Num() == 0)
    {
        if (bRunning)
        {
            bRunning = false;
            OnQueueCompleted.Broadcast();
        }
        return;
    }

    bRunning = true;

    const TPair<FNeocortexChatAction, FNeocortexChatResponseData> Item = Queue[0];
    Queue.RemoveAt(0);
    const FNeocortexChatAction Action = Item.Key;
    const FNeocortexChatResponseData Response = Item.Value;

    if (const TFunction<void(const FNeocortexChatAction&, const FNeocortexChatResponseData&)>* Handler = Handlers.Find(Action.Name))
    {
        bAwaitingFinish = true;
        (*Handler)(Action, Response);
        return;
    }

    if (OnActionStarted.IsBound())
    {
        bAwaitingFinish = true;
        OnActionStarted.Broadcast(Action, Response);
        return;
    }

    // No handler for this keyword; skip it and continue.
    OnUnhandledAction.Broadcast(Action);
    UE_LOG(LogNeocortex, Warning, TEXT("No handler registered for action '%s'. Skipping."), *Action.Name);
    ProcessNext();
}

void UNeocortexActionQueue::NotifyActionFinished()
{
    if (!bAwaitingFinish) return;
    bAwaitingFinish = false;
    ProcessNext();
}
