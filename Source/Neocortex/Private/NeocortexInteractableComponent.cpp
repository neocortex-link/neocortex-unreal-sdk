#include "NeocortexInteractableComponent.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSmartAgent.h"
#include "Engine/World.h"
#include "Neocortex.h"

UNeocortexInteractableComponent::UNeocortexInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UNeocortexInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	// Auto-register with subsystem
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UNeocortexSubsystem* Subsystem = GameInstance->GetSubsystem<UNeocortexSubsystem>())
		{
			Subsystem->RegisterInteractable(this);
		}
		else
		{
			UE_LOG(LogNeocortex, Warning, TEXT("NeocortexInteractableComponent on %s could not find NeocortexSubsystem"), 
				*GetNameSafe(GetOwner()));
		}
	}
}

void UNeocortexInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Auto-unregister from subsystem
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UNeocortexSubsystem* Subsystem = GameInstance->GetSubsystem<UNeocortexSubsystem>())
		{
			Subsystem->UnregisterInteractable(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FString UNeocortexInteractableComponent::GetId() const
{
	const FString Explicit = Id.TrimStartAndEnd();
	if (!Explicit.IsEmpty()) return Explicit;

	const AActor* Owner = GetOwner();
	FString Sanitized = (Owner ? Owner->GetName() : GetName()).ToLower();
	for (TCHAR& C : Sanitized.GetCharArray())
	{
		if (C != 0 && !((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))) C = '-';
	}
	// Without the editor play prefix, so an id is the same in the editor and in a build.
	const uint32 Hash = GetTypeHash(UWorld::RemovePIEPrefix(Owner ? Owner->GetPathName() : GetPathName()));
	return FString::Printf(TEXT("%s-%08x"), *Sanitized, Hash);
}

FString UNeocortexInteractableComponent::GetCharacterId() const
{
	if (!CharacterId.IsEmpty()) return CharacterId;
	const UNeocortexSmartAgent* Agent = GetOwner() ? GetOwner()->FindComponentByClass<UNeocortexSmartAgent>() : nullptr;
	return Agent ? Agent->ProjectId : FString();
}

FNeocortexInteractable UNeocortexInteractableComponent::ToNeocortexInteractable() const
{
	FNeocortexInteractable Interactable;
	Interactable.Id = GetId();
	Interactable.CharacterId = GetCharacterId();
	Interactable.Type = !Type.IsEmpty() ? Type : (Interactable.CharacterId.IsEmpty() ? TEXT("OBJECT") : TEXT("CHARACTER"));
	Interactable.Name = !Name.IsEmpty() ? Name : (GetOwner() ? GetOwner()->GetName() : GetName());
	Interactable.IsSubject = IsSubject;
	Interactable.Properties = Properties;
	Interactable.Position = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	return Interactable;
}
