#include "UI/NeocortexGroupChatUI.h"
#include "NeocortexGroupDirector.h"
#include "NeocortexSmartAgent.h"

UNeocortexGroupChatUI::UNeocortexGroupChatUI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    Layout = ENeocortexChatLayout::Side;
    bShowSenderNames = true;
}

void UNeocortexGroupChatUI::SetDirector(UNeocortexGroupDirector* InDirector)
{
    BindDirector(false);
    Director = InDirector;
    BindDirector(true);
}

void UNeocortexGroupChatUI::BindDirector(bool bBind)
{
    if (!Director) return;
    if (bBind)
    {
        Director->OnSpeaker.AddDynamic(this, &UNeocortexGroupChatUI::HandleSpeaker);
        Director->OnTurnStarted.AddDynamic(this, &UNeocortexGroupChatUI::HandleTurnStarted);
        Director->OnTurnFinished.AddDynamic(this, &UNeocortexGroupChatUI::HandleTurnFinished);
        Director->OnPlayerSpeech.AddDynamic(this, &UNeocortexGroupChatUI::HandlePlayerSpeech);
        Director->OnRequestError.AddDynamic(this, &UNeocortexGroupChatUI::HandleGroupError);
        return;
    }
    Director->OnSpeaker.RemoveDynamic(this, &UNeocortexGroupChatUI::HandleSpeaker);
    Director->OnTurnStarted.RemoveDynamic(this, &UNeocortexGroupChatUI::HandleTurnStarted);
    Director->OnTurnFinished.RemoveDynamic(this, &UNeocortexGroupChatUI::HandleTurnFinished);
    Director->OnPlayerSpeech.RemoveDynamic(this, &UNeocortexGroupChatUI::HandlePlayerSpeech);
    Director->OnRequestError.RemoveDynamic(this, &UNeocortexGroupChatUI::HandleGroupError);
}

void UNeocortexGroupChatUI::NativeDestruct()
{
    BindDirector(false);
    Super::NativeDestruct();
}

bool UNeocortexGroupChatUI::IsBusy() const
{
    if (!Director) return false;
    if (Director->IsBusy()) return true;
    for (const UNeocortexSmartAgent* Member : Director->Agents)
    {
        if (Member && Member->IsSpeaking()) return true;
    }
    return false;
}

bool UNeocortexGroupChatUI::CanTalk() const
{
    return Director && Director->Agents.Num() > 0;
}

void UNeocortexGroupChatUI::Submit(const FString& Text)
{
    if (Director) Director->Send(Text);
}

void UNeocortexGroupChatUI::SubmitAudio(const TArray<uint8>& Wav)
{
    if (Director) Director->SendAudio(Wav);
}

void UNeocortexGroupChatUI::HandleSpeaker(const FNeocortexGroupMessage& Message)
{
    FString Text;
    for (const FNeocortexChatLine& Line : Message.Lines)
    {
        if (!Text.IsEmpty() && !Line.Text.IsEmpty()) Text += TEXT(" ");
        Text += Line.Text;
    }
    SetTyping(false);
    AddMessage(Text, false, Message.Name);
}

void UNeocortexGroupChatUI::HandleTurnStarted()
{
    SetTyping(true);
}

void UNeocortexGroupChatUI::HandleTurnFinished()
{
    SetTyping(false);
}

void UNeocortexGroupChatUI::HandlePlayerSpeech(const FString& Text)
{
    AddMessage(Text, true, PlayerName);
}

void UNeocortexGroupChatUI::HandleGroupError(const FNeocortexRequestError& Error)
{
    SetTyping(false);
    AddNotice(Error.IsOutOfCredits() ? TEXT("Out of credits.") : Error.Message, true);
}
