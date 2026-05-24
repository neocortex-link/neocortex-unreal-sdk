// C++
#include "NeocortexHttpClient.h"
#include "HttpModule.h"
#include "Neocortex.h"
#include "NeocortexSettings.h"
#include "Async/Async.h"

void UNeocortexHttpClient::Init(const FString& InBaseUrl, const FNeocortexHttpOptions& HttpOptions)
{
    BaseUrl = InBaseUrl;
    Opts = HttpOptions;
    Opts.ApiKey = GetDefault<UNeocortexSettings>()->ApiKey;
    UE_LOG(LogNeocortex, Log, TEXT("UNeocortexHttpClient initialized (base: %s)"), *BaseUrl);
}

FNeocortexRequestHandle UNeocortexHttpClient::PostJson(const FString& Path,
                                                       const FString& JsonBody,
                                                       const TCHAR* Accept,
                                                       FNeocortexHttpRawDelegate Callback)
{
    FNeocortexRequestHandle Handle;

    const FString Url = BaseUrl / Path;
    const FString ApiKey = Opts.ApiKey;
    const FString AcceptStr = Accept ? FString(Accept) : FString();
    const float Timeout = Opts.TimeoutSeconds;

    UE_LOG(LogNeocortex, Log, TEXT("POST %s body=%s"), *Url, *JsonBody);

    FRequestFactory Factory = [Url, JsonBody, ApiKey, AcceptStr, Timeout]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(Url);
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        if (!AcceptStr.IsEmpty()) Req->SetHeader(TEXT("Accept"), *AcceptStr);
        if (!ApiKey.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), ApiKey);
        Req->SetTimeout(Timeout);
        Req->SetContentAsString(JsonBody);
        return Req;
    };

    SendWithRetry(Handle, MoveTemp(Factory), MoveTemp(Callback), 0);
    return Handle;
}

FNeocortexRequestHandle UNeocortexHttpClient::PostMultipart(const FString& Path,
                                                            const TMap<FString, FString>& Fields,
                                                            const FString& FileField,
                                                            const FString& FileName,
                                                            const FString& MimeType,
                                                            const TArray<uint8>& Bytes,
                                                            const TCHAR* Accept,
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
    const FString ApiKey = Opts.ApiKey;
    const FString AcceptStr = Accept ? FString(Accept) : FString();
    const float Timeout = Opts.TimeoutSeconds;

    FRequestFactory Factory = [Url, Body, ContentType, ApiKey, AcceptStr, Timeout]()
    {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
        Req->SetURL(Url);
        Req->SetVerb(TEXT("POST"));
        Req->SetHeader(TEXT("Content-Type"), ContentType);
        if (!AcceptStr.IsEmpty()) Req->SetHeader(TEXT("Accept"), *AcceptStr);
        if (!ApiKey.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), ApiKey);
        Req->SetTimeout(Timeout);
        Req->SetContent(Body);
        return Req;
    };

    SendWithRetry(Handle, MoveTemp(Factory), MoveTemp(Callback), 0);
    return Handle;
}

void UNeocortexHttpClient::SendWithRetry(const FNeocortexRequestHandle& Handle,
                                          FRequestFactory Factory,
                                          FNeocortexHttpRawDelegate Callback,
                                          int32 Attempt)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Factory();
    InFlight.Add(Handle.Id, Req);

    TWeakObjectPtr<UNeocortexHttpClient> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, Handle, Callback, Factory, Attempt](
            TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>,
            FHttpResponsePtr Resp, bool bOK)
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->InFlight.Remove(Handle.Id);

            const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
            const bool bRetryable = !bOK || !Resp.IsValid() || Code >= 500;
            if (bRetryable && Attempt < WeakThis->Opts.MaxRetries)
            {
                const float Delay = WeakThis->Opts.RetryBackoffSeconds * (Attempt + 1);
                AsyncTask(ENamedThreads::GameThread, [WeakThis, Handle, Callback, Factory, Attempt, Delay]()
                {
                    if (!WeakThis.IsValid()) return;
                    UWorld* W = GEngine->GetCurrentPlayWorld();
                    if (!W)
                    {
                        // No world during level transition — fail rather than silently drop the retry
                        Callback.ExecuteIfBound(FString(), nullptr);
                        return;
                    }
                    FTimerHandle Timer;
                    W->GetTimerManager().SetTimer(Timer, [WeakThis, Handle, Callback, Factory, Attempt]()
                    {
                        if (!WeakThis.IsValid()) return;
                        WeakThis->SendWithRetry(Handle, Factory, Callback, Attempt + 1);
                    }, Delay, false);
                });
                return;
            }

            const FString Raw = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            // Dispatch on game thread: HTTP callbacks are normally game-thread, but not guaranteed on all platforms.
            AsyncTask(ENamedThreads::GameThread, [Callback, Raw, Resp]()
            {
                Callback.ExecuteIfBound(Raw, Resp);
            });
        });

    Req->ProcessRequest();
}

void UNeocortexHttpClient::Cancel(const FNeocortexRequestHandle& Handle)
{
    if (TSharedRef<IHttpRequest, ESPMode::ThreadSafe>* Req = InFlight.Find(Handle.Id))
    {
        (*Req)->CancelRequest();
        InFlight.Remove(Handle.Id);
    }
}
