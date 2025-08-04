#include "API/Services/ApiRequest.h"
#include "JsonObjectConverter.h"
#include "NeocortexSettings.h"
#include "API/Models/ApiPayload.h"
#include "Sound/SoundWave.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

FApiRequest::FApiRequest()
{
}

FString FApiRequest::GetApiKey() const
{
	const UNeocortexSettings* NeocortexSettings = GetDefault<UNeocortexSettings>();
	return NeocortexSettings->ApiKey;
}

TSharedPtr<FJsonObject> FApiRequest::ConvertChatMessagesToJson() const
{
	TArray<TSharedPtr<FJsonValue>> JsonMessages;
	for (const FMessage& Msg : Messages)
	{
		TSharedPtr<FJsonObject> JsonMsg = MakeShareable(new FJsonObject());
		JsonMsg->SetStringField("content", Msg.Content);
		JsonMsg->SetStringField("role", Msg.Role);
		JsonMessages.Add(MakeShareable(new FJsonValueObject(JsonMsg)));
	}

	TSharedPtr<FJsonObject> Wrapper = MakeShareable(new FJsonObject());
	Wrapper->SetArrayField("messages", JsonMessages);
	return Wrapper;
}

void FApiRequest::SendText(const FString& ProjectId, const FString& Text, bool bExpectAudio)
{
	if (ProjectId.IsEmpty())
	{
		OnRequestFailed(TEXT("Project ID is required"));
		return;
	}

	FString ApiKey = GetApiKey();
	if (ApiKey.IsEmpty())
	{
		OnRequestFailed(TEXT("API Key is required."));
		return;
	}

	SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	SetHeader(TEXT("x-api-key"), ApiKey);

	FMessage Msg;
	Msg.Content = *Text;
	Msg.Role = TEXT("user");
	Messages.Add(Msg);

	// Step 3: Chat
	TSharedPtr<FJsonObject> ChatData = ConvertChatMessagesToJson();

	FApiPayload ChatPayload;
	ChatPayload.Url = FString::Printf(TEXT("%s/chat/%s"), *BaseUrl, *ProjectId);
	ChatPayload.Method = TEXT("POST");
	ChatPayload.Data = GetBytesFromJson(ChatData);

	//TODO fix 
	Send(ChatPayload, [this, bExpectAudio, ProjectId](FHttpResponsePtr Response, bool bSuccess)
	{
		if (bSuccess)
		{
			HandleChatResponse(Response);
	
			if (bExpectAudio)
			{
				// Step 4: Audio
				TSharedPtr<FJsonObject> AudioJson = MakeShareable(new FJsonObject());
				AudioJson->SetStringField("text", Messages.Last().Content);
	
				FApiPayload AudioPayload;
				AudioPayload.Url = FString::Printf(TEXT("%s/audio/%s"), *BaseUrl, *ProjectId);
				AudioPayload.Method = TEXT("POST");
				AudioPayload.Data = GetBytesFromJson(AudioJson);
				AudioPayload.DataType = EApiResponseDataType::Audio;
	
				Send(AudioPayload, [this](FHttpResponsePtr Response, bool bSuccess)
				{
					if (bSuccess)
					{
						HandleAudioResponse(Response);
					}
					else
					{
						OnRequestFailed(TEXT("Audio response failed."));
					}
				});
			}
		}
		else
		{
			OnRequestFailed(TEXT("Chat request failed."));
		}
	});
}

void FApiRequest::SendAudio(const FString& ProjectId, USoundWave* SoundWave, bool bExpectAudio)
{
	if (ProjectId.IsEmpty())
	{
		OnRequestFailed(TEXT("Project ID is required"));
		return;
	}

	FString ApiKey = GetApiKey();
	if (ApiKey.IsEmpty())
	{
		OnRequestFailed(TEXT("API Key is required."));
		return;
	}

	SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	SetHeader(TEXT("x-api-key"), ApiKey);

	TArray<uint8> AudioData;
	// TODO: Encode Audio (WAV, MP3 etc.)
		
	FApiPayload TranscribePayload;
	TranscribePayload.Url = FString::Printf(TEXT("%s/transcribe/%s"), *BaseUrl, *ProjectId);
	TranscribePayload.Method = TEXT("POST");
	TranscribePayload.Data = AudioData;

	Send(TranscribePayload, [this](FHttpResponsePtr Response, bool bSuccess)
	{
		if (bSuccess)
		{
			HandleTranscriptionResponse(Response);
		}
		else
		{
			OnRequestFailed(TEXT("Transcription failed."));
		}
	});
}

void FApiRequest::HandleTranscriptionResponse(FHttpResponsePtr Response)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
	{
		const FString Transcription = Json->GetStringField("transcription");
		Messages.Add({Transcription, TEXT("user")});
		OnTranscriptionReceived(Transcription);
	}
}

void FApiRequest::HandleChatResponse(FHttpResponsePtr Response)
{
	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
	{
		FString Message = Json->GetStringField("message");
		FString Action = Json->GetStringField("action");

		Messages.Add({Message, TEXT("assistant")});
		OnChatResponseReceived({Message, Action});
	}
}

// TODO: Handle audio decoding properly
void FApiRequest::HandleAudioResponse(FHttpResponsePtr Response)
{
	// Assume raw PCM or WAV and decode as needed
	// You can use custom decoding or plugins here

	TArray<uint8> AudioData = Response->GetContent();

	// For demo purposes, we're not decoding
	//USoundWave* Sound = NewObject<USoundWave>();
	

	//OnAudioResponseReceived(Sound);
}

