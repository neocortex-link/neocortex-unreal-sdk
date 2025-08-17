#include "NeocortexSubsystem.h"
#include "HttpModule.h"
#include "JsonHelper.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Neocortex.h"
#include "NeocortexSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Async/Async.h"

void UNeocortexSubsystem::AppendUtf8(TArray<uint8>& Dest, const FString& Text)
{
    FTCHARToUTF8 Conv(*Text);
    Dest.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
}
FNeoRequestError UNeocortexSubsystem::MakeHttpError(const FString& Where, const FHttpResponsePtr& Resp, bool bOK, const FString& Extra)
{
    FNeoRequestError E;
    E.Code = Resp.IsValid() ? Resp->GetResponseCode() : -1;
    E.Message = FString::Printf(TEXT("%s failed (%s) %s"),
        *Where,
        bOK ? TEXT("transport OK; bad/parse response") : TEXT("transport error"),
        *Extra);
    return E;
}

// ---------- Sessions ----------
FString UNeocortexSubsystem::LoadSession(const FString& CharacterId) const
{
    FString Id;
    const FString Key = SessionPrefix + CharacterId;
    GConfig->GetString(*SessionSection, *Key, Id, GGameIni);
    return Id;
}
void UNeocortexSubsystem::SaveSession(const FString& CharacterId, const FString& SessionId) const
{
    const FString Key = SessionPrefix + CharacterId;
    GConfig->SetString(*SessionSection, *Key, *SessionId, GGameIni);
    GConfig->Flush(false, GGameIni);
}
FString UNeocortexSubsystem::GetSessionId(const FString& CharacterId) const
{
    return LoadSession(CharacterId);
}
void UNeocortexSubsystem::ClearSessionId(const FString& CharacterId)
{
    SaveSession(CharacterId, TEXT(""));
}

// ---------- Core HTTP ----------
void UNeocortexSubsystem::PostJson(
    const FString& Path,
    const FString& JsonBody,
    TFunction<void(const FString&, const FHttpResponsePtr&, bool)> OnComplete,
    const TCHAR* Accept)
{
    auto& Http = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> R = Http.CreateRequest();
    R->SetURL(BaseUrl / Path);
    R->SetVerb(TEXT("POST"));
    R->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (Accept && *Accept) R->SetHeader(TEXT("Accept"), Accept);
    if (!ApiKey.IsEmpty()) R->SetHeader(TEXT("x-api-key"), ApiKey);
    R->SetTimeout(TimeoutSeconds);
    R->SetContentAsString(JsonBody);
    TWeakObjectPtr<UNeocortexSubsystem> WeakThis(this); 
    R->OnProcessRequestComplete().BindLambda(
        [WeakThis, OnComplete = MoveTemp(OnComplete)](TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Req, FHttpResponsePtr Resp, bool bOK)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            const FString Raw = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            AsyncTask(ENamedThreads::GameThread, [OnComplete, Raw, Resp, bOK]() { OnComplete(Raw, Resp, bOK); });
        });
    R->ProcessRequest();
}

void UNeocortexSubsystem::PostMultipart(
    const FString& Path,
    const TMap<FString, FString>& Fields,
    const FString& FileFieldName,
    const FString& FileName,
    const FString& MimeType,
    const TArray<uint8>& FileBytes,
    TFunction<void(const FString&, const FHttpResponsePtr&, bool)> OnComplete,
    const TCHAR* Accept)
{
    auto& Http = FHttpModule::Get();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();

    const FString Boundary = TEXT("----NeoBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);

    TArray<uint8> Body;
    auto AddLine = [&Body](const FString& S){ AppendUtf8(Body, S); AppendUtf8(Body, TEXT("\r\n")); };

    for (const auto& KV : Fields)
    {
        AddLine(TEXT("--") + Boundary);
        AddLine(FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\""), *KV.Key));
        AddLine(TEXT(""));
        AddLine(KV.Value);
    }
    // File
    AddLine(TEXT("--") + Boundary);
    AddLine(FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\""), *FileFieldName, *FileName));
    AddLine(FString::Printf(TEXT("Content-Type: %s"), *MimeType));
    AddLine(TEXT(""));
    Body.Append(FileBytes);
    AddLine(TEXT(""));
    AddLine(TEXT("--") + Boundary + TEXT("--"));

    Request->SetURL(BaseUrl / Path);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
    if (Accept && *Accept) Request->SetHeader(TEXT("Accept"), Accept);
    if (!ApiKey.IsEmpty()) Request->SetHeader(TEXT("x-api-key"), ApiKey);
    Request->SetTimeout(TimeoutSeconds);
    Request->SetContent(Body);
    TWeakObjectPtr<UNeocortexSubsystem> WeakThis(this); 
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, OnComplete = MoveTemp(OnComplete)](TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Req, FHttpResponsePtr Resp, bool bOK)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            const FString Raw = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            AsyncTask(ENamedThreads::GameThread, [OnComplete, Raw, Resp, bOK]() { OnComplete(Raw, Resp, bOK); });
        });
    Request->ProcessRequest();
}

void UNeocortexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ApiKey = GetDefault<UNeocortexSettings>()->ApiKey;
}

void UNeocortexSubsystem::Deinitialize()
{
    Super::Deinitialize();
}

void UNeocortexSubsystem::TextToText(const FString& CharacterId, const FString& Message,
                                     FOnChatResponse OnSuccess, FOnRequestFail OnFailure)
{
    if (CharacterId.IsEmpty()) { OnFailure.ExecuteIfBound({-1, TEXT("characterId is required")}); return; }

    FChatRequest Req{ LoadSession(CharacterId), CharacterId, Message };
    FString Body;
    if (!ToJsonString(Req, Body)) { OnFailure.ExecuteIfBound({-1, TEXT("Serialize /chat failed")}); return; }
    TWeakObjectPtr<UNeocortexSubsystem> WeakThis(this);
    PostJson(TEXT("chat"), Body,
        [WeakThis, CharacterId, OnSuccess, OnFailure](const FString& Raw, const FHttpResponsePtr& Resp, bool bOK)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            if (!bOK || !Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                OnFailure.ExecuteIfBound(MakeHttpError(TEXT("POST /chat"), Resp, bOK));
                UE_LOG(LogNeocortex, Error, TEXT("Failed to send /chat request: %s"), *Raw);
                return;
            }
            FChatResponseData Data;
            if (FromJsonString(Raw, Data))
            {
                WeakThis->SaveSession(CharacterId, Data.SessionId);
                OnSuccess.ExecuteIfBound(Data);
            }
            else
            {
                UE_LOG(LogNeocortex, Error, TEXT("Failed to parse /chat"));
                OnFailure.ExecuteIfBound({Resp->GetResponseCode(), TEXT("Parse /chat JSON failed")});
            }
        });
}

void UNeocortexSubsystem::GetChatHistory(const FString& CharacterId, int32 Limit,
                                         FOnChatHistoryResponse OnSuccess, FOnRequestFail OnFailure)
{
    if (CharacterId.IsEmpty()) { OnFailure.ExecuteIfBound({-1, TEXT("characterId is required")}); return; }

    FChatHistoryRequest Req{ LoadSession(CharacterId), FMath::Max(1, Limit) };
    FString Body;
    if (!ToJsonString(Req, Body)) { OnFailure.ExecuteIfBound({-1, TEXT("Serialize /chat/session failed")}); return; }

    PostJson(TEXT("chat/session"), Body,
        [OnSuccess, OnFailure](const FString& Raw, const FHttpResponsePtr& Resp, bool bOK)
        {
            if (!bOK || !Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                OnFailure.ExecuteIfBound(MakeHttpError(TEXT("POST /chat/session"), Resp, bOK));
                return;
            }
            FChatHistoryResponseData Data;
            if (FromJsonString(Raw, Data)) OnSuccess.ExecuteIfBound(Data);
            else OnFailure.ExecuteIfBound({Resp->GetResponseCode(), TEXT("Parse /chat/session JSON failed")});
        });
}

void UNeocortexSubsystem::TextToAudio(const FString& CharacterId, const FString& Message,
                                      FOnAudioResponse OnSuccess, FOnRequestFail OnFailure)
{
    // First get chat response text (Unity parity)
    TextToText(CharacterId, Message,
        FOnChatResponse::CreateLambda([this, CharacterId, OnSuccess, OnFailure](const FChatResponseData& Chat)
        {
            // Then request audio for Chat.Response
            // build { characterId, message }
            TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("characterId"), CharacterId);
            Obj->SetStringField(TEXT("message"), Chat.Response);

            FString Body;
            TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
                TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
            FJsonSerializer::Serialize(Obj, W);

            // Binary response
            auto& Http = FHttpModule::Get();
            TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
            Request->SetURL(BaseUrl / TEXT("audio/generate"));
            Request->SetVerb(TEXT("POST"));
            Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            Request->SetHeader(TEXT("Accept"), TEXT("*/*"));
            if (!ApiKey.IsEmpty()) Request->SetHeader(TEXT("x-api-key"), ApiKey);
            Request->SetTimeout(TimeoutSeconds);
            Request->SetContentAsString(Body);

            Request->OnProcessRequestComplete().BindLambda(
                [OnSuccess, OnFailure](TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Req, FHttpResponsePtr Response, bool bOK)
                {
                    if (!bOK || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
                    {
                        OnFailure.ExecuteIfBound(MakeHttpError(TEXT("POST /audio/generate"), Response, bOK));
                        return;
                    }
                    const TArray<uint8>& Bytes = Response->GetContent();
                    AsyncTask(ENamedThreads::GameThread, [OnSuccess, Bytes]()
                    {
                        OnSuccess.ExecuteIfBound(Bytes);
                    });
                });
            Request->ProcessRequest();
        }),
        OnFailure);
}

void UNeocortexSubsystem::AudioToText_File(const FString& CharacterId, const FString& WavFilePath,
                                           FOnAudioTranscribeResponse OnSuccess, FOnRequestFail OnFailure)
{
    if (CharacterId.IsEmpty()) { OnFailure.ExecuteIfBound({-1, TEXT("characterId is required")}); return; }
    TArray<uint8> FileBytes;
    if (!FPaths::FileExists(WavFilePath) || !FFileHelper::LoadFileToArray(FileBytes, *WavFilePath))
    {
        OnFailure.ExecuteIfBound({-1, FString::Printf(TEXT("File not found/unreadable: %s"), *WavFilePath)});
        return;
    }

    TMap<FString, FString> Fields; Fields.Add(TEXT("characterId"), CharacterId);
    TWeakObjectPtr<UNeocortexSubsystem> WeakThis(this);
    PostMultipart(TEXT("audio/transcribe"), Fields, TEXT("audio"),
                  FPaths::GetCleanFilename(WavFilePath), TEXT("audio/wav"), FileBytes,
        [WeakThis, OnSuccess, OnFailure](const FString& Raw, const FHttpResponsePtr& Response, bool bOK)
        {
            if (!WeakThis.IsValid())
            {
                return;
            }
            if (!bOK || !Response.IsValid() || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
            {
                OnFailure.ExecuteIfBound(MakeHttpError(TEXT("POST /audio/transcribe"), Response, bOK));
                return;
            }
            FAudioTranscribeResponseData Data;
            if (FromJsonString(Raw, Data)) OnSuccess.ExecuteIfBound(Data);
            else OnFailure.ExecuteIfBound({Response->GetResponseCode(), TEXT("Parse /audio/transcribe JSON failed")});
        });
}
