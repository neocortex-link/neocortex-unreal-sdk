#pragma once

#include "CoreMinimal.h"
#include "ChatResponse.h"
#include "Sound/SoundWave.h"

struct FNeocortexResponseHandler
{
	TFunction<void(const FChatResponse&)> OnChat;
	TFunction<void(USoundWave*)> OnAudio;
	TFunction<void(const FString&)> OnTranscription;
	TFunction<void(const FString&)> OnError;

	static FNeocortexResponseHandler WithChat(
		TFunction<void(const FChatResponse&)> OnChat,
		TFunction<void(USoundWave*)> OnAudio,
		TFunction<void(const FString&)> OnError)
	{
		return { MoveTemp(OnChat), MoveTemp(OnAudio), nullptr, MoveTemp(OnError) };
	}

	static FNeocortexResponseHandler WithAudio(
		TFunction<void(const FString&)> OnTranscription,
		TFunction<void(const FString&)> OnError)
	{
		return { nullptr, nullptr, MoveTemp(OnTranscription), MoveTemp(OnError) };
	}
};
