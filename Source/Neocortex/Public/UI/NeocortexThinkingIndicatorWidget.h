#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexThinkingIndicatorWidget.generated.h"

class USoundWave;
class UWidgetAnimation;

UENUM(BlueprintType)
enum class EThinkingIndicatorState : uint8
{
	Idle,
	Show,
	Play,
	Hide
};

UCLASS()
class Neocortex_API UNeocortexThinkingIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	bool bStartHidden = false;

	// Triggers
	UFUNCTION(BlueprintCallable)
	void PlayShow();

	UFUNCTION(BlueprintCallable)
	void PlayIdle();

	UFUNCTION(BlueprintCallable)
	void PlayThinking();

	UFUNCTION(BlueprintCallable)
	void StopThinking();

	UFUNCTION(BlueprintCallable)
	void PlayHide();

protected:
	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	UWidgetAnimation* IdleAnimation;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	UWidgetAnimation* ShowAnimation;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	UWidgetAnimation* HideAnimation;

	UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
	UWidgetAnimation* ThinkingAnimation;

	UFUNCTION()
	void OnShowAnimationFinished();
	UFUNCTION()
	void OnHideAnimationFinished();

private:
	EThinkingIndicatorState CurrentState = EThinkingIndicatorState::Idle;

	FWidgetAnimationDynamicEvent ShowFinishedDelegate;
	FWidgetAnimationDynamicEvent HideFinishedDelegate;
};
