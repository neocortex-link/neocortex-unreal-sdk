#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexActionQueue.generated.h"

class UNeocortexSmartAgent;

/**
 * Fired when a dequeued action starts. Run your behavior for Action.Name (on Action.TargetId, when
 * set), then call NotifyActionFinished so the next action begins. Response is the full chat
 * response the action came from.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNeocortexActionStarted, const FNeocortexChatAction&, Action, const FNeocortexChatResponseData&, Response);

/** Fired when a dequeued action has no listener/handler. The action is skipped. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexUnhandledAction, const FNeocortexChatAction&, Action);

/** Fired once the queue drains (no more pending actions). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNeocortexQueueCompleted);

/**
 * Runs the ordered FNeocortexChatResponseData::Actions stack returned by the agent one at a time,
 * waiting for each action to be acknowledged before starting the next. Add it next to a
 * UNeocortexSmartAgent; it auto-subscribes to the agent's OnChat.
 *
 * Each action is announced via OnActionStarted; your game logic performs it (instantly or over
 * time) and calls NotifyActionFinished() when done, which advances the queue.
 * UNeocortexSmartAgent::RegisterAction runs actions without this component, and is the simpler
 * choice for new code.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexActionQueue : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeocortexActionQueue();

    /** Agent whose responses drive the queue. Falls back to a sibling component if left null. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TObjectPtr<UNeocortexSmartAgent> Agent = nullptr;

    /** Fired when an action starts. Call NotifyActionFinished when your handler is done. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Actions")
    FOnNeocortexActionStarted OnActionStarted;

    /** Fired when a dequeued action has no listener. It is skipped automatically. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Actions")
    FOnNeocortexUnhandledAction OnUnhandledAction;

    /** Fired once the queue drains. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Actions")
    FOnNeocortexQueueCompleted OnQueueCompleted;

    /** Call from your action handler when it has finished, to advance to the next action. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Actions")
    void NotifyActionFinished();

    /** True while an action sequence is currently being processed. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex|Actions")
    bool IsRunning() const { return bRunning; }

    /** Registers a C++ handler for a keyword. The handler must call NotifyActionFinished when done. */
    void RegisterAction(const FString& Keyword, TFunction<void(const FNeocortexChatAction&, const FNeocortexChatResponseData&)> Handler);

    /** Removes a previously registered C++ handler. */
    void UnregisterAction(const FString& Keyword);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleChatResponse(const FNeocortexChatResponseData& Response);

    void ProcessNext();

    /** Optional C++ handlers, keyed by action keyword. */
    TMap<FString, TFunction<void(const FNeocortexChatAction&, const FNeocortexChatResponseData&)>> Handlers;

    /** Pending (action, response) pairs to run in order. */
    TArray<TPair<FNeocortexChatAction, FNeocortexChatResponseData>> Queue;

    bool bRunning = false;
    bool bAwaitingFinish = false;
};
