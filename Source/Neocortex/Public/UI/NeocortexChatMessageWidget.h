#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "NeocortexChatMessageWidget.generated.h"

UCLASS()
class NEOCORTEX_API UNeocortexChatMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetMessage(const FString& Text, bool bIsUser, bool bIsLTR = false);

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MessageText;

	UPROPERTY(meta = (BindWidget))
	UBorder* Background;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* LayoutBox;

	UPROPERTY(meta = (BindWidget))
	UWidget* LeftPadding;

	UPROPERTY(meta = (BindWidget))
	UWidget* RightPadding;

	virtual void NativeConstruct() override;

private:
	const FLinearColor UserMessageColor = FLinearColor(0.0f, 0.5f, 0.9f, 1.0f);
	const FLinearColor AgentMessageColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

	FString CorrectRTL(const FString& Input) const;
};
