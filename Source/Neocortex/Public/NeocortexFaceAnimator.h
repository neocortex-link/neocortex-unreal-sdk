#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexQuizTypes.h"
#include "NeocortexFaceAnimator.generated.h"

class UNeocortexSmartAgent;
class UNeocortexQuizAgent;
class USkeletalMeshComponent;
class USoundWave;

/**
 * Lip sync for a character's face, driven by the words being spoken and timed to the voice, on
 * ARKit style morph targets (jawOpen, mouthFunnel, mouthPucker...). The mouth only moves while
 * the agent's speech plays, and rests when it stops.
 *
 * Add it next to a UNeocortexSmartAgent. Meshes default to every skeletal mesh on the owner that
 * has morph targets.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexFaceAnimator : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeocortexFaceAnimator();

    /** The agent whose speech this face follows. Falls back to the one on the same actor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TObjectPtr<UNeocortexSmartAgent> Agent = nullptr;

    /** A quiz host to follow instead of (or as well as) a Smart Agent. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TObjectPtr<UNeocortexQuizAgent> QuizAgent = nullptr;

    /** The meshes to animate. Empty: every skeletal mesh on the owner with morph targets. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TArray<TObjectPtr<USkeletalMeshComponent>> Meshes;

    /** How fast the mouth moves toward each shape. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (ClampMin = "0.0"))
    float BlendSpeed = 10.0f;

    /** Moves the mouth through the shapes of Text over Duration seconds. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void PlayText(const FString& Text, float Duration);

    /** Random mouth movement, for speech you have no text for. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void StartSpeaking();

    /** Closes the mouth. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Rest();

    /** Holds a morph target (any ARKit style name, any casing) at a weight on top of speech. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void PlayExpression(const FString& ShapeName, float Weight = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ClearExpressions() { Expression.Empty(); }

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FShape
    {
        TWeakObjectPtr<USkeletalMeshComponent> Mesh;
        FName Name;
        FString Canonical;
    };
    struct FStep
    {
        float At;
        int32 Viseme;
    };

    TArray<FShape> Shapes;
    TMap<FString, float> Targets;
    TMap<FString, float> Expression;
    TArray<FStep> Timeline;
    double TimelineStart = 0.0;
    float TimelineLength = 0.0f;
    FString ReplyText;
    FString LineText;
    bool bRandomTalk = false;
    double NextRandomTime = 0.0;
    bool bAudioStarted = false;

    UFUNCTION()
    void HandleChat(const FNeocortexChatResponseData& Response);

    UFUNCTION()
    void HandleLine(const FNeocortexChatLine& Line);

    UFUNCTION()
    void HandleAudio(USoundWave* Clip);

    UFUNCTION()
    void HandleReplyFinished();

    UFUNCTION()
    void HandleHostLine(const FNeocortexQuizLine& Line);

    void IndexShapes();
    bool IsAgentAudioPlaying() const;
    bool IsStreamingSpeech() const;
    static TArray<FStep> BuildTimeline(const FString& Text);
    static FString Canonical(const FString& ShapeName);
};
