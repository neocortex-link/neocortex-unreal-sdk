#include "AsyncActions/NeocortexChatAsyncAction.h"
#include "NeocortexResponseHandler.h"
#include "NeocortexSubsystem.h"
#include "API/Services/ApiRequest.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"


void UNeocortexSubsystem::Deinitialize()
{
	for (auto& Request : ActiveRequests)
	{
		if (Request.IsValid())
		{
			Request->Abort();
		}
	}
	ActiveRequests.Empty();
}

UNeocortexChatAsyncAction* UNeocortexChatAsyncAction::SendChatRequest(UObject* WorldContextObject, const FString& ProjectId, const FString& Text, bool bExpectAudio)
{
	UNeocortexChatAsyncAction* Node = NewObject<UNeocortexChatAsyncAction>();
	Node->WorldContextObject = WorldContextObject;
	Node->ProjectId = ProjectId;
	Node->Text = Text;
	Node->bExpectAudio = bExpectAudio;
	return Node;
}

void UNeocortexChatAsyncAction::Activate()
{
	if (!WorldContextObject) return;

	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject);
	if (!GameInstance) return;

	UNeocortexSubsystem* Subsystem = GameInstance->GetSubsystem<UNeocortexSubsystem>();
	if (!Subsystem) return;

	Subsystem->SendTextRequest(ProjectId, Text, bExpectAudio, FNeocortexResponseHandler::WithChat(
		[this](const FChatResponse& Response)
		{
			OnChat.Broadcast(Response);
		},
		[this](USoundWave* Audio)
		{
			OnAudio.Broadcast(Audio);
		},
		[this](const FString& Error)
		{
			OnError.Broadcast(Error);
		}));
}
