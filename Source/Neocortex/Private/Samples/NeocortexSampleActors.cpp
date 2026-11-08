#include "Samples/NeocortexSampleActors.h"
#include "Samples/NeocortexSampleBase.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexQuizAgent.h"
#include "NeocortexFaceAnimator.h"
#include "NeocortexInteractableComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/EngineVersionComparison.h"

static const TCHAR* CoraMeshPath = TEXT("/Neocortex/Samples/Cora/cora.cora");

// Character

ANeocortexSampleCharacter::ANeocortexSampleCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // The model faces along the actor's forward.
    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(Root);
    Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

    Voice = CreateDefaultSubobject<UAudioComponent>(TEXT("Voice"));
    Voice->SetupAttachment(Root);
    Voice->bAutoActivate = false;

    // So other characters perceive this one, and the server knows who is speaking.
    Presence = CreateDefaultSubobject<UNeocortexInteractableComponent>(TEXT("Presence"));
    Motion = CreateDefaultSubobject<UNeocortexSampleMotion>(TEXT("Motion"));

    Face = CreateDefaultSubobject<UNeocortexFaceAnimator>(TEXT("Face"));
    Face->Meshes.Add(Mesh);

    Agent = CreateOptionalDefaultSubobject<UNeocortexSmartAgent>(TEXT("Agent"));
    if (Agent)
    {
        Agent->AudioComponent = Voice;
        Face->Agent = Agent;
    }
}

FString ANeocortexSampleCharacter::GetCharacterId() const
{
    if (Agent && !Agent->ProjectId.IsEmpty()) return Agent->ProjectId;
    if (QuizAgent && !QuizAgent->CharacterId.IsEmpty()) return QuizAgent->CharacterId;
    const FNeocortexSamplePicks Picks = FNeocortexSamplePicks::Load();
    if (PickIndex <= 0) return Picks.CharacterId;
    return Picks.OtherCharacterIds.IsValidIndex(PickIndex - 1) ? Picks.OtherCharacterIds[PickIndex - 1] : FString();
}

// Cora shows in the editor as soon as the actor is placed.
void ANeocortexSampleCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
#if UE_VERSION_OLDER_THAN(5, 1, 0)
    if (!Mesh->SkeletalMesh)
    {
        if (USkeletalMesh* Cora = LoadObject<USkeletalMesh>(nullptr, CoraMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) Mesh->SetSkeletalMesh(Cora);
    }
#else
    if (!Mesh->GetSkeletalMeshAsset())
    {
        if (USkeletalMesh* Cora = LoadObject<USkeletalMesh>(nullptr, CoraMeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet)) Mesh->SetSkeletalMeshAsset(Cora);
    }
#endif
}

// The launcher's pick is filled in before the agents start, so they load the right history.
void ANeocortexSampleCharacter::PreInitializeComponents()
{
    Super::PreInitializeComponents();
    if (!GetWorld() || !GetWorld()->IsGameWorld()) return;
    const FString Id = GetCharacterId();
    if (Agent && Agent->ProjectId.IsEmpty()) Agent->ProjectId = Id;
    if (QuizAgent && QuizAgent->CharacterId.IsEmpty()) QuizAgent->CharacterId = Id;
    if (QuizAgent && QuizAgent->QuestionSetId.IsEmpty()) QuizAgent->QuestionSetId = FNeocortexSamplePicks::Load().QuestionSetId;
    Presence->CharacterId = Id;
}

void ANeocortexSampleCharacter::BeginPlay()
{
    Motion->Setup(Mesh, Agent, QuizAgent);
    Super::BeginPlay();
}

ANeocortexSampleQuizHost::ANeocortexSampleQuizHost(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Agent")))
{
    QuizAgent = CreateDefaultSubobject<UNeocortexQuizAgent>(TEXT("QuizAgent"));
    QuizAgent->AudioComponent = Voice;
    Face->QuizAgent = QuizAgent;
}

// Shape

ANeocortexSampleShape::ANeocortexSampleShape()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetRootComponent(Mesh);
    Interactable = CreateDefaultSubobject<UNeocortexInteractableComponent>(TEXT("Interactable"));
}

void ANeocortexSampleShape::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Apply();
}

void ANeocortexSampleShape::BeginPlay()
{
    Apply();
    Super::BeginPlay();
}

// A 30 cm square or a 50 cm circle, 10 cm tall, half in the floor, in its colour.
void ANeocortexSampleShape::Apply()
{
    const bool bSquare = Shape == ENeocortexSampleShapeKind::Square;
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, bSquare ? TEXT("/Engine/BasicShapes/Cube.Cube") : TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
    Mesh->SetRelativeScale3D(bSquare ? FVector(0.3f, 0.3f, 0.1f) : FVector(0.5f, 0.5f, 0.1f));
    if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
    {
        UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Base, this);
        Paint->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Paint);
    }
    const FString ShapeName = bSquare ? TEXT("square") : TEXT("circle");
    FString Title = ColorName.Left(1).ToUpper() + ColorName.Mid(1) + TEXT(" ") + (bSquare ? TEXT("Square") : TEXT("Circle"));
    Interactable->Name = Title;
    Interactable->Properties.Reset();
    Interactable->Properties.Add({ TEXT("color"), ColorName });
    Interactable->Properties.Add({ TEXT("shape"), ShapeName });
}
