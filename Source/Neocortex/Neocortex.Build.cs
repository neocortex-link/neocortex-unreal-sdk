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
			"Core", "Voice"
		});

		PrivateDependencyModuleNames.AddRange(new[] {
			"CoreUObject", "Engine", "Slate", "SlateCore", "Json", "JsonUtilities",
			"HTTP", "UMG", "Projects", "DeveloperSettings", "AudioCaptureCore", "AudioCapture", "InputCore"
		});

		// The UI icons ship as images next to the plugin and load at runtime, so every packaged
		// game has them without project settings.
		string IconsDir = Path.Combine(PluginDirectory, "Extras", "UI");
		if (Directory.Exists(IconsDir))
		{
			foreach (string Icon in Directory.GetFiles(IconsDir, "*.png"))
			{
				RuntimeDependencies.Add(Icon);
			}
		}

		// Streamed speech and streamed replies read the response while it arrives, through the
		// engine's own libcurl. Elsewhere they fall back to whole responses.
		bool bStreaming = Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Android);
		if (bStreaming)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL");
			// Newer engines build libcurl with HTTP/2, which needs nghttp2 linked too.
			if (Directory.Exists(Path.Combine(EngineDirectory, "Source", "ThirdParty", "nghttp2")))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "nghttp2", "zlib");
			}
			PrivateDependencyModuleNames.Add("SSL");
		}
		PrivateDefinitions.Add("NEOCORTEX_WITH_STREAMING=" + (bStreaming ? "1" : "0"));

		// Runtime microphone permission requests on Android (used by the recorder and the
		// UNeocortexMicrophonePermission Blueprint utility).
		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("AndroidPermission");
		}
	}
}