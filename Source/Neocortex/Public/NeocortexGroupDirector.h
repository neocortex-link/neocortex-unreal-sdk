#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NeocortexTypes.h"
#include "NeocortexHttpClient.h"
#include "NeocortexGroupDirector.generated.h"

class UNeocortexSmartAgent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupSpeaker, const FNeocortexGroupMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupResponse, const FNeocortexGroupChatResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGroupTurnEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupError, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupRequestError, const FNeocortexRequestError&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroupHistory, const TArray<FNeocortexChatHistoryEntry>&, Messages, const FString&, NextCursor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGroupPlayerSpeech, const FString&, Text);

/**
 * Runs a group conversation across several Smart Agents: sends one turn for the whole cast and
 * routes each speaker's reply back to its own agent, one after the other. Send lets the cast
 * decide who answers, SendTo addresses one, ContinueScene lets them talk among themselves.
 * Characters can join and leave while it runs. A cast of two or more needs a Pro or Team API key.
 *
 * Each agent delivers its lines in its own ChatLinesMode and voice. Drive the agents through the
 * director while a scene runs, not one by one.
 */
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class NEOCORTEX_API UNeocortexGroupDirector : public UActorComponent
{
    GENERATED_BODY()
public:
    UNeocortexGroupDirector();

    /** The characters in the scene, up to three. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TArray<TObjectPtr<UNeocortexSmartAgent>> Agents;

    /** Most characters that speak in one director driven turn before it hands back. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex", meta = (ClampMin = "1", ClampMax = "5"))
    int32 MaxTurns = 5;

    /** Send every UNeocortexInteractableComponent in the world with each turn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    bool bIncludeAllInteractables = true;

    /** The shared scene session. Empty until the first turn; the server creates it. */
    UPROPERTY(BlueprintReadOnly, Category = "Neocortex")
    FString SessionId;

    /** Fired before a character speaks its turn: use it for a shared transcript or name label. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupSpeaker OnSpeaker;

    /** The whole group turn, once, before the speakers are played out. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupResponse OnGroupResponse;

    /** A turn starts, for example to lock the input field while the cast talks. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupTurnEvent OnTurnStarted;

    /** Every speaker in the turn has finished (also fired when a turn could not run). */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupTurnEvent OnTurnFinished;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupError OnError;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupRequestError OnRequestError;

    /** The shared session's transcript, every speaker, oldest first. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupHistory OnHistory;

    /** The transcript of the player's spoken input (SendAudio), so a UI can show it. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|Events")
    FOnGroupPlayerSpeech OnPlayerSpeech;

    /** The player says something to the group; the cast decides who answers. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Send(const FString& Message);

    /** The player addresses one character; it answers. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendTo(UNeocortexSmartAgent* Agent, const FString& Message);

    /** The player addresses one character by its id; it answers. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendToCharacter(const FString& CharacterId, const FString& Message);

    /** No player input: the cast talks among themselves. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ContinueScene();

    /** Transcribes the player's speech (WAV) and sends it like Send, firing OnPlayerSpeech with the transcript. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SendAudio(const TArray<uint8>& WavData);

    /** Adds a character to the cast mid scene; the cast acknowledges the arrival naturally. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void AddAgent(UNeocortexSmartAgent* Agent);

    /** Removes a character from the cast mid scene. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void RemoveAgent(UNeocortexSmartAgent* Agent);

    /** Forgets the scene session so the next turn starts a fresh conversation. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void ClearSession() { SessionId.Reset(); }

    /** Loads a page of the scene's transcript (OnHistory). Pass the previous NextCursor as Before to page back. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void RequestHistory(int32 Limit = 20, const FString& Before = TEXT(""));

    /** Cancels the turn in flight and silences every character. */
    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void Abort();

    /** True while a group turn is being fetched or played out. */
    UFUNCTION(BlueprintPure, Category = "Neocortex")
    bool IsBusy() const { return bBusy; }

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool bBusy = false;
    int32 TurnToken = 0;
    FNeocortexCancelTokenPtr RequestToken;

    class UNeocortexService* GetService() const;
    TArray<FString> CastIds() const;
    UNeocortexSmartAgent* FindAgent(const FString& CharacterId) const;
    void RunTurn(const FString& Message, const FString& SpeakerCharacterId);
    void PlaySpeaker(TArray<FNeocortexGroupMessage> Messages, int32 Index, int32 Token);
    void EndTurn();
    void Fail(const FNeocortexRequestError& Error);
};
