#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "NeocortexTypes.h"

/**
 * One POST whose response body is handed over as it arrives, rather than when it ends. Runs on
 * its own thread; OnChunk and OnFinished fire on the game thread. Exactly one OnFinished ends a
 * stream that was not cancelled.
 */
class FNeocortexHttpStream : public TSharedFromThis<FNeocortexHttpStream, ESPMode::ThreadSafe>
{
public:
    DECLARE_DELEGATE_OneParam(FOnChunk, const TArray<uint8>& /*Bytes*/);
    /** Error is null when the body arrived whole with a 2xx status. */
    DECLARE_DELEGATE_OneParam(FOnFinished, const FNeocortexRequestError* /*Error*/);

    /** False on platforms where responses cannot be read while they arrive. */
    static bool IsSupported();

    ~FNeocortexHttpStream();

    /**
     * Sends the request. TimeoutSeconds bounds connecting and any silence while the body arrives.
     * @param Headers "Name: Value" lines
     */
    void Open(const FString& Url, const TArray<FString>& Headers, const FString& Body, float TimeoutSeconds);

    /** Stops the request. Neither delegate fires afterwards. */
    void Cancel();

    bool IsFinished() const { return bFinished; }

    FOnChunk OnChunk;
    FOnFinished OnFinished;

    /** State shared with the worker thread. */
    struct FShared;

private:
    TSharedPtr<FShared, ESPMode::ThreadSafe> Shared;
    FTSTicker::FDelegateHandle TickerHandle;
    bool bFinished = false;

    bool Tick(float DeltaTime);
    void Finish(const FNeocortexRequestError* Error);
};

/**
 * Reads a streamed WAV body: one header declaring an unknown length, then 16-bit PCM. Samples
 * come out as they arrive; a sample split across two chunks is carried over.
 */
class FNeocortexPcmStreamParser
{
public:
    /** Returns raw 16-bit little endian samples, header removed. Empty until the header is read. */
    TArray<uint8> Feed(const TArray<uint8>& Bytes);

    /** Call when the body has ended. A body too short to hold a header never was audio. */
    void Complete();

    bool IsHeaderParsed() const { return bHeaderParsed; }
    /** True when the body was not a WAV at all, which means the server sent an error instead. */
    bool IsNotAudio() const { return bRejected; }

    int32 SampleRate = 0;
    int32 Channels = 0;

private:
    bool bHeaderParsed = false;
    bool bRejected = false;
    TArray<uint8> HeaderScan;
    TArray<uint8> Carry;

    /** Where the samples start, -1 when this is not 16-bit PCM, -2 when more bytes are needed. */
    int32 ReadFormat();
    TArray<uint8> TakeSamples(const uint8* Data, int32 Num);
};
