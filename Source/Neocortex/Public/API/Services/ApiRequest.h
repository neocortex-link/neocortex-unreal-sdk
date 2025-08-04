#pragma once

#include "CoreMinimal.h"
#include "ChatResponse.h"
#include "Message.h"
#include "WebRequest.h"
#include "Dom/JsonObject.h"
#include "Sound/SoundWave.h"

class Neocortex_API FApiRequest : public FWebRequest
{
public:
	FApiRequest();

	TFunction<void(const FString&)> OnTranscriptionReceived;
	TFunction<void(USoundWave*)> OnAudioResponseReceived;
	TFunction<void(const FChatResponse&)> OnChatResponseReceived;
	TFunction<void(const FString&)> OnRequestFailed;

	void SendText(const FString& ProjectId, const FString& Text, bool bExpectAudio);
	void SendAudio(const FString& ProjectId, USoundWave* SoundWave, bool bExpectAudio);
	
private:
	const FString BaseUrl = TEXT("https://api.neocortex.link/v1");
	TArray<FMessage> Messages;
	FString LastMessage;

	FString GetApiKey() const;
	TSharedPtr<FJsonObject> ConvertChatMessagesToJson() const;

	void HandleTranscriptionResponse(FHttpResponsePtr Response);
	void HandleChatResponse(FHttpResponsePtr Response);
	void HandleAudioResponse(FHttpResponsePtr Response);
	
};
