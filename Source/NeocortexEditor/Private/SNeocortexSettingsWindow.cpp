#include "SNeocortexSettingsWindow.h"
#include "NeocortexEditorData.h"
#include "NeocortexSettings.h"
#include "UI/NeocortexUIStyle.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NeocortexSettingsWindow"

namespace
{
    const FLinearColor PageColor(FColor(0x14, 0x14, 0x14));
    const FLinearColor CardColor(FColor(0x1E, 0x1E, 0x1E));
    const FLinearColor CardEdge(FColor(0x2E, 0x2E, 0x2E));
    const FLinearColor RowColor(FColor(0x26, 0x26, 0x26));
    const FLinearColor QuietColor(FColor(0x33, 0x33, 0x33));
    const FLinearColor QuietHover(FColor(0x40, 0x40, 0x40));
    const FLinearColor AmberHover(FColor(0xFB, 0xB5, 0x3C));
    const FLinearColor DarkInk(FColor(0x14, 0x14, 0x14));
    const double CopiedSeconds = 1.5;
    // Rows shown before a list scrolls.
    const int32 VisibleRows = 5;
    const float RowHeight = 42.0f;

    FSlateFontInfo Font(const ANSICHAR* Style, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Style, Size); }

    TSharedRef<SWidget> Text(const FText& Value, const FSlateFontInfo& InFont, const FLinearColor& Color)
    {
        return SNew(STextBlock).Text(Value).Font(InFont).ColorAndOpacity(FSlateColor(Color));
    }

    FButtonStyle MakeButton(const FLinearColor& Normal, const FLinearColor& Hover, const FLinearColor& Pressed, float Radius)
    {
        FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        Style.SetNormal(NeocortexUI::Rounded(Normal, Radius));
        Style.SetHovered(NeocortexUI::Rounded(Hover, Radius));
        Style.SetPressed(NeocortexUI::Rounded(Pressed, Radius));
        Style.SetDisabled(NeocortexUI::Rounded(Normal * FLinearColor(1, 1, 1, 0.4f), Radius));
        Style.SetNormalPadding(FMargin(12.0f, 5.0f));
        Style.SetPressedPadding(FMargin(12.0f, 6.0f, 12.0f, 4.0f));
        return Style;
    }
}

void SNeocortexSettingsWindow::MakeStyles()
{
    PageBrush = NeocortexUI::Rounded(PageColor, 0.0f);
    CardBrush = NeocortexUI::Rounded(CardColor, 10.0f, CardEdge, 1.0f);
    RowBrush = NeocortexUI::Rounded(RowColor, 6.0f);
    BadgeBrush = NeocortexUI::Rounded(FLinearColor::Transparent, 9.0f, NeocortexUI::Amber, 1.0f);
    PrimaryButton = MakeButton(NeocortexUI::Amber, AmberHover, NeocortexUI::AmberDisc, 8.0f);
    QuietButton = MakeButton(QuietColor, QuietHover, RowColor, 6.0f);

    // The banner without its outer tenth on each side.
    if (UTexture2D* Banner = NeocortexUI::Icon(TEXT("banner")))
    {
        BannerBrush = NeocortexUI::Image(Banner, FVector2D(Banner->GetSizeX() * 0.8f, Banner->GetSizeY()));
        BannerBrush.SetUVRegion(FBox2D(FVector2D(0.1f, 0.0f), FVector2D(0.9f, 1.0f)));
    }
}

void SNeocortexSettingsWindow::Construct(const FArguments& InArgs)
{
    MakeStyles();
    const UNeocortexSettings* Settings = GetDefault<UNeocortexSettings>();
    const FNeocortexEditorData& Data = FNeocortexEditorData::Get();

    auto Scrolling = [](const TSharedPtr<SVerticalBox>& List)
    {
        return SNew(SBox).MaxDesiredHeight(VisibleRows * RowHeight)
        [
            SNew(SScrollBox) + SScrollBox::Slot()[ List.ToSharedRef() ]
        ];
    };
    SAssignNew(CharacterList, SVerticalBox);
    SAssignNew(QuestionSetList, SVerticalBox);

    const TSharedRef<SWidget> KeyBody = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SAssignNew(KeyBox, SEditableTextBox)
            .Text(FText::FromString(Settings->ApiKey))
            .Font(Font("Mono", 10))
            .IsPassword_Lambda([this]() { return !bShowKey; })
            .HintText(LOCTEXT("KeyHint", "Paste the key from your Neocortex dashboard"))
            .OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type Commit) { if (Commit == ETextCommit::OnEnter) SaveKey(); })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(10, 0).VAlign(VAlign_Center)
        [
            SNew(SCheckBox)
            .IsChecked_Lambda([this]() { return bShowKey ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowKey = State == ECheckBoxState::Checked; })
            [ Text(LOCTEXT("Show", "Show"), Font("Regular", 10), NeocortexUI::BodyText) ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SButton).ButtonStyle(&PrimaryButton).OnClicked(this, &SNeocortexSettingsWindow::SaveKey)
            [ Text(LOCTEXT("Save", "Save and refresh"), Font("Bold", 10), DarkInk) ]
        ];

    auto CountOf = [](const TArray<FNeocortexEditorEntry>* Entries)
    {
        return TAttribute<FText>::Create([Entries]() { return Entries->Num() > 0 ? FText::AsNumber(Entries->Num()) : FText::GetEmpty(); });
    };

    ChildSlot
    [
        SNew(SBorder).BorderImage(&PageBrush).Padding(0)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot().Padding(16.0f, 12.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 12)
                [
                    SNew(SBox).MinDesiredWidth(520.0f).MaxDesiredHeight(110.0f)
                    .Visibility(BannerBrush.GetResourceObject() ? EVisibility::Visible : EVisibility::Collapsed)
                    [
                        SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[ SNew(SImage).Image(&BannerBrush) ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ MakeCard(LOCTEXT("Key", "API KEY"), FText::GetEmpty(), KeyBody) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ MakeCard(LOCTEXT("Account", "ACCOUNT"), FText::GetEmpty(), SAssignNew(AccountSlot, SBox)) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [ MakeCard(LOCTEXT("Characters", "CHARACTERS"), CountOf(&Data.Characters), Scrolling(CharacterList)) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                [ MakeCard(LOCTEXT("QuestionSets", "QUESTION SETS"), CountOf(&Data.QuestionSets), Scrolling(QuestionSetList)) ]

                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(36.0f)
                    [
                        SNew(SButton).ButtonStyle(&PrimaryButton)
                        .HAlign(HAlign_Center).VAlign(VAlign_Center)
                        .OnClicked(this, &SNeocortexSettingsWindow::OpenSamples)
                        [ Text(LOCTEXT("OpenSamples", "Open Samples"), Font("Bold", 11), DarkInk) ]
                    ]
                ]
            ]
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

TSharedRef<SWidget> SNeocortexSettingsWindow::MakeCard(const FText& Title, const TAttribute<FText>& Count, const TSharedRef<SWidget>& Body)
{
    return SNew(SBorder).BorderImage(&CardBrush).Padding(FMargin(12.0f, 10.0f))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 7)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ Text(Title, Font("Bold", 9), NeocortexUI::Amber) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
            [
                SNew(STextBlock).Text(Count).Font(Font("Regular", 9)).ColorAndOpacity(FSlateColor(NeocortexUI::Muted))
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()[ Body ]
    ];
}

TSharedRef<SWidget> SNeocortexSettingsWindow::MakeAccount()
{
    const FNeocortexEditorData& Data = FNeocortexEditorData::Get();
    if (Data.IsLoading())
    {
        return Text(LOCTEXT("Loading", "Loading..."), Font("Regular", 10), NeocortexUI::Muted);
    }
    if (!Data.bHasAccount)
    {
        const FText Message = Data.LastError.IsEmpty() ? LOCTEXT("NoAccount", "Save your API key to see your account.") : FText::FromString(Data.LastError);
        return SNew(STextBlock).Text(Message).AutoWrapText(true).Font(Font("Regular", 10))
            .ColorAndOpacity(FSlateColor(Data.LastError.IsEmpty() ? NeocortexUI::Muted : NeocortexUI::ErrorColor));
    }
    const FText Refresh = Data.NextRefresh.IsEmpty() ? FText::GetEmpty()
        : FText::Format(LOCTEXT("Refresh", "Refreshes {0}"), FText::FromString(Data.NextRefresh));
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBorder).BorderImage(&BadgeBrush).Padding(FMargin(9.0f, 2.0f))
            [ Text(FText::FromString(Data.Tier.ToUpper()), Font("Bold", 9), NeocortexUI::Amber) ]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(10, 0)
        [ Text(FText::FromString(Data.Email), Font("Regular", 10), NeocortexUI::BodyText) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
                [ Text(FText::AsNumber(Data.Credits), Font("Bold", 14), NeocortexUI::Ink) ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(6, 0, 0, 3)
                [ Text(LOCTEXT("Credits", "credits left"), Font("Regular", 9), NeocortexUI::Muted) ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
            [ Text(Refresh, Font("Regular", 9), NeocortexUI::Muted) ]
        ];
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

// Opens the sample launcher map, after offering to save the current one.
FReply SNeocortexSettingsWindow::OpenSamples()
{
    const FString Map = TEXT("/Neocortex/Samples/Maps/NeocortexSamples");
    if (!FEditorFileUtils::SaveDirtyPackages(true, true, false)) return FReply::Handled();
    FEditorFileUtils::LoadMap(FPackageName::LongPackageNameToFilename(Map, FPackageName::GetMapPackageExtension()));
    return FReply::Handled();
}

TSharedRef<SWidget> SNeocortexSettingsWindow::MakeRow(const FString& Name, const FString& Id, const FString& Detail)
{
    return SNew(SBorder).BorderImage(&RowBrush).Padding(FMargin(10.0f, 5.0f, 6.0f, 5.0f))
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[ Text(FText::FromString(Name), Font("Bold", 10), NeocortexUI::Ink) ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
                [ Text(FText::FromString(Detail), Font("Regular", 9), NeocortexUI::Muted) ]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 1, 0, 0)
            [ Text(FText::FromString(Id), Font("Mono", 9), NeocortexUI::Muted) ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).MinDesiredWidth(76.0f)
            [
                SNew(SButton).ButtonStyle(&QuietButton).HAlign(HAlign_Center)
                .OnClicked_Lambda([this, Id]()
                {
                    FPlatformApplicationMisc::ClipboardCopy(*Id);
                    CopiedId = Id;
                    CopiedAt = FPlatformTime::Seconds();
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock).Font(Font("Bold", 9))
                    .Text_Lambda([this, Id]()
                    {
                        const bool bCopied = CopiedId == Id && FPlatformTime::Seconds() - CopiedAt < CopiedSeconds;
                        return bCopied ? LOCTEXT("Copied", "Copied") : LOCTEXT("Copy", "Copy id");
                    })
                    .ColorAndOpacity_Lambda([this, Id]()
                    {
                        const bool bCopied = CopiedId == Id && FPlatformTime::Seconds() - CopiedAt < CopiedSeconds;
                        return FSlateColor(bCopied ? NeocortexUI::Good : NeocortexUI::Ink);
                    })
                ]
            ]
        ]
    ];
}

void SNeocortexSettingsWindow::Rebuild()
{
    const FNeocortexEditorData& Data = FNeocortexEditorData::Get();
    AccountSlot->SetContent(MakeAccount());

    auto Fill = [this, &Data](const TSharedPtr<SVerticalBox>& List, const TArray<FNeocortexEditorEntry>& Entries, const FText& Empty)
    {
        List->ClearChildren();
        for (const FNeocortexEditorEntry& Entry : Entries)
        {
            List->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[ MakeRow(Entry.Name, Entry.Id, Entry.Detail) ];
        }
        if (Entries.Num() == 0)
        {
            const FText Message = Data.IsLoading() ? LOCTEXT("LoadingList", "Loading...") : Empty;
            List->AddSlot().AutoHeight()[ Text(Message, Font("Regular", 10), NeocortexUI::Muted) ];
        }
    };
    Fill(CharacterList, Data.Characters, LOCTEXT("NoCharacters", "No characters yet. Create one in your Neocortex dashboard."));
    Fill(QuestionSetList, Data.QuestionSets, LOCTEXT("NoSets", "No question sets yet. Create one under Question Sets in your dashboard."));
}

#undef LOCTEXT_NAMESPACE
