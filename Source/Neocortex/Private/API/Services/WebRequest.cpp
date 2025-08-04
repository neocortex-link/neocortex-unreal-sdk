#include "API/Services/WebRequest.h"
#include "HttpModule.h"
#include "API/Models/ApiPayload.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

FWebRequest::FWebRequest()
{
	HttpRequest = FHttpModule::Get().CreateRequest();
	Progress = 0.0f;
}

FWebRequest::~FWebRequest()
{
	Abort();
}

void FWebRequest::SetHeader(const FString& Key, const FString& Value)
{
	Headers.Add(Key, Value);
}

void FWebRequest::Abort()
{
	if (HttpRequest.IsValid())
	{
		HttpRequest->CancelRequest();
	}
}

void FWebRequest::Send(const FApiPayload& Payload, TFunction<void(FHttpResponsePtr Response, bool bSuccess)> Callback)
{
	HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Payload.Url);
	HttpRequest->SetVerb(Payload.Method);

	for (const auto& Header : Headers)
	{
		HttpRequest->SetHeader(Header.Key, Header.Value);
	}

	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContent(Payload.Data);

	HttpRequest->OnRequestProgress().BindLambda([this](FHttpRequestPtr Request, int32 BytesSent, int32 BytesReceived)
	{
		if (Request->GetContentLength() > 0)
		{
			Progress = static_cast<float>(BytesSent) / static_cast<float>(Request->GetContentLength());
		}
	});

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, Callback](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (bWasSuccessful && Response.IsValid())
			{
				Callback(Response, true);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("[WebRequest Error] %s"), Response.IsValid() ? *Response->GetContentAsString() : TEXT("No Response"));
				Callback(Response, false);
			}
		}
	);

	HttpRequest->ProcessRequest();
}

TArray<uint8> FWebRequest::GetBytesFromJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return TArray((const uint8*)TCHAR_TO_UTF8(*OutputString), OutputString.Len());
}

float FWebRequest::GetProgress() const
{
	return Progress;
}