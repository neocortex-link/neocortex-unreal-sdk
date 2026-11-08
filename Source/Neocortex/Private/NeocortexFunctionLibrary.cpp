#include "NeocortexFunctionLibrary.h"
#include "Enums/NeocortexEnumTypes.h"

// Code points that carry no speech: punctuation, symbols, emoji and their joiners and modifiers.
static bool IsSilentSymbol(uint32 C)
{
    return (C >= 0x2000 && C <= 0x2BFF)      // punctuation, arrows, math, technical, dingbats
        || (C >= 0x2E00 && C <= 0x2E7F)      // supplemental punctuation
        || (C >= 0x3000 && C <= 0x303F)      // CJK punctuation
        || (C >= 0xE000 && C <= 0xF8FF)      // private use
        || (C >= 0xFE00 && C <= 0xFE0F)      // variation selectors
        || (C >= 0xFE10 && C <= 0xFE6F)      // vertical, combining half and small forms
        || (C >= 0x1F000 && C <= 0x1FAFF)    // emoji and pictographs
        || (C >= 0xE0000 && C <= 0xE007F);   // tag characters
}

bool UNeocortexFunctionLibrary::HasSomethingToSay(const FString& Text)
{
    const int32 Len = Text.Len();
    for (int32 i = 0; i < Len; ++i)
    {
        uint32 C = static_cast<uint32>(Text[i]);
        if (C >= 0xD800 && C <= 0xDBFF && i + 1 < Len)
        {
            const uint32 Low = static_cast<uint32>(Text[i + 1]);
            if (Low >= 0xDC00 && Low <= 0xDFFF)
            {
                C = 0x10000 + ((C - 0xD800) << 10) + (Low - 0xDC00);
                ++i;
            }
        }

        if ((C >= '0' && C <= '9') || (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z')) return true;
        // Letters of every other script start at Latin-1 À; skip the signs and lone accents among them.
        const bool bLatinSign = C == 0x00D7 || C == 0x00F7;
        const bool bCombiningMark = C >= 0x0300 && C <= 0x036F;
        if (C >= 0x00C0 && !bLatinSign && !bCombiningMark && !IsSilentSymbol(C)) return true;
    }
    return false;
}

EEmotions UNeocortexFunctionLibrary::EmotionFromString(const FString& Name)
{
    const UEnum* E = StaticEnum<EEmotions>();
    const FString Trimmed = Name.TrimStartAndEnd();
    if (!E || Trimmed.IsEmpty()) return EEmotions::Neutral;

    for (int32 i = 0; i < E->NumEnums() - 1; ++i)
    {
        if (E->GetNameStringByIndex(i).Equals(Trimmed, ESearchCase::IgnoreCase))
        {
            return static_cast<EEmotions>(E->GetValueByIndex(i));
        }
    }
    return EEmotions::Neutral;
}

FString UNeocortexFunctionLibrary::EmotionToUpper(EEmotions Emotion)
{
    const UEnum* E = StaticEnum<EEmotions>();
    FString S = E ? E->GetNameStringByValue(static_cast<int64>(Emotion)) : TEXT("Neutral");
    return S.ToUpper();
}
