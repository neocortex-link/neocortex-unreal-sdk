#include "NeocortexInteractableComponent.h"

UNeocortexInteractableComponent::UNeocortexInteractableComponent()
{
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
