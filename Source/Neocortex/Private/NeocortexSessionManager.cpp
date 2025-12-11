#include "NeocortexSessionManager.h"
#include "Misc/ConfigCacheIni.h"

void UNeocortexSessionManager::Init(const FString& SectionIn, const FString& PrefixIn)
{
	Section = SectionIn;
	Prefix = PrefixIn;
	Cache.Empty();
}

FString UNeocortexSessionManager::Get(const FString& ProjectId) const
{
	FScopeLock Lock(&Mutex);

	if (const FString* Found = Cache.Find(ProjectId))
	{
		return *Found;
	}

	FString V;
	const FString Key = Prefix + ProjectId;
	GConfig->GetString(*Section, *Key, V, GGameIni);

	// Cache the loaded value; safe because Cache is mutable
	Cache.Add(ProjectId, V);
	return V;
}

void UNeocortexSessionManager::Set(const FString& ProjectId, const FString& SessionId)
{
	{
		FScopeLock Lock(&Mutex);
		Cache.Add(ProjectId, SessionId);
	}
	FlushSingle(Prefix + ProjectId, SessionId);
}

void UNeocortexSessionManager::Clear(const FString& ProjectId)
{
	Set(ProjectId, TEXT(""));
}

void UNeocortexSessionManager::FlushSingle(const FString& Key, const FString& Value) const
{
	GConfig->SetString(*Section, *Key, *Value, GGameIni);
	GConfig->Flush(false, GGameIni);
}
