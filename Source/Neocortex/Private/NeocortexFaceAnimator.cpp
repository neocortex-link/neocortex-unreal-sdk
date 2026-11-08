#include "NeocortexFaceAnimator.h"
#include "NeocortexSmartAgent.h"
#include "Neocortex.h"
#include "NeocortexQuizAgent.h"
#include "NeocortexSpeech.h"
#include "NeocortexHttpStream.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundWave.h"
#include "Misc/EngineVersionComparison.h"

namespace
{
    enum EViseme { A, E, I, O, U, M_B_P, F_V, CH_J_SH, W_Q, L_N_T_D, R, REST, VisemeCount };

    struct FWeight { const TCHAR* Shape; float Value; };

    const TArray<FWeight>& VisemeShape(int32 Viseme)
    {
        static const TArray<FWeight> Shapes[VisemeCount] = {
            { { TEXT("jawopen"), 0.4f }, { TEXT("mouthlowerdownleft"), 0.3f }, { TEXT("mouthlowerdownright"), 0.3f } },
            { { TEXT("jawopen"), 0.15f }, { TEXT("mouthsmileleft"), 0.3f }, { TEXT("mouthsmileright"), 0.3f }, { TEXT("mouthstretchleft"), 0.4f }, { TEXT("mouthstretchright"), 0.4f } },
            { { TEXT("jawopen"), 0.1f }, { TEXT("mouthsmileleft"), 0.4f }, { TEXT("mouthsmileright"), 0.4f }, { TEXT("mouthdimpleleft"), 0.2f }, { TEXT("mouthdimpleright"), 0.2f } },
            { { TEXT("jawopen"), 0.3f }, { TEXT("mouthfunnel"), 0.8f } },
            { { TEXT("jawopen"), 0.15f }, { TEXT("mouthpucker"), 0.8f } },
            { { TEXT("mouthclose"), 0.2f }, { TEXT("mouthpressleft"), 0.2f }, { TEXT("mouthpressright"), 0.2f }, { TEXT("mouthrolllower"), 0.1f }, { TEXT("mouthrollupper"), 0.1f } },
            { { TEXT("jawopen"), 0.1f }, { TEXT("mouthrolllower"), 0.6f }, { TEXT("mouthshrugupper"), 0.2f } },
            { { TEXT("jawopen"), 0.1f }, { TEXT("mouthfunnel"), 0.3f }, { TEXT("mouthshrugupper"), 0.2f }, { TEXT("mouthshruglower"), 0.2f } },
            { { TEXT("jawopen"), 0.1f }, { TEXT("mouthpucker"), 0.9f } },
            { { TEXT("mouthstretchleft"), 0.3f }, { TEXT("mouthstretchright"), 0.3f } },
            { { TEXT("jawopen"), 0.1f }, { TEXT("mouthfunnel"), 0.2f }, { TEXT("mouthpucker"), 0.2f }, { TEXT("mouthstretchleft"), 0.1f }, { TEXT("mouthstretchright"), 0.1f } },
            {},
        };
        return Shapes[FMath::Clamp(Viseme, 0, VisemeCount - 1)];
    }

    int32 CharViseme(TCHAR C)
    {
        switch (C)
        {
        case 'a': return A;
        case 'e': return E;
        case 'i': case 'y': return I;
        case 'o': return O;
        case 'u': return U;
        case 'm': case 'b': case 'p': return M_B_P;
        case 'f': case 'v': return F_V;
        case 'c': case 'j': case 'q': case 'x': case 's': case 'h': return CH_J_SH;
        case 'w': case 'z': return W_Q;
        case 'l': case 'n': case 't': case 'd': return L_N_T_D;
        case 'r': case 'g': return R;
        default: return REST;
        }
    }

    bool IsVowel(TCHAR C)
    {
        return C == 'a' || C == 'e' || C == 'i' || C == 'o' || C == 'u' || C == 'y';
    }
}

UNeocortexFaceAnimator::UNeocortexFaceAnimator()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UNeocortexFaceAnimator::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (!Agent && !QuizAgent && Owner) Agent = Owner->FindComponentByClass<UNeocortexSmartAgent>();
    if (Meshes.Num() == 0 && Owner)
    {
        TArray<USkeletalMeshComponent*> Found;
        Owner->GetComponents<USkeletalMeshComponent>(Found);
        for (USkeletalMeshComponent* Mesh : Found) Meshes.Add(Mesh);
    }
    IndexShapes();

    if (Agent)
    {
        Agent->OnChat.AddDynamic(this, &UNeocortexFaceAnimator::HandleChat);
        Agent->OnChatLineStarted.AddDynamic(this, &UNeocortexFaceAnimator::HandleLine);
        Agent->OnAudio.AddDynamic(this, &UNeocortexFaceAnimator::HandleAudio);
        Agent->OnReplyFinished.AddDynamic(this, &UNeocortexFaceAnimator::HandleReplyFinished);
    }
    if (QuizAgent)
    {
        QuizAgent->OnHostLine.AddDynamic(this, &UNeocortexFaceAnimator::HandleHostLine);
        QuizAgent->OnHostFinished.AddDynamic(this, &UNeocortexFaceAnimator::HandleReplyFinished);
    }
}

void UNeocortexFaceAnimator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (Agent)
    {
        Agent->OnChat.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleChat);
        Agent->OnChatLineStarted.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleLine);
        Agent->OnAudio.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleAudio);
        Agent->OnReplyFinished.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleReplyFinished);
    }
    if (QuizAgent)
    {
        QuizAgent->OnHostLine.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleHostLine);
        QuizAgent->OnHostFinished.RemoveDynamic(this, &UNeocortexFaceAnimator::HandleReplyFinished);
    }
    Super::EndPlay(EndPlayReason);
}

void UNeocortexFaceAnimator::PlayText(const FString& Text, float Duration)
{
    bRandomTalk = false;
    Timeline = BuildTimeline(Text);
    TimelineLength = Duration;
    TimelineStart = FPlatformTime::Seconds();
    bAudioStarted = false;
}

void UNeocortexFaceAnimator::StartSpeaking()
{
    Timeline.Reset();
    bRandomTalk = true;
    NextRandomTime = 0.0;
}

void UNeocortexFaceAnimator::Rest()
{
    Timeline.Reset();
    bRandomTalk = false;
    Targets.Reset();
}

void UNeocortexFaceAnimator::PlayExpression(const FString& ShapeName, float Weight)
{
    Expression.Add(Canonical(ShapeName), FMath::Clamp(Weight, 0.0f, 1.0f));
}

void UNeocortexFaceAnimator::HandleChat(const FNeocortexChatResponseData& Response)
{
    ReplyText = Response.SpokenResponse.IsEmpty() ? Response.Response : Response.SpokenResponse;
}

bool UNeocortexFaceAnimator::IsStreamingSpeech() const
{
    return Agent && Agent->bStreamSpeech && FNeocortexSpeechJob::bStreamedSpeechSupported && FNeocortexHttpStream::IsSupported();
}

// Streamed speech is heard before its length is known, so the mouth starts with the line on an
// estimated length and rests when the voice stops. A whole clip retimes it on arrival.
void UNeocortexFaceAnimator::HandleLine(const FNeocortexChatLine& Line)
{
    LineText = Line.SpeechText();
    if (!Agent || !IsStreamingSpeech()) return;

    const bool bPerLine = Agent->ChatLinesMode == EChatLinesMode::PerLineAudio;
    if (!bPerLine && Agent->ChatLinesMode != EChatLinesMode::SingleAudio) return;
    // One clip voices the whole reply: the mouth starts with its first line only.
    if (!bPerLine && Timeline.Num() > 0) return;

    const FString& Text = bPerLine ? LineText : ReplyText;
    PlayText(Text, FMath::Max(0.5f, Text.Len() * FNeocortexSpeechJob::SecondsPerCharacter));
}

void UNeocortexFaceAnimator::HandleAudio(USoundWave* Clip)
{
    if (!Clip || !Agent) return;
    const bool bPerLine = Agent->ChatLinesMode == EChatLinesMode::PerLineAudio;
    PlayText(bPerLine && !LineText.IsEmpty() ? LineText : ReplyText, Clip->Duration);
}

// A host line is raised as its voice becomes audible; the mouth rests when the voice stops.
void UNeocortexFaceAnimator::HandleHostLine(const FNeocortexQuizLine& Line)
{
    if (!QuizAgent || !QuizAgent->bSpeakHostLines) return;
    LineText = Line.SpeechText();
    PlayText(LineText, FMath::Max(0.5f, LineText.Len() * FNeocortexSpeechJob::SecondsPerCharacter));
}

void UNeocortexFaceAnimator::HandleReplyFinished()
{
    Rest();
}

bool UNeocortexFaceAnimator::IsAgentAudioPlaying() const
{
    const UAudioComponent* Voice = Agent && Agent->AudioComponent ? Agent->AudioComponent.Get()
        : QuizAgent ? QuizAgent->AudioComponent.Get() : nullptr;
    return Voice && Voice->IsPlaying();
}

void UNeocortexFaceAnimator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const double Now = FPlatformTime::Seconds();
    TMap<FString, float> Wanted;

    if (bRandomTalk)
    {
        if (Now >= NextRandomTime)
        {
            NextRandomTime = Now + FMath::RandRange(0.05, 0.2);
            Targets.Reset();
            for (const FWeight& W : VisemeShape(FMath::RandRange(0, REST - 1))) Targets.Add(W.Shape, W.Value);
        }
        Wanted = Targets;
    }
    else if (Timeline.Num() > 0)
    {
        const float Elapsed = static_cast<float>(Now - TimelineStart);
        const bool bPlaying = IsAgentAudioPlaying();
        if (bPlaying) bAudioStarted = true;
        // Once the voice has started and stopped, the mouth rests with it.
        if (Elapsed >= TimelineLength || (bAudioStarted && !bPlaying))
        {
            Timeline.Reset();
        }
        else
        {
            const float Position = Elapsed / FMath::Max(0.001f, TimelineLength);
            int32 Current = REST;
            for (const FStep& Step : Timeline)
            {
                if (Step.At > Position) break;
                Current = Step.Viseme;
            }
            for (const FWeight& W : VisemeShape(Current)) Wanted.Add(W.Shape, W.Value);
        }
    }

    const float Alpha = FMath::Min(1.0f, DeltaTime * BlendSpeed);
    for (const FShape& Shape : Shapes)
    {
        USkeletalMeshComponent* Mesh = Shape.Mesh.Get();
        if (!Mesh) continue;
        float Target = Wanted.FindRef(Shape.Canonical);
        Target = FMath::Max(Target, Expression.FindRef(Shape.Canonical));
        const float Value = Mesh->GetMorphTarget(Shape.Name);
        Mesh->SetMorphTarget(Shape.Name, FMath::Lerp(Value, Target, Alpha));
    }
}

TArray<UNeocortexFaceAnimator::FStep> UNeocortexFaceAnimator::BuildTimeline(const FString& Text)
{
    // Every letter is a step, vowels held three times as long; a pause closes each word.
    struct FRaw { int32 Viseme; float Weight; };
    TArray<FRaw> Raw;
    float Total = 0.0f;
    bool bInWord = false;
    const FString Lower = Text.ToLower();
    for (const TCHAR C : Lower)
    {
        if (C >= 'a' && C <= 'z')
        {
            const float Weight = IsVowel(C) ? 3.0f : 1.0f;
            Raw.Add({ CharViseme(C), Weight });
            Total += Weight;
            bInWord = true;
        }
        else if (bInWord)
        {
            Raw.Add({ REST, 2.0f });
            Total += 2.0f;
            bInWord = false;
        }
    }
    if (bInWord)
    {
        Raw.Add({ REST, 2.0f });
        Total += 2.0f;
    }

    TArray<FStep> Out;
    float At = 0.0f;
    for (const FRaw& R : Raw)
    {
        Out.Add({ At / FMath::Max(Total, 1.0f), R.Viseme });
        At += R.Weight;
    }
    return Out;
}

void UNeocortexFaceAnimator::IndexShapes()
{
    Shapes.Reset();
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        if (!Mesh) continue;
#if UE_VERSION_OLDER_THAN(5, 1, 0)
        USkeletalMesh* Asset = Mesh->SkeletalMesh;
#else
        USkeletalMesh* Asset = Mesh->GetSkeletalMeshAsset();
#endif
        if (!Asset) continue;
        for (const UMorphTarget* Morph : Asset->GetMorphTargets())
        {
            if (!Morph) continue;
            FShape Shape;
            Shape.Mesh = Mesh;
            Shape.Name = Morph->GetFName();
            Shape.Canonical = Canonical(Morph->GetName());
            Shapes.Add(Shape);
        }
    }
    UE_LOG(LogNeocortex, Log, TEXT("Lip sync on %s: %d morph targets."), *GetNameSafe(GetOwner()), Shapes.Num());
}

// "Head.mouthSmile_L", "MouthSmileLeft" and "mouthSmileLeft" all become "mouthsmileleft".
FString UNeocortexFaceAnimator::Canonical(const FString& ShapeName)
{
    FString Out = ShapeName;
    int32 Dot = INDEX_NONE;
    if (Out.FindLastChar(TEXT('.'), Dot)) Out.RightChopInline(Dot + 1);
    Out.ToLowerInline();
    if (Out.EndsWith(TEXT("_l"))) Out = Out.LeftChop(2) + TEXT("left");
    else if (Out.EndsWith(TEXT("_r"))) Out = Out.LeftChop(2) + TEXT("right");
    return Out.Replace(TEXT("_"), TEXT(""));
}
