#include "Samples/NeocortexSampleLauncher.h"
#include "Samples/NeocortexSamplePieces.h"
#include "NeocortexService.h"
#include "NeocortexSubsystem.h"
#include "NeocortexSettings.h"
#include "NeocortexMicrophoneRecorderComponent.h"
#include "UI/NeocortexUIStyle.h"
#include "UI/NeocortexWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

using namespace NeocortexUI;
using FScreen = UNeocortexSampleFrameUI;

const TArray<ANeocortexSampleLauncher::FEntry>& ANeocortexSampleLauncher::Samples()
{
    static const TArray<FEntry> List = {
        { TEXT("/Neocortex/Samples/Maps/1_TextChat"), TEXT("Text Chat"), TEXT("Type to Cora; each line drops in and is voiced in its own emotion.") },
        { TEXT("/Neocortex/Samples/Maps/2_AudioChat"), TEXT("Audio Chat"), TEXT("Talk through the microphone, with voice detection or push to talk.") },
        { TEXT("/Neocortex/Samples/Maps/3_Actions"), TEXT("Actions"), TEXT("Ask Cora to dance or jump; stacked actions run in order.") },
        { TEXT("/Neocortex/Samples/Maps/4_Interactables"), TEXT("Interactables"), TEXT("Cora perceives the shapes on the floor and walks to them.") },
        { TEXT("/Neocortex/Samples/Maps/5_GroupChat"), TEXT("Group Chat"), TEXT("Three characters, one conversation, a live roster. Pro or Team key.") },
        { TEXT("/Neocortex/Samples/Maps/6_UsageGating"), TEXT("Usage Gating"), TEXT("Check credits and caps before every message.") },
        { TEXT("/Neocortex/Samples/Maps/7_QuizShow"), TEXT("Quiz Show"), TEXT("Cora hosts a quiz for one to four players.") },
    };
    return List;
}

static const TCHAR* const CardNames[] = { TEXT("Card1"), TEXT("Card2"), TEXT("Card3"), TEXT("Card4"), TEXT("Card5"), TEXT("Card6"), TEXT("Card7") };

void ANeocortexSampleLauncher::BuildScreen(UWidgetTree* Tree, UCanvasPanel* Root) const
{
    // What every sample uses, in a row above the cards: an amber caption over each dropdown.
    UHorizontalBox* Choices = FScreen::Make<UHorizontalBox>(Tree, TEXT("Choices"));
    FScreen::PlaceOn(Root, Choices, FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.0f, 150.0f), FVector2D(1608.0f, 80.0f));
    auto Choice = [Tree, Choices](const TCHAR* Name, const FString& Caption, const FString& Hint)
    {
        UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* ColumnSlot = Choices->AddChildToHorizontalBox(Column);
        ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ColumnSlot->SetPadding(FMargin(Choices->GetChildrenCount() == 1 ? 0.0f : 36.0f, 0.0f, 0.0f, 0.0f));
        UNeocortexLabel* Label = Tree->ConstructWidget<UNeocortexLabel>(UNeocortexLabel::StaticClass());
        Label->Text = Caption;
        Label->Size = 18;
        Label->bBold = true;
        Label->Color = Amber;
        Label->bWrap = false;
        Column->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
        USizeBox* Height = Tree->ConstructWidget<USizeBox>();
        Height->SetHeightOverride(48.0f);
        UNeocortexDropdown* Dropdown = FScreen::Make<UNeocortexDropdown>(Tree, Name);
        Dropdown->Placeholder = Hint;
        Height->SetContent(Dropdown);
        Column->AddChildToVerticalBox(Height);
    };
    Choice(TEXT("CharacterPicker"), TEXT("CHARACTER"), TEXT("Cora"));
    Choice(TEXT("QuestionSetPicker"), TEXT("QUESTION SET (QUIZ SHOW)"), FString());
    Choice(TEXT("MicrophonePicker"), TEXT("MICROPHONE"), TEXT("Default"));

    // The cards: three a row with 24 between them, the last row centred.
    UVerticalBox* Grid = FScreen::Make<UVerticalBox>(Tree, TEXT("Cards"));
    FScreen::PlaceOn(Root, Grid, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 48.0f), FVector2D(1608.0f, 648.0f));
    const TArray<FEntry>& List = Samples();
    UHorizontalBox* Row = nullptr;
    for (int32 i = 0; i < List.Num() && i < UE_ARRAY_COUNT(CardNames); ++i)
    {
        if (i % 3 == 0)
        {
            Row = Tree->ConstructWidget<UHorizontalBox>();
            UVerticalBoxSlot* RowSlot = Grid->AddChildToVerticalBox(Row);
            RowSlot->SetHorizontalAlignment(HAlign_Center);
            RowSlot->SetPadding(FMargin(0.0f, i == 0 ? 0.0f : 24.0f, 0.0f, 0.0f));
        }
        UNeocortexSampleCard* Card = FScreen::Make<UNeocortexSampleCard>(Tree, CardNames[i]);
        Card->Number = i + 1;
        Card->Title = List[i].Title;
        Card->Description = List[i].Description;
        Card->Map = List[i].Map;
        USizeBox* CardSize = Tree->ConstructWidget<USizeBox>();
        CardSize->SetWidthOverride(520.0f);
        CardSize->SetHeightOverride(200.0f);
        CardSize->SetContent(Card);
        Row->AddChildToHorizontalBox(CardSize)->SetPadding(FMargin(i % 3 == 0 ? 0.0f : 24.0f, 0.0f, 0.0f, 0.0f));
    }

    // What to fix, at the bottom, when something is wrong.
    UNeocortexLabel* Problem = FScreen::Make<UNeocortexLabel>(Tree, TEXT("Status"));
    Problem->Size = 22;
    Problem->bBold = true;
    Problem->Color = ErrorColor;
    Problem->Justification = ETextJustify::Center;
    FScreen::PlaceOn(Root, Problem, FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.0f, -64.0f), FVector2D(800.0f, 32.0f));
}

void ANeocortexSampleLauncher::SetupSample()
{
    CharacterPicker = FindWidget<UNeocortexDropdown>(TEXT("CharacterPicker"));
    QuestionSetPicker = FindWidget<UNeocortexDropdown>(TEXT("QuestionSetPicker"));
    MicrophonePicker = FindWidget<UNeocortexDropdown>(TEXT("MicrophonePicker"));
    Status = FindWidget<UNeocortexLabel>(TEXT("Status"));

    static const FName Handlers[] = {
        GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open1), GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open2),
        GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open3), GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open4),
        GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open5), GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open6),
        GET_FUNCTION_NAME_CHECKED(ANeocortexSampleLauncher, Open7),
    };
    for (int32 i = 0; i < UE_ARRAY_COUNT(CardNames); ++i)
    {
        if (UNeocortexSampleCard* Card = FindWidget<UNeocortexSampleCard>(CardNames[i]))
        {
            FScriptDelegate Delegate;
            Delegate.BindUFunction(this, Handlers[i]);
            Card->OnClicked.Add(Delegate);
        }
    }

    if (MicrophonePicker)
    {
        MicrophonePicker->ClearOptions();
        MicrophonePicker->AddOption(TEXT("Default"));
        for (const FString& Device : UNeocortexMicrophoneRecorderComponent::GetAvailableDeviceNames()) MicrophonePicker->AddOption(Device);
        const FString Saved = FNeocortexSamplePicks::Load().Microphone;
        MicrophonePicker->SetSelectedOption(Saved.IsEmpty() || MicrophonePicker->FindOptionIndex(Saved) == INDEX_NONE ? FString(TEXT("Default")) : Saved);
        MicrophonePicker->OnPicked.AddDynamic(this, &ANeocortexSampleLauncher::HandlePicked);
    }
    if (CharacterPicker) CharacterPicker->OnPicked.AddDynamic(this, &ANeocortexSampleLauncher::HandlePicked);
    if (QuestionSetPicker) QuestionSetPicker->OnPicked.AddDynamic(this, &ANeocortexSampleLauncher::HandlePicked);
    LoadLists();
}

void ANeocortexSampleLauncher::ShowProblem(const FString& Text)
{
    if (Status) Status->SetText(Text);
}

void ANeocortexSampleLauncher::LoadLists()
{
    if (GetDefault<UNeocortexSettings>()->ApiKey.TrimStartAndEnd().IsEmpty())
    {
        ShowProblem(TEXT("Add your API key in Tools > Neocortex first."));
        return;
    }
    const UGameInstance* GI = GetGameInstance();
    UNeocortexService* Service = GI && GI->GetSubsystem<UNeocortexSubsystem>() ? GI->GetSubsystem<UNeocortexSubsystem>()->GetService() : nullptr;
    if (!Service) return;

    const FNeocortexSamplePicks Saved = FNeocortexSamplePicks::Load();
    TWeakObjectPtr<ANeocortexSampleLauncher> Weak(this);
    Service->GetCharacters(
        FNeocortexCharactersDelegate::CreateLambda([Weak, Saved](const TArray<FNeocortexCharacterSummary>& List)
        {
            if (!Weak.IsValid() || !Weak->CharacterPicker) return;
            Weak->CharacterList = List;
            Weak->CharacterPicker->ClearOptions();
            for (const FNeocortexCharacterSummary& Entry : List) Weak->CharacterPicker->AddOption(Entry.Name);
            int32 Index = List.IndexOfByPredicate([&Saved](const FNeocortexCharacterSummary& E) { return E.Id == Saved.CharacterId; });
            if (Index == INDEX_NONE && List.Num() > 0) Index = 0;
            if (Index != INDEX_NONE) Weak->CharacterPicker->SetSelectedIndex(Index);
            if (List.Num() == 0) Weak->ShowProblem(TEXT("No characters on this key yet. Create one in your Neocortex dashboard."));
            Weak->SavePicks();
        }),
        FNeocortexErrorDelegate::CreateLambda([Weak](const FNeocortexRequestError& Error)
        {
            if (Weak.IsValid()) Weak->ShowProblem(Error.Message);
        }));
    Service->GetQuestionSets(
        FNeocortexQuestionSetsDelegate::CreateLambda([Weak, Saved](const TArray<FNeocortexQuestionSetSummary>& List)
        {
            if (!Weak.IsValid() || !Weak->QuestionSetPicker) return;
            Weak->QuestionSetList = List;
            Weak->QuestionSetPicker->ClearOptions();
            for (const FNeocortexQuestionSetSummary& Entry : List)
            {
                Weak->QuestionSetPicker->AddOption(FString::Printf(TEXT("%s (%d questions)"), *Entry.Name, Entry.QuestionCount));
            }
            int32 Index = List.IndexOfByPredicate([&Saved](const FNeocortexQuestionSetSummary& E) { return E.Id == Saved.QuestionSetId; });
            if (Index == INDEX_NONE && List.Num() > 0) Index = 0;
            if (Index != INDEX_NONE) Weak->QuestionSetPicker->SetSelectedIndex(Index);
            Weak->SavePicks();
        }),
        FNeocortexErrorDelegate::CreateLambda([](const FNeocortexRequestError&) {}));
}

void ANeocortexSampleLauncher::SavePicks()
{
    FNeocortexSamplePicks Picks = FNeocortexSamplePicks::Load();
    const int32 Character = CharacterPicker ? CharacterPicker->GetSelectedIndex() : INDEX_NONE;
    if (CharacterList.IsValidIndex(Character))
    {
        Picks.CharacterId = CharacterList[Character].Id;
        Picks.CharacterName = CharacterList[Character].Name;
        // The group sample needs each character once, so the others join in list order.
        Picks.OtherCharacterIds.Reset();
        Picks.OtherCharacterNames.Reset();
        for (const FNeocortexCharacterSummary& Entry : CharacterList)
        {
            if (Entry.Id == Picks.CharacterId) continue;
            Picks.OtherCharacterIds.Add(Entry.Id);
            Picks.OtherCharacterNames.Add(Entry.Name);
        }
    }
    const int32 Set = QuestionSetPicker ? QuestionSetPicker->GetSelectedIndex() : INDEX_NONE;
    if (QuestionSetList.IsValidIndex(Set)) Picks.QuestionSetId = QuestionSetList[Set].Id;
    if (MicrophonePicker) Picks.Microphone = MicrophonePicker->GetSelectedIndex() > 0 ? MicrophonePicker->GetSelectedOption() : FString();
    Picks.Save();
}

void ANeocortexSampleLauncher::HandlePicked(const FString& Item, bool bByPlayer)
{
    if (bByPlayer) SavePicks();
}

void ANeocortexSampleLauncher::Open(int32 Index)
{
    const TArray<FEntry>& List = Samples();
    if (List.IsValidIndex(Index)) UGameplayStatics::OpenLevel(this, FName(List[Index].Map));
}

void ANeocortexSampleLauncher::Open1() { Open(0); }
void ANeocortexSampleLauncher::Open2() { Open(1); }
void ANeocortexSampleLauncher::Open3() { Open(2); }
void ANeocortexSampleLauncher::Open4() { Open(3); }
void ANeocortexSampleLauncher::Open5() { Open(4); }
void ANeocortexSampleLauncher::Open6() { Open(5); }
void ANeocortexSampleLauncher::Open7() { Open(6); }
