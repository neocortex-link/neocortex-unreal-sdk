#pragma once
#include "CoreMinimal.h"
#include "WebImage.h"
#include "NeocortexHttpClient.generated.h"

/** Delegate fired when an HTTP request completes with raw response data. */
DECLARE_DELEGATE_TwoParams(FNeocortexHttpRawDelegate, const FString& /*Body*/, const FHttpResponsePtr& /*Resp*/);

/** Handle for tracking and canceling in-flight HTTP requests. */
USTRUCT()
struct FNeocortexRequestHandle
{
    GENERATED_BODY()
    FGuid Id;
    FNeocortexRequestHandle() : Id(FGuid::NewGuid()) {}
    bool IsValid() const { return Id.IsValid(); }
};

/** Configuration options for HTTP client behavior. */
USTRUCT()
struct FNeocortexHttpOptions
{
    GENERATED_BODY()
    int32 TimeoutSeconds = 30;
    int32 MaxRetries = 2;
    float RetryBackoffSeconds = 0.5f;
    FString ApiKey;
};

/**
 * HTTP client for making requests to Neocortex API.
 * Handles JSON and multipart requests with automatic retry logic and request tracking.
 */
UCLASS()
class NEOCORTEX_API UNeocortexHttpClient : public UObject
{
    GENERATED_BODY()
public:
    /**
     * Initializes the client with base URL and options.
     * @param InBaseUrl Base URL for all API requests
     * @param HttpOptions Configuration options for the client
     */
    void Init(const FString& InBaseUrl, const FNeocortexHttpOptions& HttpOptions);

    /**
     * Sends a JSON POST request.
     * @param Path API endpoint path (relative to base URL)
     * @param JsonBody JSON payload as string
     * @param Accept Expected response content type
     * @param Callback Delegate called when request completes
     * @return Handle for tracking or canceling the request
     */
    FNeocortexRequestHandle PostJson(const FString& Path,
                               const FString& JsonBody,
                               const TCHAR* Accept,
                               FNeocortexHttpRawDelegate Callback);

    /**
     * Sends a multipart/form-data POST request with file upload.
     * @param Path API endpoint path
     * @param Fields Form fields to include
     * @param FileField Name of the file field
     * @param FileName Name of the uploaded file
     * @param MimeType MIME type of the file
     * @param Bytes File data as byte array
     * @param Accept Expected response content type
     * @param Callback Delegate called when request completes
     * @return Handle for tracking or canceling the request
     */
    FNeocortexRequestHandle PostMultipart(const FString& Path,
                                    const TMap<FString,FString>& Fields,
                                    const FString& FileField,
                                    const FString& FileName,
                                    const FString& MimeType,
                                    const TArray<uint8>& Bytes,
                                    const TCHAR* Accept,
                                    FNeocortexHttpRawDelegate Callback);
    /**
     * Cancels an in-flight HTTP request.
     * @param Handle Handle returned from PostJson or PostMultipart
     */
    void Cancel(const FNeocortexRequestHandle& Handle);

private:
    /** Base URL for all API requests. */
    FString BaseUrl = TEXT("https://neocortex.link/api/v2");
    
    /** HTTP client configuration options. */
    FNeocortexHttpOptions Opts;
    
    /** Map of active requests tracked by GUID. */
    TMap<FGuid, TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> InFlight;

    /**
     * Attaches completion callback with retry logic to an HTTP request.
     * @param H Request handle for tracking
     * @param Req HTTP request object
     * @param Cb Callback delegate to invoke on completion
     * @param Attempt Current retry attempt number
     */
    void AttachCompletion(const FNeocortexRequestHandle& H,
                          TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req,
                          FNeocortexHttpRawDelegate Cb,
                          int32 Attempt);
};
