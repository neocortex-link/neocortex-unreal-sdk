#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "NeocortexTextChatInput.generated.h"

/** Delegate broadcast when user submits a chat message. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextChatSend, const FString&, Message);

/**
 * Text chat input widget for sending messages to AI characters.
 * Provides a text field and send button, supporting both button clicks and Enter key submission.
 */
UCLASS()
class NEOCORTEX_API UNeocortexTextChatInput : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Editable text field for user input. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* InputField;

	/** Button to submit the chat message. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* SendButton;

	/** Event fired when user submits a message via button or Enter key. */
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTextChatSend OnSendText;

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	/** Handles send button click events. */
	UFUNCTION()
	void HandleSendClicked();

	/**
	 * Handles text field commit events (Enter key press).
	 * @param Text The submitted text content
	 * @param CommitMethod How the text was committed (e.g., OnEnter, OnUserMovedFocus)
	 */
	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
