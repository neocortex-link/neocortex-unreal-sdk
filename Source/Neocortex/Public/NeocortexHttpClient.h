#pragma once
#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "NeocortexTypes.h"
#include "NeocortexHttpClient.generated.h"

/**
 * Delegate fired when an HTTP request completes. Error is null on success (a 2xx answer) and
 * describes the failure otherwise. It is not fired for a cancelled request.
 */
DECLARE_DELEGATE_ThreeParams(FNeocortexHttpRawDelegate, const FString& /*Body*/, const FHttpResponsePtr& /*Resp*/, const FNeocortexRequestError* /*Error*/);

/** Handle for tracking and canceling in-flight HTTP requests. */
USTRUCT()
struct FNeocortexRequestHandle
{
    GENERATED_BODY()
    FGuid Id;
    FNeocortexRequestHandle() : Id(FGuid::NewGuid()) {}
    bool IsValid() const { return Id.IsValid(); }
};

/**
 * Groups the requests of one operation (a transcription, the chat turn after it, the speech
 * after that) so they can be cancelled together. Once cancelled, nothing in the group calls
 * back and no further step is sent.
 */
struct NEOCORTEX_API FNeocortexCancelToken
{
    bool IsCancelled() const { return bCancelled; }

private:
    friend class UNeocortexHttpClient;
    bool bCancelled = false;
    TArray<FGuid> InFlight;
};

using FNeocortexCancelTokenPtr = TSharedPtr<FNeocortexCancelToken, ESPMode::ThreadSafe>;

/** Per request behavior. */
struct FNeocortexRequestOptions
{
    /** Seconds before this side gives up waiting. */
    float TimeoutSeconds = 30.0f;

    /** How many times a request that failed with a retryable error is sent again. */
    int32 Retries = 0;

    /** Optional group to cancel this request with. */
    FNeocortexCancelTokenPtr Token;

    /** API error codes the caller handles itself, so they are not logged as errors. */
    TArray<FString> HandledCodes;
};

/**
 * HTTP client for making requests to Neocortex API.
 * Handles JSON and multipart requests with retry logic, timeouts and request tracking.
 */
UCLASS()
class NEOCORTEX_API UNeocortexHttpClient : public UObject
{
    GENERATED_BODY()
public:
    /** Seconds between two attempts of a retried request. */
    static constexpr float RetryDelaySeconds = 1.5f;

    /**
     * Initializes the client with its base URL.
     * @param InBaseUrl Base URL for all API requests
     */
    void Init(const FString& InBaseUrl);

    /** Sends a JSON POST request. */
    FNeocortexRequestHandle PostJson(const FString& Path,
                               const FString& JsonBody,
                               const TCHAR* Accept,
                               const FNeocortexRequestOptions& Options,
                               FNeocortexHttpRawDelegate Callback);

    /** Sends a GET request. Path may already contain a "?query" string. */
    FNeocortexRequestHandle GetJson(const FString& Path,
                              const TCHAR* Accept,
                              const FNeocortexRequestOptions& Options,
                              FNeocortexHttpRawDelegate Callback);

    /** Sends a multipart/form-data POST request with one file upload. */
    FNeocortexRequestHandle PostMultipart(const FString& Path,
                                    const TMap<FString,FString>& Fields,
                                    const FString& FileField,
                                    const FString& FileName,
                                    const FString& MimeType,
                                    const TArray<uint8>& Bytes,
                                    const TCHAR* Accept,
                                    const FNeocortexRequestOptions& Options,
                                    FNeocortexHttpRawDelegate Callback);

    /** Cancels an in-flight HTTP request. Its callback is not fired. */
    void Cancel(const FNeocortexRequestHandle& Handle);

    /** Cancels every request in the token's group, now and later. */
    void Cancel(const FNeocortexCancelTokenPtr& Token);

    /** Updates the API key used for subsequent requests. Existing in-flight requests are unaffected. */
    void SetApiKey(const FString& NewKey) { ApiKey = NewKey; }

    /** The base URL requests go to, without a trailing slash. */
    const FString& GetBaseUrl() const { return BaseUrl; }

    /** Builds the classified error for a failed or missing response. */
    static FNeocortexRequestError MakeError(const FHttpResponsePtr& Resp, bool bTimedOut, const FString& Url);

    /** Builds the classified error for an answer with this status and body. Status 0: no answer. */
    static FNeocortexRequestError MakeError(int32 Status, const FString& Body, bool bTimedOut, const FString& Url);

    /** The API key requests are sent with. */
    const FString& GetApiKey() const { return ApiKey; }

private:
    FString BaseUrl = TEXT("https://api.neocortex.link/v3");

    FString ApiKey;

    /** Active requests, tracked by handle id. */
    TMap<FGuid, TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> InFlight;

    /** Handles waiting for a retry to be sent. Cancelling one removes it. */
    TSet<FGuid> PendingRetries;

    using FRequestFactory = TFunction<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>()>;

    // Creates a fresh request via Factory and sends it. A retry calls Factory again, so each
    // attempt uses a new request object.
    void Send(const FNeocortexRequestHandle& Handle,
              FRequestFactory Factory,
              FNeocortexRequestOptions Options,
              FNeocortexHttpRawDelegate Callback,
              int32 Attempt);

    void Forget(const FNeocortexRequestHandle& Handle, const FNeocortexCancelTokenPtr& Token);
};
