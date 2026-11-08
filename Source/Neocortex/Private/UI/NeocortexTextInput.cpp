#include "UI/NeocortexTextInput.h"
#include "UI/NeocortexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"

using namespace NeocortexUI;

TSharedRef<SWidget> UNeocortexTextInput::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        WidgetTree->RootWidget = Row;

        Field = MakeInput(WidgetTree, Hint);
        Field->OnTextCommitted.AddDynamic(this, &UNeocortexTextInput::HandleCommitted);
        UHorizontalBoxSlot* FieldSlot = Row->AddChildToHorizontalBox(Field);
        FieldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        FieldSlot->SetVerticalAlignment(VAlign_Fill);

        USizeBox* SendSize = WidgetTree->ConstructWidget<USizeBox>();
        SendSize->SetWidthOverride(64.0f);
        SendSize->SetHeightOverride(64.0f);
        SendButton = MakeRoundButton(WidgetTree, TEXT("send_icon"), TEXT("Send"), 64.0f, 40.0f);
        SendButton->OnClicked.AddDynamic(this, &UNeocortexTextInput::HandleSend);
        SendSize->SetContent(SendButton);
        UHorizontalBoxSlot* SendSlot = Row->AddChildToHorizontalBox(SendSize);
        SendSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
        SendSlot->SetVerticalAlignment(VAlign_Center);
        SetInputEnabled(bEnabled);
    }
    return Super::RebuildWidget();
}

void UNeocortexTextInput::SetHint(const FString& InHint)
{
    Hint = InHint;
    if (Field) Field->SetHintText(FText::FromString(Hint));
}

void UNeocortexTextInput::SetInputEnabled(bool bInEnabled)
{
    bEnabled = bInEnabled;
    if (Field) Field->SetIsReadOnly(!bEnabled);
    if (SendButton) SendButton->SetIsEnabled(bEnabled);
}

void UNeocortexTextInput::HandleSend()
{
    if (!Field || !bEnabled) return;
    const FString Text = Field->GetText().ToString().TrimStartAndEnd();
    if (Text.IsEmpty()) return;
    Field->SetText(FText::GetEmpty());
    OnSubmitted.Broadcast(Text);
}

void UNeocortexTextInput::HandleCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (Method != ETextCommit::OnEnter) return;
    HandleSend();
    // Keep typing without clicking the field again.
    if (Field) Field->SetKeyboardFocus();
}
