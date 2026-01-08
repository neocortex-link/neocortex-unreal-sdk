#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "Components/ActorComponent.h"
#include "NeocortexInteractableComponent.generated.h"

/**
 * Component marking an actor as interactable within the Neocortex world state.
 * Automatically registers with the subsystem on BeginPlay and unregisters on EndPlay.
 * Used to provide contextual information to AI agents about objects in the world.
 */
UCLASS(ClassGroup=(Neocortex), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexInteractableComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNeocortexInteractableComponent();
	
	/** Type classification of this interactable (e.g., "Character", "Object", "Item"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	FString Type; 
	
	/** Display name of this interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	FString Name;
	
	/** Whether this interactable is the subject of agent focus. */
	UPROPERTY(BlueprintReadOnly, Category = "Neocortex")
	bool IsSubject;
	
	/** Custom properties providing additional context about this interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	TArray<FNeocortexInteractableProperty> Properties;
	
	/**
	 * Converts this component's data to a serializable interactable structure.
	 * @return Interactable data with current actor position
	 */
	FNeocortexInteractable ToNeocortexInteractable() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
