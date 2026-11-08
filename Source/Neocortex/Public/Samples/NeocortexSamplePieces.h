#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/NeocortexWidgets.h"
#include "NeocortexSamplePieces.generated.h"

class UScrollBox;

/**
 * Sample only: the pieces the sample screens are made of. Each draws itself in code, so a sample's
 * Widget Blueprint holds just the pieces, their text and their place on the screen.
 */

/** The frame of every sample screen: a dotted backdrop, amber corner brackets and the logo over the subtitle. */
UCLASS()
class NEOCORTEX_API UNeocortexSampleChrome : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Under "NEOCORTEX", for example "Unreal SDK · Text Chat Demo". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FString Subtitle;

    /** Black behind the dots, for screens without a scene behind them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    bool bOpaque = false;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** The demo brief: an amber "DEMO BRIEF" over a thin rule, a bold lead and numbered steps. */
UCLASS()
class NEOCORTEX_API UNeocortexSampleBrief : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample", meta = (MultiLine = "true"))
    FString Lead;

    /** One line per step; <b>bold</b> works inside. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample", meta = (MultiLine = "true"))
    TArray<FString> Steps;

    /** A dark card behind the brief, for scenes with a floor behind it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    bool bCard = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    int32 FontSize = 24;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};

/** A dark card with an amber heading and a scrolling list of lines, such as the usage log. */
UCLASS()
class NEOCORTEX_API UNeocortexSampleLogCard : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FString Heading = TEXT("LOG");

    UFUNCTION(BlueprintCallable, Category = "Neocortex Sample")
    void AddLine(const FString& Line, const FLinearColor& Color);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UPROPERTY(Transient) TObjectPtr<UScrollBox> Lines;
};

/** A sample card on the launcher: the number and title over a short description. The whole card is a button. */
UCLASS()
class NEOCORTEX_API UNeocortexSampleCard : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    int32 Number = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample", meta = (MultiLine = "true"))
    FString Description;

    /** The map the card opens. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neocortex Sample")
    FString Map;

    UPROPERTY(BlueprintAssignable, Category = "Neocortex Sample")
    FOnNeocortexClicked OnClicked;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    UFUNCTION() void HandleClicked();
};
