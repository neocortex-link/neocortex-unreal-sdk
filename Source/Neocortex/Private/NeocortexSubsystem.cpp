#include "NeocortexSubsystem.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexService.h"
#include "NeocortexSettings.h"
#include "NeocortexInteractableComponent.h"
#include "Neocortex.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
	RegisteredInteractables.Empty();
	Super::Deinitialize();
}

void UNeocortexSubsystem::RegisterInteractable(UNeocortexInteractableComponent* Interactable)
{
	if (!Interactable)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("Attempted to register null interactable"));
		return;
	}

	if (!RegisteredInteractables.Contains(Interactable))
	{
		RegisteredInteractables.Add(Interactable);
		UE_LOG(LogNeocortex, Log, TEXT("Registered interactable: %s"), *Interactable->Name);
	}
}

void UNeocortexSubsystem::UnregisterInteractable(UNeocortexInteractableComponent* Interactable)
{
	if (!Interactable)
	{
		return;
	}

	if (RegisteredInteractables.Remove(Interactable) > 0)
	{
		UE_LOG(LogNeocortex, Verbose, TEXT("Unregistered interactable: %s"), *Interactable->Name);
	} 
}

TArray<UNeocortexInteractableComponent*> UNeocortexSubsystem::GetAllInteractables() const
{
	// Filter out any null entries (shouldn't happen but safety first)
	TArray<UNeocortexInteractableComponent*> ValidInteractables;
	for (UNeocortexInteractableComponent* Interactable : RegisteredInteractables)
	{
		if (Interactable && IsValid(Interactable))
		{
			ValidInteractables.Add(Interactable);
		}
	}
	return ValidInteractables;
}

TArray<UNeocortexInteractableComponent*> UNeocortexSubsystem::GetInteractablesInRadius(const FVector& Location, float Radius) const
{
	TArray<UNeocortexInteractableComponent*> NearbyInteractables;
	const float RadiusSquared = Radius * Radius;

	for (UNeocortexInteractableComponent* Interactable : RegisteredInteractables)
	{
		if (!Interactable || !IsValid(Interactable) || !Interactable->GetOwner())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Location, Interactable->GetOwner()->GetActorLocation());
		if (DistanceSquared <= RadiusSquared)
		{
			NearbyInteractables.Add(Interactable);
		}
	}

	return NearbyInteractables;
}

FString UNeocortexSubsystem::CreateInteractablesMetadata() const
{
	TArray<UNeocortexInteractableComponent*> ValidInteractables = GetAllInteractables();
	
	if (ValidInteractables.Num() == 0)
	{
		return TEXT("");  // Return empty string, not "[]", matching Unity SDK
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(ValidInteractables.Num());

	for (UNeocortexInteractableComponent* Component : ValidInteractables)
	{
		FNeocortexInteractable InteractableData = Component->ToNeocortexInteractable();
		TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(InteractableData);
		if (JsonObject.IsValid())
		{
			JsonArray.Add(MakeShared<FJsonValueObject>(JsonObject));
		}
	}

	// Serialize to compact JSON string (no pretty-printing)
	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(JsonArray, Writer);
	
	return Json;
}

FString UNeocortexSubsystem::CreateInteractablesMetadataInRadius(const FVector& Location, float Radius) const
{
	TArray<UNeocortexInteractableComponent*> NearbyInteractables = GetInteractablesInRadius(Location, Radius);
	
	if (NearbyInteractables.Num() == 0)
	{
		return TEXT("");  // Return empty string, not "[]", matching Unity SDK
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(NearbyInteractables.Num());

	for (UNeocortexInteractableComponent* Component : NearbyInteractables)
	{
		FNeocortexInteractable InteractableData = Component->ToNeocortexInteractable();
		TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(InteractableData);
		if (JsonObject.IsValid())
		{
			JsonArray.Add(MakeShared<FJsonValueObject>(JsonObject));
		}
	}

	// Serialize to compact JSON string (no pretty-printing)
	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(JsonArray, Writer);

	return Json;
}

