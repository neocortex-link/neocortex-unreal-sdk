#pragma once

#include "CoreMinimal.h"

/**
 * Lightweight wrapper around dr_mp3 for decoding MP3 data to PCM16 format.
 * Provides single-shot decoding of entire MP3 files into memory.
 */
struct NEOCORTEX_API FNeocortexDrMp3Decoder
{
    /**
     * Decodes an entire MP3 byte array into interleaved 16-bit PCM samples.
     * @param Data Pointer to MP3 encoded data
     * @param Size Size of the MP3 data in bytes
     * @param OutPcm16 Output array containing interleaved PCM16 samples (frames * channels total samples)
     * @param OutSampleRate Output sample rate in Hz (typically 44100 or 48000)
     * @param OutChannels Output channel count (1 for mono, 2 for stereo)
     * @return True if decoding succeeded, false if the data is invalid or corrupted
     */
    static bool DecodeAllPCM16(const uint8* Data, int32 Size, TArray<int16>& OutPcm16, int32& OutSampleRate, int32& OutChannels);
};
