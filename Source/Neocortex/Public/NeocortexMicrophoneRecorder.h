#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceCapture.h"

class FWavEncoder;

/**
 * Low-level microphone recording class for capturing and processing audio input.
 * Captures raw PCM16 audio data and provides WAV encoding and resampling utilities.
 * Not exposed to Blueprint - use UNeocortexMicrophoneRecorder for Blueprint integration.
 */
class NEOCORTEX_API FNeocortexMicrophoneRecorder
{
public:
    /** Logs all available audio input devices to the console. */
    void ListInputDevices() const;

    /**
     * Creates a microphone recorder with specified audio format.
     * @param InSampleRate Target sample rate in Hz (default 48000)
     * @param InNumChannels Number of audio channels (1=mono, 2=stereo, default 2)
     */
    FNeocortexMicrophoneRecorder(int32 InSampleRate = 48000, int32 InNumChannels = 2);

    ~FNeocortexMicrophoneRecorder();

    /**
     * Begins capturing audio from the default input device.
     * @return True if recording started successfully, false if device is unavailable or already recording
     */
    bool StartRecording();

    /** Stops capturing audio and flushes any remaining buffered data. */
    void StopRecording();

    /**
     * Processes buffered audio data from the voice capture system.
     * @param DeltaTime Time elapsed since last tick in seconds
     */
    void Tick(float DeltaTime);

    /**
     * Returns the raw captured PCM16 audio data (little-endian, interleaved if stereo).
     * @return Reference to buffer containing raw PCM samples
     */
    const TArray<uint8>& GetPcmData() const { return PcmBuffer; }

    /**
     * Encodes the captured PCM data into a complete WAV file.
     * @return WAV file as byte array with RIFF header
     */
    TArray<uint8> GetWavData() const;

    /**
     * Converts captured audio to 16kHz mono and encodes as WAV.
     * @return Resampled and downmixed WAV file (optimized for speech recognition)
     */
    TArray<uint8> GetWavData16kMono() const;

    /**
     * Checks if the current hardware configuration supports 16kHz mono capture.
     * @return True if native 16kHz mono is available, false if resampling is required
     */
    bool Supports16kMono() const;

    /** Returns true if currently capturing audio. */
    bool IsRecording() const { return bIsRecording; }

    /** Returns the configured sample rate in Hz. */
    int32 GetSampleRate() const { return SampleRate; }

    /** Returns the configured channel count. */
    int32 GetNumChannels() const { return NumChannels; }

private:
    /** Platform-specific voice capture interface. */
    TSharedPtr<IVoiceCapture> VoiceCapture;

    /** Accumulated PCM16 audio samples (little-endian, interleaved). */
    TArray<uint8> PcmBuffer;

    /** True when actively capturing audio. */
    bool bIsRecording = false;

    /** Target sample rate in Hz. */
    int32 SampleRate = 48000;

    /** Number of audio channels (1=mono, 2=stereo). */
    int32 NumChannels = 2;
};
