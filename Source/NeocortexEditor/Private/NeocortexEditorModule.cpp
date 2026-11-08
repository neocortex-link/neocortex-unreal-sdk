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
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "NeocortexEditor"

static const FName SettingsTabName(TEXT("NeocortexSettings"));

// The Neocortex logo for the Tools menu entry and the window's tab.
static TSharedPtr<FSlateStyleSet> MenuStyle;

static void RegisterMenuStyle()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Neocortex"));
    if (!Plugin.IsValid()) return;
    MenuStyle = MakeShared<FSlateStyleSet>(TEXT("NeocortexEditorStyle"));
    MenuStyle->Set(TEXT("Neocortex.TabIcon"), new FSlateImageBrush(Plugin->GetBaseDir() / TEXT("Resources/MenuIcon16.png"), FVector2D(16.0f, 16.0f)));
    FSlateStyleRegistry::RegisterSlateStyle(*MenuStyle);
}

class FNeocortexEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        RegisterMenuStyle();

        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SettingsTabName,
            FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
            {
                return SNew(SDockTab).TabRole(ETabRole::NomadTab)[ SNew(SNeocortexSettingsWindow) ];
            }))
            .SetDisplayName(LOCTEXT("TabTitle", "Neocortex"))
            .SetTooltipText(LOCTEXT("TabTooltip", "Your Neocortex API key, account, characters and question sets."))
            .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
            .SetIcon(MenuStyle.IsValid() ? FSlateIcon(MenuStyle->GetStyleSetName(), TEXT("Neocortex.TabIcon")) : FSlateIcon());

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
        if (MenuStyle.IsValid())
        {
            FSlateStyleRegistry::UnRegisterSlateStyle(*MenuStyle);
            MenuStyle.Reset();
        }
    }
};

IMPLEMENT_MODULE(FNeocortexEditorModule, NeocortexEditor)

#undef LOCTEXT_NAMESPACE
