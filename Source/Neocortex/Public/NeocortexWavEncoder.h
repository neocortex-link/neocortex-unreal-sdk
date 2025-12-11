#pragma once

#include "CoreMinimal.h"

/**
 * Utility class for encoding PCM audio data to WAV format.
 * Provides static methods for audio format conversion.
 */
class FNeocortexWavEncoder
{
public:
	/**
	 * Encodes 16-bit PCM audio data to WAV format.
	 * @param PcmData Raw PCM audio samples as bytes
	 * @param SampleRate Audio sample rate in Hz (e.g., 44100, 48000)
	 * @param NumChannels Number of audio channels (1 for mono, 2 for stereo)
	 * @return WAV-formatted audio data including header
	 */
	static TArray<uint8> EncodePcm16ToWav(const TArray<uint8>& PcmData, int32 SampleRate, int32 NumChannels);
};
