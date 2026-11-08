#include "UI/NeocortexChatUI.h"
#include "UI/NeocortexUIStyle.h"
#include "UI/NeocortexChatTranscript.h"
#include "UI/NeocortexTextInput.h"
#include "UI/NeocortexVoiceInput.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

using namespace NeocortexUI;

// The layouts, in pixels at 1920 x 1080.
static constexpr float BottomWidth = 768.0f;
static constexpr float SideWidth = 512.0f;
static constexpr float InputHeight = 64.0f;

// The side backdrop: black, clear at the top and dark at the bottom, with rounded lower corners.
static UTexture2D* FadeBackdrop(int32 Width, int32 Height)
{
    static TMap<FIntPoint, TWeakObjectPtr<UTexture2D>> Cache;
    const FIntPoint Key(Width, Height);
    if (UTexture2D* Cached = Cache.FindRef(Key).Get()) return Cached;

    UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
    if (!Texture) return nullptr;
    const float Radius = 24.0f;
    FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
    for (int32 Y = 0; Y < Height; ++Y)
    {
        const float Ramp = 0.85f * float(Y) / FMath::Max(1, Height - 1);
        for (int32 X = 0; X < Width; ++X)
        {
            float Cover = 1.0f;
            const float DX = X < Radius ? Radius - X : (X > Width - 1 - Radius ? X - (Width - 1 - Radius) : 0.0f);
            const float DY = Y > Height - 1 - Radius ? Y - (Height - 1 - Radius) : (Y < Radius ? Radius - Y : 0.0f);
            if (DX > 0.0f && DY > 0.0f) Cover = FMath::Clamp(Radius + 0.5f - FMath::Sqrt(DX * DX + DY * DY), 0.0f, 1.0f);
            Pixels[Y * Width + X] = FColor(0, 0, 0, uint8(FMath::Clamp(Ramp * Cover, 0.0f, 1.0f) * 255.0f));
        }
    }
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
    Texture->SRGB = false;
    Texture->UpdateResource();
    Texture->AddToRoot();
    Cache.Add(Key, Texture);
    return Texture;
}

static UCanvasPanelSlot* PlaceOnCanvas(UCanvasPanel* Root, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Pivot, const FVector2D& Offset, const FVector2D& Size)
{
    UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(Widget);
    CanvasSlot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
    CanvasSlot->SetAlignment(Pivot);
    CanvasSlot->SetPosition(Offset);
    CanvasSlot->SetSize(Size);
    return CanvasSlot;
}

TSharedRef<SWidget> UNeocortexChatUI::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UNeocortexChatUI::BuildTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;

    Transcript = WidgetTree->ConstructWidget<UNeocortexChatTranscript>(UNeocortexChatTranscript::StaticClass());
    Transcript->bShowSenderNames = bShowSenderNames;

    // The input row: the text field (filling the row), then the voice controls.
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (bShowTextInput)
    {
        TextInput = WidgetTree->ConstructWidget<UNeocortexTextInput>(UNeocortexTextInput::StaticClass());
        TextInput->SetHint(InputHint);
        TextInput->OnSubmitted.AddDynamic(this, &UNeocortexChatUI::HandleTyped);
        UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(TextInput);
        TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        TextSlot->SetVerticalAlignment(VAlign_Fill);
    }
    if (bShowVoiceInput || Recorder)
    {
        VoiceInput = WidgetTree->ConstructWidget<UNeocortexVoiceInput>(UNeocortexVoiceInput::StaticClass());
        VoiceInput->SetMicrophone(Recorder);
        VoiceInput->SetPushToTalk(bPushToTalk);
        UHorizontalBoxSlot* VoiceSlot = Row->AddChildToHorizontalBox(VoiceInput);
        VoiceSlot->SetVerticalAlignment(VAlign_Center);
        if (TextInput)
        {
            VoiceSlot->SetPadding(FMargin(16.0f, 0.0f, 0.0f, 0.0f));
        }
        else
        {
            // Voice only: the discs sit in the middle of the row.
            VoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            VoiceSlot->SetHorizontalAlignment(HAlign_Center);
        }
    }

    if (Layout == ENeocortexChatLayout::Side)
    {
        // Left middle, 40 above centre, on a backdrop that fades toward the top.
        const FVector2D Area(SideWidth, ConversationHeight);
        const FVector2D Offset(32.0f, -40.0f);
        UImage* Backdrop = WidgetTree->ConstructWidget<UImage>();
        if (UTexture2D* Fade = FadeBackdrop(int32(Area.X), int32(Area.Y))) Backdrop->SetBrush(Image(Fade, Area));
        Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
        PlaceOnCanvas(Root, Backdrop, FVector2D(0.0f, 0.5f), FVector2D(0.0f, 0.5f), Offset, Area);
        Transcript->MaxTextWidth = Area.X - 16.0f - 48.0f - 8.0f - 48.0f - 36.0f;
        PlaceOnCanvas(Root, Transcript, FVector2D(0.0f, 0.5f), FVector2D(0.0f, 0.5f), Offset + FVector2D(8.0f, 0.0f), Area - FVector2D(16.0f, 16.0f));
        PlaceOnCanvas(Root, Row, FVector2D(0.0f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(32.0f, Offset.Y + Area.Y * 0.5f + 16.0f), FVector2D(SideWidth, InputHeight));
    }
    else
    {
        // Centred, the conversation 112 above the bottom and the input 32 above it.
        Transcript->MaxTextWidth = BottomWidth - 48.0f - 8.0f - 48.0f - 36.0f;
        PlaceOnCanvas(Root, Transcript, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -112.0f), FVector2D(BottomWidth, ConversationHeight));
        PlaceOnCanvas(Root, Row, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -32.0f), FVector2D(BottomWidth, InputHeight));
    }
}

void UNeocortexChatUI::SetAgent(UNeocortexSmartAgent* InAgent)
{
    BindAgent(false);
    Agent = InAgent;
    BindAgent(true);
}

void UNeocortexChatUI::BindAgent(bool bBind)
{
    if (!Agent) return;
    if (bBind)
    {
        Agent->OnChat.AddDynamic(this, &UNeocortexChatUI::HandleChat);
        Agent->OnChatLineStarted.AddDynamic(this, &UNeocortexChatUI::HandleLine);
        Agent->OnComposingNextLine.AddDynamic(this, &UNeocortexChatUI::HandleComposing);
        Agent->OnReplyFinished.AddDynamic(this, &UNeocortexChatUI::HandleFinished);
        Agent->OnTranscribed.AddDynamic(this, &UNeocortexChatUI::HandleTranscribed);
        Agent->OnChatHistory.AddDynamic(this, &UNeocortexChatUI::HandleHistory);
        Agent->OnRequestError.AddDynamic(this, &UNeocortexChatUI::HandleError);
        return;
    }
    Agent->OnChat.RemoveDynamic(this, &UNeocortexChatUI::HandleChat);
    Agent->OnChatLineStarted.RemoveDynamic(this, &UNeocortexChatUI::HandleLine);
    Agent->OnComposingNextLine.RemoveDynamic(this, &UNeocortexChatUI::HandleComposing);
    Agent->OnReplyFinished.RemoveDynamic(this, &UNeocortexChatUI::HandleFinished);
    Agent->OnTranscribed.RemoveDynamic(this, &UNeocortexChatUI::HandleTranscribed);
    Agent->OnChatHistory.RemoveDynamic(this, &UNeocortexChatUI::HandleHistory);
    Agent->OnRequestError.RemoveDynamic(this, &UNeocortexChatUI::HandleError);
}

void UNeocortexChatUI::SetMicrophone(UNeocortexMicrophoneRecorderComponent* InRecorder)
{
    if (Recorder) Recorder->OnWavReady.RemoveDynamic(this, &UNeocortexChatUI::HandleWav);
    Recorder = InRecorder;
    if (Recorder) Recorder->OnWavReady.AddDynamic(this, &UNeocortexChatUI::HandleWav);
    if (VoiceInput) VoiceInput->SetMicrophone(Recorder);
}

void UNeocortexChatUI::SetPushToTalk(bool bEnabled)
{
    bPushToTalk = bEnabled;
    IdleTime = 0.0f;
    if (Recorder) Recorder->StopRecording(false);
    if (VoiceInput) VoiceInput->SetPushToTalk(bEnabled);
}

bool UNeocortexChatUI::IsBusy() const
{
    return Agent && Agent->IsRequestPending();
}

bool UNeocortexChatUI::CanTalk() const
{
    return Agent && !Agent->ProjectId.IsEmpty();
}

FString UNeocortexChatUI::SpeakerName() const
{
    return CharacterName.IsEmpty() ? FString(TEXT("Character")) : CharacterName;
}

// The microphone listens once the character has been quiet for a moment, so it never hears its
// own reply, and waits while there is no one to talk to.
void UNeocortexChatUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!Recorder) return;
    const bool bCanTalk = CanTalk();
    const bool bBusy = !bCanTalk || IsBusy();
    if (VoiceInput) VoiceInput->SetBusy(bCanTalk && IsBusy());
    if (!bCanTalk && !bWarnedNoCharacter)
    {
        bWarnedNoCharacter = true;
        AddNotice(TEXT("No character is set, so the microphone stays off."));
    }
    if (bPushToTalk) return;

    IdleTime = bBusy ? 0.0f : IdleTime + InDeltaTime;
    if (bBusy && Recorder->IsListening() && !Recorder->IsUserSpeaking()) Recorder->StopListening();
    if (!bBusy && !Recorder->IsListening() && IdleTime > 0.4f && !Recorder->StartListening())
    {
        AddNotice(TEXT("The microphone could not be started. The Output Log says why."), true);
        SetPushToTalk(true);
    }
}

void UNeocortexChatUI::NativeDestruct()
{
    BindAgent(false);
    if (Recorder)
    {
        Recorder->StopListening();
        Recorder->OnWavReady.RemoveDynamic(this, &UNeocortexChatUI::HandleWav);
    }
    Super::NativeDestruct();
}

void UNeocortexChatUI::AddMessage(const FString& Text, bool bIsPlayer, const FString& Sender)
{
    if (Transcript) Transcript->AddMessage(Text, bIsPlayer, Sender);
}

void UNeocortexChatUI::AddNotice(const FString& Text, bool bIsError)
{
    if (Transcript) Transcript->AddNotice(Text, bIsError);
}

void UNeocortexChatUI::SetTyping(bool bTyping)
{
    if (Transcript) Transcript->SetTyping(bTyping);
}

void UNeocortexChatUI::SetInputEnabled(bool bEnabled)
{
    if (TextInput) TextInput->SetInputEnabled(bEnabled);
}

void UNeocortexChatUI::ClearMessages()
{
    if (Transcript) Transcript->ClearMessages();
}

void UNeocortexChatUI::HandleTyped(const FString& Text)
{
    AddMessage(Text, true, PlayerName);
    OnSubmitted.Broadcast(Text);
    if (bAutoSend) Submit(Text);
}

void UNeocortexChatUI::Submit(const FString& Text)
{
    if (!Agent) return;
    if (Recorder) Recorder->StopListening();
    SetTyping(true);
    if (bAskForAudio) Agent->SendMessageForAudio(Text);
    else Agent->SendMessage(Text);
}

void UNeocortexChatUI::HandleWav(const TArray<uint8>& Wav)
{
    if (Wav.Num() > 0 && CanTalk()) SubmitAudio(Wav);
}

void UNeocortexChatUI::SubmitAudio(const TArray<uint8>& Wav)
{
    if (!Agent) return;
    SetTyping(true);
    Agent->SendAudioForAudio(Wav);
}

void UNeocortexChatUI::HandleChat(const FNeocortexChatResponseData& Response)
{
    if (!Response.Name.IsEmpty()) CharacterName = Response.Name;
    // In Off mode there are no line events: the reply is one bubble.
    if (Agent && Agent->ChatLinesMode == EChatLinesMode::Off)
    {
        SetTyping(false);
        AddMessage(Response.Response, false, SpeakerName());
    }
}

void UNeocortexChatUI::HandleLine(const FNeocortexChatLine& Line)
{
    SetTyping(false);
    AddMessage(Line.Text, false, SpeakerName());
}

void UNeocortexChatUI::HandleComposing()
{
    SetTyping(true);
}

void UNeocortexChatUI::HandleFinished()
{
    SetTyping(false);
}

void UNeocortexChatUI::HandleTranscribed(const FString& Text)
{
    AddMessage(Text, true, PlayerName);
}

void UNeocortexChatUI::HandleHistory(const TArray<FNeocortexChatHistoryEntry>& Messages, const FString& NextCursor)
{
    for (const FNeocortexChatHistoryEntry& Entry : Messages)
    {
        if (!Entry.IsUser() && !Entry.Name.IsEmpty() && CharacterName.IsEmpty()) CharacterName = Entry.Name;
        AddMessage(Entry.Content, Entry.IsUser(), Entry.IsUser() ? PlayerName : (Entry.Name.IsEmpty() ? SpeakerName() : Entry.Name));
    }
}

void UNeocortexChatUI::HandleError(const FNeocortexRequestError& RequestError)
{
    SetTyping(false);
    AddNotice(RequestError.IsOutOfCredits() ? TEXT("Out of credits.") : RequestError.Message, true);
}
