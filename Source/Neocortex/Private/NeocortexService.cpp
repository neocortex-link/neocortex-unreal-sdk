#include "NeocortexService.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSessionManager.h"
#include "NeocortexSerializer.h"
#include "NeocortexFunctionLibrary.h"
#include "NeocortexHttpStream.h"
#include "NeocortexChatStream.h"
#include "Containers/Ticker.h"
#include "Neocortex.h"
#include "JsonObjectConverter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// Stable per-device identifier.
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

static FString ToJsonString(const TSharedRef<FJsonObject>& Obj)
{
    FString Body;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
    FJsonSerializer::Serialize(Obj, Writer);
    return Body;
}

static FString GetString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    FString Out;
    if (Obj.IsValid()) Obj->TryGetStringField(Field, Out);
    return Out;
}

static const TArray<TSharedPtr<FJsonValue>>& GetArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    static const TArray<TSharedPtr<FJsonValue>> Empty;
    const TArray<TSharedPtr<FJsonValue>>* Out = nullptr;
    return Obj.IsValid() && Obj->TryGetArrayField(Field, Out) ? *Out : Empty;
}

// Reads an actions array that may hold objects or bare names.
static TArray<FNeocortexChatAction> ParseActions(const TArray<TSharedPtr<FJsonValue>>& Raw)
{
    TArray<FNeocortexChatAction> Out;
    for (const TSharedPtr<FJsonValue>& Item : Raw)
    {
        FNeocortexChatAction Action;
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (Item.IsValid() && Item->TryGetObject(Obj))
        {
            Action.Name = GetString(*Obj, TEXT("name"));
            Action.TargetId = GetString(*Obj, TEXT("targetId"));
        }
        else if (Item.IsValid())
        {
            Item->TryGetString(Action.Name);
        }
        if (!Action.Name.IsEmpty()) Out.Add(Action);
    }
    return Out;
}

// The scene's interactables after the turn, as an array or as a JSON string holding one.
static TArray<FNeocortexInteractable> ParseMetadata(const TSharedPtr<FJsonObject>& Root)
{
    TArray<FNeocortexInteractable> Out;
    TArray<TSharedPtr<FJsonValue>> Items = GetArray(Root, TEXT("metadata"));
    FString AsString;
    if (Items.Num() == 0 && Root->TryGetStringField(TEXT("metadata"), AsString) && !AsString.IsEmpty())
    {
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(AsString), Items);
    }
    for (const TSharedPtr<FJsonValue>& Item : Items)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        FNeocortexInteractable Entity;
        if (Item.IsValid() && Item->TryGetObject(Obj)
            && FJsonObjectConverter::JsonObjectToUStruct((*Obj).ToSharedRef(), &Entity, 0, 0))
        {
            Out.Add(Entity);
        }
    }
    return Out;
}

FNeocortexChatLine UNeocortexService::ParseChatLine(const TSharedPtr<FJsonObject>& Obj)
{
    FNeocortexChatLine Line;
    Line.Text = GetString(Obj, TEXT("text"));
    Line.SpokenText = GetString(Obj, TEXT("spokenText"));
    Line.Emotion = UNeocortexFunctionLibrary::EmotionFromString(GetString(Obj, TEXT("emotion")));
    return Line;
}

FNeocortexGroupMessage UNeocortexService::ParseGroupMessage(const TSharedPtr<FJsonObject>& Obj)
{
    FNeocortexGroupMessage Message;
    Message.CharacterId = GetString(Obj, TEXT("characterId"));
    Message.Name = GetString(Obj, TEXT("name"));
    Message.FlowState = GetString(Obj, TEXT("flowState"));
    Message.SpokenMessage = GetString(Obj, TEXT("spokenMessage"));
    Message.Actions = ParseActions(GetArray(Obj, TEXT("actions")));
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Obj, TEXT("lines")))
    {
        const TSharedPtr<FJsonObject>* LineObj = nullptr;
        if (Item.IsValid() && Item->TryGetObject(LineObj)) Message.Lines.Add(ParseChatLine(*LineObj));
    }
    return Message;
}

FNeocortexGroupChatResponse UNeocortexService::ParseGroupChat(const TSharedPtr<FJsonObject>& Root)
{
    FNeocortexGroupChatResponse Out;
    if (!Root.IsValid()) return Out;
    Out.SessionId = GetString(Root, TEXT("sessionId"));
    Out.Metadata = ParseMetadata(Root);
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("messages")))
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (Item.IsValid() && Item->TryGetObject(Obj)) Out.Messages.Add(ParseGroupMessage(*Obj));
    }
    return Out;
}

FNeocortexChatResponseData UNeocortexService::FlattenMessage(const FNeocortexGroupMessage& Message, const FString& SessionId,
                                                             const TArray<FNeocortexInteractable>& Metadata)
{
    FNeocortexChatResponseData Out;
    Out.SessionId = SessionId;
    Out.Metadata = Metadata;
    Out.CharacterId = Message.CharacterId;
    Out.Name = Message.Name;
    Out.FlowState = Message.FlowState;
    Out.Actions = Message.Actions;
    Out.Lines = Message.Lines;
    if (Out.Actions.Num() > 0) Out.Action = Out.Actions[0].Name;

    bool bAnySpoken = false;
    for (const FNeocortexChatLine& Line : Message.Lines)
    {
        bAnySpoken |= !Line.SpokenText.IsEmpty();
        Out.Response += Line.Text;
        Out.SpokenResponse += Line.SpeechText();
    }
    if (!bAnySpoken)
    {
        Out.SpokenResponse = Message.SpokenMessage.IsEmpty() ? Out.Response : Message.SpokenMessage;
    }
    if (Out.Lines.Num() > 0) Out.Emotion = Out.Lines[0].Emotion;
    return Out;
}

static TSharedPtr<FJsonObject> ParseObject(const FString& Raw)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root)) Root.Reset();
    return Root;
}

static bool ParseHistory(const FString& Raw, FNeocortexChatHistoryPage& Out)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid()) return false;

    Out = FNeocortexChatHistoryPage();
    Out.SessionId = GetString(Root, TEXT("sessionId"));
    Out.NextCursor = GetString(Root, TEXT("nextCursor"));
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("messages")))
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;

        FNeocortexChatHistoryEntry Entry;
        Entry.Content = GetString(*Obj, TEXT("content"));
        Entry.Sender = GetString(*Obj, TEXT("sender"));
        Entry.SpeakerCharacterId = GetString(*Obj, TEXT("speakerCharacterId"));
        Entry.Name = GetString(*Obj, TEXT("name"));
        Entry.AddressedTo = GetString(*Obj, TEXT("addressedTo"));
        Entry.Emotion = UNeocortexFunctionLibrary::EmotionFromString(GetString(*Obj, TEXT("emotion")));
        Entry.CreatedAt = GetString(*Obj, TEXT("createdAt"));
        for (const FNeocortexChatAction& Action : ParseActions(GetArray(*Obj, TEXT("actions"))))
        {
            Entry.Actions.Add(Action.Name);
        }
        Out.Messages.Add(Entry);
    }
    return true;
}

static FNeocortexRequestOptions MakeOptions(float Timeout, int32 Retries, const FNeocortexCancelTokenPtr& Token)
{
    FNeocortexRequestOptions Options;
    Options.TimeoutSeconds = Timeout;
    Options.Retries = Retries;
    Options.Token = Token;
    return Options;
}

static bool IsCancelled(const FNeocortexCancelTokenPtr& Token)
{
    return Token.IsValid() && Token->IsCancelled();
}

const FString& UNeocortexService::GetDefaultPlayerId()
{
    return GetPlayerId();
}

void UNeocortexService::Init(UNeocortexHttpClient* Http, UNeocortexSessionManager* Session)
{
    HttpRef = Http;
    SessionRef = Session;
}

void UNeocortexService::Abort(const FNeocortexCancelTokenPtr& Token)
{
    if (HttpRef) HttpRef->Cancel(Token);
}

static TSharedRef<FJsonObject> ChatBody(const TArray<FString>& CharacterIds, const FString& SessionId, const FString& Message,
    const FString& SpeakerCharacterId, int32 MaxTurns, const FString& Metadata, const FString& Events)
{
    TArray<TSharedPtr<FJsonValue>> Cast;
    for (const FString& Id : CharacterIds) Cast.Add(MakeShared<FJsonValueString>(Id));

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetArrayField(TEXT("characterIds"), Cast);
    Obj->SetStringField(TEXT("playerId"), GetPlayerId());
    if (!SessionId.IsEmpty())          Obj->SetStringField(TEXT("sessionId"), SessionId);
    if (!Message.IsEmpty())            Obj->SetStringField(TEXT("message"), Message);
    if (!SpeakerCharacterId.IsEmpty()) Obj->SetStringField(TEXT("speakerCharacterId"), SpeakerCharacterId);
    if (MaxTurns > 0)                  Obj->SetNumberField(TEXT("maxTurns"), MaxTurns);
    if (!Metadata.IsEmpty())           Obj->SetStringField(TEXT("metadata"), Metadata);
    if (!Events.IsEmpty())             Obj->SetStringField(TEXT("events"), Events);
    return Obj;
}

void UNeocortexService::RequestChat(const TArray<FString>& CharacterIds, const FString& SessionId, const FString& Message,
                                    const FString& SpeakerCharacterId, int32 MaxTurns, const FString& Metadata, const FString& Events,
                                    FNeocortexGroupChatDelegate OnResponse, FNeocortexErrorDelegate OnFail, FNeocortexCancelTokenPtr Token)
{
    if (CharacterIds.Num() == 0)
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("Character ID is required.")));
        return;
    }

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat"),
        ToJsonString(ChatBody(CharacterIds, SessionId, Message, SpeakerCharacterId, MaxTurns, Metadata, Events)),
        TEXT("application/json"), MakeOptions(ChatTimeoutSeconds, 0, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                if (!Root.IsValid())
                {
                    OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The chat response could not be read.")));
                    return;
                }
                OnResponse.ExecuteIfBound(ParseGroupChat(Root));
            }));
}

void UNeocortexService::TextToText(const FString& CharacterId, const FString& Message,
                                    FNeocortexChatDelegate OnChatResponse, FNeocortexErrorDelegate OnFail,
                                    const FString& Metadata, const FString& Events, FNeocortexCancelTokenPtr Token)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("Character ID is required.")));
        return;
    }

    // One character is a cast of one on the unified chat endpoint.
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    RequestChat({ CharacterId }, SessionRef->Get(CharacterId), Message, FString(), 0, Metadata, Events,
        FNeocortexGroupChatDelegate::CreateLambda([WeakThis, CharacterId, OnChatResponse](const FNeocortexGroupChatResponse& Turn)
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->SessionRef->Set(CharacterId, Turn.SessionId);
            FNeocortexGroupMessage Speaker;
            if (Turn.Messages.Num() > 0) Speaker = Turn.Messages[0];
            if (Speaker.CharacterId.IsEmpty()) Speaker.CharacterId = CharacterId;
            OnChatResponse.ExecuteIfBound(FlattenMessage(Speaker, Turn.SessionId, Turn.Metadata));
        }),
        OnFail, Token);
}

void UNeocortexService::GenerateAudio(const FString& CharacterId, const FString& Message, const FString& Emotion,
                                      FNeocortexAudioDelegate OnAudio, FNeocortexErrorDelegate OnFail,
                                      const FString& SpokenText, FNeocortexCancelTokenPtr Token)
{
    const FString Speech = UNeocortexFunctionLibrary::HasSomethingToSay(SpokenText) ? SpokenText : Message;
    if (!UNeocortexFunctionLibrary::HasSomethingToSay(Speech))
    {
        OnAudio.ExecuteIfBound(TArray<uint8>());
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("characterId"), CharacterId);
    Obj->SetStringField(TEXT("message"),     Speech);
    Obj->SetStringField(TEXT("spokenText"),  Speech);
    Obj->SetStringField(TEXT("emotion"),     Emotion.ToUpper());

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("audio/generate"), ToJsonString(Obj), TEXT("*/*"),
        MakeOptions(AudioTimeoutSeconds, 1, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnAudio, OnFail](const FString& /*Raw*/, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                UE_LOG(LogNeocortex, Verbose, TEXT("POST /audio/generate: %d bytes"), Resp->GetContentLength());
                OnAudio.ExecuteIfBound(Resp->GetContent());
            }));
}

void UNeocortexService::TextToAudio(const FString& CharacterId, const FString& Message,
                                     FNeocortexChatDelegate OnChatResponse, FNeocortexAudioDelegate OnAudioResponse,
                                     FNeocortexErrorDelegate OnFail, const FString& Metadata, const FString& Events,
                                     FNeocortexCancelTokenPtr Token)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    TextToText(CharacterId, Message,
        FNeocortexChatDelegate::CreateLambda(
            [WeakThis, CharacterId, OnChatResponse, OnAudioResponse, OnFail, Token](const FNeocortexChatResponseData& Chat)
            {
                if (!WeakThis.IsValid()) return;
                OnChatResponse.ExecuteIfBound(Chat);
                if (IsCancelled(Token)) return;
                WeakThis->GenerateAudio(CharacterId, Chat.Response, UNeocortexFunctionLibrary::EmotionToUpper(Chat.Emotion),
                    OnAudioResponse, OnFail, Chat.SpokenResponse, Token);
            }),
        OnFail, Metadata, Events, Token);
}

void UNeocortexService::AudioToText(const FString& CharacterId, const TArray<uint8>& WavBytes,
                                     FNeocortexTranscribeDelegate OnTranscribeResponse, FNeocortexErrorDelegate OnFail,
                                     FNeocortexCancelTokenPtr Token)
{
    if (CharacterId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("Character ID is required.")));
        return;
    }

    TMap<FString, FString> Fields;
    Fields.Add(TEXT("characterId"), CharacterId);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostMultipart(TEXT("audio/transcribe"), Fields,
        TEXT("audio"), TEXT("input.wav"), TEXT("audio/wav"), WavBytes, nullptr,
        MakeOptions(AudioTimeoutSeconds, 1, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnTranscribeResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                TSharedPtr<FJsonObject> Root;
                if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
                {
                    OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The transcription response could not be read.")));
                    return;
                }
                FNeocortexAudioTranscribeResponseData Data;
                Data.Response = GetString(Root, TEXT("response"));
                if (Data.Response.IsEmpty()) Data.Response = GetString(Root, TEXT("text"));
                OnTranscribeResponse.ExecuteIfBound(Data);
            }));
}

void UNeocortexService::AudioToAudio(const FString& CharacterId, const TArray<uint8>& WavBytes,
                                      FNeocortexChatDelegate OnChatResponse, FNeocortexAudioDelegate OnAudioResponse,
                                      FNeocortexErrorDelegate OnFail, const FString& Metadata, const FString& Events,
                                      FNeocortexCancelTokenPtr Token)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    AudioToText(CharacterId, WavBytes,
        FNeocortexTranscribeDelegate::CreateLambda(
            [WeakThis, CharacterId, OnChatResponse, OnAudioResponse, OnFail, Metadata, Events, Token](const FNeocortexAudioTranscribeResponseData& Transcription)
            {
                if (!WeakThis.IsValid() || IsCancelled(Token)) return;
                WeakThis->TextToAudio(CharacterId, Transcription.Response, OnChatResponse, OnAudioResponse, OnFail, Metadata, Events, Token);
            }),
        OnFail,
        Token);
}

void UNeocortexService::GetChatHistory(const FString& SessionId, int32 Limit, const FString& Before,
                                        FNeocortexChatHistoryDelegate OnChatHistoryResponse, FNeocortexErrorDelegate OnFail,
                                        FNeocortexCancelTokenPtr Token)
{
    if (SessionId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("A session id is required to load chat history. Send a message first.")));
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("sessionId"), SessionId);
    Obj->SetNumberField(TEXT("limit"), Limit);
    if (!Before.IsEmpty()) Obj->SetStringField(TEXT("before"), Before);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("chat/session"), ToJsonString(Obj), TEXT("application/json"),
        MakeOptions(LookupTimeoutSeconds, 1, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnChatHistoryResponse, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                FNeocortexChatHistoryPage Page;
                if (ParseHistory(Raw, Page)) OnChatHistoryResponse.ExecuteIfBound(Page);
                else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The chat history response could not be read.")));
            }));
}

void UNeocortexService::GetAccount(FNeocortexAccountDelegate OnAccount, FNeocortexErrorDelegate OnFail)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->GetJson(TEXT("account"), TEXT("application/json"),
        MakeOptions(LookupTimeoutSeconds, 1, nullptr),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnAccount, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                FNeocortexAccountResponse Data;
                if (FNeocortexSerializer::FromJson(Raw, Data)) OnAccount.ExecuteIfBound(Data);
                else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The account response could not be read.")));
            }));
}

void UNeocortexService::GetUsage(const FString& PlayerId, const FString& CharacterId,
                                  FNeocortexUsageDelegate OnUsage, FNeocortexErrorDelegate OnFail)
{
    TArray<FString> Query;
    if (!PlayerId.IsEmpty())
        Query.Add(FString::Printf(TEXT("playerId=%s"), *FGenericPlatformHttp::UrlEncode(PlayerId)));
    if (!CharacterId.IsEmpty())
        Query.Add(FString::Printf(TEXT("characterId=%s"), *FGenericPlatformHttp::UrlEncode(CharacterId)));

    FString Path = TEXT("usage");
    if (Query.Num() > 0)
        Path += TEXT("?") + FString::Join(Query, TEXT("&"));

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->GetJson(Path, TEXT("application/json"),
        MakeOptions(LookupTimeoutSeconds, 1, nullptr),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnUsage, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                FNeocortexUsageResponse Data;
                if (FNeocortexSerializer::FromJson(Raw, Data)) OnUsage.ExecuteIfBound(Data);
                else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The usage response could not be read.")));
            }));
}

// Streams

TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> UNeocortexService::OpenStream(const FString& Path, const FString& JsonBody,
                                                                                const TCHAR* Accept, float TimeoutSeconds)
{
    TArray<FString> Headers;
    Headers.Add(TEXT("Content-Type: application/json"));
    if (Accept) Headers.Add(FString::Printf(TEXT("Accept: %s"), Accept));
    if (!HttpRef->GetApiKey().IsEmpty()) Headers.Add(FString::Printf(TEXT("x-api-key: %s"), *HttpRef->GetApiKey()));

    TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> Stream = MakeShared<FNeocortexHttpStream, ESPMode::ThreadSafe>();
    Stream->Open(HttpRef->GetBaseUrl() / Path, Headers, JsonBody, TimeoutSeconds);
    return Stream;
}

TSharedPtr<FNeocortexChatStream> UNeocortexService::RequestChatStream(const FString& CharacterId, const FString& Message,
                                                                      const FString& Metadata, const FString& Events)
{
    if (!FNeocortexHttpStream::IsSupported() || CharacterId.IsEmpty()) return nullptr;

    const FString Body = ToJsonString(ChatBody({ CharacterId }, SessionRef->Get(CharacterId), Message, FString(), 0, Metadata, Events));
    TSharedPtr<FNeocortexChatStream> Stream = MakeShared<FNeocortexChatStream>(CharacterId);
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    Stream->OnSession.BindLambda([WeakThis, CharacterId](const FString& SessionId)
    {
        if (WeakThis.IsValid() && !SessionId.IsEmpty()) WeakThis->SessionRef->Set(CharacterId, SessionId);
    });
    Stream->Attach(OpenStream(TEXT("chat/stream"), Body, TEXT("text/event-stream"), ChatTimeoutSeconds));
    return Stream;
}

// Quiz

static int32 GetInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    int32 Out = 0;
    if (Obj.IsValid()) Obj->TryGetNumberField(Field, Out);
    return Out;
}

static bool GetBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    bool Out = false;
    if (Obj.IsValid()) Obj->TryGetBoolField(Field, Out);
    return Out;
}

static TArray<FString> GetStrings(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    TArray<FString> Out;
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Obj, Field))
    {
        FString Value;
        if (Item.IsValid() && Item->TryGetString(Value)) Out.Add(Value);
    }
    return Out;
}

static TSharedPtr<FJsonObject> GetObject(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
{
    const TSharedPtr<FJsonObject>* Out = nullptr;
    return Obj.IsValid() && Obj->TryGetObjectField(Field, Out) ? *Out : nullptr;
}

// Matches a server string ("multipleChoice", "multiple_choice", "MULTIPLE_CHOICE") against an
// enum's names, ignoring case and separators. Unknown values give the fallback.
template <typename TEnum>
static TEnum ParseEnum(const FString& Value, TEnum Fallback)
{
    const FString Wanted = Value.Replace(TEXT("_"), TEXT("")).Replace(TEXT("-"), TEXT("")).ToLower();
    const UEnum* E = StaticEnum<TEnum>();
    if (!E || Wanted.IsEmpty()) return Fallback;
    for (int32 i = 0; i < E->NumEnums() - 1; ++i)
    {
        if (E->GetNameStringByIndex(i).ToLower() == Wanted) return static_cast<TEnum>(E->GetValueByIndex(i));
    }
    return Fallback;
}

static FNeocortexQuizQuestion ParseQuestion(const TSharedPtr<FJsonObject>& Obj)
{
    FNeocortexQuizQuestion Q;
    Q.QuestionId = GetString(Obj, TEXT("questionId"));
    Q.Keyword = GetString(Obj, TEXT("keyword"));
    Q.Type = GetString(Obj, TEXT("type"));
    Q.QuestionText = GetString(Obj, TEXT("questionText"));
    Q.bHasHint = GetBool(Obj, TEXT("hasHint"));
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Obj, TEXT("options")))
    {
        const TSharedPtr<FJsonObject>* OptionObj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(OptionObj)) continue;
        FNeocortexQuizOption Option;
        Option.Id = GetString(*OptionObj, TEXT("id"));
        Option.Text = GetString(*OptionObj, TEXT("text"));
        Q.Options.Add(Option);
    }
    return Q;
}

static TArray<FNeocortexQuizLeaderboardEntry> ParseLeaderboard(const TArray<TSharedPtr<FJsonValue>>& Raw)
{
    TArray<FNeocortexQuizLeaderboardEntry> Out;
    for (const TSharedPtr<FJsonValue>& Item : Raw)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
        FNeocortexQuizLeaderboardEntry Entry;
        Entry.ParticipantId = GetString(*Obj, TEXT("participantId"));
        Entry.DisplayName = GetString(*Obj, TEXT("displayName"));
        Entry.TotalScore = GetInt(*Obj, TEXT("totalScore"));
        Entry.Rank = GetInt(*Obj, TEXT("rank"));
        Out.Add(Entry);
    }
    return Out;
}

static FNeocortexQuizTurn ParseQuizTurn(const TSharedPtr<FJsonObject>& Root)
{
    FNeocortexQuizTurn Turn;
    Turn.SessionId = GetString(Root, TEXT("sessionId"));
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("lines")))
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
        FNeocortexQuizLine Line;
        Line.Text = GetString(*Obj, TEXT("text"));
        Line.SpokenText = GetString(*Obj, TEXT("spokenText"));
        Line.Emotion = UNeocortexFunctionLibrary::EmotionFromString(GetString(*Obj, TEXT("emotion")));
        Line.Role = ParseEnum(GetString(*Obj, TEXT("role")), ENeocortexQuizLineRole::Reaction);
        Turn.Lines.Add(Line);
    }
    Turn.Actions = ParseActions(GetArray(Root, TEXT("actions")));
    Turn.Expecting = ParseEnum(GetString(Root, TEXT("expecting")), ENeocortexQuizExpecting::Nothing);
    Turn.Intent = ParseEnum(GetString(Root, TEXT("intent")), ENeocortexQuizIntent::Unclear);
    if (const TSharedPtr<FJsonObject> Question = GetObject(Root, TEXT("question")))
    {
        Turn.bHasQuestion = true;
        Turn.Question = ParseQuestion(Question);
    }
    if (const TSharedPtr<FJsonObject> Upcoming = GetObject(Root, TEXT("upcoming")))
    {
        Turn.bHasUpcoming = true;
        Turn.Upcoming.QuestionId = GetString(Upcoming, TEXT("questionId"));
        Turn.Upcoming.Keyword = GetString(Upcoming, TEXT("keyword"));
    }
    if (const TSharedPtr<FJsonObject> Progress = GetObject(Root, TEXT("progress")))
    {
        Turn.bHasProgress = true;
        Turn.Progress.Asked = GetInt(Progress, TEXT("asked"));
        Turn.Progress.Total = GetInt(Progress, TEXT("total"));
    }
    for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("result")))
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
        FNeocortexQuizResult Result;
        Result.ParticipantId = GetString(*Obj, TEXT("participantId"));
        Result.Verdict = ParseEnum(GetString(*Obj, TEXT("verdict")), ENeocortexQuizVerdict::Unanswered);
        Result.Score = GetInt(*Obj, TEXT("score"));
        Turn.Results.Add(Result);
    }
    Turn.Leaderboard = ParseLeaderboard(GetArray(Root, TEXT("leaderboard")));
    Turn.Pending = GetStrings(Root, TEXT("pending"));
    Turn.Answered = GetStrings(Root, TEXT("answered"));
    Turn.Attempt = GetInt(Root, TEXT("attempt"));
    Turn.AttemptsLeft = GetInt(Root, TEXT("attemptsLeft"));
    Turn.bHintGiven = GetBool(Root, TEXT("hintGiven"));
    Turn.bDone = GetBool(Root, TEXT("done"));
    return Turn;
}

static FNeocortexQuizState ParseQuizState(const TSharedPtr<FJsonObject>& Root)
{
    const TSharedPtr<FJsonObject> Obj = GetObject(Root, TEXT("state"));
    FNeocortexQuizState State;
    State.Phase = GetString(Obj, TEXT("phase"));
    State.TotalQuestions = GetInt(Obj, TEXT("totalQuestions"));
    State.QuestionsRemaining = GetInt(Obj, TEXT("questionsRemaining"));
    if (const TSharedPtr<FJsonObject> Question = GetObject(Obj, TEXT("activeQuestion")))
    {
        State.bHasActiveQuestion = true;
        State.ActiveQuestion = ParseQuestion(Question);
    }
    State.AnsweredParticipantIds = GetStrings(Obj, TEXT("answeredParticipantIds"));
    State.PendingParticipantIds = GetStrings(Obj, TEXT("pendingParticipantIds"));
    State.Attempt = GetInt(Obj, TEXT("attempt"));
    State.AttemptsLeft = GetInt(Obj, TEXT("attemptsLeft"));
    State.bHintGiven = GetBool(Obj, TEXT("hintGiven"));
    State.Leaderboard = ParseLeaderboard(GetArray(Obj, TEXT("leaderboard")));
    if (State.Leaderboard.Num() == 0) State.Leaderboard = ParseLeaderboard(GetArray(Root, TEXT("leaderboard")));
    State.Expecting = ParseEnum(GetString(Obj, TEXT("expecting")), ENeocortexQuizExpecting::Nothing);
    return State;
}

static const TCHAR* QuizFinishedCode = TEXT("quiz_finished");
static const TCHAR* OperationInProgressCode = TEXT("operation_in_progress");
static const TCHAR* StateChangedCode = TEXT("state_changed");
static constexpr int32 QuizBusyRetries = 2;
static constexpr float QuizBusyRetryDelay = 1.5f;

void UNeocortexService::BeginQuiz(const FString& CharacterId, const FString& QuestionSetId, const FString& PlayerId,
                                  const TArray<FNeocortexQuizParticipant>& Participants, int32 QuestionCount, const FString& SelectionMode,
                                  FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail, FNeocortexCancelTokenPtr Token)
{
    if (CharacterId.IsEmpty() || QuestionSetId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(CharacterId.IsEmpty()
            ? TEXT("Character ID is required.") : TEXT("Question Set ID is required.")));
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("characterId"), CharacterId);
    Obj->SetStringField(TEXT("questionSetId"), QuestionSetId);
    Obj->SetStringField(TEXT("playerId"), PlayerId.IsEmpty() ? GetPlayerId() : PlayerId);
    if (Participants.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> Roster;
        for (const FNeocortexQuizParticipant& P : Participants)
        {
            TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("id"), P.Id);
            Entry->SetStringField(TEXT("displayName"), P.DisplayName);
            Entry->SetStringField(TEXT("type"), P.Type.IsEmpty() ? TEXT("human") : *P.Type);
            Roster.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Obj->SetArrayField(TEXT("participants"), Roster);
    }
    if (QuestionCount > 0) Obj->SetNumberField(TEXT("questionCount"), QuestionCount);
    if (!SelectionMode.IsEmpty()) Obj->SetStringField(TEXT("selectionMode"), SelectionMode);

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("quiz"), ToJsonString(Obj), TEXT("application/json"), MakeOptions(ChatTimeoutSeconds, 0, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnTurn, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                if (Root.IsValid()) OnTurn.ExecuteIfBound(ParseQuizTurn(Root));
                else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The quiz response could not be read.")));
            }));
}

void UNeocortexService::ContinueQuiz(const FString& SessionId, bool bHasSay, const FString& Say, const FString& OptionId,
                                     const FString& ParticipantId, bool bTyped,
                                     FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail, FNeocortexCancelTokenPtr Token)
{
    if (SessionId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("Session ID is required.")));
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("sessionId"), SessionId);
    // An empty string is a deliberate pass, so only "no say at all" is left out.
    if (bHasSay) Obj->SetStringField(TEXT("say"), Say);
    if (!OptionId.IsEmpty()) Obj->SetStringField(TEXT("optionId"), OptionId);
    if (!ParticipantId.IsEmpty()) Obj->SetStringField(TEXT("participantId"), ParticipantId);
    if (bTyped && bHasSay && !Say.IsEmpty()) Obj->SetBoolField(TEXT("typed"), true);

    FNeocortexRequestOptions Options = MakeOptions(ChatTimeoutSeconds, 0, Token);
    Options.HandledCodes = { QuizFinishedCode, OperationInProgressCode, StateChangedCode };
    SendQuizTurn(ToJsonString(Obj), Options, 0, OnTurn, OnFail);
}

void UNeocortexService::SendQuizTurn(const FString& Body, const FNeocortexRequestOptions& Options, int32 Attempt,
                                     FNeocortexQuizTurnDelegate OnTurn, FNeocortexErrorDelegate OnFail)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->PostJson(TEXT("quiz"), Body, TEXT("application/json"), Options,
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, Body, Options, Attempt, OnTurn, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (!Error)
                {
                    const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                    if (Root.IsValid()) OnTurn.ExecuteIfBound(ParseQuizTurn(Root));
                    else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The quiz response could not be read.")));
                    return;
                }
                if (Error->Status != 409)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }

                // The run is over and the server hands back the closing turn it already gave.
                // That is an answer, not a failure: a client whose last response was lost still
                // gets to speak the sign off.
                if (Error->Code == QuizFinishedCode)
                {
                    const TSharedPtr<FJsonObject> Closing = GetObject(ParseObject(Raw), TEXT("turn"));
                    if (Closing.IsValid()) OnTurn.ExecuteIfBound(ParseQuizTurn(Closing));
                    else OnFail.ExecuteIfBound(*Error);
                    return;
                }

                const bool bBusy = Error->Code == OperationInProgressCode;
                const bool bMoved = Error->Code == StateChangedCode;
                if ((!bBusy && !bMoved) || Attempt >= QuizBusyRetries)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
                    [WeakThis, Body, Options, Attempt, OnTurn, OnFail](float)
                    {
                        if (WeakThis.IsValid()) WeakThis->SendQuizTurn(Body, Options, Attempt + 1, OnTurn, OnFail);
                        return false;
                    }), bBusy ? QuizBusyRetryDelay : 0.0f);
            }));
}

void UNeocortexService::GetQuizSession(const FString& SessionId, FNeocortexQuizStateDelegate OnState, FNeocortexErrorDelegate OnFail,
                                       FNeocortexCancelTokenPtr Token)
{
    if (SessionId.IsEmpty())
    {
        OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("Session ID is required.")));
        return;
    }

    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->GetJson(FString::Printf(TEXT("quiz/session?sessionId=%s"), *FGenericPlatformHttp::UrlEncode(SessionId)),
        TEXT("application/json"), MakeOptions(LookupTimeoutSeconds, 1, Token),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnState, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                if (Root.IsValid()) OnState.ExecuteIfBound(ParseQuizState(Root));
                else OnFail.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The quiz session response could not be read.")));
            }));
}

// Lists

void UNeocortexService::GetCharacters(FNeocortexCharactersDelegate OnList, FNeocortexErrorDelegate OnFail)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->GetJson(TEXT("characters"), TEXT("application/json"), MakeOptions(LookupTimeoutSeconds, 1, nullptr),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnList, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                TArray<FNeocortexCharacterSummary> Out;
                const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("characters")))
                {
                    const TSharedPtr<FJsonObject>* Obj = nullptr;
                    if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
                    FNeocortexCharacterSummary Entry;
                    Entry.Id = GetString(*Obj, TEXT("id"));
                    Entry.Name = GetString(*Obj, TEXT("name"));
                    if (!Entry.Id.IsEmpty()) Out.Add(Entry);
                }
                OnList.ExecuteIfBound(Out);
            }));
}

void UNeocortexService::GetQuestionSets(FNeocortexQuestionSetsDelegate OnList, FNeocortexErrorDelegate OnFail)
{
    TWeakObjectPtr<UNeocortexService> WeakThis(this);
    HttpRef->GetJson(TEXT("question-sets"), TEXT("application/json"), MakeOptions(LookupTimeoutSeconds, 1, nullptr),
        FNeocortexHttpRawDelegate::CreateLambda(
            [WeakThis, OnList, OnFail](const FString& Raw, const FHttpResponsePtr& Resp, const FNeocortexRequestError* Error)
            {
                if (!WeakThis.IsValid()) return;
                if (Error)
                {
                    OnFail.ExecuteIfBound(*Error);
                    return;
                }
                TArray<FNeocortexQuestionSetSummary> Out;
                const TSharedPtr<FJsonObject> Root = ParseObject(Raw);
                for (const TSharedPtr<FJsonValue>& Item : GetArray(Root, TEXT("questionSets")))
                {
                    const TSharedPtr<FJsonObject>* Obj = nullptr;
                    if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
                    FNeocortexQuestionSetSummary Entry;
                    Entry.Id = GetString(*Obj, TEXT("id"));
                    Entry.Name = GetString(*Obj, TEXT("name"));
                    Entry.QuestionCount = GetInt(*Obj, TEXT("questionCount"));
                    if (!Entry.Id.IsEmpty()) Out.Add(Entry);
                }
                OnList.ExecuteIfBound(Out);
            }));
}
