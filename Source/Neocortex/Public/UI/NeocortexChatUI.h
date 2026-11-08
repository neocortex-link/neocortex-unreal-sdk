#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexTypes.h"
#include "NeocortexChatUI.generated.h"

class UNeocortexSmartAgent;
class UNeocortexMicrophoneRecorderComponent;
class UNeocortexChatTranscript;
class UNeocortexTextInput;
class UNeocortexVoiceInput;
class UCanvasPanel;

/** Fired with what the player typed, before it is sent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexChatSubmitted, const FString&, Text);

/** Where the chat sits on screen. */
UENUM(BlueprintType)
enum class ENeocortexChatLayout : uint8
{
    /** Centred at the bottom: the conversation over the scene, the input under it. */
    Bottom,
    /** On the left, over a dark backdrop that fades out toward the top, the input under it. */
    Side
};

/**
 * A complete chat with one character, built in code: the conversation as bubbles and under it a
 * text field, voice controls, or both. With a microphone the chat listens on its own, sends what
 * the player says once they pause and listens again after each reply; in push to talk the player
 * holds a button while speaking. No widget asset is needed:
 *
 *   UNeocortexChatUI* Chat = CreateWidget<UNeocortexChatUI>(PlayerController, UNeocortexChatUI::StaticClass());
 *   Chat->SetAgent(Agent);
 *   Chat->SetMicrophone(Microphone);   // optional: adds the voice controls
 *   Chat->AddToViewport();
 */
UCLASS()
class NEOCORTEX_API UNeocortexChatUI : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Where the chat sits. Set before the widget is shown. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    ENeocortexChatLayout Layout = ENeocortexChatLayout::Bottom;

    /** Height of the conversation in pixels at 1920 x 1080. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    float ConversationHeight = 512.0f;

    /** Show the text field. Turn off for voice only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bShowTextInput = true;

    /** Show the voice controls; SetMicrophone gives them the microphone to listen with. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bShowVoiceInput = false;

    /** Show the speaker's name in each bubble, for groups. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bShowSenderNames = false;

    /** Name on the player's bubbles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString PlayerName = TEXT("You");

    /** Hint in the empty text field. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString InputHint = TEXT("Say something...");

    /** Ask for speech with each typed message (SendMessageForAudio) instead of text only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bAskForAudio = true;

    /** Send what the player types to the agent. Turn off to decide yourself, from OnSubmitted. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bAutoSend = true;

    /** Fired with each message the player types, before it is sent. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex")
    FOnNeocortexChatSubmitted OnSubmitted;

    /** Connects the chat to a character. Its stored conversation shows once loaded (bLoadHistoryOnStart). */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetAgent(UNeocortexSmartAgent* InAgent);

    /** The microphone the voice controls listen with; shows them too. Call before the widget is shown. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetMicrophone(UNeocortexMicrophoneRecorderComponent* InRecorder);

    /** Push to talk: record only while the hold button is pressed. Off, the chat listens on its own. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetPushToTalk(bool bEnabled);

    /** Adds a bubble. Sender names it and gives its avatar the initial. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void AddMessage(const FString& Text, bool bIsPlayer, const FString& Sender);

    /** Adds a centred line, for errors or scene events. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void AddNotice(const FString& Text, bool bIsError = false);

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetTyping(bool bTyping);

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetInputEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ClearMessages();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

    /** The player typed something. Sends it to the agent. */
    virtual void Submit(const FString& Text);
    /** The player finished speaking. Sends the recording to the agent. */
    virtual void SubmitAudio(const TArray<uint8>& Wav);
    /** True while a reply is on its way or being spoken; the microphone waits meanwhile. */
    virtual bool IsBusy() const;
    /** True when there is someone to talk to. */
    virtual bool CanTalk() const;
    /** The name on the character's bubbles. */
    virtual FString SpeakerName() const;

    UPROPERTY() TObjectPtr<UNeocortexSmartAgent> Agent;
    UPROPERTY() TObjectPtr<UNeocortexMicrophoneRecorderComponent> Recorder;
    UPROPERTY() TObjectPtr<UNeocortexChatTranscript> Transcript;
    UPROPERTY() TObjectPtr<UNeocortexTextInput> TextInput;
    UPROPERTY() TObjectPtr<UNeocortexVoiceInput> VoiceInput;
    FString CharacterName;

private:
    UFUNCTION() void HandleTyped(const FString& Text);
    UFUNCTION() void HandleWav(const TArray<uint8>& Wav);
    UFUNCTION() void HandleChat(const FNeocortexChatResponseData& Response);
    UFUNCTION() void HandleLine(const FNeocortexChatLine& Line);
    UFUNCTION() void HandleComposing();
    UFUNCTION() void HandleFinished();
    UFUNCTION() void HandleTranscribed(const FString& Text);
    UFUNCTION() void HandleHistory(const TArray<FNeocortexChatHistoryEntry>& Messages, const FString& NextCursor);
    UFUNCTION() void HandleError(const FNeocortexRequestError& Error);

    void BuildTree();
    void BindAgent(bool bBind);

    bool bPushToTalk = false;
    bool bWarnedNoCharacter = false;
    /** How long the character has been quiet, so listening resumes only after a reply. */
    float IdleTime = 0.0f;
};
