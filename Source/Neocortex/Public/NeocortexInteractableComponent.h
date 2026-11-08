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
	
	/** Stable id actions refer to (FNeocortexChatAction::TargetId). Leave empty to derive one from the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	FString Id;

	/** Set when this actor IS a Neocortex character. Leave empty to use the Smart Agent on the same actor, if any. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	FString CharacterId;

	/** Type classification of this interactable ("CHARACTER" or "OBJECT"). Leave empty to derive it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	FString Type; 
	
	/** What the character calls this thing. Leave empty to use the actor's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	FString Name;
	
	/** Whether this interactable is the subject of agent focus. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
	bool IsSubject = false;
	
	/** What this thing is and its state (e.g. Color: Red, State: Open). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Neocortex")
	TArray<FNeocortexInteractableProperty> Properties;

	/** The id actions use: Id when set, else "<actor-name>-<hash of the actor path>". */
	UFUNCTION(BlueprintPure, Category = "Neocortex")
	FString GetId() const;

	/** The character id this interactable represents, or empty for an object. */
	UFUNCTION(BlueprintPure, Category = "Neocortex")
	FString GetCharacterId() const;
	
	/**
	 * Converts this component's data to a serializable interactable structure.
	 * @return Interactable data with current actor position
	 */
	FNeocortexInteractable ToNeocortexInteractable() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
