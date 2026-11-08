#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/StrongObjectPtr.h"
#include "NeocortexTypes.h"
#include "NeocortexHttpStream.h"

class UNeocortexService;
class UAudioComponent;
class USoundWaveProcedural;

namespace NeocortexAudio
{
    /** Decodes MP3 bytes into a playable wave owned by Outer; null on failure. OutDuration in seconds. */
    USoundWaveProcedural* DecodeMp3(UObject* Outer, const TArray<uint8>& Bytes, float& OutDuration);
}

/**
 * Speaks one piece of text on an audio component: streamed as it is synthesized when the
 * platform and server allow it, else fetched as a whole clip. Fetching starts at once; playback
 * waits for Play, so the next line can be fetched while the current one is still heard.
 */
class FNeocortexSpeechJob : public TSharedFromThis<FNeocortexSpeechJob>
{
public:
    DECLARE_DELEGATE(FOnStarted);
    /** bSpoke is false when nothing was heard: nothing to speak, or the speech failed. */
    DECLARE_DELEGATE_OneParam(FOnFinished, bool /*bSpoke*/);
    DECLARE_DELEGATE_OneParam(FOnClip, USoundWaveProcedural* /*Clip*/);
    DECLARE_DELEGATE_OneParam(FOnError, const FNeocortexRequestError& /*Error*/);

    /** False once the server answered that it has no streamed speech; whole clips are used then. */
    static bool bStreamedSpeechSupported;

    /** Seconds of speech per character, for timing streamed speech before its length is known. */
    static constexpr float SecondsPerCharacter = 0.065f;

    FNeocortexSpeechJob(UNeocortexService* InService, UAudioComponent* InAudio, UObject* InOuter,
        const FString& InCharacterId, const FString& InText, const FString& InSpokenText, EEmotions InEmotion, bool bInStream);
    ~FNeocortexSpeechJob();

    /** Starts fetching the speech. */
    void Start();

    /** Lets the speech play as soon as enough of it has arrived. */
    void Play();

    /** Stops the download and the voice. No delegate fires afterwards. */
    void Stop();

    bool IsStreaming() const { return bStreaming; }
    bool HasStarted() const { return bStarted; }

    /** Fired as the speech becomes audible. */
    FOnStarted OnStarted;
    /** Fired once the speech has been heard, or could not be. */
    FOnFinished OnFinished;
    /** Fired with a whole clip as it starts playing (not for streamed speech). */
    FOnClip OnClip;
    /** Fired when the speech failed and nothing will be heard. */
    FOnError OnError;

private:
    TWeakObjectPtr<UNeocortexService> Service;
    TWeakObjectPtr<UAudioComponent> Audio;
    TWeakObjectPtr<UObject> Outer;
    FString CharacterId;
    FString Text;
    FString SpokenText;
    EEmotions Emotion;
    bool bStreaming;

    bool bPlayAllowed = false;
    bool bStarted = false;
    bool bDone = false;
    bool bStopped = false;
    bool bDownloadDone = false;
    double PlayStartTime = 0.0;
    double DrainDeadline = 0.0;
    float SecondsQueued = 0.0f;

    TSharedPtr<FNeocortexHttpStream, ESPMode::ThreadSafe> Stream;
    FNeocortexPcmStreamParser Parser;
    TStrongObjectPtr<USoundWaveProcedural> Wave;
    float ClipDuration = 0.0f;
    FTSTicker::FDelegateHandle TickerHandle;

    void StartStream();
    void StartBuffered();
    void OnStreamChunk(const TArray<uint8>& Bytes);
    void OnStreamFinished(const FNeocortexRequestError* Error);
    bool Tick(float DeltaTime);
    void BeginPlayback();
    void Finish(bool bSpoke);
    bool IsOurSoundPlaying() const;
};
