#include "NeocortexWavEncoder.h"

static void WriteUInt32LE(TArray<uint8>& Out, uint32 Value)
{
	Out.Add(Value & 0xFF);
	Out.Add((Value >> 8) & 0xFF);
	Out.Add((Value >> 16) & 0xFF);
	Out.Add((Value >> 24) & 0xFF);
}

static void WriteUInt16LE(TArray<uint8>& Out, uint16 Value)
{
	Out.Add(Value & 0xFF);
	Out.Add((Value >> 8) & 0xFF);
}

TArray<uint8> FNeocortexWavEncoder::EncodePcm16ToWav(const TArray<uint8>& PcmData, int32 SampleRate, int32 NumChannels)
{
	const uint16 BitsPerSample = 16;
	const uint16 AudioFormatPCM = 1;
	const uint32 ByteRate = SampleRate * NumChannels * (BitsPerSample / 8);
	const uint16 BlockAlign = NumChannels * (BitsPerSample / 8);
	const uint32 DataSize = PcmData.Num();
	const uint32 RiffChunkSize = 36 + DataSize;

	TArray<uint8> Wav;
	Wav.Reserve(44 + DataSize);

	// RIFF header
	Wav.Append({'R','I','F','F'});
	WriteUInt32LE(Wav, RiffChunkSize);
	Wav.Append({'W','A','V','E'});

	// fmt chunk
	Wav.Append({'f','m','t',' '});
	WriteUInt32LE(Wav, 16);                 // PCM fmt chunk size
	WriteUInt16LE(Wav, AudioFormatPCM);     // Audio format
	WriteUInt16LE(Wav, (uint16)NumChannels);
	WriteUInt32LE(Wav, (uint32)SampleRate);
	WriteUInt32LE(Wav, ByteRate);
	WriteUInt16LE(Wav, BlockAlign);
	WriteUInt16LE(Wav, BitsPerSample);

	// data chunk
	Wav.Append({'d','a','t','a'});
	WriteUInt32LE(Wav, DataSize);
	Wav.Append(PcmData);

	return Wav;
}
