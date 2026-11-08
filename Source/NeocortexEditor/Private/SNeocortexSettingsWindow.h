#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SVerticalBox;
class STextBlock;

/** Tools > Neocortex: the API key, the account behind it, its characters and question sets. */
class SNeocortexSettingsWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNeocortexSettingsWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SNeocortexSettingsWindow() override;

private:
    TSharedPtr<SEditableTextBox> KeyBox;
    TSharedPtr<STextBlock> AccountText;
    TSharedPtr<SVerticalBox> CharacterList;
    TSharedPtr<SVerticalBox> QuestionSetList;
    FDelegateHandle ChangedHandle;
    bool bShowKey = false;

    void Rebuild();
    FReply SaveKey();
    TSharedRef<SWidget> MakeRow(const FString& Name, const FString& Id, const FString& Detail);
};
