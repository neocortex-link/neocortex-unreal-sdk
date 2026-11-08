#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

class SEditableTextBox;
class SVerticalBox;
class SBox;

/** Tools > Neocortex: the API key, the account behind it, its characters and question sets, and the samples. */
class SNeocortexSettingsWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNeocortexSettingsWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SNeocortexSettingsWindow() override;

private:
    TSharedPtr<SEditableTextBox> KeyBox;
    TSharedPtr<SBox> AccountSlot;
    TSharedPtr<SVerticalBox> CharacterList;
    TSharedPtr<SVerticalBox> QuestionSetList;
    FDelegateHandle ChangedHandle;
    bool bShowKey = false;

    /** The id copied last and when, so its button can say so for a moment. */
    FString CopiedId;
    double CopiedAt = 0.0;

    FSlateBrush BannerBrush;
    FSlateBrush PageBrush;
    FSlateBrush CardBrush;
    FSlateBrush RowBrush;
    FSlateBrush BadgeBrush;
    FButtonStyle PrimaryButton;
    FButtonStyle QuietButton;

    void MakeStyles();
    void Rebuild();
    FReply SaveKey();
    FReply OpenSamples();
    TSharedRef<SWidget> MakeCard(const FText& Title, const TAttribute<FText>& Count, const TSharedRef<SWidget>& Body);
    TSharedRef<SWidget> MakeAccount();
    TSharedRef<SWidget> MakeRow(const FString& Name, const FString& Id, const FString& Detail);
};
