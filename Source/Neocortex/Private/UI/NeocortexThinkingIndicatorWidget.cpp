#include "UI/NeocortexThinkingIndicatorWidget.h"

void UNeocortexThinkingIndicatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ShowAnimation)
	{
		ShowFinishedDelegate.BindDynamic(this, &UNeocortexThinkingIndicatorWidget::OnShowAnimationFinished);
		BindToAnimationFinished(ShowAnimation, ShowFinishedDelegate);
	}

	if (HideAnimation)
	{
		HideFinishedDelegate.BindDynamic(this, &UNeocortexThinkingIndicatorWidget::OnHideAnimationFinished);
		BindToAnimationFinished(HideAnimation, HideFinishedDelegate);
	}

	if (bStartHidden)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UNeocortexThinkingIndicatorWidget::NativeDestruct()
{
	Super::NativeDestruct();
	
	if (ShowAnimation) 
	{
		UnbindFromAnimationFinished(ShowAnimation, ShowFinishedDelegate);
	}
	if (HideAnimation)
	{
		UnbindFromAnimationFinished(HideAnimation, HideFinishedDelegate);
	}
}

void UNeocortexThinkingIndicatorWidget::PlayShow()
{
	if (ShowAnimation)
	{
		StopAllAnimations();
		PlayAnimation(ShowAnimation, 0.f, 1, EUMGSequencePlayMode::Forward);
		CurrentState = EThinkingIndicatorState::Show;
	}
}

void UNeocortexThinkingIndicatorWidget::OnShowAnimationFinished()
{
	PlayIdle();
}

void UNeocortexThinkingIndicatorWidget::PlayIdle()
{
	if (IdleAnimation)
	{
		StopAllAnimations();
		PlayAnimation(IdleAnimation, 0.f, 0); // loop forever
		CurrentState = EThinkingIndicatorState::Idle;
	}
}

void UNeocortexThinkingIndicatorWidget::PlayThinking()
{
	if (ThinkingAnimation)
	{
		StopAllAnimations();
		PlayAnimation(ThinkingAnimation, 0.f, 0); // loop forever
		CurrentState = EThinkingIndicatorState::Play;
	}
}

void UNeocortexThinkingIndicatorWidget::StopThinking()
{
	StopAllAnimations();
	PlayIdle();
}

void UNeocortexThinkingIndicatorWidget::PlayHide()
{
	if (HideAnimation)
	{
		StopAllAnimations();
		PlayAnimation(HideAnimation, 0.f, 1, EUMGSequencePlayMode::Forward);
		CurrentState = EThinkingIndicatorState::Hide;
	}
}

void UNeocortexThinkingIndicatorWidget::OnHideAnimationFinished()
{
	// Optional: hide the widget
	SetVisibility(ESlateVisibility::Hidden);
}