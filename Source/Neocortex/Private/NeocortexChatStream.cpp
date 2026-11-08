#include "NeocortexChatStream.h"
#include "NeocortexService.h"

void FNeocortexChatStream::Attach(const TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe>& InHttp)
{
    Http = InHttp;
    TWeakPtr<FNeocortexChatStream> Weak = AsShared();
    Parser.OnEvent.BindLambda([Weak](const FString& Name, const TSharedPtr<FJsonObject>& Data)
    {
        if (const TSharedPtr<FNeocortexChatStream> Self = Weak.Pin()) Self->HandleEvent(Name, Data);
    });
    Http->OnChunk.BindLambda([Weak](const TArray<uint8>& Bytes)
    {
        if (const TSharedPtr<FNeocortexChatStream> Self = Weak.Pin()) Self->Parser.Feed(Bytes);
    });
    Http->OnFinished.BindLambda([Weak](const FNeocortexRequestError* Error)
    {
        const TSharedPtr<FNeocortexChatStream> Self = Weak.Pin();
        if (!Self.IsValid()) return;
        Self->Parser.Complete();
        if (Self->bEnded) return;
        Self->Fail(Error ? *Error : FNeocortexRequestError::FromMessage(TEXT("The chat stream ended before the turn was complete.")));
    });
}

void FNeocortexChatStream::Cancel()
{
    bEnded = true;
    if (Http.IsValid()) Http->Cancel();
}

void FNeocortexChatStream::Fail(const FNeocortexRequestError& Error)
{
    bEnded = true;
    OnFailed.ExecuteIfBound(Error);
}

void FNeocortexChatStream::HandleEvent(const FString& Name, const TSharedPtr<FJsonObject>& Data)
{
    if (bEnded) return;

    if (Name == TEXT("session"))
    {
        Data->TryGetStringField(TEXT("sessionId"), SessionId);
        OnSession.ExecuteIfBound(SessionId);
    }
    else if (Name == TEXT("delta"))
    {
        FString Text;
        int32 Line = 0;
        Data->TryGetStringField(TEXT("text"), Text);
        Data->TryGetNumberField(TEXT("line"), Line);
        OnDelta.ExecuteIfBound(Text, Line);
    }
    else if (Name == TEXT("line"))
    {
        OnLine.ExecuteIfBound(UNeocortexService::ParseChatLine(Data));
    }
    else if (Name == TEXT("done"))
    {
        FNeocortexGroupChatResponse Response = UNeocortexService::ParseGroupChat(Data);
        if (Response.SessionId.IsEmpty()) Response.SessionId = SessionId;
        // A solo turn ends on the flat reply; it becomes a cast of one.
        if (!Data->HasField(TEXT("messages")))
        {
            FNeocortexGroupMessage Solo = UNeocortexService::ParseGroupMessage(Data);
            if (Solo.CharacterId.IsEmpty()) Solo.CharacterId = SoloCharacterId;
            Response.Messages = { Solo };
        }
        bEnded = true;
        OnCompleted.ExecuteIfBound(Response);
    }
    else if (Name == TEXT("error"))
    {
        FNeocortexRequestError Error;
        Error.Status = 500;
        Data->TryGetNumberField(TEXT("status"), Error.Status);
        Data->TryGetStringField(TEXT("code"), Error.Code);
        if (!Data->TryGetStringField(TEXT("error"), Error.Message)) Error.Message = TEXT("The chat stream failed.");
        Fail(Error);
    }
}
