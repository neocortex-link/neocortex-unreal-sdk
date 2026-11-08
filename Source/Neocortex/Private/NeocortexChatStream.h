#pragma once
#include "CoreMinimal.h"
#include "NeocortexTypes.h"
#include "NeocortexHttpStream.h"

/**
 * One streamed chat turn from /v3/chat/stream. EXPERIMENTAL on the server.
 *
 * A solo turn raises OnSession, OnDelta*, OnLine*, then OnCompleted. Exactly one of OnCompleted
 * or OnFailed ends a stream that was not cancelled.
 */
class FNeocortexChatStream : public TSharedFromThis<FNeocortexChatStream>
{
public:
    DECLARE_DELEGATE_OneParam(FOnSession, const FString& /*SessionId*/);
    DECLARE_DELEGATE_TwoParams(FOnDelta, const FString& /*Text*/, int32 /*LineIndex*/);
    DECLARE_DELEGATE_OneParam(FOnLine, const FNeocortexChatLine& /*Line*/);
    DECLARE_DELEGATE_OneParam(FOnCompleted, const FNeocortexGroupChatResponse& /*Response*/);
    DECLARE_DELEGATE_OneParam(FOnFailed, const FNeocortexRequestError& /*Error*/);

    explicit FNeocortexChatStream(const FString& InSoloCharacterId) : SoloCharacterId(InSoloCharacterId) {}

    void Attach(const TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe>& InHttp);
    void Cancel();

    FOnSession OnSession;
    FOnDelta OnDelta;
    FOnLine OnLine;
    FOnCompleted OnCompleted;
    FOnFailed OnFailed;

private:
    FString SoloCharacterId;
    FString SessionId;
    bool bEnded = false;
    TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> Http;
    FNeocortexSseParser Parser;

    void HandleEvent(const FString& Name, const TSharedPtr<FJsonObject>& Data);
    void Fail(const FNeocortexRequestError& Error);
};
