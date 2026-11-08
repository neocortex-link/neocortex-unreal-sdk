#include "NeocortexUsageGate.h"
#include "Neocortex.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

UNeocortexUsageGate* UNeocortexUsageGate::CreateUsageGate(UObject* WorldContextObject)
{
    UNeocortexUsageGate* Gate = NewObject<UNeocortexUsageGate>(WorldContextObject ? WorldContextObject : (UObject*)GetTransientPackage());
    if (WorldContextObject)
    {
        Gate->World = WorldContextObject->GetWorld();
    }
    return Gate;
}

UNeocortexService* UNeocortexUsageGate::ResolveService() const
{
    if (World.IsValid())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UNeocortexSubsystem* Sub = GI->GetSubsystem<UNeocortexSubsystem>())
            {
                return Sub->GetService();
            }
        }
    }
    return nullptr;
}

FString UNeocortexUsageGate::MakeQueryKey(const FString& PlayerId, const FString& CharacterId)
{
    return FString::Printf(TEXT("%s|%s"), *PlayerId, *CharacterId);
}

bool UNeocortexUsageGate::IsGateOpen(const FNeocortexUsageResponse& Usage)
{
    return Usage.Status != EUsageStatus::Empty
        && !Usage.Player.bOverLimit
        && !Usage.Character.bOverLimit;
}

void UNeocortexUsageGate::RefreshUsage(const FString& PlayerId, const FString& CharacterId)
{
    UNeocortexService* Service = ResolveService();
    if (!Service)
    {
        HandleFail(FNeocortexRequestError::FromMessage(TEXT("The Neocortex service is not available.")));
        return;
    }

    const FString EffectivePlayerId = PlayerId.IsEmpty() ? UNeocortexService::GetDefaultPlayerId() : PlayerId;
    const FString QueryKey = MakeQueryKey(EffectivePlayerId, CharacterId);

    TWeakObjectPtr<UNeocortexUsageGate> WeakThis(this);
    Service->GetUsage(EffectivePlayerId, CharacterId,
        FNeocortexUsageDelegate::CreateLambda([WeakThis, QueryKey](const FNeocortexUsageResponse& Usage)
        {
            if (WeakThis.IsValid()) WeakThis->HandleUsage(Usage, QueryKey);
        }),
        FNeocortexErrorDelegate::CreateLambda([WeakThis](const FNeocortexRequestError& Error)
        {
            if (WeakThis.IsValid()) WeakThis->HandleFail(Error);
        }));
}

void UNeocortexUsageGate::RefreshAccount()
{
    UNeocortexService* Service = ResolveService();
    if (!Service)
    {
        HandleFail(FNeocortexRequestError::FromMessage(TEXT("The Neocortex service is not available.")));
        return;
    }

    TWeakObjectPtr<UNeocortexUsageGate> WeakThis(this);
    Service->GetAccount(
        FNeocortexAccountDelegate::CreateLambda([WeakThis](const FNeocortexAccountResponse& Account)
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->LastAccount = Account;
            WeakThis->bHasAccount = true;
            WeakThis->OnAccountUpdated.Broadcast(Account);
        }),
        FNeocortexErrorDelegate::CreateLambda([WeakThis](const FNeocortexRequestError& Error)
        {
            if (WeakThis.IsValid()) WeakThis->HandleFail(Error);
        }));
}

bool UNeocortexUsageGate::CanUseSmartNPC(const FString& PlayerId, const FString& CharacterId)
{
    const FString EffectivePlayerId = PlayerId.IsEmpty() ? UNeocortexService::GetDefaultPlayerId() : PlayerId;
    const FString QueryKey = MakeQueryKey(EffectivePlayerId, CharacterId);

    const double Now = FPlatformTime::Seconds();
    const bool bCacheFresh = bHasUsage
        && LastQueryKey == QueryKey
        && (Now - LastFetchTime) < MinRefreshInterval;

    // Kick a background refresh when the cache is stale or for a new query; the result updates the
    // cache for subsequent calls and raises the threshold events.
    if (!bCacheFresh)
    {
        RefreshUsage(PlayerId, CharacterId);
    }

    // Fails open: with no data at all, don't block the game.
    if (!bHasUsage)
    {
        return true;
    }

    return IsGateOpen(LastUsage);
}

void UNeocortexUsageGate::StartAutoRefresh(float IntervalSeconds, const FString& PlayerId, const FString& CharacterId)
{
    StopAutoRefresh();
    if (!World.IsValid())
    {
        HandleFail(FNeocortexRequestError::FromMessage(TEXT("Usage gate has no world for auto-refresh.")));
        return;
    }

    // Fetch immediately, then on the interval.
    RefreshUsage(PlayerId, CharacterId);

    TWeakObjectPtr<UNeocortexUsageGate> WeakThis(this);
    const FString CapturedPlayer = PlayerId;
    const FString CapturedCharacter = CharacterId;
    World->GetTimerManager().SetTimer(AutoRefreshTimer,
        FTimerDelegate::CreateLambda([WeakThis, CapturedPlayer, CapturedCharacter]()
        {
            if (WeakThis.IsValid()) WeakThis->RefreshUsage(CapturedPlayer, CapturedCharacter);
        }),
        FMath::Max(IntervalSeconds, 1.0f), true);
}

void UNeocortexUsageGate::StopAutoRefresh()
{
    if (World.IsValid())
    {
        World->GetTimerManager().ClearTimer(AutoRefreshTimer);
    }
    AutoRefreshTimer.Invalidate();
}

void UNeocortexUsageGate::HandleUsage(const FNeocortexUsageResponse& Usage, const FString& QueryKey)
{
    LastUsage = Usage;
    bHasUsage = true;
    LastFetchTime = FPlatformTime::Seconds();
    LastQueryKey = QueryKey;

    OnUsageUpdated.Broadcast(Usage);

    const bool bLow = Usage.Status == EUsageStatus::Low;
    const bool bEmpty = Usage.Status == EUsageStatus::Empty;
    const bool bPlayerOver = Usage.Player.bOverLimit;
    const bool bCharacterOver = Usage.Character.bOverLimit;

    // Threshold events fire on the transition only, so a popup hooked to them shows once.
    if (bLow && !bWasLow) OnLowCredits.Broadcast(Usage);
    if (bEmpty && !bWasEmpty) OnCreditsEmpty.Broadcast(Usage);
    if (bPlayerOver && !bWasPlayerOverLimit) OnPlayerOverLimit.Broadcast(Usage);
    if (bCharacterOver && !bWasCharacterOverLimit) OnCharacterOverLimit.Broadcast(Usage);

    bWasLow = bLow;
    bWasEmpty = bEmpty;
    bWasPlayerOverLimit = bPlayerOver;
    bWasCharacterOverLimit = bCharacterOver;
}

void UNeocortexUsageGate::HandleFail(const FNeocortexRequestError& Error)
{
    UE_LOG(LogNeocortex, Warning, TEXT("UsageGate request failed: %s"), *Error.Message);
    OnRequestFailed.Broadcast(Error.Message);
}
