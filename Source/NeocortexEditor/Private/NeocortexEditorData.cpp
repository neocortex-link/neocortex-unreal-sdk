#include "NeocortexEditorData.h"
#include "NeocortexHttpClient.h"
#include "NeocortexSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FNeocortexEditorData& FNeocortexEditorData::Get()
{
    static FNeocortexEditorData Data;
    return Data;
}

bool FNeocortexEditorData::HasKey() const
{
    return !GetDefault<UNeocortexSettings>()->ApiKey.TrimStartAndEnd().IsEmpty();
}

void FNeocortexEditorData::EnsureLoaded()
{
    if (!bLoadedOnce && !IsLoading()) Refresh();
}

static TSharedPtr<FJsonObject> ParseRoot(const FString& Raw)
{
    TSharedPtr<FJsonObject> Root;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root);
    return Root;
}

static TArray<FNeocortexEditorEntry> ParseEntries(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field)
{
    TArray<FNeocortexEditorEntry> Out;
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Root.IsValid() || !Root->TryGetArrayField(Field, Items)) return Out;
    for (const TSharedPtr<FJsonValue>& Item : *Items)
    {
        const TSharedPtr<FJsonObject>* Obj = nullptr;
        if (!Item.IsValid() || !Item->TryGetObject(Obj)) continue;
        FNeocortexEditorEntry Entry;
        (*Obj)->TryGetStringField(TEXT("id"), Entry.Id);
        (*Obj)->TryGetStringField(TEXT("name"), Entry.Name);
        int32 Count = 0;
        if ((*Obj)->TryGetNumberField(TEXT("questionCount"), Count)) Entry.Detail = FString::Printf(TEXT("%d questions"), Count);
        if (!Entry.Id.IsEmpty()) Out.Add(Entry);
    }
    return Out;
}

void FNeocortexEditorData::Refresh()
{
    bLoadedOnce = true;
    LastError.Reset();
    if (!HasKey())
    {
        AccountSummary.Reset();
        Characters.Reset();
        QuestionSets.Reset();
        LastError = TEXT("Add your API key to see your account, characters and question sets.");
        OnChanged.Broadcast();
        return;
    }

    if (!Http.IsValid()) Http.Reset(NewObject<UNeocortexHttpClient>());
    Http->Init(GetDefault<UNeocortexSettings>()->GetBaseUrl());
    Http->SetApiKey(GetDefault<UNeocortexSettings>()->ApiKey.TrimStartAndEnd());

    FNeocortexRequestOptions Options;
    Options.TimeoutSeconds = 15.0f;
    Options.Retries = 1;
    Pending = 3;

    Http->GetJson(TEXT("account"), TEXT("application/json"), Options,
        FNeocortexHttpRawDelegate::CreateLambda([this](const FString& Raw, const FHttpResponsePtr&, const FNeocortexRequestError* Error)
        {
            if (Error)
            {
                LastError = Error->Message;
                AccountSummary.Reset();
            }
            else if (const TSharedPtr<FJsonObject> Root = ParseRoot(Raw))
            {
                FString Tier, Email, Refresh;
                int32 Credits = 0;
                Root->TryGetStringField(TEXT("tier"), Tier);
                Root->TryGetStringField(TEXT("email"), Email);
                Root->TryGetNumberField(TEXT("creditsRemaining"), Credits);
                Root->TryGetStringField(TEXT("nextRefresh"), Refresh);
                AccountSummary = FString::Printf(TEXT("%s plan, %s\n%d credits left%s"), *Tier, *Email, Credits,
                    Refresh.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", next refresh %s"), *Refresh.Left(10)));
            }
            Finish();
        }));
    Http->GetJson(TEXT("characters"), TEXT("application/json"), Options,
        FNeocortexHttpRawDelegate::CreateLambda([this](const FString& Raw, const FHttpResponsePtr&, const FNeocortexRequestError* Error)
        {
            Characters = Error ? TArray<FNeocortexEditorEntry>() : ParseEntries(ParseRoot(Raw), TEXT("characters"));
            Finish();
        }));
    Http->GetJson(TEXT("question-sets"), TEXT("application/json"), Options,
        FNeocortexHttpRawDelegate::CreateLambda([this](const FString& Raw, const FHttpResponsePtr&, const FNeocortexRequestError* Error)
        {
            QuestionSets = Error ? TArray<FNeocortexEditorEntry>() : ParseEntries(ParseRoot(Raw), TEXT("questionSets"));
            Finish();
        }));
}

void FNeocortexEditorData::Finish()
{
    if (--Pending <= 0)
    {
        Pending = 0;
        OnChanged.Broadcast();
    }
}
