#pragma once
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class UNeocortexHttpClient;

/** One character or question set the key can use. */
struct FNeocortexEditorEntry
{
    FString Id;
    FString Name;
    /** Extra detail, such as a question count. */
    FString Detail;
};

/**
 * What the editor knows about the account behind the project's API key: the account itself,
 * its characters and its question sets. Fetched on demand and shared by the settings window and
 * the pickers in the details panel.
 */
class FNeocortexEditorData
{
public:
    static FNeocortexEditorData& Get();

    /** Fetches everything again with the current key. */
    void Refresh();

    /** Fetches once, unless a fetch already ran or is running. */
    void EnsureLoaded();

    bool IsLoading() const { return Pending > 0; }
    bool HasKey() const;

    FString AccountSummary;
    FString LastError;
    TArray<FNeocortexEditorEntry> Characters;
    TArray<FNeocortexEditorEntry> QuestionSets;

    /** Fired when a fetch finishes. */
    FSimpleMulticastDelegate OnChanged;

private:
    TStrongObjectPtr<UNeocortexHttpClient> Http;
    int32 Pending = 0;
    bool bLoadedOnce = false;

    void Finish();
};
