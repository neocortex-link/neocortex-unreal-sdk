#include "NeocortexService.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexSerializer.h"
#include "Neocortex.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"

// Returns the enum value name without its class prefix, e.g. "EEmotions::Neutral" -> "Neutral".
static FString EmotionToString(EEmotions Emotion)
{
    const UEnum* E = StaticEnum<EEmotions>();
    FString S = E ? E->GetNameStringByValue(static_cast<int64>(Emotion)) : TEXT("Neutral");
    int32 Sep = INDEX_NONE;
    S.FindLastChar(TEXT(':'), Sep);
    return Sep != INDEX_NONE ? S.Mid(Sep + 1) : S;
}

// Stable per-device identifier — matches Unity's SystemInfo.deviceUniqueIdentifier.
static const FString& GetPlayerId()
{
    static FString CachedId;
    if (CachedId.IsEmpty())
    {
        CachedId = FPlatformMisc::GetDeviceId();
        if (CachedId.IsEmpty())
            CachedId = FMD5::HashAnsiString(*(FString(FPlatformProcess::ComputerName()) + FPlatformProcess::UserName()));
    }
    return CachedId;
}

void UNeocortexService::Init(UNeocortexHttpClient* Http, UNeocortexSessionManager* Session)
{
    HttpRef = Http;
    SessionRef = Session;
}

FNeocortexRequestError UNeocortexService::MakeError(const FString& Where, const FHttpResponsePtr& Response, bool IsSuccessful) const
{
    FNeocortexRequestError E;
    E.Code = Response.IsValid() ? Response->GetResponseCode() : -1;
    E.Message = FString::Printf(TEXT("%s failed (%s)"), *Where,
        IsSuccessful ? TEXT("transport OK; bad/parse response") : TEXT("transport error"));
    return E;
}

void UNeocortexService::TextToText(const FString& CharacterId, const FString& Message,
                                    FNeocortexChatDelegate OnChatResponse, FNeocortexErrorDelegate OnFail,
                                    const FString& Metadata, const FString& Events)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("characterId required")});
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("sessionId"),   SessionRef->Get(CharacterId));
    Obj->SetStringField(TEXT("playerId"),    GetPlayerId());
    Obj->SetStringField(TEXT("characterId"), CharacterId);
    Obj->SetStringField(TEXT("message"),     Message);
    Obj->SetStringField(TEXT("metadata"),    Metadata);
    Obj->SetStringField(TEXT("events"),      Events);

    FString Body;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
    if (!FJsonSerializer::Serialize(Obj, Writer))
    {
        OnFail.ExecuteIfBound({-1, TEXT("serialize /chat failed")});
        return;
    }

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat"), Body, TEXT("application/json"),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, CharacterId, OnChatResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
            {
                if (!WeakThis.IsValid()) return;
                if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
                {
                    UE_LOG(LogNeocortex, Error, TEXT("POST /chat failed %d: %s"),
                        Resp.IsValid() ? Resp->GetResponseCode() : -1,
                        Resp.IsValid() ? *Resp->GetContentAsString() : TEXT("no response"));
                    OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /chat"), Resp, Resp.IsValid()));
                    return;
                }
                FNeocortexChatResponseData Data;
                if (FNeocortexSerializer::FromJson(Raw, Data))
                {
                    WeakThis->SessionRef->Set(CharacterId, Data.SessionId);
                    OnChatResponse.ExecuteIfBound(Data);
                }
                else
                {
                    OnFail.ExecuteIfBound({Resp->GetResponseCode(), TEXT("parse /chat failed")});
                }
            }));
}

void UNeocortexService::TextToAudio(const FString& CharacterId, const FString& Message,
                                     FNeocortexChatDelegate OnChatResponse, FNeocortexAudioDelegate OnAudioResponse,
                                     FNeocortexErrorDelegate OnFail, const FString& Metadata, const FString& Events)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    TextToText(CharacterId, Message,
        FNeocortexChatDelegate::CreateLambda(
            [WeakThis, CharacterId, OnChatResponse, OnAudioResponse, OnFail](const FNeocortexChatResponseData& Chat)
            {
                if (!WeakThis.IsValid()) return;
                OnChatResponse.ExecuteIfBound(Chat);

                TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
                Obj->SetStringField(TEXT("characterId"), CharacterId);
                Obj->SetStringField(TEXT("message"),     Chat.Response);
                Obj->SetStringField(TEXT("emotion"),     EmotionToString(Chat.Emotion));

                FString Body;
                {
                    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
                        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
                    FJsonSerializer::Serialize(Obj, W);
                }

                WeakThis->HttpRef->PostJson(TEXT("audio/generate"), Body, TEXT("*/*"),
                    FNeocortexHttpRawDelegate::CreateLambda(
                        [WeakThis, OnAudioResponse, OnFail](const FString& /*Raw*/, const FHttpResponsePtr& Resp)
                        {
                            if (!WeakThis.IsValid()) return;
                            if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
                            {
                                OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /audio/generate"), Resp, Resp.IsValid()));
                                return;
                            }
                            UE_LOG(LogNeocortex, Log, TEXT("POST /audio/generate: %d bytes"), Resp->GetContentLength());
                            OnAudioResponse.ExecuteIfBound(Resp->GetContent());
                        }));
            }),
        OnFail, Metadata, Events);
}

void UNeocortexService::AudioToText(const FString& CharacterId, const TArray<uint8>& WavBytes,
                                     FNeocortexTranscribeDelegate OnTranscribeResponse, FNeocortexErrorDelegate OnFail)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("characterId required")});
        return;
    }

    TMap<FString, FString> Fields;
    Fields.Add(TEXT("characterId"), CharacterId);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostMultipart(TEXT("audio/transcribe"), Fields,
        TEXT("audio"), TEXT("input.wav"), TEXT("audio/wav"), WavBytes, nullptr,
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnTranscribeResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
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

void UNeocortexService::AudioToAudio(const FString& CharacterId, const TArray<uint8>& WavBytes,
                                      FNeocortexChatDelegate OnChatResponse, FNeocortexAudioDelegate OnAudioResponse,
                                      FNeocortexErrorDelegate OnFail, const FString& Metadata, const FString& Events)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    AudioToText(CharacterId, WavBytes,
        FNeocortexTranscribeDelegate::CreateLambda(
            [WeakThis, CharacterId, OnChatResponse, OnAudioResponse, OnFail, Metadata, Events](const FNeocortexAudioTranscribeResponseData& Transcription)
            {
                if (!WeakThis.IsValid()) return;
                WeakThis->TextToAudio(CharacterId, Transcription.Response, OnChatResponse, OnAudioResponse, OnFail, Metadata, Events);
            }),
        OnFail);
}

void UNeocortexService::GetChatHistory(const FString& CharacterId, int32 Limit,
                                        FNeocortexChatHistoryDelegate OnChatHistoryResponse, FNeocortexErrorDelegate OnFail)
{
    const FString SessionId = SessionRef->Get(CharacterId);
    if (SessionId.IsEmpty())
    {
        OnFail.ExecuteIfBound({-1, TEXT("no active session — send a message first")});
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("sessionId"), SessionId);
    Obj->SetNumberField(TEXT("limit"), Limit);

    FString Body;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
    FJsonSerializer::Serialize(Obj, Writer);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat/session"), Body, TEXT("application/json"),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnChatHistoryResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp)
            {
                if (!WeakThis.IsValid()) return;
                if (!Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode()))
                {
                    OnFail.ExecuteIfBound(WeakThis->MakeError(TEXT("POST /chat/session"), Resp, Resp.IsValid()));
                    return;
                }
                FNeocortexChatHistoryResponseData Data;
                if (FNeocortexSerializer::FromJson(Raw, Data)) OnChatHistoryResponse.ExecuteIfBound(Data.Messages);
                else OnFail.ExecuteIfBound({Resp->GetResponseCode(), TEXT("parse /chat/session failed")});
            }));
}
