#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexThinkingIndicatorWidget.generated.h"

class USoundWave;
class UWidgetAnimation;

/** Visual states for the thinking indicator animation system. */
UENUM(BlueprintType)
enum class EThinkingIndicatorState : uint8
{
 Idle,
 Show,
 Play,
 Hide
};

/**
 * Animated thinking indicator widget for displaying AI processing status.
 * Manages state transitions between idle, show, thinking loop, and hide animations.
 */
UCLASS()
class NEOCORTEX_API UNeocortexThinkingIndicatorWidget : public UUserWidget
{
 GENERATED_BODY()

public:
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;

 /** Whether the widget should be hidden when first constructed. */
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
 bool bStartHidden = false;

 /** Plays the show/fade-in animation. */
 UFUNCTION(BlueprintCallable, Category = "Thinking Indicator")
 void PlayShow();

 /** Plays the idle animation. */
 UFUNCTION(BlueprintCallable, Category = "Thinking Indicator")
 void PlayIdle();

 /** Plays the thinking loop animation. */
 UFUNCTION(BlueprintCallable, Category = "Thinking Indicator")
 void PlayThinking();

 /** Stops the thinking loop animation. */
 UFUNCTION(BlueprintCallable, Category = "Thinking Indicator")
 void StopThinking();

 /** Plays the hide/fade-out animation. */
 UFUNCTION(BlueprintCallable, Category = "Thinking Indicator")
 void PlayHide();

protected:
 /** Animation played when widget is idle. */
 UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
 UWidgetAnimation* IdleAnimation;

 /** Animation played when widget is shown/fading in. */
 UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
 UWidgetAnimation* ShowAnimation;

 /** Animation played when widget is hidden/fading out. */
 UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
 UWidgetAnimation* HideAnimation;

 /** Animation played in a loop while AI is thinking/processing. */
 UPROPERTY(Transient, meta = (BindWidgetAnim), BlueprintReadOnly)
 UWidgetAnimation* ThinkingAnimation;

 /** Callback triggered when show animation completes. */
 UFUNCTION()
 void OnShowAnimationFinished();

 /** Callback triggered when hide animation completes. */
 UFUNCTION()
 void OnHideAnimationFinished();

private:
 /** Current state of the thinking indicator. */
 EThinkingIndicatorState CurrentState = EThinkingIndicatorState::Idle;

 /** Delegate bound to show animation completion event. */
 FWidgetAnimationDynamicEvent ShowFinishedDelegate;

 /** Delegate bound to hide animation completion event. */
 FWidgetAnimationDynamicEvent HideFinishedDelegate;
};
