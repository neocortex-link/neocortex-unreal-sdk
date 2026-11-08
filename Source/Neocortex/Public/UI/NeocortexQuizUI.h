#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexQuizTypes.h"
#include "NeocortexQuizUI.generated.h"

class UNeocortexQuizAgent;
class UNeocortexChatTranscript;
class UNeocortexTextInput;
class UButton;
class UCheckBox;
class UTextBlock;
class UWrapBox;
class UHorizontalBox;
class UBorder;

/**
 * A quiz show screen for a UNeocortexQuizAgent, built in code, for one to four players at one
 * screen. The host's lines and the answers run as bubbles in the middle, tapped choices sit under
 * them, and "reply as" chips pick who answers next, with each player's score. The question card,
 * Start, Abort and Resync are on the right, with Hint, Pass and Pass Everyone at the bottom right.
 *
 *   UNeocortexQuizUI* Screen = CreateWidget<UNeocortexQuizUI>(PlayerController, UNeocortexQuizUI::StaticClass());
 *   Screen->SetQuiz(QuizAgent);
 *   Screen->AddToViewport();
 */
UCLASS()
class NEOCORTEX_API UNeocortexQuizUI : public UUserWidget
{
    GENERATED_BODY()
public:
    UNeocortexQuizUI(const FObjectInitializer& ObjectInitializer);

    /** The players, one to four. Set before the widget is shown. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    TArray<FString> PlayerNames;

    /** The host's name on its bubbles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex")
    FString HostName = TEXT("Host");

    UFUNCTION(BlueprintCallable, Category = "Neocortex")
    void SetQuiz(UNeocortexQuizAgent* InQuiz);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY() TObjectPtr<UNeocortexQuizAgent> Quiz;
    UPROPERTY() TObjectPtr<UNeocortexChatTranscript> Transcript;
    UPROPERTY() TObjectPtr<UNeocortexTextInput> Answer;
    UPROPERTY() TObjectPtr<UWrapBox> Options;
    UPROPERTY() TObjectPtr<UTextBlock> ProgressText;
    UPROPERTY() TObjectPtr<UTextBlock> KeywordText;
    UPROPERTY() TObjectPtr<UTextBlock> QuestionText;
    UPROPERTY() TObjectPtr<UTextBlock> UpcomingText;
    UPROPERTY() TObjectPtr<UTextBlock> StatusText;
    UPROPERTY() TObjectPtr<UButton> StartButton;
    UPROPERTY() TObjectPtr<UButton> AbortButton;
    UPROPERTY() TObjectPtr<UButton> ResyncButton;
    UPROPERTY() TObjectPtr<UButton> HintButton;
    UPROPERTY() TObjectPtr<UButton> PassButton;
    UPROPERTY() TObjectPtr<UButton> PassRestButton;
    UPROPERTY() TObjectPtr<UCheckBox> TypedToggle;
    UPROPERTY() TArray<TObjectPtr<UButton>> Chips;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> Scores;

    TArray<FNeocortexQuizParticipant> Players;
    TArray<FLinearColor> Verdicts;
    TArray<FString> OptionIds;
    TArray<FString> OptionTexts;
    int32 Current = 0;
    /** Players who answered this round and whose answer the server has not taken in yet. */
    TSet<FString> Sent;
    FString ErrorText;

    void BuildTree();
    void Refresh();
    bool IsAccepting() const;
    void SelectNext();
    bool IsWaitingOn(int32 Index) const;
    void MarkSent(int32 Index);
    void Select(int32 Index);
    void AnswerAsCurrent(const FString& Text, bool bTyped);
    void Note(const FString& Text, const FLinearColor& Color);
    int32 SeatOf(const FString& ParticipantId) const;
    void ChooseOption(int32 Index);

    UFUNCTION() void HandleHostLine(const FNeocortexQuizLine& Line);
    UFUNCTION() void HandleQuestion(const FNeocortexQuizQuestion& Question);
    UFUNCTION() void HandleUpcoming(const FNeocortexQuizUpcoming& Upcoming);
    UFUNCTION() void HandlePending(const TArray<FString>& Pending);
    UFUNCTION() void HandleResult(const TArray<FNeocortexQuizResult>& Results);
    UFUNCTION() void HandleLeaderboard(const TArray<FNeocortexQuizLeaderboardEntry>& Leaderboard);
    UFUNCTION() void HandleFinished(const TArray<FNeocortexQuizLeaderboardEntry>& Leaderboard);
    UFUNCTION() void HandleExpecting(ENeocortexQuizExpecting Expecting);
    UFUNCTION() void HandleBusy(bool bBusy);
    UFUNCTION() void HandleDropped(const FString& Text, ENeocortexQuizInputDropReason Reason);
    UFUNCTION() void HandleError(const FNeocortexRequestError& Error);
    UFUNCTION() void HandleCredits(const FString& Code);
    UFUNCTION() void HandleTyped(const FString& Text);
    UFUNCTION() void HandleStart();
    UFUNCTION() void HandleAbort();
    UFUNCTION() void HandleResync();
    UFUNCTION() void HandleHint();
    UFUNCTION() void HandlePass();
    UFUNCTION() void HandlePassRest();
    UFUNCTION() void HandleOption0();
    UFUNCTION() void HandleOption1();
    UFUNCTION() void HandleOption2();
    UFUNCTION() void HandleOption3();
    UFUNCTION() void HandleOption4();
    UFUNCTION() void HandleOption5();
    UFUNCTION() void HandleChip0();
    UFUNCTION() void HandleChip1();
    UFUNCTION() void HandleChip2();
    UFUNCTION() void HandleChip3();
};
