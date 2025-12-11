#pragma once
#include "CoreMinimal.h"
#include "NeocortexSessionManager.generated.h"

/**
 * Manages persistent session IDs for characters across gameplay sessions.
 * Session data is cached in memory and persisted to configuration files.
 * Thread-safe for concurrent access.
 */
UCLASS()
class NEOCORTEX_API UNeocortexSessionManager : public UObject
{
    GENERATED_BODY()
public:
    /**
     * Initializes the session manager with configuration section and key prefix.
     * @param Section Configuration file section name (e.g., "NeocortexSessions")
     * @param Prefix Key prefix for session entries (e.g., "Character_")
     */
    void Init(const FString& Section, const FString& Prefix);

    /**
     * Retrieves the session ID for a given character.
     * @param ProjectId Unique identifier for Neocortex Smart NPC character
     * @return The stored session ID, or empty string if not found
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Session")
    FString Get(const FString& ProjectId) const;

    /**
     * Stores or updates the session ID for a character.
     * @param ProjectId Unique identifier for Neocortex Smart NPC character
     * @param SessionId Session ID to associate with the character
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Session")
    void Set(const FString& ProjectId, const FString& SessionId);

    /**
     * Removes the session ID for a character.
     * @param ProjectId Unique identifier for Neocortex Smart NPC character
     */
    UFUNCTION(BlueprintCallable, Category="Neocortex|Session")
    void Clear(const FString& ProjectId);

private:
    /** Configuration file section name. */
    FString Section;
    
    /** Key prefix for session entries. */
    FString Prefix;

    /** Protects concurrent access to Cache. */
    mutable FCriticalSection Mutex;
    
    /** In-memory cache of character ID to session ID mappings. */
    mutable TMap<FString,FString> Cache;

    /**
     * Persists a single session entry to the configuration file.
     * @param Key Full configuration key
     * @param Value Session ID to persist
     */
    void FlushSingle(const FString& Key, const FString& Value) const;
};
