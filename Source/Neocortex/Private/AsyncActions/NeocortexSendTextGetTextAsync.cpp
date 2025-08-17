#include "AsyncActions/NeocortexSendTextGetTextAsync.h"
#include "Neocortex.h"
#include "NeocortexSubsystem.h"
#include "Engine/World.h"


UNeocortexSendTextGetTextAsync* UNeocortexSendTextGetTextAsync::SendTextGetText(UObject* WorldContextObject, const FString& ProjectId, const FString& Text)
{
	UE_LOG(LogNeocortex, Log, TEXT("Send text request from UNeocortexSendTextGetTextAsync 1"));
	UNeocortexSendTextGetTextAsync* Action = NewObject<UNeocortexSendTextGetTextAsync>();
	Action->ProjectId = ProjectId;
	Action->Text = Text;
	Action->ContextObject = WorldContextObject;
	return Action;
}

void UNeocortexSendTextGetTextAsync::Activate()
{	
	if (!ContextObject)
	{
		//HandleFailure(TEXT("ContextObject is null"));
		return;
	}

	if (UWorld* World = ContextObject->GetWorld())
	{
		if (UNeocortexSubsystem* Subsystem = World->GetGameInstance()->GetSubsystem<UNeocortexSubsystem>())
		{
			Subsystem->TextToText(ProjectId, Text,
				FOnChatResponse::CreateUObject(this, &UNeocortexSendTextGetTextAsync::HandleSuccess),
				FOnRequestFail::CreateUObject(this, &UNeocortexSendTextGetTextAsync::HandleFailure));
			return;
		}
	}
	//HandleFailure(TEXT("ContextObject is null"));
}

void UNeocortexSendTextGetTextAsync::HandleSuccess(const FChatResponseData& Response)
{
	OnSuccess.Broadcast(Response);
}

void UNeocortexSendTextGetTextAsync::HandleFailure(const FNeoRequestError& Error)
{
	OnFailure.Broadcast(Error);
}

