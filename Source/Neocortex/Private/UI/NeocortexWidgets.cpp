#include "UI/NeocortexWidgets.h"
#include "UI/NeocortexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"

using namespace NeocortexUI;

// Label

TSharedRef<SWidget> UNeocortexLabel::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        Block = MakeText(WidgetTree, Text, Size, Color, bBold);
        Block->SetAutoWrapText(bWrap);
        Block->SetJustification(Justification);
        WidgetTree->RootWidget = Block;
    }
    return Super::RebuildWidget();
}

void UNeocortexLabel::SetText(const FString& InText)
{
    Text = InText;
    if (Block) Block->SetText(FText::FromString(Text));
}

void UNeocortexLabel::SetColor(const FLinearColor& InColor)
{
    Color = InColor;
    if (Block) Block->SetColorAndOpacity(FSlateColor(Color));
}

// Pill button

TSharedRef<SWidget> UNeocortexPillButton::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        Button = MakeButton(WidgetTree, Label, FontSize);
        Button->OnClicked.AddDynamic(this, &UNeocortexPillButton::HandleClicked);
        Button->SetIsEnabled(bWantEnabled);
        WidgetTree->RootWidget = Button;
    }
    return Super::RebuildWidget();
}

void UNeocortexPillButton::SetLabel(const FString& InLabel)
{
    Label = InLabel;
    SetButtonLabel(Button, Label);
}

void UNeocortexPillButton::SetEnabled(bool bEnabled)
{
    bWantEnabled = bEnabled;
    if (Button) Button->SetIsEnabled(bEnabled);
}

void UNeocortexPillButton::HandleClicked()
{
    OnClicked.Broadcast();
}

// Toggle

TSharedRef<SWidget> UNeocortexToggle::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UWidget* Row = nullptr;
        Box = MakeToggle(WidgetTree, Label, Row);
        Box->SetIsChecked(bChecked);
        Box->SetIsEnabled(bWantEnabled);
        Box->OnCheckStateChanged.AddDynamic(this, &UNeocortexToggle::HandleChanged);
        if (UHorizontalBox* Line = Cast<UHorizontalBox>(Row)) Caption = Cast<UTextBlock>(Line->GetChildAt(0));
        WidgetTree->RootWidget = Row;
    }
    return Super::RebuildWidget();
}

void UNeocortexToggle::SetChecked(bool bInChecked)
{
    bChecked = bInChecked;
    if (Box && Box->IsChecked() != bChecked) Box->SetIsChecked(bChecked);
}

void UNeocortexToggle::SetEnabled(bool bEnabled)
{
    bWantEnabled = bEnabled;
    if (Box) Box->SetIsEnabled(bEnabled);
}

void UNeocortexToggle::SetLabel(const FString& InLabel)
{
    Label = InLabel;
    if (Caption) Caption->SetText(FText::FromString(Label));
}

void UNeocortexToggle::HandleChanged(bool bIsChecked)
{
    bChecked = bIsChecked;
    OnToggled.Broadcast(bIsChecked);
}

// Dropdown

TSharedRef<SWidget> UNeocortexDropdown::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) EnsureCombo();
    return Super::RebuildWidget();
}

void UNeocortexDropdown::EnsureCombo()
{
    if (Combo || !WidgetTree) return;
    Combo = WidgetTree->ConstructWidget<UComboBoxString>();
    StyleComboBox(Combo);
    if (Options.Num() == 0 && !Placeholder.IsEmpty()) Combo->AddOption(Placeholder);
    for (const FString& Option : Options) Combo->AddOption(Option);
    if (!Selected.IsEmpty()) Combo->SetSelectedOption(Selected);
    else if (Combo->GetOptionCount() > 0) Combo->SetSelectedIndex(0);
    Combo->OnSelectionChanged.AddDynamic(this, &UNeocortexDropdown::HandleSelection);
    WidgetTree->RootWidget = Combo;
}

void UNeocortexDropdown::ClearOptions()
{
    Options.Reset();
    Selected.Reset();
    if (Combo) Combo->ClearOptions();
}

void UNeocortexDropdown::AddOption(const FString& Option)
{
    // The designer placeholder gives way to real options.
    if (Combo && Options.Num() == 0 && Combo->GetOptionCount() > 0) Combo->ClearOptions();
    Options.Add(Option);
    if (Combo) Combo->AddOption(Option);
}

void UNeocortexDropdown::SetSelectedIndex(int32 Index)
{
    if (!Options.IsValidIndex(Index)) return;
    Selected = Options[Index];
    if (Combo) Combo->SetSelectedIndex(Index);
}

void UNeocortexDropdown::SetSelectedOption(const FString& Option)
{
    if (!Options.Contains(Option)) return;
    Selected = Option;
    if (Combo) Combo->SetSelectedOption(Option);
}

int32 UNeocortexDropdown::GetSelectedIndex() const
{
    return Options.IndexOfByKey(Selected);
}

FString UNeocortexDropdown::GetSelectedOption() const
{
    return Selected;
}

int32 UNeocortexDropdown::FindOptionIndex(const FString& Option) const
{
    return Options.IndexOfByKey(Option);
}

void UNeocortexDropdown::HandleSelection(FString Item, ESelectInfo::Type Selection)
{
    if (!Options.Contains(Item)) return;
    Selected = Item;
    OnPicked.Broadcast(Item, Selection != ESelectInfo::Direct);
}
