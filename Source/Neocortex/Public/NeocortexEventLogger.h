#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Enums/NeocortexEnumTypes.h"
#include "NeocortexEventLogger.generated.h"

/**
 * Event log sent with each chat request to give the character context about recent game events.
 * Storage lives in UNeocortexSubsystem so it is isolated per GameInstance (PIE-safe).
 * Max 20 events, max 64 chars per entry, priority-sorted (High first, newest-first within priority).
 * Call PushEvent() as things happen in your game. Events are consumed (cleared) automatically when sent.
 */
UCLASS()
class NEOCORTEX_API UNeocortexEventLogger : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Records a game event. Content is silently truncated to 64 characters. Ignored when log is full (20 entries). */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Events", meta=(WorldContext="WorldContextObject"))
    static void PushEvent(UObject* WorldContextObject, ENeocortexEventPriority Priority, const FString& Content);

    /** Clears all recorded events. */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Events", meta=(WorldContext="WorldContextObject"))
    static void ClearEvents(UObject* WorldContextObject);

    /**
     * Returns current events as a compact JSON string and clears the log (consume semantics).
     * Called internally by UNeocortexSmartAgent before each request — not needed in Blueprint.
     */
    static FString ConsumeLogsJson(UObject* WorldContextObject);
};
