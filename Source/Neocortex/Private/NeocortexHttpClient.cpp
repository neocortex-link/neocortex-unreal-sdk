// C++
#include "NeocortexHttpClient.h"
#include "HttpModule.h"
#include "Neocortex.h"
#include "NeocortexSettings.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FString UNeocortexHttpClient::BaseUrlOverride;

// How much of an unrecognised error body is worth repeating in a message.
static constexpr int32 MaxErrorBodyChars = 200;

void UNeocortexHttpClient::Init(const FString& InBaseUrl)
{
    BaseUrl = InBaseUrl;
    BaseUrl.RemoveFromEnd(TEXT("/"));
    ApiKey = GetDefault<UNeocortexSettings>()->ApiKey.TrimStartAndEnd();
    UE_LOG(LogNeocortex, Log, TEXT("UNeocortexHttpClient initialized (base: %s)"), *BaseUrl);
}

FNeocortexRequestError UNeocortexHttpClient::MakeError(const FHttpResponsePtr& Resp, bool bTimedOut, const FString& Url)
{
    return Resp.IsValid()
        ? MakeError(Resp->GetResponseCode(), Resp->GetContentAsString(), bTimedOut, Url)
        : MakeError(0, FString(), bTimedOut, Url);
}

FNeocortexRequestError UNeocortexHttpClient::MakeError(int32 Status, const FString& Body, bool bTimedOut, const FString& Url)
{
    FNeocortexRequestError E;
    E.Url = Url;
    E.bTimedOut = bTimedOut;
    if (Status <= 0)
    {
        E.Message = bTimedOut ? TEXT("The request timed out.") : TEXT("Could not reach the Neocortex API.");
        return E;
    }

    E.Status = Status;
    TSharedPtr<FJsonObject> Json;
    if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) && Json.IsValid())
    {
        Json->TryGetStringField(TEXT("code"), E.Code);
        Json->TryGetStringField(TEXT("error"), E.Message);
    }
    if (E.Message.IsEmpty())
    {
        const FString Trimmed = Body.TrimStartAndEnd();
        E.Message = !Trimmed.IsEmpty() && !Trimmed.StartsWith(TEXT("<")) && Trimmed.Len() <= MaxErrorBodyChars
            ? Trimmed
            : FString::Printf(TEXT("Request failed with status %d."), E.Status);
    }
    return E;
}

FNeocortexRequestHandle UNeocortexHttpClient::PostJson(const FString& Path,
                                                       const FString& JsonBody,
                                                       const TCHAR* Accept,
                                                       const FNeocortexRequestOptions& Options,
                                                       FNeocortexHttpRawDelegate Callback)
{
    FNeocortexRequestHandle Handle;

    const FString Url = BaseUrl / Path;
    const FString Key = ApiKey;
    const FString AcceptStr = Accept ? FString(Accept) : FString();
    const float Timeout = Options.TimeoutSeconds;

    UE_LOG(LogNeocortex, Verbose, TEXT("POST %s body=%s"), *Url, *JsonBody);

    FRequestFactory Factory = [Url, JsonBody, Key, AcceptStr, Timeout]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(Url);
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        if (!AcceptStr.IsEmpty()) Req->SetHeader(TEXT("Accept"), *AcceptStr);
        if (!Key.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), Key);
        Req->SetTimeout(Timeout);
        Req->SetContentAsString(JsonBody);
        return Req;
    };

    Send(Handle, MoveTemp(Factory), Options, MoveTemp(Callback), 0);
    return Handle;
}

FNeocortexRequestHandle UNeocortexHttpClient::GetJson(const FString& Path,
                                                      const TCHAR* Accept,
                                                      const FNeocortexRequestOptions& Options,
                                                      FNeocortexHttpRawDelegate Callback)
{
    FNeocortexRequestHandle Handle;

    const FString Url = BaseUrl / Path;
    const FString Key = ApiKey;
    const FString AcceptStr = Accept ? FString(Accept) : FString();
    const float Timeout = Options.TimeoutSeconds;

    UE_LOG(LogNeocortex, Verbose, TEXT("GET %s"), *Url);

    FRequestFactory Factory = [Url, Key, AcceptStr, Timeout]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(Url);
        Req->SetVerb(TEXT("GET"));
        if (!AcceptStr.IsEmpty()) Req->SetHeader(TEXT("Accept"), *AcceptStr);
        if (!Key.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), Key);
        Req->SetTimeout(Timeout);
        return Req;
    };

    Send(Handle, MoveTemp(Factory), Options, MoveTemp(Callback), 0);
    return Handle;
}

FNeocortexRequestHandle UNeocortexHttpClient::PostMultipart(const FString& Path,
                                                            const TMap<FString, FString>& Fields,
                                                            const FString& FileField,
                                                            const FString& FileName,
                                                            const FString& MimeType,
                                                            const TArray<uint8>& Bytes,
                                                            const TCHAR* Accept,
                                                            const FNeocortexRequestOptions& Options,
                                                            FNeocortexHttpRawDelegate Callback)
{
    FNeocortexRequestHandle Handle;

    const FString Boundary = TEXT("----NeoBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString ContentType = FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary);

    // Build the body once; retries reuse the same bytes.
    TArray<uint8> Body;
    auto AddLine = [&Body](const FString& S)
    {
        FTCHARToUTF8 Conv(*S);
        Body.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
        Body.Append({'\r', '\n'});
    };
    for (const auto& KV : Fields)
    {
        AddLine(TEXT("--") + Boundary);
        AddLine(FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\""), *KV.Key));
        AddLine(TEXT(""));
        AddLine(KV.Value);
    }
    AddLine(TEXT("--") + Boundary);
    AddLine(FString::Printf(TEXT("Content-Disposition: form-data; name=\"%s\"; filename=\"%s\""), *FileField, *FileName));
    AddLine(FString::Printf(TEXT("Content-Type: %s"), *MimeType));
    AddLine(TEXT(""));
    Body.Append(Bytes);
    AddLine(TEXT(""));
    AddLine(TEXT("--") + Boundary + TEXT("--"));

    const FString Url = BaseUrl / Path;
    const FString Key = ApiKey;
    const FString AcceptStr = Accept ? FString(Accept) : FString();
    const float Timeout = Options.TimeoutSeconds;

    FRequestFactory Factory = [Url, Body, ContentType, Key, AcceptStr, Timeout]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(Url);
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), ContentType);
        if (!AcceptStr.IsEmpty()) Req->SetHeader(TEXT("Accept"), *AcceptStr);
        if (!Key.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), Key);
        Req->SetTimeout(Timeout);
        Req->SetContent(Body);
        return Req;
    };

    Send(Handle, MoveTemp(Factory), Options, MoveTemp(Callback), 0);
    return Handle;
}

void UNeocortexHttpClient::Send(const FNeocortexRequestHandle& Handle,
                                FRequestFactory Factory,
                                FNeocortexRequestOptions Options,
                                FNeocortexHttpRawDelegate Callback,
                                int32 Attempt)
{
    // A retry that was cancelled while it waited is no longer pending.
    const bool bRetryCancelled = Attempt > 0 && PendingRetries.Remove(Handle.Id) == 0;
    if (bRetryCancelled || (Options.Token.IsValid() && Options.Token->bCancelled))
    {
        Forget(Handle, Options.Token);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Factory();
    InFlight.Add(Handle.Id, Req);
    if (Options.Token.IsValid()) Options.Token->InFlight.AddUnique(Handle.Id);

    const double StartTime = FPlatformTime::Seconds();
    TWeakObjectPtr<UNeocortexHttpClient> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, Handle, Callback, Factory, Options, Attempt, StartTime](
            FHttpRequestPtr Request, FHttpResponsePtr Resp, bool bOK)
        {
            if (!WeakThis.IsValid()) return;
            UNeocortexHttpClient* Self = WeakThis.Get();

            // A cancelled request is not a failure worth reporting.
            const bool bWasTracked = Self->InFlight.Remove(Handle.Id) > 0;
            if (!bWasTracked || (Options.Token.IsValid() && Options.Token->bCancelled))
            {
                Self->Forget(Handle, Options.Token);
                return;
            }

            const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
            if (bOK && Resp.IsValid() && EHttpResponseCodes::IsOk(Code))
            {
                Self->Forget(Handle, Options.Token);
                const FString Raw = Resp->GetContentAsString();
                // HTTP callbacks are normally on the game thread, but not on every platform.
                AsyncTask(ENamedThreads::GameThread, [Callback, Raw, Resp]()
                {
                    Callback.ExecuteIfBound(Raw, Resp, nullptr);
                });
                return;
            }

            const bool bTimedOut = !Resp.IsValid()
                && FPlatformTime::Seconds() - StartTime >= Options.TimeoutSeconds - 0.5;
            const FString Url = Request.IsValid() ? Request->GetURL() : FString();
            const FNeocortexRequestError Error = MakeError(Resp, bTimedOut, Url);

            if (Error.IsRetryable() && Attempt < Options.Retries)
            {
                UE_LOG(LogNeocortex, Warning, TEXT("%s (%s), retrying."), *Error.Message, *Url);
                Self->PendingRetries.Add(Handle.Id);
                // Waiting on the core ticker works with or without a world (level transitions, editor).
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
                    [WeakThis, Handle, Callback, Factory, Options, Attempt](float)
                    {
                        if (WeakThis.IsValid()) WeakThis->Send(Handle, Factory, Options, Callback, Attempt + 1);
                        return false;
                    }), RetryDelaySeconds);
                return;
            }

            Self->Forget(Handle, Options.Token);
            if (!Options.HandledCodes.Contains(Error.Code))
            {
                UE_LOG(LogNeocortex, Error, TEXT("%s%s (%s)"), *Error.Message,
                    Error.Code.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" [%s]"), *Error.Code), *Url);
            }
            const FString Raw = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            AsyncTask(ENamedThreads::GameThread, [Callback, Raw, Resp, Error]()
            {
                Callback.ExecuteIfBound(Raw, Resp, &Error);
            });
        });

    Req->ProcessRequest();
}

void UNeocortexHttpClient::Forget(const FNeocortexRequestHandle& Handle, const FNeocortexCancelTokenPtr& Token)
{
    if (Token.IsValid()) Token->InFlight.Remove(Handle.Id);
}

void UNeocortexHttpClient::Cancel(const FNeocortexRequestHandle& Handle)
{
    if (TSharedRef<IHttpRequest, ESPMode::ThreadSafe>* Req = InFlight.Find(Handle.Id))
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = *Req;
        InFlight.Remove(Handle.Id);
        Request->CancelRequest();
    }
    else
    {
        PendingRetries.Remove(Handle.Id);
    }
}

void UNeocortexHttpClient::Cancel(const FNeocortexCancelTokenPtr& Token)
{
    if (!Token.IsValid()) return;
    Token->bCancelled = true;
    const TArray<FGuid> Ids = Token->InFlight;
    Token->InFlight.Reset();
    for (const FGuid& Id : Ids)
    {
        PendingRetries.Remove(Id);
        if (TSharedRef<IHttpRequest, ESPMode::ThreadSafe>* Req = InFlight.Find(Id))
        {
            TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = *Req;
            InFlight.Remove(Id);
            Request->CancelRequest();
        }
    }
}
