#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "ChatResponse.h"
#include "NeocortexChatAsyncAction.generated.h"

class UNeocortexSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatResponseBP, const FChatResponse&, Response);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAudioResponseBP, USoundWave*, Audio);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatErrorBP, const FString&, Error);

UCLASS()
class Neocortex_API UNeocortexChatAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnChatResponseBP OnChat;

	UPROPERTY(BlueprintAssignable)
	FOnAudioResponseBP OnAudio;

	UPROPERTY(BlueprintAssignable)
	FOnChatErrorBP OnError;

	UFUNCTION(BlueprintCallable, Category = "Neocortex|API", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UNeocortexChatAsyncAction* SendChatRequest(UObject* WorldContextObject, const FString& ProjectId, const FString& Text, bool bExpectAudio);

	virtual void Activate() override;

private:
	UPROPERTY()
	UObject* WorldContextObject;

	FString ProjectId;
	FString Text;
	bool bExpectAudio;
};
