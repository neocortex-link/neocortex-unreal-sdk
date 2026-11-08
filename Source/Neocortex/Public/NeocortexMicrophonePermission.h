#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeocortexMicrophonePermission.generated.h"

/**
 * Blueprint utility for microphone permission on mobile.
 *
 * On Android these map to the RECORD_AUDIO runtime permission. On iOS the OS prompts automatically
 * from the NSMicrophoneUsageDescription plist entry, and on desktop the microphone is always
 * available, so HasMicrophonePermission returns true there and RequestMicrophonePermission is a
 * no-op. Call RequestMicrophonePermission early (e.g. on menu load) so the prompt is out of the way
 * before the player first records.
 */
UCLASS()
class NEOCORTEX_API UNeocortexMicrophonePermission : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** True when microphone capture is permitted (always true on iOS/desktop). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Neocortex|Microphone")
    static bool HasMicrophonePermission();

    /** Requests the microphone permission from the OS (Android). No-op on iOS/desktop. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex|Microphone")
    static void RequestMicrophonePermission();
};
