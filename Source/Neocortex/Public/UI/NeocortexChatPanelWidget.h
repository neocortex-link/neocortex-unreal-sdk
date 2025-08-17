#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Enums/EnumTypes.h"
#include "NeocortexChatPanelWidget.generated.h"

class UNeocortexChatMessageWidget;

UCLASS()
class NEOCORTEX_API UNeocortexChatPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void AddMessage(const FString& Text, bool bIsUser);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
	EWritingDirection WritingDirection = EWritingDirection::LeftToRight;

protected:
	UPROPERTY(meta = (BindWidget))
	class UScrollBox* ChatScrollBox;

	UPROPERTY(EditAnywhere, Category = "Chat")
	TSubclassOf<UNeocortexChatMessageWidget> MessageItemClass;

	UPROPERTY(EditAnywhere, Category = "Chat")
	TSubclassOf<UNeocortexChatMessageWidget> WritingIndicatorClass;

	UPROPERTY()
	UNeocortexChatMessageWidget* WritingIndicator;

	virtual void NativeConstruct() override;

private:
	void ScrollToBottom();
};
