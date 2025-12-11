// C++
#include "NeocortexDrMp3.h"

// Define implementation once in this TU.
#define DR_MP3_IMPLEMENTATION
#include "ThirdParty/dr_mp3/dr_mp3.h"

static inline int16 ClampFloatToS16(float x)
{
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    const float scaled = x >= 0.0f ? (x * 32767.0f) : (x * 32768.0f);
    return static_cast<int16>(scaled);
}

bool FNeocortexDrMp3Decoder::DecodeAllPCM16(const uint8* Data, int32 Size, TArray<int16>& OutPcm16, int32& OutSampleRate, int32& OutChannels)
{
    OutPcm16.Reset();
    OutSampleRate = 0;
    OutChannels = 0;

    if (!Data || Size <= 0) return false;

    drmp3 mp3{};
    if (!drmp3_init_memory(&mp3, Data, static_cast<size_t>(Size), nullptr))
        return false;

    OutSampleRate = static_cast<int32>(mp3.sampleRate);
    OutChannels   = static_cast<int32>(mp3.channels);
    if (OutSampleRate <= 0 || (OutChannels != 1 && OutChannels != 2))
    {
        drmp3_uninit(&mp3);
        return false;
    }

    const drmp3_uint64 frameCount = drmp3_get_pcm_frame_count(&mp3);
    drmp3_uint64 targetFrames = frameCount ? frameCount : (1152ull * 64ull);
    const uint64 targetSamples = static_cast<uint64>(targetFrames) * static_cast<uint64>(OutChannels);

    TArray<float> FloatBuf;
    FloatBuf.SetNumUninitialized(static_cast<int32>(targetSamples));

    drmp3_uint64 readFrames = drmp3_read_pcm_frames_f32(&mp3, targetFrames, FloatBuf.GetData());
    const uint64 readSamples = static_cast<uint64>(readFrames) * static_cast<uint64>(OutChannels);

    OutPcm16.SetNumUninitialized(static_cast<int32>(readSamples));
    for (uint64 i = 0; i < readSamples; ++i)
        OutPcm16[static_cast<int32>(i)] = ClampFloatToS16(FloatBuf[static_cast<int32>(i)]);

    drmp3_uninit(&mp3);
    return OutPcm16.Num() > 0;
}
