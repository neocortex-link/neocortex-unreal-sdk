#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NeocortexTypes.h"
#include "NeocortexFunctionLibrary.generated.h"

/** Blueprint helpers for request errors, emotions and speech text. */
UCLASS()
class NEOCORTEX_API UNeocortexFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** True when trying the same request again a moment later can succeed (busy service, timeout). */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsRetryable(const FNeocortexRequestError& Error) { return Error.IsRetryable(); }

    /** True for an invalid or missing API key (401). */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsUnauthorized(const FNeocortexRequestError& Error) { return Error.IsUnauthorized(); }

    /** True when the key is inactive, a plan is required, or the caller is not a team member (403). */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsForbidden(const FNeocortexRequestError& Error) { return Error.IsForbidden(); }

    /** True when the team, the character or the player is out of credits. */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsOutOfCredits(const FNeocortexRequestError& Error) { return Error.IsOutOfCredits(); }

    /** True when requests are being sent too fast (429 that is not a credit limit). */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsRateLimited(const FNeocortexRequestError& Error) { return Error.IsRateLimited(); }

    /** True when speech synthesis failed. */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsSpeechFailure(const FNeocortexRequestError& Error) { return Error.IsSpeechFailure(); }

    /** True when speech recognition failed. */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Errors")
    static bool IsTranscriptionFailure(const FNeocortexRequestError& Error) { return Error.IsTranscriptionFailure(); }

    /**
     * True when the text has something to speak: a letter or a digit in any script. Blank,
     * punctuation-only and emoji-only text is silence and is never sent for speech.
     */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Text")
    static bool HasSomethingToSay(const FString& Text);

    /** Reads an emotion name in any casing ("happy", "HAPPY"). Unknown names are Neutral. */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Text")
    static EEmotions EmotionFromString(const FString& Name);

    /** The emotion's name in uppercase, as speech requests expect it (e.g. "HAPPY"). */
    UFUNCTION(BlueprintPure, Category = "Neocortex|Text")
    static FString EmotionToUpper(EEmotions Emotion);
};
