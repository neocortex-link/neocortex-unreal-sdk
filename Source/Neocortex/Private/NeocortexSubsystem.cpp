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
#include "Policies/CondensedJsonPrintPolicy.h"

static constexpr int32 MaxEvents      = 20;
static constexpr int32 MaxContentLen  = 64;

void UNeocortexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Http = NewObject<UNeocortexHttpClient>(this);
	SessionManager = NewObject<UNeocortexSessionManager>(this);
	Service = NewObject<UNeocortexService>(this);

	Http->Init(GetDefault<UNeocortexSettings>()->GetBaseUrl());
	SessionManager->Init(TEXT("NeocortexSessions"), TEXT("Session_"));
	Service->Init(Http, SessionManager);

#if WITH_EDITOR
	// Propagate API key changes made in Project Settings without requiring a restart.
	GetMutableDefault<UNeocortexSettings>()->SettingsChanged.BindLambda([this](const FName& PropertyName)
	{
		if (Http && PropertyName == GET_MEMBER_NAME_CHECKED(UNeocortexSettings, ApiKey))
			Http->SetApiKey(GetDefault<UNeocortexSettings>()->ApiKey.TrimStartAndEnd());
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

FString UNeocortexSubsystem::SerializeInteractables(const TArray<FNeocortexInteractable>& Entities)
{
	if (Entities.IsEmpty()) return TEXT("");

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	JsonArray.Reserve(Entities.Num());
	for (const FNeocortexInteractable& Entity : Entities)
	{
		TSharedRef<FJsonObject> Position = MakeShared<FJsonObject>();
		Position->SetNumberField(TEXT("x"), Entity.Position.X);
		Position->SetNumberField(TEXT("y"), Entity.Position.Y);
		Position->SetNumberField(TEXT("z"), Entity.Position.Z);

		TArray<TSharedPtr<FJsonValue>> Properties;
		for (const FNeocortexInteractableProperty& Property : Entity.Properties)
		{
			TSharedRef<FJsonObject> Pair = MakeShared<FJsonObject>();
			Pair->SetStringField(TEXT("name"), Property.Name);
			Pair->SetStringField(TEXT("value"), Property.Value);
			Properties.Add(MakeShared<FJsonValueObject>(Pair));
		}

		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("id"), Entity.Id);
		Obj->SetStringField(TEXT("name"), Entity.Name);
		Obj->SetObjectField(TEXT("position"), Position);
		Obj->SetArrayField(TEXT("properties"), Properties);
		Obj->SetStringField(TEXT("type"), Entity.Type);
		Obj->SetBoolField(TEXT("isSubject"), Entity.IsSubject);
		if (!Entity.CharacterId.IsEmpty()) Obj->SetStringField(TEXT("characterId"), Entity.CharacterId);
		JsonArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(JsonArray, Writer);
	return Json;
}

static TArray<FNeocortexInteractable> ToEntities(const TArray<UNeocortexInteractableComponent*>& Components)
{
	TArray<FNeocortexInteractable> Out;
	Out.Reserve(Components.Num());
	for (const UNeocortexInteractableComponent* Component : Components)
	{
		Out.Add(Component->ToNeocortexInteractable());
	}
	return Out;
}

FString UNeocortexSubsystem::CreateInteractablesMetadata() const
{
	return SerializeInteractables(ToEntities(GetAllInteractables()));
}

FString UNeocortexSubsystem::CreateInteractablesMetadataInRadius(const FVector& Location, float Radius) const
{
	return SerializeInteractables(ToEntities(GetInteractablesInRadius(Location, Radius)));
}

FString UNeocortexSubsystem::CreatePerceptionMetadata(const FVector& Origin, float Radius, int32 MaxEntities,
	const FString& SelfCharacterId, const FString& SelfName) const
{
	TArray<FNeocortexInteractable> Entities = ToEntities(Radius > 0.0f ? GetInteractablesInRadius(Origin, Radius) : GetAllInteractables());
	Entities.Sort([&Origin](const FNeocortexInteractable& A, const FNeocortexInteractable& B)
	{
		return FVector::DistSquared(Origin, A.Position) < FVector::DistSquared(Origin, B.Position);
	});
	if (Entities.Num() > FMath::Max(1, MaxEntities))
	{
		Entities.SetNum(FMath::Max(1, MaxEntities));
	}

	// The server presents the entry carrying the speaker's id as the character itself.
	const bool bRepresented = SelfCharacterId.IsEmpty() || Entities.ContainsByPredicate(
		[&SelfCharacterId](const FNeocortexInteractable& E) { return E.CharacterId == SelfCharacterId; });
	if (!bRepresented)
	{
		FNeocortexInteractable Me;
		Me.Id = SelfCharacterId;
		Me.CharacterId = SelfCharacterId;
		Me.Name = SelfName;
		Me.Position = Origin;
		Me.Type = TEXT("CHARACTER");
		Me.Properties.Add({ TEXT("name"), SelfName });
		Me.Properties.Add({ TEXT("kind"), TEXT("character") });
		Entities.Insert(Me, 0);
	}

	return SerializeInteractables(Entities);
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

