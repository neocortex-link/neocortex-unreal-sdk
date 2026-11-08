#include "Samples/NeocortexSampleAnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

static float Advance(float Time, float Delta, const UAnimSequence* Clip, bool bLoop)
{
    const float Length = Clip ? Clip->GetPlayLength() : 0.0f;
    if (Length <= 0.0f) return 0.0f;
    Time += Delta;
    return bLoop ? FMath::Fmod(Time, Length) : FMath::Min(Time, Length);
}

void UNeocortexSampleAnimInstance::Play(UAnimSequence* Clip, float InBlendSeconds, bool bLoop)
{
    if (Clip == Current && bLoop == bLoopCurrent) return;
    Previous = Current;
    PreviousTime = CurrentTime;
    bLoopPrevious = bLoopCurrent;
    Current = Clip;
    CurrentTime = 0.0f;
    bLoopCurrent = bLoop;
    BlendSeconds = Previous ? InBlendSeconds : 0.0f;
    BlendElapsed = 0.0f;
}

bool UNeocortexSampleAnimInstance::IsFinished() const
{
    return Current && !bLoopCurrent && CurrentTime >= Current->GetPlayLength();
}

void UNeocortexSampleAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    CurrentTime = Advance(CurrentTime, DeltaSeconds, Current, bLoopCurrent);
    if (Previous)
    {
        PreviousTime = Advance(PreviousTime, DeltaSeconds, Previous, bLoopPrevious);
        BlendElapsed += DeltaSeconds;
        if (BlendElapsed >= BlendSeconds) Previous = nullptr;
    }
}

void FNeocortexSampleAnimProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    const UNeocortexSampleAnimInstance* Instance = CastChecked<UNeocortexSampleAnimInstance>(InAnimInstance);
    Current = Instance->Current;
    Previous = Instance->Previous;
    CurrentTime = Instance->CurrentTime;
    PreviousTime = Instance->PreviousTime;
    bLoopCurrent = Instance->bLoopCurrent;
    bLoopPrevious = Instance->bLoopPrevious;
    Alpha = Instance->BlendSeconds > 0.0f ? FMath::Clamp(Instance->BlendElapsed / Instance->BlendSeconds, 0.0f, 1.0f) : 1.0f;
}

static void Extract(const UAnimSequence* Clip, float Time, bool bLoop, FPoseContext& Into)
{
    FAnimationPoseData Pose(Into);
    Clip->GetAnimationPose(Pose, FAnimExtractContext(Time, false, {}, bLoop));
}

bool FNeocortexSampleAnimProxy::Evaluate(FPoseContext& Output)
{
    if (!Current) return false;

    if (!Previous || Alpha >= 1.0f)
    {
        Extract(Current, CurrentTime, bLoopCurrent, Output);
        return true;
    }

    FPoseContext From(Output);
    FPoseContext To(Output);
    Extract(Previous, PreviousTime, bLoopPrevious, From);
    Extract(Current, CurrentTime, bLoopCurrent, To);
    FAnimationPoseData FromData(From);
    FAnimationPoseData ToData(To);
    FAnimationPoseData Result(Output);
    FAnimationRuntime::BlendTwoPosesTogether(ToData, FromData, Alpha, Result);
    return true;
}
