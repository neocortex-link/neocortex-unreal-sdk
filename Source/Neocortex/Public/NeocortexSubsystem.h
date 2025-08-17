#pragma once

#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpResponse.h"
#include "NeocortexSubsystem.generated.h"

// ===== Delegates for Blueprint call sites =====
DECLARE_DELEGATE_OneParam(FOnChatResponse, const FChatResponseData&);
DECLARE_DELEGATE_OneParam(FOnChatHistoryResponse, const FChatHistoryResponseData&);
DECLARE_DELEGATE_OneParam(FOnAudioResponse, const TArray<uint8>&);
DECLARE_DELEGATE_OneParam(FOnAudioTranscribeResponse, const FAudioTranscribeResponseData&);
DECLARE_DELEGATE_OneParam(FOnRequestFail, const FNeoRequestError&);

/**
 * One-stop high-level API for NeoCortex.
 * Call these from UMG or any Blueprint/C++ without adding components.
 */
UCLASS()
class NEOCORTEX_API UNeocortexSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    
    void SetBaseUrl(const FString& InBaseUrl) { BaseUrl = InBaseUrl; }

    void SetTimeoutSeconds(float Seconds) { TimeoutSeconds = Seconds; }

    void TextToText(const FString& CharacterId, const FString& Message, FOnChatResponse OnSuccess, FOnRequestFail OnFailure);

    void GetChatHistory(const FString& CharacterId, int32 Limit,
                        FOnChatHistoryResponse OnSuccess, FOnRequestFail OnFailure);

    void TextToAudio(const FString& CharacterId, const FString& Message,
                     FOnAudioResponse OnSuccess, FOnRequestFail OnFailure);

    void AudioToText_File(const FString& CharacterId, const FString& WavFilePath,
                          FOnAudioTranscribeResponse OnSuccess, FOnRequestFail OnFailure);

    FString GetSessionId(const FString& CharacterId) const;

    void ClearSessionId(const FString& CharacterId);

private:
    // HTTP helpers
    void PostJson(const FString& Path, const FString& JsonBody,
                  TFunction<void(const FString&, const FHttpResponsePtr&, bool)> OnComplete,
                  const TCHAR* Accept = TEXT("application/json"));
    void PostMultipart(const FString& Path,
                       const TMap<FString, FString>& Fields,
                       const FString& FileFieldName,
                       const FString& FileName,
                       const FString& MimeType,
                       const TArray<uint8>& FileBytes,
                       TFunction<void(const FString&, const FHttpResponsePtr&, bool)> OnComplete,
                       const TCHAR* Accept = TEXT("application/json"));
    
    static void AppendUtf8(TArray<uint8>& Dest, const FString& Text);
    static FNeoRequestError MakeHttpError(const FString& Where, const FHttpResponsePtr& Resp, bool bOK, const FString& Extra = TEXT(""));

    // Session per characterId
    FString LoadSession(const FString& CharacterId) const;
    void    SaveSession(const FString& CharacterId, const FString& SessionId) const;

private:
    FString BaseUrl = TEXT("https://neocortex.link/api/v2");
    FString ApiKey;               // sent as x-api-key
    float   TimeoutSeconds = 30.f;

    // ini section + prefix so keys: Session.neocortex.<characterId>
    FString SessionSection = TEXT("/Script/Neocortex.Session");
    FString SessionPrefix  = TEXT("Session.neocortex.");
};