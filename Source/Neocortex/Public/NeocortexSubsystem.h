#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "NeocortexSubsystem.generated.h"

/**
 * Game instance subsystem managing Neocortex service lifecycle and dependencies.
 * Provides centralized access to HTTP client, session manager, and service layer.
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
};
