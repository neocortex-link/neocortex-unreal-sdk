#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NeocortexWidgets.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNeocortexClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNeocortexToggled, bool, bIsChecked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNeocortexPicked, const FString&, Item, bool, bByPlayer);

/**
 * The Neocortex look as ready-made widgets, built in code so they look the same in the UMG
 * designer, in game and in a packaged build: drop them into any Widget Blueprint and set their
 * text and size in the Details panel.
 */

/** Text in the Neocortex style: Arial at a pixel size. */
UCLASS()
class NEOCORTEX_API UNeocortexLabel : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI", meta = (MultiLine = "true"))
    FString Text;

    /** Pixels at 1920 x 1080. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    int32 Size = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    bool bBold = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Left;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    bool bWrap = true;

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetText(const FString& InText);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetColor(const FLinearColor& InColor);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Block;
};

/** A pill button: a dark pill with an amber outline and a bold amber label. */
UCLASS()
class NEOCORTEX_API UNeocortexPillButton : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FString Label = TEXT("Button");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    int32 FontSize = 24;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|UI")
    FOnNeocortexClicked OnClicked;

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetLabel(const FString& InLabel);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetEnabled(bool bEnabled);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY(Transient) TObjectPtr<UButton> Button;
    bool bWantEnabled = true;
    UFUNCTION() void HandleClicked();
};

/** A toggle: a bold amber label, then an amber outlined box. */
UCLASS()
class NEOCORTEX_API UNeocortexToggle : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FString Label = TEXT("Toggle");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    bool bChecked = false;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex|UI")
    FOnNeocortexToggled OnToggled;

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetChecked(bool bInChecked);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetLabel(const FString& InLabel);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY(Transient) TObjectPtr<UCheckBox> Box;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Caption;
    bool bWantEnabled = true;
    UFUNCTION() void HandleChanged(bool bIsChecked);
};

/** A pill dropdown with an amber arrow and an amber list. */
UCLASS()
class NEOCORTEX_API UNeocortexDropdown : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Shown in the designer until options are added. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex|UI")
    FString Placeholder;

    /** Fired when the selection changes; bByPlayer is false when code set it. */
    UPROPERTY(BlueprintAssignable, Category = "Neocortex|UI")
    FOnNeocortexPicked OnPicked;

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void ClearOptions();

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void AddOption(const FString& Option);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetSelectedIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "Neocortex|UI")
    void SetSelectedOption(const FString& Option);

    UFUNCTION(BlueprintPure, Category = "Neocortex|UI")
    int32 GetSelectedIndex() const;

    UFUNCTION(BlueprintPure, Category = "Neocortex|UI")
    FString GetSelectedOption() const;

    UFUNCTION(BlueprintPure, Category = "Neocortex|UI")
    int32 FindOptionIndex(const FString& Option) const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> Combo;
    TArray<FString> Options;
    FString Selected;
    void EnsureCombo();
    UFUNCTION() void HandleSelection(FString Item, ESelectInfo::Type Selection);
};
