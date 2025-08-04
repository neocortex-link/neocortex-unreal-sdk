#include "NeocortexSubsystem.h"

#include "NeocortexResponseHandler.h"
#include "API/Services/ApiRequest.h"

TSharedPtr<FApiRequest> UNeocortexSubsystem::CreateRequest()
{
    TSharedPtr<FApiRequest> Request = MakeShared<FApiRequest>();
    ActiveRequests.Add(Request);
    return Request;
}

void UNeocortexSubsystem::SendTextRequest(const FString& ProjectId, const FString& Text, bool bExpectAudio, FNeocortexResponseHandler Handler)
{
    TSharedPtr<FApiRequest> Request = CreateRequest();
    Request->OnTranscriptionReceived = Handler.OnTranscription;
    Request->OnAudioResponseReceived = Handler.OnAudio;
    Request->OnChatResponseReceived = Handler.OnChat;
    Request->OnRequestFailed = Handler.OnError;
    Request->SendText(ProjectId, Text, bExpectAudio);
}

void UNeocortexSubsystem::SendAudioRequest(const FString& ProjectId, USoundWave* SoundWave, bool bExpectAudio, FNeocortexResponseHandler Handler)
{
    TSharedPtr<FApiRequest> Request = CreateRequest();
    Request->OnTranscriptionReceived = Handler.OnTranscription;
    Request->OnAudioResponseReceived = Handler.OnAudio;
    Request->OnChatResponseReceived = Handler.OnChat;
    Request->OnRequestFailed = Handler.OnError;
    Request->SendAudio(ProjectId, SoundWave, bExpectAudio);
}
