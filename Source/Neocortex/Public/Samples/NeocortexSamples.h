#pragma once
#include "CoreMinimal.h"
#include "Samples/NeocortexSampleBase.h"
#include "NeocortexSamples.generated.h"

class UNeocortexChatUI;
class UNeocortexChatTranscript;
class UNeocortexTextInput;
class UNeocortexUsageGate;
class UNeocortexGroupDirector;
class UNeocortexQuizAgent;
class UNeocortexMicrophoneRecorderComponent;
class UCheckBox;
class UScrollBox;

/** Sample 1: type to the character; each line drops in and is voiced in its own emotion. */
UCLASS()
class NEOCORTEX_API ANeocortexTextChatSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Text Chat"); }
    virtual FString GetLead() const override { return TEXT("This scene demonstrates the basic text chat of Neocortex."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Type in the chat box and have a conversation with Cora."),
            TEXT("Restart the scene and pick the conversation up where you left off."),
            TEXT("Press New Chat Session to start from scratch, as if it is the first time you chat with Cora."),
        };
    }
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexChatUI> Chat;

private:
    UFUNCTION() void HandleNewSession();
};

/** Sample 2: talk through the microphone, with voice detection or push to talk, and hear the answer. */
UCLASS()
class NEOCORTEX_API ANeocortexAudioChatSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Audio Chat"); }
    virtual FString GetLead() const override { return TEXT("This scene demonstrates the basic audio chat of Neocortex."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Pick the microphone you want to use from the dropdown."),
            TEXT("Start the scene and speak to Cora."),
            TEXT("Switch on the Push-to-Talk toggle at the bottom right to hold a button while you speak, instead of automatic voice detection."),
        };
    }
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexChatUI> Chat;
    UPROPERTY() TObjectPtr<UNeocortexMicrophoneRecorderComponent> Microphone;

private:
    UFUNCTION() void HandleNewSession();
    UFUNCTION() void HandleMicrophonePicked(const FString& Item, bool bByPlayer);
    UFUNCTION() void HandlePushToTalk(bool bIsChecked);
};

/** Sample 3: ask the character to dance or jump; stacked actions run in order. */
UCLASS()
class NEOCORTEX_API ANeocortexActionsSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Actions"); }
    virtual FString GetLead() const override { return TEXT("This scene demonstrates single and stacked actions with Neocortex."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Cora has two actions in this scene, Dance and Jump."),
            TEXT("Tell Cora to <b>dance</b> (speak, or type) and watch the action play her animation."),
            TEXT("Then tell her to <b>jump</b> and then <b>dance</b>, and watch stacked actions run in order."),
        };
    }
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexChatUI> Chat;
};

/** Sample 4: the character perceives the shapes on the floor and walks to them. */
UCLASS()
class NEOCORTEX_API ANeocortexInteractablesSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Interactables"); }
    virtual FString GetLead() const override { return TEXT("This scene demonstrates a character's spatial understanding through Interactables."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Look at the <b>Neocortex Interactable</b> components on the shapes."),
            TEXT("Start the scene and tell Cora to go to the <b>Red Square</b>."),
            TEXT("Tell her to go to the <b>Yellow Circle</b>, then the <b>Blue Square</b>."),
        };
    }
    virtual FNeocortexBriefLayout GetBriefLayout() const override;
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexChatUI> Chat;
};

/** Sample 5: three characters in one conversation, joining and leaving. Needs a Pro or Team key. */
UCLASS()
class NEOCORTEX_API ANeocortexGroupChatSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Group Chat"); }
    virtual FString GetLead() const override { return TEXT("Neocortex's group chat, where characters or the player can join or leave at any time."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Pick your characters in the sample launcher; the group uses the first three."),
            TEXT("Use the toggles at the top of the screen to make characters join or leave."),
            TEXT("In this demo, joining or leaving starts the next conversation round."),
            TEXT("Speak to the group to start another round. Group chat needs a Pro or Team key."),
        };
    }
    virtual FNeocortexBriefLayout GetBriefLayout() const override;
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

public:
    /** Where each joining character stands while in the conversation. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    TArray<TObjectPtr<AActor>> Seats;

    /** Where each joining character waits while out of it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    TArray<TObjectPtr<AActor>> Offstage;

protected:
    UPROPERTY() TObjectPtr<UNeocortexGroupDirector> Director;
    UPROPERTY() TObjectPtr<class UNeocortexGroupChatUI> Chat;
    /** The characters who join and leave; the host stays. */
    UPROPERTY() TArray<TObjectPtr<UNeocortexSmartAgent>> Members;
    UPROPERTY() TArray<TObjectPtr<class UNeocortexToggle>> Toggles;
    TArray<bool> Present;
    bool bMoving = false;

private:
    UFUNCTION() void HandleToggle0(bool bIsChecked);
    UFUNCTION() void HandleToggle1(bool bIsChecked);
    UFUNCTION() void HandleTurnStarted();
    UFUNCTION() void HandleTurnFinished();
    void Toggle(int32 Index, bool bJoin);
    void SetTogglesEnabled(bool bEnabled);
    void FaceCamera(AActor* Body) const;
};

/** Sample 6: check credits and caps before every message. */
UCLASS()
class NEOCORTEX_API ANeocortexUsageGatingSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Usage Gating"); }
    virtual FString GetLead() const override { return TEXT("This scene demonstrates the usage gating of Neocortex."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("In the Neocortex dashboard, set your per player and per day usage limits on the API key."),
            TEXT("Run the scene and start chatting."),
            TEXT("Watch the usage log on the left, and read the sample's code."),
        };
    }
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexChatUI> Chat;
    UPROPERTY() TObjectPtr<UNeocortexUsageGate> Gate;
    UPROPERTY() TObjectPtr<UNeocortexSmartAgent> Agent;
    UPROPERTY() TObjectPtr<class UNeocortexSampleLogCard> UsageLog;

private:
    void Log(const FString& Line, const FLinearColor& Color);
    UFUNCTION() void HandleSubmitted(const FString& Text);
    UFUNCTION() void HandleLine(const FNeocortexChatLine& Line);
    UFUNCTION() void HandleUsage(const FNeocortexUsageResponse& Usage);
    UFUNCTION() void HandleAccount(const FNeocortexAccountResponse& Account);
    UFUNCTION() void HandleLow(const FNeocortexUsageResponse& Usage);
    UFUNCTION() void HandleEmpty(const FNeocortexUsageResponse& Usage);
    UFUNCTION() void HandlePlayerCap(const FNeocortexUsageResponse& Usage);
    UFUNCTION() void HandleCharacterCap(const FNeocortexUsageResponse& Usage);
    UFUNCTION() void HandleGateError(const FString& Error);
    UFUNCTION() void HandleRefresh();
    UFUNCTION() void HandleNewSession();
};

/** Sample 7: the character hosts a quiz from one of your question sets, for one to four players. */
UCLASS()
class NEOCORTEX_API ANeocortexQuizShowSample : public ANeocortexSampleBase
{
    GENERATED_BODY()
protected:
    virtual FString GetTitle() const override { return TEXT("Quiz Show"); }
    virtual FString GetLead() const override { return TEXT("Cora hosts a quiz for one to four players."); }
    virtual TArray<FString> GetSteps() const override
    {
        return {
            TEXT("Press Start."),
            TEXT("Pick who you reply as, then type or tap an option."),
            TEXT("Hint, Pass, Abort and Resync help a run along."),
        };
    }
    virtual FNeocortexBriefLayout GetBriefLayout() const override;
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;
    virtual void SetupSample() override;

    UPROPERTY() TObjectPtr<UNeocortexQuizAgent> Quiz;
};
