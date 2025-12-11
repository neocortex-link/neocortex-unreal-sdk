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
    UE_LOG (LogNeocortex, Log, TEXT("Initialized UNeoHttpClient with API Key: %s"), *Opts.ApiKey);
}

FNeocortexRequestHandle UNeocortexHttpClient::PostJson(const FString& Path,
                                           const FString& JsonBody,
                                           const TCHAR* Accept,
                                           FNeocortexHttpRawDelegate Callback)
{
    auto& Http = FHttpModule::Get();
    FNeocortexRequestHandle Handle;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();
    Req->SetURL(BaseUrl / Path);
    UE_LOG(LogNeocortex, Log, TEXT("POST %s"), *(BaseUrl / Path));
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    if (Accept && *Accept) Req->SetHeader(TEXT("Accept"), Accept);
    if (!Opts.ApiKey.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), Opts.ApiKey);
    Req->SetTimeout(Opts.TimeoutSeconds);
    Req->SetContentAsString(JsonBody);
    UE_LOG(LogNeocortex, Log, TEXT("POST BODY = %s"), *(JsonBody));

    InFlight.Add(Handle.Id, Req);
    AttachCompletion(Handle, Req, Callback, 0);
    Req->ProcessRequest();
    return Handle;
}

FNeocortexRequestHandle UNeocortexHttpClient::PostMultipart(const FString& Path,
                                                const TMap<FString,FString>& Fields,
                                                const FString& FileField,
                                                const FString& FileName,
                                                const FString& MimeType,
                                                const TArray<uint8>& Bytes,
                                                const TCHAR* Accept,
                                                FNeocortexHttpRawDelegate Callback)
{
    auto& Http = FHttpModule::Get();
    FNeocortexRequestHandle Handle;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = Http.CreateRequest();

    const FString Boundary = TEXT("----NeoBoundary") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<uint8> Body;
    auto AddLine = [&Body](const FString& S)
    {
        FTCHARToUTF8 Conv(*S);
        Body.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
        Body.Append({'\r','\n'});
    };
    for (auto& KV : Fields)
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

    Req->SetURL(BaseUrl / Path);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), FString::Printf(TEXT("multipart/form-data; boundary=%s"), *Boundary));
    if (Accept && *Accept) Req->SetHeader(TEXT("Accept"), Accept);
    if (!Opts.ApiKey.IsEmpty()) Req->SetHeader(TEXT("x-api-key"), Opts.ApiKey);
    Req->SetTimeout(Opts.TimeoutSeconds);
    Req->SetContent(Body);
    InFlight.Add(Handle.Id, Req);
    AttachCompletion(Handle, Req, Callback, 0);
    Req->ProcessRequest();
    return Handle;
}

void UNeocortexHttpClient::AttachCompletion(const FNeocortexRequestHandle& H,
                                      TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req,
                                      FNeocortexHttpRawDelegate Cb,
                                      int32 Attempt)
{
    TWeakObjectPtr<UNeocortexHttpClient> WeakThis(this);
    Req->OnProcessRequestComplete().BindLambda(
        [WeakThis, H, Cb, Attempt](TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request,
                                   FHttpResponsePtr Resp, bool bOK)
        {
            if (!WeakThis.IsValid()) return;
            WeakThis->InFlight.Remove(H.Id);
            const bool Retryable = !bOK || !Resp.IsValid() || !EHttpResponseCodes::IsOk(Resp->GetResponseCode());
            if (Retryable && Attempt < WeakThis->Opts.MaxRetries)
            {
                // simple linear backoff
                const float Delay = WeakThis->Opts.RetryBackoffSeconds * (Attempt + 1);
                AsyncTask(ENamedThreads::GameThread, [WeakThis, H, Cb, Request, Attempt, Delay]()
                {
                    if (!WeakThis.IsValid()) return;
                    FTimerHandle Timer;
                    if (UWorld* W = GEngine->GetCurrentPlayWorld())
                    {
                        W->GetTimerManager().SetTimer(Timer, [WeakThis, H, Cb, Attempt, Request]()
                        {
                            if (!WeakThis.IsValid()) return;
                            WeakThis->AttachCompletion(H, Request.ToSharedRef(), Cb, Attempt + 1);
                            Request->ProcessRequest();
                        }, Delay, false);
                    }
                });
                return;
            }
            const FString Raw = Resp.IsValid() ? Resp->GetContentAsString() : FString();
            AsyncTask(ENamedThreads::GameThread, [Cb, Raw, Resp]() { Cb.ExecuteIfBound(Raw, Resp); });
        });
}

void UNeocortexHttpClient::Cancel(const FNeocortexRequestHandle& Handle)
{
    if (TSharedRef<IHttpRequest, ESPMode::ThreadSafe>* Req = InFlight.Find(Handle.Id))
    {
        (*Req)->CancelRequest();
        InFlight.Remove(Handle.Id);
    }
}
