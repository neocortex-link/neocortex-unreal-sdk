using System.IO;
using UnrealBuildTool;

public class Neocortex : ModuleRules
{
	public Neocortex(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDefinitions.Add("PLATFORM_SUPPORTS_VOICE_CAPTURE=1");

		// ThirdParty under 'Plugins/Neocortex/Source/ThirdParty'
		string thirdParty = Path.Combine(ModuleDirectory, "..", "ThirdParty");
		PublicIncludePaths.Add(thirdParty);
		PublicIncludePaths.Add(Path.Combine(thirdParty, "dr_mp3"));
		

		if (Target.Platform == UnrealTargetPlatform.Android ||
		    Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicDefinitions.Add("DR_MP3_NO_SIMD=1");
		}

		PublicDependencyModuleNames.AddRange(new[] {
			"Core", "HTTP", "Voice", "ImageDownload"
		});

		PrivateDependencyModuleNames.AddRange(new[] {
			"CoreUObject", "Engine", "Slate", "SlateCore", "Json", "JsonUtilities",
			"HTTP", "UMG", "Projects", "DeveloperSettings", "ImageDownload", "AudioCaptureCore"
		});
		
		
	}
}