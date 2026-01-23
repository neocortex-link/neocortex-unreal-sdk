#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "NeocortexTypes.h"
#include "NeocortexSubsystem.generated.h"

/**
 * Game instance subsystem managing Neocortex service lifecycle and dependencies.
 * Provides centralized access to HTTP client, session manager, and service layer.
 * Manages a registry of interactable components for efficient world state queries.
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
};
