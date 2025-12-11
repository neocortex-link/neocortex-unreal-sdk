#include "NeocortexService.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Neocortex.h"

void UNeocortexService::Init(UNeocortexHttpClient* Http, UNeocortexSessionManager* Session)
{
    HttpRef = Http;
    SessionRef = Session;
}

FNeocortexRequestError UNeocortexService::MakeError(const FString& Where, const FHttpResponsePtr& Response, bool IsSuccessful) const
{
    FNeocortexRequestError E;
    E.Code = Response.IsValid() ? Response->GetResponseCode() : -1;
    E.Message = FString::Printf(TEXT("%s failed (%s)"), *Where, IsSuccessful ? TEXT("transport OK; bad/parse response") : TEXT("transport error"));
    return E;
}

void UNeocortexService::TextToText(const FString& CharacterId, const FString& Message, FNeocortexChatDelegate OnChatResponse, FNeocortexErrorDelegate OnFail)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("characterId required")});
        return;
    }
    const FNeocortexChatRequest Req { 
        SessionRef->Get(CharacterId), 
        CharacterId, 
        Message 
    };
    
    FString Body;
    if (!FNeocortexSerializer::ToJson(Req, Body))
    {
        OnFail.ExecuteIfBound({-1, TEXT("serialize /chat failed")});
        return;
    }
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat"), Body, TEXT("application/json"),
        FNeocortexHttpRawDelegate::CreateLambda([WeakThis, CharacterId, OnChatResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
        {
            if (!WeakThis.IsValid()) return;
            if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /chat"), Resp, Resp.IsValid()));
                return;
            }
            FNeocortexChatResponseData Data;
            if (FNeocortexSerializer::FromJson(Raw, Data))
            {
                WeakThis->SessionRef->Set(CharacterId, Data.SessionId);
                OnChatResponse.ExecuteIfBound(Data);
                UE_LOG(LogNeocortex, Log, TEXT("Received TextToText response  %s"), *Raw);
            }
            else
            {
                OnFail.ExecuteIfBound({Resp->GetResponseCode(), TEXT("parse /chat failed")});
            }
        }));
}

void UNeocortexService::TextToAudio(const FString& CharacterId, const FString& Message, FNeocortexChatDelegate OnChatResponse, FNeocortexAudioDelegate OnAudioResponse, FNeocortexErrorDelegate OnFail)
{
    UE_LOG(LogNeocortex, Log, TEXT("TextToAudio request: characterId=%s, message=%s"), *CharacterId, *Message);
    TextToText(CharacterId, Message,
        FNeocortexChatDelegate::CreateLambda([this, CharacterId, OnAudioResponse, OnFail, OnChatResponse](const FNeocortexChatResponseData& Chat)
        {
            OnChatResponse.ExecuteIfBound(Chat);
            TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("characterId"), CharacterId);
            Obj->SetStringField(TEXT("message"), Chat.Response);
            FString Body;
            {
                TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
                    TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
                FJsonSerializer::Serialize(Obj, W);
            }
            TWeakObjectPtr<UNeocortexService> WeakThis(this);
            HttpRef->PostJson(TEXT("audio/generate"), Body, TEXT("*/*"),
                FNeocortexHttpRawDelegate::CreateLambda([WeakThis, OnAudioResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
                {
                    if (!WeakThis.IsValid()) return;
                    if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
                    {
                        OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /audio/generate"), Resp, Resp.IsValid()));
                        return;
                    }
                    // print number of bytes received
                    UE_LOG(LogNeocortex, Log, TEXT("Received TextToAudio response: %d bytes"), Resp->GetContentLength());
                    OnAudioResponse.ExecuteIfBound(Resp->GetContent());
                }));
        }),
        OnFail);
}

void UNeocortexService::AudioToText(const FString& CharacterId, const TArray<uint8>& WavBytes, FNeocortexTranscribeDelegate OnTranscribeResponse, FNeocortexErrorDelegate OnFail)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("characterId required")});
        return;
    }
    TMap<FString,FString> Fields;
    Fields.Add(TEXT("characterId"), CharacterId);
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostMultipart(TEXT("audio/transcribe"), Fields, TEXT("audio"),
        TEXT("input.wav"), TEXT("audio/wav"), WavBytes, nullptr,
        FNeocortexHttpRawDelegate::CreateLambda([WeakThis, OnTranscribeResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
        {
            if (!WeakThis.IsValid()) return;
            if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /audio/transcribe"), Resp, Resp.IsValid()));
                return;
            }
            FNeocortexAudioTranscribeResponseData Data;
            if (FNeocortexSerializer::FromJson(Raw, Data)) OnTranscribeResponse.ExecuteIfBound(Data);
            else OnFail.ExecuteIfBound({Resp->GetResponseCode(), TEXT("parse /audio/transcribe failed")});
        }));
}

void UNeocortexService::GetChatHistory(const FString& CharacterId, int32 Limit, FNeocortexChatHistoryDelegate OnChatHistoryResponse, FNeocortexErrorDelegate OnFail)
{
    FString SessionId = SessionRef->Get(CharacterId);
    if (SessionId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("sessionId required")});
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetNumberField(TEXT("limit"), Limit);
    Obj->SetStringField(TEXT("sessionId"), SessionId);

    FString Body;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
    FJsonSerializer::Serialize(Obj, Writer);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat/session"), Body, TEXT("application/json"),
        FNeocortexHttpRawDelegate::CreateLambda([WeakThis, OnChatHistoryResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
        {
            if (!WeakThis.IsValid()) return;
            if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
            {
                OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /chat/session"), Resp, Resp.IsValid()));
                return;
            }
            FNeocortexChatHistoryResponseData Data;
            if (FNeocortexSerializer::FromJson(Raw, Data))
            {
                OnChatHistoryResponse.ExecuteIfBound(Data.Messages);
            }
            else
            {
                OnFail.ExecuteIfBound({Resp->GetResponseCode(), TEXT("parse /chat/session failed")});
            }
        }));
}