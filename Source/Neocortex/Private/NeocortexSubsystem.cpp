#include "NeocortexSubsystem.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexService.h"
#include "NeocortexSettings.h"
#include "NeocortexInteractableComponent.h"
#include "Neocortex.h"
#include "Dom/JsonObject.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

static constexpr int32 MaxEvents      = 20;
static constexpr int32 MaxContentLen  = 64;

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

	Http->Init(TEXT("https://api.neocortex.link/v2"), Options);
	SessionManager->Init(TEXT("NeocortexSessions"), TEXT("Session_"));
	Service->Init(Http, SessionManager);

#if WITH_EDITOR
	// Propagate API key changes made in Project Settings without requiring a restart.
	GetMutableDefault<UNeocortexSettings>()->SettingsChanged.BindLambda([this](const FName& PropertyName)
	{
		if (Http && PropertyName == GET_MEMBER_NAME_CHECKED(UNeocortexSettings, ApiKey))
			Http->SetApiKey(GetDefault<UNeocortexSettings>()->ApiKey);
	});
#endif
}

void UNeocortexSubsystem::Deinitialize()
{
#if WITH_EDITOR
	GetMutableDefault<UNeocortexSettings>()->SettingsChanged.Unbind();
#endif
	RegisteredInteractables.Empty();
	EventLog.Empty();
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

FString UNeocortexSubsystem::SerializeInteractables(const TArray<UNeocortexInteractableComponent*>& Components)
{
	if (Components.IsEmpty()) return TEXT("");

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(Components.Num());
	for (UNeocortexInteractableComponent* Component : Components)
	{
		TSharedPtr<FJsonObject> Obj = FJsonObjectConverter::UStructToJsonObject(Component->ToNeocortexInteractable());
		if (Obj.IsValid()) JsonArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(JsonArray, Writer);
	return Json;
}

FString UNeocortexSubsystem::CreateInteractablesMetadata() const
{
	return SerializeInteractables(GetAllInteractables());
}

FString UNeocortexSubsystem::CreateInteractablesMetadataInRadius(const FVector& Location, float Radius) const
{
	return SerializeInteractables(GetInteractablesInRadius(Location, Radius));
}

void UNeocortexSubsystem::PushEvent(ENeocortexEventPriority Priority, const FString& Content)
{
	FScopeLock Lock(&EventMutex);
	if (EventLog.Num() >= MaxEvents)
	{
		UE_LOG(LogNeocortex, Warning, TEXT("NeocortexEventLogger: max %d events reached, call ClearEvents() to reset"), MaxEvents);
		return;
	}
	EventLog.Add({Priority, FDateTime::UtcNow().ToIso8601(), Content.Left(MaxContentLen)});
}

void UNeocortexSubsystem::ClearEvents()
{
	FScopeLock Lock(&EventMutex);
	EventLog.Empty();
}

FString UNeocortexSubsystem::ConsumeLogsJson()
{
	FScopeLock Lock(&EventMutex);
	if (EventLog.IsEmpty()) return TEXT("");

	// High priority first; newest (highest index) first within same priority.
	TArray<int32> Indices;
	Indices.Reserve(EventLog.Num());
	for (int32 i = 0; i < EventLog.Num(); ++i) Indices.Add(i);
	Indices.StableSort([this](int32 A, int32 B)
	{
		if (EventLog[A].Priority != EventLog[B].Priority)
			return static_cast<uint8>(EventLog[A].Priority) > static_cast<uint8>(EventLog[B].Priority);
		return A > B;
	});

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(Indices.Num());
	for (int32 Idx : Indices)
	{
		const FEventEntry& E = EventLog[Idx];
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("priority"), static_cast<int32>(E.Priority));
		Obj->SetStringField(TEXT("date"),     E.Date);
		Obj->SetStringField(TEXT("content"),  E.Content);
		JsonArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(JsonArray, Writer);

	EventLog.Empty(); // consume
	return Json;
}

