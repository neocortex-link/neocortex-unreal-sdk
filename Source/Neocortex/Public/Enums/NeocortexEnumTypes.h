#pragma once

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
