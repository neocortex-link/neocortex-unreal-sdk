#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "NeocortexTypes.h"
#include "NeocortexSubsystem.generated.h"

/**
 * Game instance subsystem managing Neocortex service lifecycle and dependencies.
 * Provides centralized access to HTTP client, session manager, and service layer.
 * Manages a registry of interactable components for efficient world state queries.
 * Manages the event log so it is isolated per GameInstance (safe in PIE).
 * Automatically initializes and cleans up on game instance start/end.
 */
UCLASS()
class NEOCORTEX_API UNeocortexSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Returns the Neocortex service instance for API interactions. */
	class UNeocortexService* GetService() const { return Service; }

	/** Returns the session manager for persistent character sessions. */
	class UNeocortexSessionManager* GetSessionManager() const { return SessionManager; }

	/**
	 * Registers an interactable component with the subsystem.
	 * Called automatically by components on BeginPlay.
	 * @param Interactable Component to register
	 */
	void RegisterInteractable(class UNeocortexInteractableComponent* Interactable);

	/**
	 * Unregisters an interactable component from the subsystem.
	 * Called automatically by components on EndPlay.
	 * @param Interactable Component to unregister
	 */
	void UnregisterInteractable(class UNeocortexInteractableComponent* Interactable);

	/**
	 * Gets all currently registered interactables.
	 * @return Array of all active interactable components
	 */
	TArray<class UNeocortexInteractableComponent*> GetAllInteractables() const;

	/**
	 * Gets all interactables within a specified radius of a location.
	 * @param Location World location to search from
	 * @param Radius Search radius in world units
	 * @return Array of interactables within range
	 */
	TArray<class UNeocortexInteractableComponent*> GetInteractablesInRadius(const FVector& Location, float Radius) const;

	/**
	 * Creates a JSON metadata string containing all interactables.
	 * Used for sending world state to the API.
	 * @return JSON array string of interactable data
	 */
	FString CreateInteractablesMetadata() const;

	/**
	 * Creates a JSON metadata string for interactables within range of a location.
	 * @param Location World location to search from
	 * @param Radius Search radius in world units
	 * @return JSON array string of interactable data within range
	 */
	FString CreateInteractablesMetadataInRadius(const FVector& Location, float Radius) const;

	/**
	 * Builds what a character perceives: interactables nearest first, capped at MaxEntities,
	 * plus the character itself when no interactable represents it.
	 * @param Origin Where the character stands
	 * @param Radius Only interactables within this many world units; 0 for unlimited
	 * @param MaxEntities Ceiling on how many interactables one turn carries
	 * @param SelfCharacterId The speaking character's id; empty to leave the character out
	 * @param SelfName What the character is called when it is added for itself
	 * @return JSON array string, or empty when there is nothing to perceive
	 */
	FString CreatePerceptionMetadata(const FVector& Origin, float Radius, int32 MaxEntities,
		const FString& SelfCharacterId, const FString& SelfName) const;

	// -------------------------------------------------------------------------
	// Event log: per-GameInstance storage used by UNeocortexEventLogger.
	// -------------------------------------------------------------------------

	/** Records a game event. Truncated to 64 chars. Dropped when log is full (20 entries). */
	void PushEvent(ENeocortexEventPriority Priority, const FString& Content);

	/** Clears all recorded events. */
	void ClearEvents();

	/**
	 * Returns current events as a compact JSON string and clears the log (consume semantics).
	 * Returns empty string when there are no events.
	 */
	FString ConsumeLogsJson();

private:
	/** HTTP client for API communication. */
	UPROPERTY()
	class UNeocortexHttpClient* Http = nullptr;

	/** Session manager for character state persistence. */
	UPROPERTY()
	class UNeocortexSessionManager* SessionManager = nullptr;

	/** Service layer for Neocortex API operations. */
	UPROPERTY()
	class UNeocortexService* Service = nullptr;

	/** Registry of all active interactable components in the world. */
	UPROPERTY()
	TArray<class UNeocortexInteractableComponent*> RegisteredInteractables;

	/** Serializes interactables to a compact JSON array string, or empty when there are none. */
	static FString SerializeInteractables(const TArray<FNeocortexInteractable>& Entities);

	// Event log storage, accessed via PushEvent / ClearEvents / ConsumeLogsJson.
	struct FEventEntry { ENeocortexEventPriority Priority; FString Date; FString Content; };
	mutable FCriticalSection EventMutex;
	TArray<FEventEntry> EventLog;
};
