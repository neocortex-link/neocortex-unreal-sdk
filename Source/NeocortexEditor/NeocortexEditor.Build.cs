using UnrealBuildTool;

public class NeocortexEditor : ModuleRules
{
	public NeocortexEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[] {
			"Core", "CoreUObject", "Engine", "Slate", "SlateCore", "InputCore", "UnrealEd", "EditorStyle", "PropertyEditor",
			"ToolMenus", "WorkspaceMenuStructure", "ApplicationCore", "HTTP", "Json", "DeveloperSettings", "Neocortex",
			"UMG", "UMGEditor", "Kismet", "AssetRegistry"
		});
	}
}
