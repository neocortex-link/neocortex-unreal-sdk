#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "Components/ActorComponent.h"
#include "NeocortexInteractableComponent.generated.h"

UCLASS(ClassGroup=(Neocortex), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexInteractableComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNeocortexInteractableComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Type; 
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Name;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool IsSubject;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FNeocortexInteractableProperty> Properties;
	
	FNeocortexInteractable ToNeocortexInteractable() const;
};
