#include "UI/NeocortexUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformMisc.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace NeocortexUI
{
    const FLinearColor Amber(FColor(0xF5, 0x9E, 0x0B));
    const FLinearColor AmberRim(FColor(0xF1, 0x9B, 0x0B));
    const FLinearColor AmberDisc(FColor(0xC5, 0x7D, 0x00));
    const FLinearColor AmberLevel(FColor(0xB9, 0x46, 0x20));
    const FLinearColor AmberWait(FColor(0xA4, 0x41, 0x17));
    const FLinearColor AmberField(FColor(0xF3, 0x8B, 0x01));
    const FLinearColor Surface(FColor(0x20, 0x20, 0x20));
    const FLinearColor SurfaceDark(FColor(0x1F, 0x1F, 0x1F));
    const FLinearColor SurfaceHover(FColor(0x2A, 0x2A, 0x2A));
    const FLinearColor CardFill(0.0f, 0.0f, 0.0f, 0.91f);
    const FLinearColor Ink(FColor(0xDB, 0xDB, 0xDB));
    const FLinearColor BodyText(FColor(0xC8, 0xC8, 0xC8));
    const FLinearColor IconInk(FColor(0x28, 0x28, 0x28));
    const FLinearColor Placeholder(FLinearColor(FColor(0x8C, 0x8C, 0x8C)) * FLinearColor(1, 1, 1, 0.5f));
    const FLinearColor Corner(FColor(0xDA, 0xA5, 0x20));
    const FLinearColor Muted(FColor(0x8C, 0x8C, 0x8C));
    const FLinearColor ErrorColor(FColor(0xEE, 0x6B, 0x6B));
    const FLinearColor Good(FColor(0x5C, 0xC9, 0x8F));
    const FLinearColor Half(FColor(0xF0, 0xC4, 0x5C));

    // Arial from the system when it is there, as fonts cannot be shipped with the plugin; the
    // engine's own sans serif otherwise.
    static FString FindArial(bool bBold, bool bItalic)
    {
        TArray<FString> Candidates;
        const TCHAR* Windows[] = { TEXT("arial.ttf"), TEXT("arialbd.ttf"), TEXT("ariali.ttf"), TEXT("arialbi.ttf") };
        const TCHAR* Mac[] = { TEXT("Arial.ttf"), TEXT("Arial Bold.ttf"), TEXT("Arial Italic.ttf"), TEXT("Arial Bold Italic.ttf") };
        const TCHAR* Liberation[] = { TEXT("LiberationSans-Regular.ttf"), TEXT("LiberationSans-Bold.ttf"), TEXT("LiberationSans-Italic.ttf"), TEXT("LiberationSans-BoldItalic.ttf") };
        const int32 Index = (bBold ? 1 : 0) + (bItalic ? 2 : 0);
        const FString WinDir = FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR"));
        if (!WinDir.IsEmpty()) Candidates.Add(FPaths::Combine(WinDir, TEXT("Fonts"), Windows[Index]));
        Candidates.Add(FPaths::Combine(TEXT("/System/Library/Fonts/Supplemental"), Mac[Index]));
        Candidates.Add(FPaths::Combine(TEXT("/Library/Fonts"), Mac[Index]));
        Candidates.Add(FPaths::Combine(TEXT("/usr/share/fonts/truetype/liberation"), Liberation[Index]));
        Candidates.Add(FPaths::Combine(TEXT("/usr/share/fonts/truetype/liberation2"), Liberation[Index]));
        for (const FString& Path : Candidates)
        {
            if (FPaths::FileExists(Path)) return Path;
        }
        return FString();
    }

    FSlateFontInfo Font(int32 PixelSize, bool bBold, bool bItalic)
    {
        static TMap<int32, FString> Files;
        const int32 Key = (bBold ? 1 : 0) + (bItalic ? 2 : 0);
        if (!Files.Contains(Key)) Files.Add(Key, FindArial(bBold, bItalic));
        // Slate sizes are points at 96 DPI; three quarters of a pixel size matches it.
        const int32 Size = FMath::Max(1, FMath::RoundToInt(PixelSize * 0.75f));
        const FString& File = Files[Key];
        if (!File.IsEmpty())
        {
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            return FSlateFontInfo(File, Size);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
        return FCoreStyle::GetDefaultFontStyle(bBold ? (bItalic ? "BoldItalic" : "Bold") : (bItalic ? "Italic" : "Regular"), Size);
    }

    FSlateBrush Rounded(const FLinearColor& Fill, float Radius, const FLinearColor& Border, float BorderWidth)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(Fill);
        Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Brush.OutlineSettings.Color = FSlateColor(Border);
        Brush.OutlineSettings.Width = BorderWidth;
        return Brush;
    }

    FSlateBrush Image(UTexture2D* Texture, const FVector2D& Size, const FLinearColor& Tint)
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(Texture);
        Brush.ImageSize = Size;
        Brush.TintColor = FSlateColor(Tint);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        return Brush;
    }

    // The images in the plugin's Extras/UI folder, loaded once and kept for the session.
    UTexture2D* Icon(const TCHAR* Name)
    {
        static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
        if (UTexture2D* Cached = Cache.FindRef(Name).Get()) return Cached;

        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Neocortex"));
        if (!Plugin.IsValid()) return nullptr;
        TArray<uint8> Png;
        const FString File = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Extras"), TEXT("UI"), FString(Name) + TEXT(".png"));
        if (!FFileHelper::LoadFileToArray(Png, *File, FILEREAD_Silent)) return nullptr;

        UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(Png);
        if (!Texture) return nullptr;
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->AddToRoot();
        Cache.Add(Name, Texture);
        return Texture;
    }

    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 PixelSize, const FLinearColor& Color, bool bBold)
    {
        UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
        Block->SetText(FText::FromString(Text));
        Block->SetFont(Font(PixelSize, bBold));
        Block->SetColorAndOpacity(FSlateColor(Color));
        Block->SetAutoWrapText(true);
        return Block;
    }

    UBorder* MakeBox(UWidgetTree* Tree, const FLinearColor& Color, float Radius, const FMargin& Padding)
    {
        UBorder* Box = Tree->ConstructWidget<UBorder>();
        Box->SetBrush(Rounded(Color, Radius));
        Box->SetPadding(Padding);
        return Box;
    }

    FButtonStyle PillButtonStyle()
    {
        FButtonStyle Style;
        const float Radius = 24.0f;
        Style.SetNormal(Rounded(FLinearColor(FColor(0x21, 0x21, 0x21)), Radius, Amber * FLinearColor(1, 1, 1, 0.5f), 2.0f));
        Style.SetHovered(Rounded(SurfaceHover, Radius, Amber * FLinearColor(1, 1, 1, 0.8f), 2.0f));
        Style.SetPressed(Rounded(FLinearColor(FColor(0x17, 0x17, 0x17)), Radius, Amber, 2.0f));
        Style.SetDisabled(Rounded(FLinearColor(FColor(0x1A, 0x1A, 0x1A)), Radius, Amber * FLinearColor(1, 1, 1, 0.2f), 2.0f));
        Style.SetNormalPadding(FMargin(24.0f, 8.0f));
        Style.SetPressedPadding(FMargin(24.0f, 8.0f));
        return Style;
    }

    UButton* MakeButton(UWidgetTree* Tree, const FString& Label, int32 FontSize)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        UTextBlock* Text = MakeText(Tree, Label, FontSize, Amber, true);
        Text->SetAutoWrapText(false);
        Text->SetJustification(ETextJustify::Center);
        Button->AddChild(Text);
        Button->SetStyle(PillButtonStyle());
        return Button;
    }

    void SetButtonLabel(UButton* Button, const FString& Label)
    {
        if (UTextBlock* Text = Button ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr)
        {
            Text->SetText(FText::FromString(Label));
        }
    }

    UButton* MakeRoundButton(UWidgetTree* Tree, const TCHAR* IconName, const FString& FallbackLabel, float Size, float IconSize)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        FButtonStyle Style;
        const float Radius = Size * 0.5f;
        Style.SetNormal(Rounded(Amber, Radius));
        Style.SetHovered(Rounded(Amber * 1.12f, Radius));
        Style.SetPressed(Rounded(Amber * 0.8f, Radius));
        Style.SetDisabled(Rounded(Amber * FLinearColor(1, 1, 1, 0.4f), Radius));
        const float Pad = (Size - IconSize) * 0.5f;
        Style.SetNormalPadding(FMargin(Pad));
        Style.SetPressedPadding(FMargin(Pad));
        Button->SetStyle(Style);

        if (UTexture2D* Texture = Icon(IconName))
        {
            UImage* Glyph = Tree->ConstructWidget<UImage>();
            const float Aspect = Texture->GetSizeY() > 0 ? float(Texture->GetSizeX()) / Texture->GetSizeY() : 1.0f;
            Glyph->SetBrush(Image(Texture, FVector2D(IconSize * FMath::Min(Aspect, 1.0f), IconSize / FMath::Max(Aspect, 1.0f)), IconInk));
            Button->AddChild(Glyph);
        }
        else
        {
            UTextBlock* Text = MakeText(Tree, FallbackLabel, 18, IconInk, true);
            Text->SetAutoWrapText(false);
            Button->AddChild(Text);
        }
        return Button;
    }

    UEditableTextBox* MakeInput(UWidgetTree* Tree, const FString& Hint)
    {
        UEditableTextBox* Input = Tree->ConstructWidget<UEditableTextBox>();
        Input->SetHintText(FText::FromString(Hint));
        FEditableTextBoxStyle Style = Input->WidgetStyle;
        const float Radius = 30.0f;
        Style.SetBackgroundImageNormal(Rounded(Surface, Radius, AmberField, 2.0f));
        Style.SetBackgroundImageHovered(Rounded(Surface, Radius, AmberField, 2.0f));
        Style.SetBackgroundImageFocused(Rounded(Surface, Radius, Amber * 1.25f, 2.0f));
        Style.SetBackgroundImageReadOnly(Rounded(Surface, Radius, AmberField * FLinearColor(1, 1, 1, 0.4f), 2.0f));
        Style.SetFont(Font(24));
        Style.SetForegroundColor(FSlateColor(Ink));
        Style.SetFocusedForegroundColor(FSlateColor(Ink));
        Style.SetReadOnlyForegroundColor(FSlateColor(Placeholder));
        Style.SetPadding(FMargin(22.0f, 12.0f));
        Input->WidgetStyle = Style;
        return Input;
    }

    // The styles are set before the widgets are built, which newer engines still read from these
    // properties; the warnings about writing them directly do not apply here.
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    UCheckBox* MakeToggle(UWidgetTree* Tree, const FString& Label, UWidget*& Row)
    {
        UHorizontalBox* Box = Tree->ConstructWidget<UHorizontalBox>();
        UTextBlock* Text = MakeText(Tree, Label, 24, AmberRim, true);
        Text->SetAutoWrapText(false);
        UHorizontalBoxSlot* TextSlot = Box->AddChildToHorizontalBox(Text);
        TextSlot->SetVerticalAlignment(VAlign_Center);
        TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TextSlot->SetHorizontalAlignment(HAlign_Right);

        UCheckBox* Check = Tree->ConstructWidget<UCheckBox>();
        FCheckBoxStyle Style = Check->WidgetStyle;
        UTexture2D* OffTexture = Icon(TEXT("toggle_off"));
        UTexture2D* OnTexture = Icon(TEXT("toggle_on"));
        FSlateBrush Off = OffTexture ? Image(OffTexture, FVector2D(48.0f, 48.0f)) : Rounded(SurfaceDark, 8.0f, AmberRim, 2.0f);
        FSlateBrush On = OnTexture ? Image(OnTexture, FVector2D(48.0f, 48.0f)) : Rounded(AmberRim, 8.0f);
        Off.ImageSize = On.ImageSize = FVector2D(48.0f, 48.0f);
        Style.SetUncheckedImage(Off);
        Style.SetUncheckedHoveredImage(Off);
        Style.SetUncheckedPressedImage(Off);
        Style.SetCheckedImage(On);
        Style.SetCheckedHoveredImage(On);
        Style.SetCheckedPressedImage(On);
        Style.SetPadding(FMargin(0.0f));
        Check->WidgetStyle = Style;
        UHorizontalBoxSlot* CheckSlot = Box->AddChildToHorizontalBox(Check);
        CheckSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
        CheckSlot->SetVerticalAlignment(VAlign_Center);
        Row = Box;
        return Check;
    }

    void StyleComboBox(UComboBoxString* Combo)
    {
        FComboBoxStyle Style = Combo->WidgetStyle;
        const float Radius = 24.0f;
        FButtonStyle Button;
        Button.SetNormal(Rounded(Surface, Radius, AmberRim, 2.0f));
        Button.SetHovered(Rounded(SurfaceHover, Radius, AmberRim, 2.0f));
        Button.SetPressed(Rounded(FLinearColor(FColor(0x17, 0x17, 0x17)), Radius, AmberRim, 2.0f));
        Button.SetDisabled(Rounded(Surface, Radius, AmberRim * FLinearColor(1, 1, 1, 0.3f), 2.0f));
        Button.SetNormalPadding(FMargin(24.0f, 8.0f, 20.0f, 8.0f));
        Button.SetPressedPadding(FMargin(24.0f, 8.0f, 20.0f, 8.0f));
        Style.ComboButtonStyle.ButtonStyle = Button;
        Style.ComboButtonStyle.MenuBorderBrush = Rounded(Surface, 16.0f, AmberRim, 2.0f);
        Style.ComboButtonStyle.MenuBorderPadding = FMargin(8.0f);
        if (UTexture2D* Arrow = Icon(TEXT("dropdown_arrow")))
        {
            Style.ComboButtonStyle.DownArrowImage = Image(Arrow, FVector2D(24.0f, 24.0f), FLinearColor::White);
        }
        Combo->WidgetStyle = Style;

        FTableRowStyle Rows = Combo->ItemStyle;
        // The text stays amber in every state, so rows only change their background: lighter with
        // an amber edge under the pointer, a soft amber tint for the chosen one.
        const FSlateBrush Plain = Rounded(Surface, 12.0f);
        const FSlateBrush Hovered = Rounded(SurfaceHover, 12.0f, AmberRim, 1.5f);
        const FSlateBrush Chosen = Rounded(AmberRim * FLinearColor(1, 1, 1, 0.22f), 12.0f);
        const FSlateBrush ChosenHovered = Rounded(AmberRim * FLinearColor(1, 1, 1, 0.32f), 12.0f, AmberRim, 1.5f);
        Rows.SetEvenRowBackgroundBrush(Plain);
        Rows.SetOddRowBackgroundBrush(Plain);
        Rows.SetEvenRowBackgroundHoveredBrush(Hovered);
        Rows.SetOddRowBackgroundHoveredBrush(Hovered);
        Rows.SetActiveBrush(Chosen);
        Rows.SetActiveHoveredBrush(ChosenHovered);
        Rows.SetInactiveBrush(Chosen);
        Rows.SetInactiveHoveredBrush(ChosenHovered);
        Rows.SetTextColor(FSlateColor(AmberRim));
        Rows.SetSelectedTextColor(FSlateColor(AmberRim));
        Combo->ItemStyle = Rows;

        Combo->Font = Font(24, true);
        Combo->ForegroundColor = AmberRim;
        Combo->ContentPadding = FMargin(0.0f, 4.0f);
    }
    PRAGMA_ENABLE_DEPRECATION_WARNINGS

    // One style set per base size and colour, built once and kept for the session.
    static UDataTable* RichStyles(int32 PixelSize, const FLinearColor& Color)
    {
        static TMap<FString, TWeakObjectPtr<UDataTable>> Cache;
        const FString Key = FString::Printf(TEXT("%d_%s"), PixelSize, *Color.ToString());
        if (UDataTable* Cached = Cache.FindRef(Key).Get()) return Cached;

        UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
        Table->RowStruct = FRichTextStyleRow::StaticStruct();
        auto Add = [Table](const TCHAR* Name, const FSlateFontInfo& InFont, const FLinearColor& InColor)
        {
            FRichTextStyleRow Row;
            Row.TextStyle.SetFont(InFont);
            Row.TextStyle.SetColorAndOpacity(FSlateColor(InColor));
            Table->AddRow(FName(Name), Row);
        };
        Add(TEXT("Default"), Font(PixelSize), Color);
        Add(TEXT("b"), Font(PixelSize, true), Color);
        Add(TEXT("n"), Font(PixelSize, true), Amber);
        Add(TEXT("h"), Font(PixelSize, true), Amber);
        Table->AddToRoot();
        Cache.Add(Key, Table);
        return Table;
    }

    URichTextBlock* MakeRichText(UWidgetTree* Tree, const FString& Markup, int32 PixelSize, const FLinearColor& Color)
    {
        URichTextBlock* Text = Tree->ConstructWidget<URichTextBlock>();
        // Airy lines, like the briefs everywhere else; the setting is not exposed in code.
        if (FFloatProperty* LineHeight = FindFProperty<FFloatProperty>(URichTextBlock::StaticClass(), TEXT("LineHeightPercentage")))
        {
            LineHeight->SetPropertyValue_InContainer(Text, 1.3f);
        }
        Text->SetTextStyleSet(RichStyles(PixelSize, Color));
        Text->SetAutoWrapText(true);
        // Unreal closes every style with "</>"; accept the familiar "</b>" too.
        FString Unreal = Markup;
        for (const TCHAR* Tag : { TEXT("</b>"), TEXT("</n>"), TEXT("</h>") }) Unreal.ReplaceInline(Tag, TEXT("</>"));
        Text->SetText(FText::FromString(Unreal));
        return Text;
    }
}
