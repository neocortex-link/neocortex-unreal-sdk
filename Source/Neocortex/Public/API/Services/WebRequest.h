#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "Dom/JsonObject.h"

struct FApiPayload;

class Neocortex_API FWebRequest : public TSharedFromThis<FWebRequest>
{
public:
	FWebRequest();
	~FWebRequest();

	void SetHeader(const FString& Key, const FString& Value);
	void Abort();

	void Send(const FApiPayload& Payload, TFunction<void(FHttpResponsePtr Response, bool bSuccess)> Callback);
	static TArray<uint8> GetBytesFromJson(const TSharedPtr<FJsonObject>& JsonObject);

	float GetProgress() const;

private:
	TMap<FString, FString> Headers;
	FHttpRequestPtr HttpRequest;
	float Progress;
};
