#pragma once

UENUM(BlueprintType)
enum class EWritingDirection : uint8
{
	LeftToRight,
	RightToLeft
};

UENUM(BlueprintType)
enum class EApiResponseDataType : uint8
{
	Text,
	Audio,
	Texture
};

UENUM(BlueprintType)
enum class EMicrophoneState : uint8
{
	NotActive,
	Booting,
	Recording,
};
