#pragma once
#include "CoreMinimal.h"
#include "Samples/NeocortexSampleBase.h"
#include "NeocortexSampleLauncher.generated.h"

class UNeocortexDropdown;
class UNeocortexLabel;

/**
 * The sample launcher: what every sample uses (a character, the quiz's question set and the
 * microphone) above a card per sample. Its screen is WBP_NeocortexSamples, next to the map.
 */
UCLASS()
class NEOCORTEX_API ANeocortexSampleLauncher : public ANeocortexSampleBase
{
    GENERATED_BODY()
public:
    struct FEntry { const TCHAR* Map; const TCHAR* Title; const TCHAR* Description; };
    static const TArray<FEntry>& Samples();

    virtual FString GetTitle() const override { return TEXT("Samples"); }
    virtual FString GetSubtitle() const override { return TEXT("Unreal SDK · Samples"); }
    virtual bool HasBackButton() const override { return false; }
    virtual bool IsOpaque() const override { return true; }
    virtual void BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const override;

protected:
    virtual void SetupSample() override;

private:
    UPROPERTY() TObjectPtr<UNeocortexDropdown> CharacterPicker;
    UPROPERTY() TObjectPtr<UNeocortexDropdown> QuestionSetPicker;
    UPROPERTY() TObjectPtr<UNeocortexDropdown> MicrophonePicker;
    UPROPERTY() TObjectPtr<UNeocortexLabel> Status;

    TArray<FNeocortexCharacterSummary> CharacterList;
    TArray<FNeocortexQuestionSetSummary> QuestionSetList;

    void LoadLists();
    void SavePicks();
    void ShowProblem(const FString& Text);

    UFUNCTION() void HandlePicked(const FString& Item, bool bByPlayer);
    UFUNCTION() void Open1();
    UFUNCTION() void Open2();
    UFUNCTION() void Open3();
    UFUNCTION() void Open4();
    UFUNCTION() void Open5();
    UFUNCTION() void Open6();
    UFUNCTION() void Open7();
    void Open(int32 Index);
};
