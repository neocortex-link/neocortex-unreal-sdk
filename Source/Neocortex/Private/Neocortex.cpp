#include "Neocortex.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "VoiceModule.h"

DEFINE_LOG_CATEGORY(LogNeocortex)

#define LOCTEXT_NAMESPACE "FNeocortexModule"

// Microphone input needs the engine's voice capture, which is off unless the project turns it on.
// The Voice module reads the setting once, when it starts, so it is turned on here first: no ini
// edit and no restart, in the editor and in packaged games. If the Voice module already started
// with it off, it is started again.
static void EnableVoiceCapture()
{
	bool bEnabled = false;
	GConfig->GetBool(TEXT("Voice"), TEXT("bEnabled"), bEnabled, GEngineIni);
	if (!bEnabled)
	{
		GConfig->SetBool(TEXT("Voice"), TEXT("bEnabled"), true, GEngineIni);
	}
	FModuleManager& Modules = FModuleManager::Get();
	if (Modules.IsModuleLoaded(TEXT("Voice")) && !FVoiceModule::Get().IsVoiceEnabled())
	{
		Modules.UnloadModule(TEXT("Voice"));
		Modules.LoadModule(TEXT("Voice"));
	}
}

void FNeocortexModule::StartupModule()
{
	EnableVoiceCapture();
}

void FNeocortexModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNeocortexModule, Neocortex)
