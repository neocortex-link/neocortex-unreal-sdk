#include "NeocortexIdCustomization.h"
#include "NeocortexEditorData.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexQuizAgent.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "PropertyHandle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "NeocortexIdCustomization"

enum class ENeocortexIdKind { Character, QuestionSet };

static FText NameFor(const FString& Id, ENeocortexIdKind Kind)
{
    const FNeocortexEditorData& Data = FNeocortexEditorData::Get();
    for (const FNeocortexEditorEntry& Entry : Kind == ENeocortexIdKind::Character ? Data.Characters : Data.QuestionSets)
    {
        if (Entry.Id == Id) return FText::FromString(Entry.Name);
    }
    return FText::GetEmpty();
}

static TSharedRef<SWidget> MakeIdPicker(TSharedRef<IPropertyHandle> Handle, ENeocortexIdKind Kind)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(SEditableTextBox)
            .Font(IDetailLayoutBuilder::GetDetailFont())
            .Text_Lambda([Handle]()
            {
                FString Value;
                Handle->GetValue(Value);
                return FText::FromString(Value);
            })
            .ToolTipText_Lambda([Handle, Kind]()
            {
                FString Value;
                Handle->GetValue(Value);
                return NameFor(Value, Kind);
            })
            .OnTextCommitted_Lambda([Handle](const FText& Text, ETextCommit::Type)
            {
                Handle->SetValue(Text.ToString().TrimStartAndEnd());
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0).VAlign(VAlign_Center)
        [
            SNew(SComboButton)
            .ButtonContent()
            [
                SNew(STextBlock).Font(IDetailLayoutBuilder::GetDetailFont())
                .Text_Lambda([Handle, Kind]()
                {
                    FString Value;
                    Handle->GetValue(Value);
                    const FText Name = NameFor(Value, Kind);
                    return Name.IsEmpty() ? LOCTEXT("Pick", "Pick") : Name;
                })
            ]
            .OnGetMenuContent_Lambda([Handle, Kind]()
            {
                FNeocortexEditorData& Data = FNeocortexEditorData::Get();
                Data.EnsureLoaded();
                FMenuBuilder Menu(true, nullptr);
                const TArray<FNeocortexEditorEntry>& Entries = Kind == ENeocortexIdKind::Character ? Data.Characters : Data.QuestionSets;
                for (const FNeocortexEditorEntry& Entry : Entries)
                {
                    const FString Id = Entry.Id;
                    Menu.AddMenuEntry(FText::FromString(Entry.Name), FText::FromString(Id), FSlateIcon(),
                        FUIAction(FExecuteAction::CreateLambda([Handle, Id]() { Handle->SetValue(Id); })));
                }
                if (Entries.Num() == 0)
                {
                    const FText Hint = Data.IsLoading() ? LOCTEXT("Loading", "Loading, open the list again in a moment.")
                        : !Data.HasKey() ? LOCTEXT("NoKey", "Add your API key in Tools > Neocortex.")
                        : LOCTEXT("None", "Nothing on this key yet.");
                    Menu.AddMenuEntry(Hint, FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
                }
                Menu.AddMenuSeparator();
                Menu.AddMenuEntry(LOCTEXT("Refresh", "Refresh the list"), FText::GetEmpty(), FSlateIcon(),
                    FUIAction(FExecuteAction::CreateLambda([]() { FNeocortexEditorData::Get().Refresh(); })));
                return Menu.MakeWidget();
            })
        ];
}

static void Customize(IDetailLayoutBuilder& DetailBuilder, FName Property, ENeocortexIdKind Kind)
{
    TSharedRef<IPropertyHandle> Handle = DetailBuilder.GetProperty(Property);
    if (!Handle->IsValidHandle()) return;
    if (IDetailPropertyRow* Row = DetailBuilder.EditDefaultProperty(Handle))
    {
        Row->CustomWidget()
            .NameContent()[ Handle->CreatePropertyNameWidget() ]
            .ValueContent().MinDesiredWidth(260.0f)[ MakeIdPicker(Handle, Kind) ];
    }
}

void FNeocortexIdCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    FNeocortexEditorData::Get().EnsureLoaded();
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    const UObject* First = Objects.Num() > 0 ? Objects[0].Get() : nullptr;

    if (First && First->IsA<UNeocortexSmartAgent>())
    {
        Customize(DetailBuilder, GET_MEMBER_NAME_CHECKED(UNeocortexSmartAgent, ProjectId), ENeocortexIdKind::Character);
    }
    else if (First && First->IsA<UNeocortexQuizAgent>())
    {
        Customize(DetailBuilder, GET_MEMBER_NAME_CHECKED(UNeocortexQuizAgent, CharacterId), ENeocortexIdKind::Character);
        Customize(DetailBuilder, GET_MEMBER_NAME_CHECKED(UNeocortexQuizAgent, QuestionSetId), ENeocortexIdKind::QuestionSet);
    }
}

#undef LOCTEXT_NAMESPACE
