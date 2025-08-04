#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "NeocortexTextChatInput.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTextChatSend, const FString&, Message);

/**
 * Neocortex-style text chat input widget (Unreal equivalent of Unity version)
 */
UCLASS()
class Neocortex_API UNeocortexTextChatInput : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* InputField;

	UPROPERTY(meta = (BindWidget))
	UButton* SendButton;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnTextChatSend OnSendButtonClicked;

protected:
	virtual void NativeConstruct() override;

	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleSendClicked();

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
