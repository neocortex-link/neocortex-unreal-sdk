#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NeocortexTypes.h"
#include "NeocortexUsageGate.generated.h"

/** Fired after every successful usage fetch, and for each credit/limit threshold transition. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexUsage, const FNeocortexUsageResponse&, Usage);

/** Fired after a successful account fetch. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexAccount, const FNeocortexAccountResponse&, Account);

/** Fired when a usage/account request fails. The game keeps running (fails open). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexUsageError, const FString&, Error);

/**
 * Turns the read-only account/usage endpoints into simple flags and events so a game can gate
 * smart NPC features instead of failing mid-conversation. Results are cached; usage is only
 * re-fetched when the cache is older than MinRefreshInterval, on an explicit Refresh call, or via
 * StartAutoRefresh. These endpoints never cost a credit.
 *
 * The gate is event-driven: call RefreshUsage / StartAutoRefresh to keep the cache warm, then CanUseSmartNPC returns a
 * decision from the cache (kicking a background refresh when the cache is stale). It fails open:
 * when no usage data is available at all it returns true rather than blocking the game.
 *
 * Create one with CreateUsageGate (Blueprint) or NewObject in C++.
 */
UCLASS(BlueprintType)
class NEOCORTEX_API UNeocortexUsageGate : public UObject
{
    GENERATED_BODY()
public:
    /** Creates a usage gate bound to the given world context (needed for the service and timers). */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage", meta = (WorldContext = "WorldContextObject"))
    static UNeocortexUsageGate* CreateUsageGate(UObject* WorldContextObject);

    /** Seconds a cached usage result is served before a new request is made. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|Usage")
    float MinRefreshInterval = 30.0f;

    /** Last account info fetched (valid once bHasAccount is true). */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|Usage")
    FNeocortexAccountResponse LastAccount;

    /** Last usage info fetched (valid once bHasUsage is true). */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|Usage")
    FNeocortexUsageResponse LastUsage;

    /** True once a usage result has been cached. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|Usage")
    bool bHasUsage = false;

    /** True once an account result has been cached. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex|Usage")
    bool bHasAccount = false;

    /** Raised after every successful usage fetch. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsage OnUsageUpdated;

    /** Raised once when team credits drop below the dashboard low-credit threshold. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsage OnLowCredits;

    /** Raised once when team credits run out. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsage OnCreditsEmpty;

    /** Raised once when the queried player goes over a developer-configured cap. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsage OnPlayerOverLimit;

    /** Raised once when the queried character goes over a developer-configured cap. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsage OnCharacterOverLimit;

    /** Raised after a successful account fetch. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexAccount OnAccountUpdated;

    /** Raised when a request fails (offline, invalid key, ...). The game keeps running. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Usage")
    FOnNeocortexUsageError OnRequestFailed;

    /**
     * Fetches usage and raises the gating events. When PlayerId is empty, the default device
     * player id is used (the id the SDK sends on every chat request).
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage")
    void RefreshUsage(const FString& PlayerId, const FString& CharacterId);

    /** Fetches the developer account info and raises OnAccountUpdated. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage")
    void RefreshAccount();

    /**
     * Returns false when team credits are empty or the player/character is over a configured cap,
     * using the cached usage result. Safe to call before every chat message: when the cache is
     * stale it kicks a background refresh and answers from the current cache. Fails open: returns
     * true when no usage data has been fetched yet.
     */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage")
    bool CanUseSmartNPC(const FString& PlayerId, const FString& CharacterId);

    /** Refreshes usage on a low-frequency interval until StopAutoRefresh or world teardown. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage")
    void StartAutoRefresh(float IntervalSeconds = 300.0f, const FString& PlayerId = TEXT(""), const FString& CharacterId = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Neocortex|Usage")
    void StopAutoRefresh();

private:
    /** World used to resolve the service and drive timers. */
    TWeakObjectPtr<UWorld> World;

    /** Realtime seconds of the last successful usage fetch. */
    double LastFetchTime = TNumericLimits<double>::Lowest();

    /** Query key (player|character) of the cached usage, so a different query forces a refetch. */
    FString LastQueryKey;

    // Previous threshold states so events fire once per transition instead of on every refresh.
    bool bWasLow = false;
    bool bWasEmpty = false;
    bool bWasPlayerOverLimit = false;
    bool bWasCharacterOverLimit = false;

    FTimerHandle AutoRefreshTimer;

    class UNeocortexService* ResolveService() const;
    void HandleUsage(const FNeocortexUsageResponse& Usage, const FString& QueryKey);
    void HandleFail(const FNeocortexRequestError& Error);
    static FString MakeQueryKey(const FString& PlayerId, const FString& CharacterId);
    static bool IsGateOpen(const FNeocortexUsageResponse& Usage);
};
