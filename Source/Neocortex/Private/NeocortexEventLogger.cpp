#include "NeocortexEventLogger.h"
#include "NeocortexSubsystem.h"
#include "Neocortex.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

static UNeocortexSubsystem* GetSubsystem(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return nullptr;
    UGameInstance* GI = World->GetGameInstance();
    return GI ? GI->GetSubsystem<UNeocortexSubsystem>() : nullptr;
}

void UNeocortexEventLogger::PushEvent(UObject* WorldContextObject, ENeocortexEventPriority Priority, const FString& Content)
{
    if (UNeocortexSubsystem* Sub = GetSubsystem(WorldContextObject))
        Sub->PushEvent(Priority, Content);
}

void UNeocortexEventLogger::ClearEvents(UObject* WorldContextObject)
{
    if (UNeocortexSubsystem* Sub = GetSubsystem(WorldContextObject))
        Sub->ClearEvents();
}

FString UNeocortexEventLogger::ConsumeLogsJson(UObject* WorldContextObject)
{
    if (UNeocortexSubsystem* Sub = GetSubsystem(WorldContextObject))
        return Sub->ConsumeLogsJson();
    return TEXT("");
}
