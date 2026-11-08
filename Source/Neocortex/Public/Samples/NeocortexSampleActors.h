#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeocortexSampleActors.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UAudioComponent;
class UNeocortexInteractableComponent;
class UNeocortexSmartAgent;
class UNeocortexQuizAgent;
class UNeocortexFaceAnimator;
class UNeocortexSampleMotion;

/**
 * Sample only: a talking character placed in a sample map. Cora's model, a voice, a Smart Agent,
 * lip sync and the sample animations, all as components you can select and change in the Details
 * panel. With no Character Id set on the agent it talks as the character picked in the launcher.
 */
UCLASS(ClassGroup = (Neocortex))
class NEOCORTEX_API ANeocortexSampleCharacter : public AActor
{
    GENERATED_BODY()
public:
    ANeocortexSampleCharacter(const FObjectInitializer& ObjectInitializer);

    /** Which launcher pick this character talks as when its agent has no id: 0 is the main character, 1 and 2 the next ones. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    int32 PickIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UAudioComponent> Voice;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexInteractableComponent> Presence;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexSampleMotion> Motion;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexFaceAnimator> Face;
    /** The character's brain; empty on a quiz host. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexSmartAgent> Agent;
    /** The quiz host's brain; only on a quiz host. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexQuizAgent> QuizAgent;

    /** The character id this character talks as: its agent's, else the launcher pick. */
    FString GetCharacterId() const;

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void PreInitializeComponents() override;
    virtual void BeginPlay() override;
};

/** Sample only: a character that hosts the quiz show, with a Quiz Agent in place of the Smart Agent. */
UCLASS(ClassGroup = (Neocortex))
class NEOCORTEX_API ANeocortexSampleQuizHost : public ANeocortexSampleCharacter
{
    GENERATED_BODY()
public:
    ANeocortexSampleQuizHost(const FObjectInitializer& ObjectInitializer);
};

UENUM(BlueprintType)
enum class ENeocortexSampleShapeKind : uint8
{
    Square,
    Circle
};

/**
 * Sample only: a low coloured shape on the floor that characters perceive and walk to. Its
 * interactable carries its name, colour and shape.
 */
UCLASS(ClassGroup = (Neocortex))
class NEOCORTEX_API ANeocortexSampleShape : public AActor
{
    GENERATED_BODY()
public:
    ANeocortexSampleShape();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    ENeocortexSampleShapeKind Shape = ENeocortexSampleShapeKind::Square;

    /** The colour's name, as the character hears it, for example "red". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FString ColorName = TEXT("red");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FLinearColor Color = FLinearColor::Red;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Neocortex Sample")
    TObjectPtr<UNeocortexInteractableComponent> Interactable;

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    virtual void BeginPlay() override;

private:
    void Apply();
};
