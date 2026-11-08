#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"

class UWidgetTree;
class UTextBlock;
class UBorder;
class UButton;
class UImage;
class UEditableTextBox;
class UTexture2D;
class UCheckBox;
class UComboBoxString;
class URichTextBlock;
class UDataTable;
class UWidget;

/**
 * The look shared by every Neocortex widget and sample: dark pills with amber outlines, amber
 * accents and bold Arial, laid out on a 1920 x 1080 screen. Sizes are pixels at that resolution;
 * Unreal scales them with the screen. Icons are the images in the plugin's Extras/UI folder,
 * packaged with every game that uses the plugin.
 */
namespace NeocortexUI
{
    NEOCORTEX_API extern const FLinearColor Amber;
    /** Widget rims, toggle labels and dropdown text. */
    NEOCORTEX_API extern const FLinearColor AmberRim;
    /** The inner disc of the round voice widgets. */
    NEOCORTEX_API extern const FLinearColor AmberDisc;
    /** The live microphone level. */
    NEOCORTEX_API extern const FLinearColor AmberLevel;
    /** The silence countdown ring. */
    NEOCORTEX_API extern const FLinearColor AmberWait;
    /** The text field's outline. */
    NEOCORTEX_API extern const FLinearColor AmberField;
    NEOCORTEX_API extern const FLinearColor Surface;
    NEOCORTEX_API extern const FLinearColor SurfaceDark;
    NEOCORTEX_API extern const FLinearColor SurfaceHover;
    /** Dark cards: the brief over a floor, the usage log, the question card. */
    NEOCORTEX_API extern const FLinearColor CardFill;
    /** Text on dark surfaces. */
    NEOCORTEX_API extern const FLinearColor Ink;
    /** Body text, such as the demo brief. */
    NEOCORTEX_API extern const FLinearColor BodyText;
    /** Icons on amber. */
    NEOCORTEX_API extern const FLinearColor IconInk;
    NEOCORTEX_API extern const FLinearColor Placeholder;
    NEOCORTEX_API extern const FLinearColor Corner;
    NEOCORTEX_API extern const FLinearColor Muted;
    NEOCORTEX_API extern const FLinearColor ErrorColor;
    NEOCORTEX_API extern const FLinearColor Good;
    NEOCORTEX_API extern const FLinearColor Half;

    /** Arial (or the system's closest sans serif) at a pixel size. */
    NEOCORTEX_API FSlateFontInfo Font(int32 PixelSize, bool bBold = false, bool bItalic = false);
    /** A rounded rectangle, optionally outlined. A radius of half the height makes a pill. */
    NEOCORTEX_API FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Border = FLinearColor::Transparent, float BorderWidth = 0.0f);
    /** A brush that draws a texture at a size, tinted. */
    NEOCORTEX_API FSlateBrush Image(UTexture2D* Texture, const FVector2D& Size, const FLinearColor& Tint = FLinearColor::White);
    /** Loads one of the images in the plugin's Extras/UI folder by name (no extension); null when it is not there. */
    NEOCORTEX_API UTexture2D* Icon(const TCHAR* Name);

    NEOCORTEX_API UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 PixelSize, const FLinearColor& Color, bool bBold = false);
    NEOCORTEX_API UBorder* MakeBox(UWidgetTree* Tree, const FLinearColor& Color, float Radius, const FMargin& Padding);

    /** The look of a pill button: a dark pill with an amber outline. */
    NEOCORTEX_API FButtonStyle PillButtonStyle();
    /** A pill button with a bold amber label. */
    NEOCORTEX_API UButton* MakeButton(UWidgetTree* Tree, const FString& Label, int32 FontSize = 24);
    /** Replaces the label of a button made with MakeButton. */
    NEOCORTEX_API void SetButtonLabel(UButton* Button, const FString& Label);

    /** A round amber button with a dark icon, or a short label when the icon is missing. */
    NEOCORTEX_API UButton* MakeRoundButton(UWidgetTree* Tree, const TCHAR* IconName, const FString& FallbackLabel, float Size = 64.0f, float IconSize = 40.0f);

    /** A pill text field with an amber outline and a grey hint. */
    NEOCORTEX_API UEditableTextBox* MakeInput(UWidgetTree* Tree, const FString& Hint);

    /** A toggle: a bold amber label, then an amber outlined box. Returns the box; Row is the whole toggle. */
    NEOCORTEX_API UCheckBox* MakeToggle(UWidgetTree* Tree, const FString& Label, UWidget*& Row);

    /** Gives a dropdown the pill look, with an amber arrow and an amber list. */
    NEOCORTEX_API void StyleComboBox(UComboBoxString* Combo);

    /**
     * Text with inline styles: <b>bold</b>, <n>an amber step number</n>, <h>an amber heading</h>.
     * The base style is Size pixels in Color.
     */
    NEOCORTEX_API URichTextBlock* MakeRichText(UWidgetTree* Tree, const FString& Markup, int32 PixelSize, const FLinearColor& Color);
}
