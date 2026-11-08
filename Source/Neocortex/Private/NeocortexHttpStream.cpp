#include "NeocortexHttpStream.h"
#include "NeocortexHttpClient.h"
#include "Neocortex.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "HAL/ThreadSafeBool.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <atomic>

#if NEOCORTEX_WITH_STREAMING
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include "curl/curl.h"
#if WITH_SSL
#include "Ssl.h"
#endif
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#include "Misc/EngineVersionComparison.h"

// Removing from the front of a buffer keeps its memory for the next read.
#if UE_VERSION_OLDER_THAN(5, 4, 0)
#define NEOCORTEX_NO_SHRINK false
#else
#define NEOCORTEX_NO_SHRINK EAllowShrinking::No
#endif
#endif
#endif

struct FNeocortexHttpStream::FShared
{
    FThreadSafeBool bCancel;
    FThreadSafeBool bDone;
    std::atomic<int32> Status{0};
    TQueue<TArray<uint8>, EQueueMode::Spsc> Chunks;
    // Written by the worker only, read by the game thread once bDone is set.
    TArray<uint8> ErrorBody;
    bool bTransportFailed = false;
    bool bTimedOut = false;
    FString Url;
};

bool FNeocortexHttpStream::IsSupported()
{
    return NEOCORTEX_WITH_STREAMING != 0;
}

FNeocortexHttpStream::~FNeocortexHttpStream()
{
    if (Shared.IsValid()) Shared->bCancel = true;
    if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

#if NEOCORTEX_WITH_STREAMING
static size_t NeoStreamWrite(char* Ptr, size_t Size, size_t Count, void* User)
{
    FNeocortexHttpStream::FShared* S = static_cast<FNeocortexHttpStream::FShared*>(User);
    const size_t Bytes = Size * Count;
    if (S->bCancel) return 0;
    TArray<uint8> Chunk(reinterpret_cast<const uint8*>(Ptr), static_cast<int32>(Bytes));
    const int32 Status = S->Status.load();
    if (Status >= 200 && Status < 300) S->Chunks.Enqueue(MoveTemp(Chunk));
    else S->ErrorBody.Append(Chunk);
    return Bytes;
}

static size_t NeoStreamHeader(char* Ptr, size_t Size, size_t Count, void* User)
{
    // The status line of the final response, e.g. "HTTP/1.1 200 OK".
    FNeocortexHttpStream::FShared* S = static_cast<FNeocortexHttpStream::FShared*>(User);
    const size_t Bytes = Size * Count;
    if (Bytes > 5 && FCStringAnsi::Strncmp(Ptr, "HTTP/", 5) == 0)
    {
        for (size_t i = 5; i + 1 < Bytes; ++i)
        {
            if (Ptr[i] == ' ')
            {
                S->Status.store(FCStringAnsi::Atoi(Ptr + i + 1));
                break;
            }
        }
    }
    return Bytes;
}

static int NeoStreamProgress(void* User, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<FNeocortexHttpStream::FShared*>(User)->bCancel ? 1 : 0;
}

#if WITH_SSL
static CURLcode NeoStreamSslContext(CURL*, void* SslContext, void*)
{
    FSslModule::Get().GetCertificateManager().AddCertificatesToSslContext(static_cast<SSL_CTX*>(SslContext));
    return CURLE_OK;
}
#endif

static void EnsureCurlGlobalInit()
{
    static bool bInitialized = false;
    check(IsInGameThread());
    if (!bInitialized)
    {
        bInitialized = true;
        curl_global_init(CURL_GLOBAL_ALL);
    }
}
#endif

void FNeocortexHttpStream::Open(const FString& Url, const TArray<FString>& Headers, const FString& Body, float TimeoutSeconds)
{
    Shared = MakeShared<FShared, ESPMode::ThreadSafe>();
    Shared->Url = Url;

#if NEOCORTEX_WITH_STREAMING
    EnsureCurlGlobalInit();

    const FTCHARToUTF8 UrlUtf8(*Url);
    const FTCHARToUTF8 BodyUtf8(*Body);
    TArray<ANSICHAR> UrlBytes(UrlUtf8.Get(), UrlUtf8.Length() + 1);
    TArray<ANSICHAR> BodyBytes(BodyUtf8.Get(), BodyUtf8.Length());
    TArray<TArray<ANSICHAR>> HeaderBytes;
    for (const FString& Header : Headers)
    {
        const FTCHARToUTF8 H(*Header);
        HeaderBytes.Emplace(H.Get(), H.Length() + 1);
    }
    const long Timeout = FMath::Max(1, FMath::CeilToInt(TimeoutSeconds));

    TSharedPtr<FShared, ESPMode::ThreadSafe> S = Shared;
    Async(EAsyncExecution::Thread, [S, UrlBytes, BodyBytes, HeaderBytes, Timeout]()
    {
        CURL* Easy = curl_easy_init();
        if (!Easy)
        {
            S->bTransportFailed = true;
            S->bDone = true;
            return;
        }

        curl_slist* List = nullptr;
        for (const TArray<ANSICHAR>& H : HeaderBytes) List = curl_slist_append(List, H.GetData());
        // No 100-continue round trip before the body.
        List = curl_slist_append(List, "Expect:");

        curl_easy_setopt(Easy, CURLOPT_URL, UrlBytes.GetData());
        curl_easy_setopt(Easy, CURLOPT_POST, 1L);
        curl_easy_setopt(Easy, CURLOPT_POSTFIELDS, BodyBytes.GetData());
        curl_easy_setopt(Easy, CURLOPT_POSTFIELDSIZE, static_cast<long>(BodyBytes.Num()));
        curl_easy_setopt(Easy, CURLOPT_HTTPHEADER, List);
        curl_easy_setopt(Easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(Easy, CURLOPT_CONNECTTIMEOUT, Timeout);
        // Silence for this long ends the stream; a long reply that keeps arriving does not.
        curl_easy_setopt(Easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(Easy, CURLOPT_LOW_SPEED_TIME, Timeout);
        curl_easy_setopt(Easy, CURLOPT_WRITEFUNCTION, NeoStreamWrite);
        curl_easy_setopt(Easy, CURLOPT_WRITEDATA, S.Get());
        curl_easy_setopt(Easy, CURLOPT_HEADERFUNCTION, NeoStreamHeader);
        curl_easy_setopt(Easy, CURLOPT_HEADERDATA, S.Get());
        curl_easy_setopt(Easy, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(Easy, CURLOPT_XFERINFOFUNCTION, NeoStreamProgress);
        curl_easy_setopt(Easy, CURLOPT_XFERINFODATA, S.Get());
        curl_easy_setopt(Easy, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(Easy, CURLOPT_SSL_VERIFYHOST, 2L);
#if WITH_SSL
        curl_easy_setopt(Easy, CURLOPT_SSL_CTX_FUNCTION, NeoStreamSslContext);
#endif

        const CURLcode Result = curl_easy_perform(Easy);
        long Code = 0;
        curl_easy_getinfo(Easy, CURLINFO_RESPONSE_CODE, &Code);
        if (Code > 0) S->Status.store(static_cast<int32>(Code));
        S->bTimedOut = Result == CURLE_OPERATION_TIMEDOUT;
        S->bTransportFailed = Result != CURLE_OK;

        curl_slist_free_all(List);
        curl_easy_cleanup(Easy);
        S->bDone = true;
    });
#else
    Shared->bTransportFailed = true;
    Shared->bDone = true;
#endif

    TWeakPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> Weak = AsShared();
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaTime)
    {
        const TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> Self = Weak.Pin();
        return Self.IsValid() && Self->Tick(DeltaTime);
    }));
}

bool FNeocortexHttpStream::Tick(float)
{
    if (bFinished || !Shared.IsValid()) return false;

    // Read the flag before draining, so no chunk queued before it was set is missed.
    const bool bDone = Shared->bDone;
    TArray<uint8> Chunk;
    while (Shared->Chunks.Dequeue(Chunk))
    {
        OnChunk.ExecuteIfBound(Chunk);
        if (bFinished) return false; // cancelled from a callback
    }
    if (!bDone) return true;

    const int32 Status = Shared->Status.load();
    if (Status >= 200 && Status < 300 && !Shared->bTransportFailed)
    {
        Finish(nullptr);
    }
    else
    {
        const FUTF8ToTCHAR Body(reinterpret_cast<const ANSICHAR*>(Shared->ErrorBody.GetData()), Shared->ErrorBody.Num());
        const bool bAnswered = Status >= 300;
        const FNeocortexRequestError Error = UNeocortexHttpClient::MakeError(bAnswered ? Status : 0,
            FString(Body.Length(), Body.Get()), Shared->bTimedOut, Shared->Url);
        Finish(&Error);
    }
    return false;
}

void FNeocortexHttpStream::Finish(const FNeocortexRequestError* Error)
{
    bFinished = true;
    TickerHandle.Reset();
    OnFinished.ExecuteIfBound(Error);
}

void FNeocortexHttpStream::Cancel()
{
    if (Shared.IsValid()) Shared->bCancel = true;
    bFinished = true;
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
}

// Server-Sent Events

void FNeocortexSseParser::Feed(const TArray<uint8>& Bytes)
{
    Pending.Append(Bytes);
    int32 Newline;
    while (Pending.Find('\n', Newline))
    {
        const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), Newline);
        FString Line(Conv.Length(), Conv.Get());
        Line.RemoveFromEnd(TEXT("\r"));
        Pending.RemoveAt(0, Newline + 1, NEOCORTEX_NO_SHRINK);
        HandleLine(Line);
    }
}

void FNeocortexSseParser::Complete()
{
    if (Pending.Num() > 0)
    {
        const FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), Pending.Num());
        FString Line(Conv.Length(), Conv.Get());
        Line.RemoveFromEnd(TEXT("\r"));
        Pending.Reset();
        HandleLine(Line);
    }
    Dispatch();
}

void FNeocortexSseParser::HandleLine(const FString& Line)
{
    if (Line.IsEmpty())
    {
        Dispatch();
        return;
    }
    if (Line.StartsWith(TEXT(":"))) return;

    int32 Colon = INDEX_NONE;
    Line.FindChar(TEXT(':'), Colon);
    const FString Field = Colon == INDEX_NONE ? Line : Line.Left(Colon);
    FString Value = Colon == INDEX_NONE ? FString() : Line.Mid(Colon + 1);
    if (Value.StartsWith(TEXT(" "))) Value.RightChopInline(1);

    if (Field == TEXT("event")) EventName = Value;
    else if (Field == TEXT("data")) DataLines.Add(Value);
}

void FNeocortexSseParser::Dispatch()
{
    if (DataLines.Num() == 0 && EventName.IsEmpty()) return;

    const FString Text = FString::Join(DataLines, TEXT("\n"));
    TSharedPtr<FJsonObject> Data;
    if (Text.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Data) || !Data.IsValid())
    {
        Data = MakeShared<FJsonObject>();
    }
    FString Name = EventName;
    if (Name.IsEmpty() && !Data->TryGetStringField(TEXT("type"), Name)) Name = TEXT("message");

    EventName.Reset();
    DataLines.Reset();
    OnEvent.ExecuteIfBound(Name, Data);
}

// Streamed WAV

static uint32 ReadU32(const TArray<uint8>& B, int32 At) { return B[At] | (B[At + 1] << 8) | (B[At + 2] << 16) | (uint32(B[At + 3]) << 24); }
static uint16 ReadU16(const TArray<uint8>& B, int32 At) { return B[At] | (B[At + 1] << 8); }
static bool HasTag(const TArray<uint8>& B, int32 At, const char* Tag) { return FMemory::Memcmp(B.GetData() + At, Tag, 4) == 0; }

TArray<uint8> FNeocortexPcmStreamParser::Feed(const TArray<uint8>& Bytes)
{
    if (Bytes.Num() == 0 || bRejected) return {};
    if (bHeaderParsed) return TakeSamples(Bytes.GetData(), Bytes.Num());

    HeaderScan.Append(Bytes);
    if (HeaderScan.Num() >= 4 && !HasTag(HeaderScan, 0, "RIFF"))
    {
        bRejected = true;
        return {};
    }
    if (HeaderScan.Num() < 44) return {};

    const int32 DataOffset = ReadFormat();
    if (DataOffset == -2) return {}; // the data chunk may not have arrived yet behind a long header
    if (DataOffset < 0)
    {
        bRejected = true;
        return {};
    }

    bHeaderParsed = true;
    TArray<uint8> Out = HeaderScan.Num() > DataOffset
        ? TakeSamples(HeaderScan.GetData() + DataOffset, HeaderScan.Num() - DataOffset)
        : TArray<uint8>();
    HeaderScan.Empty();
    return Out;
}

void FNeocortexPcmStreamParser::Complete()
{
    if (!bHeaderParsed && !bRejected && HeaderScan.Num() > 0) bRejected = true;
}

int32 FNeocortexPcmStreamParser::ReadFormat()
{
    if (!HasTag(HeaderScan, 8, "WAVE")) return -1;

    int32 Offset = 12;
    while (Offset + 8 <= HeaderScan.Num())
    {
        const uint32 ChunkSize = ReadU32(HeaderScan, Offset + 4);
        if (HasTag(HeaderScan, Offset, "fmt "))
        {
            if (Offset + 24 > HeaderScan.Num()) return -2;
            Channels = ReadU16(HeaderScan, Offset + 10);
            SampleRate = static_cast<int32>(ReadU32(HeaderScan, Offset + 12));
            if (ReadU16(HeaderScan, Offset + 22) != 16) return -1;
        }
        else if (HasTag(HeaderScan, Offset, "data"))
        {
            return SampleRate > 0 && Channels > 0 ? Offset + 8 : -1;
        }
        // Chunks are word aligned; an odd size carries a pad byte.
        Offset += 8 + static_cast<int32>(ChunkSize) + static_cast<int32>(ChunkSize % 2);
    }
    return -2;
}

TArray<uint8> FNeocortexPcmStreamParser::TakeSamples(const uint8* Data, int32 Num)
{
    Carry.Append(Data, Num);
    // An odd byte at the end is the first half of a sample still in flight.
    const int32 Usable = Carry.Num() - (Carry.Num() % 2);
    TArray<uint8> Out(Carry.GetData(), Usable);
    Carry.RemoveAt(0, Usable, NEOCORTEX_NO_SHRINK);
    return Out;
}
