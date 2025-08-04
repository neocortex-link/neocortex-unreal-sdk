#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChatResponse.h"
#include "Sound/SoundWave.h"
#include "NeocortexSubsystem.generated.h"


struct FNeocortexResponseHandler;

UCLASS()
class Neocortex_API UNeocortexSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	
	void SendTextRequest(const FString& ProjectId, const FString& Text, bool bExpectAudio, FNeocortexResponseHandler Handler);
	void SendAudioRequest(const FString& ProjectId, USoundWave* SoundWave, bool bExpectAudio, FNeocortexResponseHandler Handler);

private:
	TArray<TSharedPtr<class FApiRequest>> ActiveRequests;

	TSharedPtr<FApiRequest> CreateRequest();
};