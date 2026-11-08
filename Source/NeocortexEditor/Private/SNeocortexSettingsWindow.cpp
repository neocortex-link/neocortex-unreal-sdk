#include "SNeocortexSettingsWindow.h"
#include "NeocortexEditorData.h"
#include "NeocortexSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "NeocortexSettingsWindow"

static TSharedRef<SWidget> Heading(const FText& Text)
{
    return SNew(STextBlock).Text(Text).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12));
}

void SNeocortexSettingsWindow::Construct(const FArguments& InArgs)
{
    const UNeocortexSettings* Settings = GetDefault<UNeocortexSettings>();

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot().Padding(16.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[ Heading(LOCTEXT("Key", "API key")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f)
                [
                    SAssignNew(KeyBox, SEditableTextBox)
                    .Text(FText::FromString(Settings->ApiKey))
                    .IsPassword_Lambda([this]() { return !bShowKey; })
                    .HintText(LOCTEXT("KeyHint", "Paste the key from your Neocortex dashboard"))
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(6, 0).VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() { return bShowKey ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowKey = State == ECheckBoxState::Checked; })
                    [ SNew(STextBlock).Text(LOCTEXT("Show", "Show")) ]
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton).Text(LOCTEXT("Save", "Save and refresh")).OnClicked(this, &SNeocortexSettingsWindow::SaveKey)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 12)
            [
                SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .Text_Lambda([]()
                {
                    const FString Url = GetDefault<UNeocortexSettings>()->GetBaseUrl();
                    return FText::Format(LOCTEXT("Server", "Requests go to {0}. More options under Project Settings > Neocortex."), FText::FromString(Url));
                })
            ]
            + SVerticalBox::Slot().AutoHeight()[ Heading(LOCTEXT("Account", "Account")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)[ SAssignNew(AccountText, STextBlock).AutoWrapText(true) ]
            + SVerticalBox::Slot().AutoHeight()[ Heading(LOCTEXT("Characters", "Characters")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)[ SAssignNew(CharacterList, SVerticalBox) ]
            + SVerticalBox::Slot().AutoHeight()[ Heading(LOCTEXT("QuestionSets", "Question sets")) ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 12)[ SAssignNew(QuestionSetList, SVerticalBox) ]
        ]
    ];

    ChangedHandle = FNeocortexEditorData::Get().OnChanged.AddSP(this, &SNeocortexSettingsWindow::Rebuild);
    FNeocortexEditorData::Get().EnsureLoaded();
    Rebuild();
}

SNeocortexSettingsWindow::~SNeocortexSettingsWindow()
{
    FNeocortexEditorData::Get().OnChanged.Remove(ChangedHandle);
}

FReply SNeocortexSettingsWindow::SaveKey()
{
    UNeocortexSettings* Settings = GetMutableDefault<UNeocortexSettings>();
    Settings->ApiKey = KeyBox->GetText().ToString().TrimStartAndEnd();
    Settings->TryUpdateDefaultConfigFile();
    FNeocortexEditorData::Get().Refresh();
    Rebuild();
    return FReply::Handled();
}

TSharedRef<SWidget> SNeocortexSettingsWindow::MakeRow(const FString& Name, const FString& Id, const FString& Detail)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(Detail.IsEmpty() ? Name : FString::Printf(TEXT("%s (%s)"), *Name, *Detail))) ]
            + SVerticalBox::Slot().AutoHeight()[ SNew(STextBlock).Text(FText::FromString(Id)).ColorAndOpacity(FSlateColor::UseSubduedForeground()) ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(6, 2)
        [
            SNew(SButton).Text(LOCTEXT("Copy", "Copy id"))
            .OnClicked_Lambda([Id]() { FPlatformApplicationMisc::ClipboardCopy(*Id); return FReply::Handled(); })
        ];
}

void SNeocortexSettingsWindow::Rebuild()
{
    const FNeocortexEditorData& Data = FNeocortexEditorData::Get();
    FString Account = Data.IsLoading() ? TEXT("Loading...") : Data.AccountSummary;
    if (!Data.LastError.IsEmpty() && !Data.IsLoading()) Account = Data.LastError;
    AccountText->SetText(FText::FromString(Account));

    auto Fill = [this, &Data](const TSharedPtr<SVerticalBox>& List, const TArray<FNeocortexEditorEntry>& Entries, const FText& Empty)
    {
        List->ClearChildren();
        for (const FNeocortexEditorEntry& Entry : Entries)
        {
            List->AddSlot().AutoHeight().Padding(0, 2)[ MakeRow(Entry.Name, Entry.Id, Entry.Detail) ];
        }
        if (Entries.Num() == 0 && !Data.IsLoading())
        {
            List->AddSlot().AutoHeight()[ SNew(STextBlock).Text(Empty).ColorAndOpacity(FSlateColor::UseSubduedForeground()) ];
        }
    };
    Fill(CharacterList, Data.Characters, LOCTEXT("NoCharacters", "No characters yet. Create one in your Neocortex dashboard."));
    Fill(QuestionSetList, Data.QuestionSets, LOCTEXT("NoSets", "No question sets yet. Create one under Question Sets in your dashboard."));
}

#undef LOCTEXT_NAMESPACE
