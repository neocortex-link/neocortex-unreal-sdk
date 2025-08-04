#include "UI/NeocortexTextChatInput.h"

void UNeocortexTextChatInput::NativeConstruct()
{
	Super::NativeConstruct();

	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UNeocortexTextChatInput::HandleSendClicked);
	}

	if (InputField)
	{
		InputField->OnTextCommitted.AddDynamic(this, &UNeocortexTextChatInput::HandleTextCommitted);
	}
}

void UNeocortexTextChatInput::NativeDestruct()
{
	Super::NativeDestruct();
	if (SendButton)
	{
		SendButton->OnClicked.RemoveDynamic(this, &UNeocortexTextChatInput::HandleSendClicked);
	}
	if (InputField) 
	{
		InputField->OnTextCommitted.RemoveDynamic(this, &UNeocortexTextChatInput::HandleTextCommitted);
	}
}

void UNeocortexTextChatInput::HandleSendClicked()
{
	if (!InputField) return;

	const FString Text = InputField->GetText().ToString();
	if (!Text.IsEmpty())
	{
		OnSendButtonClicked.Broadcast(Text);
		InputField->SetText(FText::GetEmpty());
	}
}

void UNeocortexTextChatInput::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleSendClicked();
	}
}
