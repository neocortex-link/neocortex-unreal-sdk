#include "NeocortexInteractableComponent.h"
#include "NeocortexSubsystem.h"
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

FNeocortexInteractable UNeocortexInteractableComponent::ToNeocortexInteractable() const
{
	FNeocortexInteractable Interactable;
	Interactable.Type = Type;
	Interactable.Name = Name;
	Interactable.IsSubject = IsSubject;
	Interactable.Properties = Properties;
	Interactable.Position = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	return Interactable;
}
