#pragma once
#include "CoreMinimal.h"
#include "JsonObjectConverter.h"
#include "Neocortex.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * Static utility struct for JSON serialization and deserialization of Unreal Engine UStructs.
 * Provides template methods to convert UStructs to/from JSON strings with logging and error handling.
 * Uses Unreal Engine's FJsonObjectConverter for the underlying serialization operations.
 */
struct FNeocortexSerializer
{
    /**
     * Serializes a UStruct to a JSON string.
     * @param Obj The UStruct object to serialize
     * @param Out The resulting JSON string (output parameter)
     * @return True if serialization succeeded, false otherwise
     */
    template<typename T>
    static bool ToJson(const T& Obj, FString& Out)
    {
        FJsonObjectConverter::UStructToJsonObjectString(Obj, Out);
        UE_LOG(LogNeocortex, VeryVerbose, TEXT("[FNeocortexSerialize] ToJson input struct: %s"), *Out);
        return FJsonObjectConverter::UStructToJsonObjectString(Obj, Out);
    }

    /**
     * Deserializes a JSON string to a UStruct.
     * Resets the output struct to default state before deserialization to avoid partial state.
     * Performs JSON validation and logs warnings on failure.
     * @param In The JSON string to deserialize
     * @param Out The resulting UStruct object (output parameter)
     * @return True if deserialization succeeded, false otherwise
     */
    template<typename T>
    static bool FromJson(const FString& In, T& Out)
    {
        /** Reset output to default to avoid partial state. */
        Out = T{};
        UE_LOG(LogNeocortex, VeryVerbose, TEXT("[FNeoSerialize] FromJson input: %s"), *In);
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(In);
        TSharedPtr<FJsonObject> JsonObject;
        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogNeocortex, Warning, TEXT("[FNeocortexSerialize] Invalid JSON: %s"), *In);
            return false;
        }

        /** Convert JSON object to UStruct (case-sensitive key matching by default). */
        if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &Out, 0, 0))
        {
            FString Pretty;
            TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
                TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Pretty);
            FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
            UE_LOG(LogNeocortex, Warning, TEXT("[FNeocortexSerialize] JsonObjectToUStruct failed for type. JSON: %s"), *Pretty);
            return false;
        }
        return true;
    }
};
