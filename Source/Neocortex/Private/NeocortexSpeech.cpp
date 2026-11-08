#include "NeocortexSpeech.h"
#include "NeocortexService.h"
#include "NeocortexFunctionLibrary.h"
#include "NeocortexDrMp3.h"
#include "Neocortex.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

// How much streamed speech must be buffered before playback starts, to stay ahead of the network.
static constexpr float PrebufferSeconds = 0.15f;
// Silence after the last streamed sample before the line counts as heard.
static constexpr float DrainSeconds = 0.1f;

bool FNeocortexSpeechJob::bStreamedSpeechSupported = true;

USoundWaveProcedural* NeocortexAudio::DecodeMp3(UObject* Outer, const TArray<uint8>& Bytes, float& OutDuration)
{
    OutDuration = 0.0f;
    if (Bytes.Num() == 0) return nullptr;

    int32 SampleRate = 0, Channels = 0;
    TArray<int16> Pcm;
    if (!FNeocortexDrMp3Decoder::DecodeAllPCM16(Bytes.GetData(), Bytes.Num(), Pcm, SampleRate, Channels) || SampleRate <= 0 || Channels <= 0)
    {
        return nullptr;
    }

    USoundWaveProcedural* SW = NewObject<USoundWaveProcedural>(Outer ? Outer : GetTransientPackage());
    SW->bLooping = false;
    SW->bCanProcessAsync = true;
    SW->SetSampleRate(SampleRate);
    SW->NumChannels = Channels;
    SW->Duration = static_cast<float>(Pcm.Num()) / static_cast<float>(SampleRate * Channels);
    SW->QueueAudio(reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16));

    OutDuration = SW->Duration;
    return SW;
}

FNeocortexSpeechJob::FNeocortexSpeechJob(UNeocortexService* InService, UAudioComponent* InAudio, UObject* InOuter,
    const FString& InCharacterId, const FString& InText, const FString& InSpokenText, EEmotions InEmotion, bool bInStream)
    : Service(InService)
    , Audio(InAudio)
    , Outer(InOuter)
    , CharacterId(InCharacterId)
    , Text(InText)
    , SpokenText(InSpokenText)
    , Emotion(InEmotion)
    , bStreaming(bInStream && bStreamedSpeechSupported && FNeocortexHttpStream::IsSupported())
{
}

FNeocortexSpeechJob::~FNeocortexSpeechJob()
{
    if (Stream.IsValid()) Stream->Cancel();
    if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

void FNeocortexSpeechJob::Start()
{
    TWeakPtr<FNeocortexSpeechJob> Weak = AsShared();
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float DeltaTime)
    {
        const TSharedPtr<FNeocortexSpeechJob> Self = Weak.Pin();
        return Self.IsValid() && Self->Tick(DeltaTime);
    }));

    if (bStreaming) StartStream();
    else StartBuffered();
}

void FNeocortexSpeechJob::Play()
{
    bPlayAllowed = true;
}

void FNeocortexSpeechJob::Stop()
{
    if (bStopped) return;
    bStopped = true;
    if (Stream.IsValid()) Stream->Cancel();
    if (IsOurSoundPlaying()) Audio->Stop();
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
}

void FNeocortexSpeechJob::StartStream()
{
    const FString Speech = UNeocortexFunctionLibrary::HasSomethingToSay(SpokenText) ? SpokenText : Text;
    if (!Service.IsValid() || !UNeocortexFunctionLibrary::HasSomethingToSay(Speech))
    {
        bDownloadDone = true;
        return;
    }

    TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("characterId"), CharacterId);
    Obj->SetStringField(TEXT("message"), Speech);
    Obj->SetStringField(TEXT("spokenText"), Speech);
    Obj->SetStringField(TEXT("emotion"), UNeocortexFunctionLibrary::EmotionToUpper(Emotion));
    FString Body;
    FJsonSerializer::Serialize(Obj, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body));

    Stream = Service->OpenStream(TEXT("audio/stream"), Body, TEXT("audio/wav"), UNeocortexService::AudioTimeoutSeconds);
    TWeakPtr<FNeocortexSpeechJob> Weak = AsShared();
    Stream->OnChunk.BindLambda([Weak](const TArray<uint8>& Bytes)
    {
        if (const TSharedPtr<FNeocortexSpeechJob> Self = Weak.Pin()) Self->OnStreamChunk(Bytes);
    });
    Stream->OnFinished.BindLambda([Weak](const FNeocortexRequestError* Error)
    {
        if (const TSharedPtr<FNeocortexSpeechJob> Self = Weak.Pin()) Self->OnStreamFinished(Error);
    });
}

void FNeocortexSpeechJob::OnStreamChunk(const TArray<uint8>& Bytes)
{
    const TArray<uint8> Samples = Parser.Feed(Bytes);
    if (!Parser.IsHeaderParsed()) return;

    if (!Wave.IsValid())
    {
        USoundWaveProcedural* SW = NewObject<USoundWaveProcedural>(Outer.IsValid() ? Outer.Get() : GetTransientPackage());
        SW->bLooping = false;
        SW->bCanProcessAsync = true;
        SW->SetSampleRate(Parser.SampleRate);
        SW->NumChannels = Parser.Channels;
        // The length is not known until the stream ends; the job stops the voice itself.
        SW->Duration = INDEFINITELY_LOOPING_DURATION;
        Wave.Reset(SW);
    }
    if (Samples.Num() > 0)
    {
        Wave->QueueAudio(Samples.GetData(), Samples.Num());
        SecondsQueued += static_cast<float>(Samples.Num()) / (sizeof(int16) * Parser.SampleRate * Parser.Channels);
    }
}

void FNeocortexSpeechJob::OnStreamFinished(const FNeocortexRequestError* Error)
{
    Parser.Complete();
    if (bStopped) return;

    if (Wave.IsValid() && SecondsQueued > 0.0f)
    {
        // A stream that ends early still plays what it delivered.
        bDownloadDone = true;
        return;
    }

    // Nothing arrived: fetch the line whole instead. That request reports if it fails too.
    if (Error && Error->Status == 404)
    {
        bStreamedSpeechSupported = false;
    }
    Wave.Reset();
    Stream.Reset();
    bStreaming = false;
    StartBuffered();
}

void FNeocortexSpeechJob::StartBuffered()
{
    if (!Service.IsValid())
    {
        bDownloadDone = true;
        return;
    }

    TWeakPtr<FNeocortexSpeechJob> Weak = AsShared();
    Service->GenerateAudio(CharacterId, Text, UNeocortexFunctionLibrary::EmotionToUpper(Emotion),
        FNeocortexAudioDelegate::CreateLambda([Weak](const TArray<uint8>& Bytes)
        {
            const TSharedPtr<FNeocortexSpeechJob> Self = Weak.Pin();
            if (!Self.IsValid() || Self->bStopped) return;
            float Duration = 0.0f;
            if (USoundWaveProcedural* SW = NeocortexAudio::DecodeMp3(Self->Outer.Get(), Bytes, Duration))
            {
                Self->Wave.Reset(SW);
                Self->ClipDuration = Duration;
            }
            else if (Bytes.Num() > 0)
            {
                Self->OnError.ExecuteIfBound(FNeocortexRequestError::FromMessage(TEXT("The speech response could not be decoded.")));
            }
            Self->bDownloadDone = true;
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak](const FNeocortexRequestError& Error)
        {
            const TSharedPtr<FNeocortexSpeechJob> Self = Weak.Pin();
            if (!Self.IsValid() || Self->bStopped) return;
            Self->OnError.ExecuteIfBound(Error);
            Self->bDownloadDone = true;
        }),
        SpokenText);
}

bool FNeocortexSpeechJob::IsOurSoundPlaying() const
{
    return Audio.IsValid() && Wave.IsValid() && Audio->Sound == Wave.Get() && Audio->IsPlaying();
}

void FNeocortexSpeechJob::BeginPlayback()
{
    bStarted = true;
    PlayStartTime = FPlatformTime::Seconds();
    if (Audio.IsValid())
    {
        Audio->SetSound(Wave.Get());
        Audio->Play();
    }
    OnStarted.ExecuteIfBound();
    if (!bStreaming && !bDone && !bStopped) OnClip.ExecuteIfBound(Wave.Get());
}

bool FNeocortexSpeechJob::Tick(float)
{
    if (bStopped || bDone) return false;
    const double Now = FPlatformTime::Seconds();

    if (!bStarted)
    {
        if (!bPlayAllowed) return true;
        if (!Wave.IsValid() || !Audio.IsValid())
        {
            if (bDownloadDone) Finish(false);
            return !bDone;
        }
        if (bStreaming && !bDownloadDone && SecondsQueued < PrebufferSeconds) return true;
        BeginPlayback();
        return !bDone && !bStopped;
    }

    // Stopped from outside (another sound took the component, or the actor went away).
    if (Now - PlayStartTime > 0.25 && !IsOurSoundPlaying())
    {
        Finish(true);
        return false;
    }

    if (bStreaming)
    {
        const bool bDrained = bDownloadDone && Wave->GetAvailableAudioByteCount() == 0;
        if (!bDrained)
        {
            DrainDeadline = 0.0;
            // A deadline as well, so a stuck voice can never hang a reply.
            if (bDownloadDone && Now > PlayStartTime + SecondsQueued + 1.0) Finish(true);
            return !bDone;
        }
        if (DrainDeadline == 0.0) DrainDeadline = Now + DrainSeconds;
        if (Now >= DrainDeadline) Finish(true);
        return !bDone;
    }

    if (Now >= PlayStartTime + ClipDuration + 0.05) Finish(true);
    return !bDone;
}

void FNeocortexSpeechJob::Finish(bool bSpoke)
{
    bDone = true;
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
    // A streamed voice keeps running on silence once its samples are spent.
    if (IsOurSoundPlaying()) Audio->Stop();
    OnFinished.ExecuteIfBound(bSpoke);
}
