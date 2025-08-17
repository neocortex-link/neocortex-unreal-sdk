#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSendTextGetTextAsync.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatResponseDynamic, const FChatResponseData&, ChatResponse);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnApiErrorDynamic, const FNeoRequestError&, Error);

UCLASS()
class NEOCORTEX_API UNeocortexSendTextGetTextAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"))
	static UNeocortexSendTextGetTextAsync* SendTextGetText(UObject* WorldContextObject, const FString& ProjectId, const FString& Text);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FOnChatResponseDynamic OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FOnApiErrorDynamic OnFailure;

private:
	void HandleSuccess(const FChatResponseData& Response);
	void HandleFailure(const FNeoRequestError& Error);

	FString ProjectId;
	FString Text;
	UObject* ContextObject;
};