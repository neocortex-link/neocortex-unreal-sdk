#include "NeocortexSubsystem.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexService.h"
#include "NeocortexSettings.h"

void UNeocortexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Http = NewObject<UNeocortexHttpClient>(this);
	SessionManager = NewObject<UNeocortexSessionManager>(this);
	Service = NewObject<UNeocortexService>(this);

	FNeocortexHttpOptions Options;
	Options.TimeoutSeconds = 30;
	Options.MaxRetries = 2;
	Options.RetryBackoffSeconds = 0.5f;
	Options.ApiKey = GetDefault<UNeocortexSettings>()->ApiKey;

	Http->Init(TEXT("https://neocortex.link/api/v2"), Options);
	SessionManager->Init(TEXT("NeocortexSessions"), TEXT("Session_"));
	Service->Init(Http, SessionManager);
}

void UNeocortexSubsystem::Deinitialize()
{
	Super::Deinitialize();
}
