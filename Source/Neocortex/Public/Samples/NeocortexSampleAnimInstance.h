#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "NeocortexSampleAnimInstance.generated.h"

class UAnimSequence;

/** Sample only: evaluates the current clip and the one it fades from, with no animation graph. */
struct FNeocortexSampleAnimProxy : public FAnimInstanceProxy
{
    FNeocortexSampleAnimProxy() = default;
    explicit FNeocortexSampleAnimProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}

    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
    virtual bool Evaluate(FPoseContext& Output) override;

    UAnimSequence* Current = nullptr;
    UAnimSequence* Previous = nullptr;
    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;
    bool bLoopCurrent = true;
    bool bLoopPrevious = true;
    /** Weight of Current: 1 once the fade is over. */
    float Alpha = 1.0f;
};

/**
 * Sample only: plays one clip at a time on a skeletal mesh and crossfades to the next, so a
 * character can idle, talk, think, walk and dance without an Animation Blueprint.
 */
UCLASS(Transient, NotBlueprintable)
class NEOCORTEX_API UNeocortexSampleAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    /** Fades to Clip over BlendSeconds. A looping clip repeats; a one shot holds its last frame. */
    void Play(UAnimSequence* Clip, float BlendSeconds, bool bLoop);

    UAnimSequence* GetCurrent() const { return Current; }

    /** True once a one shot has reached its end. */
    bool IsFinished() const;

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FNeocortexSampleAnimProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete InProxy; }
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    friend struct FNeocortexSampleAnimProxy;

    UPROPERTY() TObjectPtr<UAnimSequence> Current;
    UPROPERTY() TObjectPtr<UAnimSequence> Previous;
    float CurrentTime = 0.0f;
    float PreviousTime = 0.0f;
    bool bLoopCurrent = true;
    bool bLoopPrevious = true;
    float BlendSeconds = 0.0f;
    float BlendElapsed = 0.0f;
};
