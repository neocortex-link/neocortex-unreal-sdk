#include "Modules/ModuleManager.h"
#include "NeocortexIdCustomization.h"
#include "SNeocortexSettingsWindow.h"
#include "NeocortexSmartAgent.h"
#include "NeocortexQuizAgent.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "NeocortexEditor"

static const FName SettingsTabName(TEXT("NeocortexSettings"));

class FNeocortexEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SettingsTabName,
            FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {
                return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SNeocortexSettingsWindow) ];
            }))
            .SetDisplayName(LOCTEXT("TabTitle", "Neocortex"))
            .SetTooltipText(LOCTEXT("TabTooltip", "Your Neocortex API key, account, characters and question sets."))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

        FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
        PropertyEditor.RegisterCustomClassLayout(UNeocortexSmartAgent::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FNeocortexIdCustomization::MakeInstance));
        PropertyEditor.RegisterCustomClassLayout(UNeocortexQuizAgent::StaticClass()->GetFName(),
            FOnGetDetailCustomizationInstance::CreateStatic(&FNeocortexIdCustomization::MakeInstance));
    }

    virtual void ShutdownModule() override
    {
        if (FSlateApplication::IsInitialized())
        {
            FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SettingsTabName);
        }
        if (FPropertyEditorModule* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
        {
            PropertyEditor->UnregisterCustomClassLayout(UNeocortexSmartAgent::StaticClass()->GetFName());
            PropertyEditor->UnregisterCustomClassLayout(UNeocortexQuizAgent::StaticClass()->GetFName());
        }
    }
};

IMPLEMENT_MODULE(FNeocortexEditorModule, NeocortexEditor)

#undef LOCTEXT_NAMESPACE
