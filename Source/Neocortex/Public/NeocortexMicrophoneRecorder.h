#pragma once

#include "CoreMinimal.h"
#include "Interfaces/VoiceCapture.h"

/**
 * Low-level microphone recording class for capturing and processing audio input.
 * Captures raw PCM16 audio data and provides WAV encoding and resampling utilities.
 * Not exposed to Blueprint - use UNeocortexMicrophoneRecorder for Blueprint integration.
 */
class NEOCORTEX_API FNeocortexMicrophoneRecorder
{
public:
    /** Returns the display names of all available audio input devices. */
    static TArray<FString> GetAvailableDeviceNames();

    /** Logs all available audio input devices to the console. */
    void ListInputDevices() const;

    /**
     * Sets a preferred device by name. Takes effect on the next StartRecording() call.
     * Pass an empty string to revert to automatic selection.
     */
    void SetPreferredDevice(const FString& DeviceName);

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

    /** Returns the RMS amplitude of the most recent audio chunk (0–1). Updated each Tick. */
    float GetAmplitude() const { return Amplitude; }

    /** Returns the configured sample rate in Hz. */
    int32 GetSampleRate() const { return SampleRate; }

    /** Returns the configured channel count. */
    int32 GetNumChannels() const { return NumChannels; }

private:
    /** Platform-specific voice capture interface. */
    TSharedPtr<IVoiceCapture> VoiceCapture;

    /** Accumulated PCM16 audio samples (little-endian, interleaved). */
    TArray<uint8> PcmBuffer;

    /** True when the VoiceCapture device is open and running (includes prewarm phase). */
    bool bIsPrewarmed = false;

    /** True when actively accumulating audio into PcmBuffer. */
    bool bIsRecording = false;

    /** Device name to try first; empty means auto-select the first working device. */
    FString PreferredDeviceName;

    /** RMS amplitude of the most recent captured chunk (0–1). */
    float Amplitude = 0.f;

    /** Target sample rate in Hz. */
    int32 SampleRate = 48000;

    /** Number of audio channels (1=mono, 2=stereo). */
    int32 NumChannels = 2;

    /** Starts the VoiceCapture device without opening recording. */
    void StartPrewarm();

    /** Stops the VoiceCapture device entirely. */
    void StopPrewarm();
};
