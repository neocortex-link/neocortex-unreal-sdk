#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "NeocortexTypes.h"
#include "NeocortexSampleBase.generated.h"

class UNeocortexSmartAgent;
class UNeocortexMicrophoneRecorderComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTextBlock;
class UScrollBox;
class UButton;
class UCanvasPanel;
class UHorizontalBox;
class UVerticalBox;
class UWidgetTree;
class UCanvasPanelSlot;

/**
 * What the sample launcher picked, kept in the user's config so every sample talks to your own
 * characters without editing a level.
 */
struct NEOCORTEX_API FNeocortexSamplePicks
{
    /** The character every sample uses. */
    FString CharacterId;
    FString CharacterName;
    /** Your other characters, in order, for the group sample. */
    TArray<FString> OtherCharacterIds;
    TArray<FString> OtherCharacterNames;
    /** The question set the quiz sample plays. */
    FString QuestionSetId;
    /** The microphone the samples listen with; empty is the system default. */
    FString Microphone;

    static FNeocortexSamplePicks Load();
    void Save() const;
};

/**
 * Sample only: animates a sample character. It idles, talks while its agent speaks, thinks while a
 * reply is on its way and walks while it moves; Jump and Dance play once. Without the imported
 * clips the moves fall back to simple motion of the whole character.
 */
UCLASS(ClassGroup=(Neocortex))
class NEOCORTEX_API UNeocortexSampleMotion : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeocortexSampleMotion();

    /** Connects the motion to the character's mesh and to what it listens to. */
    void Setup(USkeletalMeshComponent* InSkin, UNeocortexSmartAgent* InAgent, class UNeocortexQuizAgent* InQuizHost);

    /** A jump. OnDone runs when it lands. */
    void Jump(TFunction<void()> OnDone);
    /** A dance. OnDone runs when it ends. */
    void Dance(TFunction<void()> OnDone);
    /** Walks to Target on the floor, facing where it goes, and stops StopDistance short. */
    void WalkTo(const FVector& Target, TFunction<void()> OnDone);
    /** Turns to face a point on the floor. */
    void Face(const FVector& Target);

    /** Walking speed in units per second. */
    /** Matches the walk cycle, so the feet stay planted instead of sliding. */
    float WalkSpeed = 160.0f;
    float StopDistance = 70.0f;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    enum class EMove : uint8 { None, Jump, Dance, Walk };
    EMove Move = EMove::None;
    float Elapsed = 0.0f;
    FVector Start = FVector::ZeroVector;
    FVector Goal = FVector::ZeroVector;
    float StartYaw = 0.0f;
    TFunction<void()> Done;

    TWeakObjectPtr<UNeocortexSmartAgent> Agent;
    TWeakObjectPtr<class UNeocortexQuizAgent> QuizHost;
    UPROPERTY() TObjectPtr<class UNeocortexSampleAnimInstance> Anim;
    UPROPERTY() TMap<FName, TObjectPtr<class UAnimSequence>> Clips;
    /** Set while a clip plays once (jump, dance). */
    bool bOneShot = false;
    FName Playing;

    void Begin(EMove InMove, TFunction<void()> OnDone);
    void End();
    void PlayClip(FName Name, bool bLoop);
    bool IsTalking() const;
    bool IsThinking() const;
};

/** Sample only: where and how the demo brief sits. */
struct FNeocortexBriefLayout
{
    /** A dark card behind the brief, for scenes with a floor behind it. */
    bool bCard = false;
    /** Placement on the 1920 x 1080 screen: a point of the screen, the brief's point on it, an offset and a size. */
    FVector2D Anchor = FVector2D(1.0f, 0.5f);
    FVector2D Pivot = FVector2D(1.0f, 0.5f);
    FVector2D Offset = FVector2D::ZeroVector;
    FVector2D Size = FVector2D(512.0f, 512.0f);
    int32 FontSize = 24;
};

/**
 * Sample only: a sample's screen, on a 1920 x 1080 layout. Each sample map shows its own Widget
 * Blueprint of this class (WBP_ followed by the map name, next to the map), which holds the frame,
 * the demo brief, the chat and the sample's buttons as pieces you can open and change in the UMG
 * designer. Without one, the screen builds the same layout in code.
 */
UCLASS()
class NEOCORTEX_API UNeocortexSampleFrameUI : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Builds the sample's layout when this screen has none of its own, and connects Back to Samples. */
    void EnsureLayout(const class ANeocortexSampleBase* Sample);

    /** A widget of the screen by its name. */
    template<class T> T* Find(const TCHAR* Name) const
    {
        return WidgetTree ? Cast<T>(WidgetTree->FindWidget(FName(Name))) : nullptr;
    }

    /** Builds a sample's whole screen into a widget tree: the frame, the brief and the sample's own widgets. */
    static UCanvasPanel* BuildLayout(UWidgetTree* Tree, const class ANeocortexSampleBase* Sample);

    /**
     * The look shared by every sample screen: a dotted backdrop (black under it when bOpaque),
     * amber corner brackets and the logo with "NEOCORTEX" over the subtitle.
     */
    static void BuildChrome(UWidgetTree* Tree, UCanvasPanel* Root, const FString& Subtitle, bool bOpaque = false);

    /**
     * Places a widget on a canvas: Anchor is a point of the screen (0 to 1), Pivot the widget's point
     * that sits there, then an offset and a size in pixels at 1920 x 1080.
     */
    static UCanvasPanelSlot* PlaceOn(UCanvasPanel* Root, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Pivot, const FVector2D& Offset, const FVector2D& Size);

    /** Makes a named widget of the given class in a tree. */
    template<class T> static T* Make(UWidgetTree* Tree, const TCHAR* Name)
    {
        return Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));
    }

    /** Opens the sample launcher. */
    UFUNCTION() void HandleBack();

private:
    bool bConnected = false;
};

/**
 * The base of every Neocortex sample. The map holds the scene: the characters, the camera and
 * anything they use, as actors you can select. On Play the sample shows its screen, connects it
 * to the characters, gives the player a mouse cursor, and Escape returns to the launcher.
 */
UCLASS(Abstract)
class NEOCORTEX_API ANeocortexSampleBase : public AActor
{
    GENERATED_BODY()
public:
    ANeocortexSampleBase();

    /** The screen to show: the sample's Widget Blueprint. Empty builds the same layout in code. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    TSubclassOf<UNeocortexSampleFrameUI> ScreenClass;

    /** The camera the sample looks through. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    TObjectPtr<class ACameraActor> ViewCamera;

    /** The characters of the scene, in the order the sample uses them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    TArray<TObjectPtr<class ANeocortexSampleCharacter>> SceneCharacters;

    /**
     * Assets the sample loads by name, such as the animations. The map builder lists them here so
     * that packaging a sample map packages them too.
     */
    UPROPERTY(EditAnywhere, Category = "Neocortex Sample")
    TArray<TSoftObjectPtr<UObject>> SampleAssets;

    /** The map the Back button opens. */
    static const TCHAR* LauncherMap;

    /** The sample's name, shown at the top left. */
    virtual FString GetTitle() const { return TEXT("Sample"); }
    /** The line under "NEOCORTEX". */
    virtual FString GetSubtitle() const;
    /** The demo brief: a bold lead and the steps to try. <b>Bold</b> works inside both. */
    virtual FString GetLead() const { return FString(); }
    virtual TArray<FString> GetSteps() const { return {}; }
    /** Where the brief sits; on the right by default. */
    virtual FNeocortexBriefLayout GetBriefLayout() const { return FNeocortexBriefLayout(); }
    /** Whether the screen has Back to Samples; the launcher does not. */
    virtual bool HasBackButton() const { return true; }
    /** Black behind the screen, for a screen without a scene. */
    virtual bool IsOpaque() const { return false; }
    /** Adds the sample's own widgets to its screen, each with a name the sample finds it by. */
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const {}

protected:
    virtual void BeginPlay() override;

    /** Connects the screen to the scene. Called once the screen exists, before it shows. */
    virtual void SetupSample() {}

    /** A widget of the screen by its name. */
    template<class T> T* FindWidget(const TCHAR* Name) const { return Frame ? Frame->Find<T>(Name) : nullptr; }

    /** The smart agent of the Nth scene character, if there is one. */
    UNeocortexSmartAgent* AgentOf(int32 Index) const;

    /** Escape returns to the launcher. */
    void HandleEscape();

    APlayerController* GetPlayer() const;
    UNeocortexSampleMotion* MotionOf(UNeocortexSmartAgent* Agent) const;

    UPROPERTY() TObjectPtr<UNeocortexSampleFrameUI> Frame;
};
