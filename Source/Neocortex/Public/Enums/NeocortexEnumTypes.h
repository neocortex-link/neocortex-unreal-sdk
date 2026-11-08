#pragma once
#include "CoreMinimal.h"
#include "NeocortexEnumTypes.generated.h"

/** Text rendering direction for localization support */
UENUM(BlueprintType)
enum class EWritingDirection : uint8
{
	/** Left-to-right scripts (English, Spanish, etc.) */
	LeftToRight,

	/** Right-to-left scripts (Arabic, Hebrew, etc.) */
	RightToLeft
   };

/** Format of API response data */
UENUM(BlueprintType)
enum class EApiResponseDataType : uint8
{
	/** Text-based response (JSON, plain text) */
	Text,

	/** Audio-based response (MP3, WAV, etc.) */
	Audio
   };

/** Current state of the microphone input system */
UENUM(BlueprintType)
enum class EMicrophoneState : uint8
{
	/** Microphone is inactive and not capturing audio */
	NotActive,

	/** Microphone is initializing hardware and preparing to record */
	Booting,

	/** Microphone is actively capturing audio data */
	Recording,
   };

/**
 * Controls how a chat reply is delivered by UNeocortexSmartAgent.
 */
UENUM(BlueprintType)
enum class EChatLinesMode : uint8
{
	/** One reply, delivered whole. Nothing is played for you. */
	Off,

	/** Lines drop in as messages with an emotion each. No audio, no extra cost. */
	Text,

	/** Lines drop in while ONE voice clip plays for the whole reply. 1 audio credit. */
	SingleAudio,

	/**
	 * Each line is voiced separately, in order, in its own emotion.
	 * WARNING: costs about 1 audio credit PER LINE. Falls back to one clip when credits run low
	 * and to text only when they are empty.
	 */
	PerLineAudio,
};

/** When the actions of a reply are run by UNeocortexSmartAgent. */
UENUM(BlueprintType)
enum class EActionTrigger : uint8
{
	/** The instant the reply arrives; the character may act before it speaks. */
	WhenResponseReceived,

	/** As the character starts speaking the reply. */
	WhenSpeechStarts,

	/** Once the character has finished speaking the reply. */
	AfterReplySpoken,
};

/** Team credit status returned by the /usage endpoint. */
UENUM(BlueprintType)
enum class EUsageStatus : uint8
{
	/** Credits available. */
	Ok,

	/** Credits below the dashboard low-credit threshold. */
	Low,

	/** Credits exhausted. */
	Empty,
};

/** Priority level for event log entries sent with API requests. */
UENUM(BlueprintType)
enum class ENeocortexEventPriority : uint8
{
    Low    = 0,
    Medium = 1,
    High   = 2,
};

/**
 * Emotional states for character expression and dialogue systems.
 * Organized by intensity quadrants: calm/energetic and positive/negative.
 */
UENUM(BlueprintType)
enum class EEmotions : uint8
{
	Neutral,
	Happy,
	Pleased,
	Disappointed,
	Upset,

	Amazed,
	Curious,
	Confused,
	Alarmed,

	Fascinated,
	Impressed,
	Annoyed,
	Angry,

	Confident,
	Reassured,
	Concerned,
	Scared,
   };
