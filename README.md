# Neocortex Unreal SDK

**Prerequisites:**

- Unreal Engine 5.0 or higher
- C++ project (Blueprint-only projects not currently supported)
- Git installed on your system (if cloning from GitHub)
---

## Installation Steps
**1. Clone the Plugin**
   - Navigate to your project's root directory and clone the plugin into the Plugins folder:
```text
cd YourProject
mkdir Plugins
cd Plugins
git clone https://github.com/neocortex-link/neocortex-unreal-sdk Neocortex
Your project structure should look like:
```
Your project structure should look like:
```text
YourProject/
├── Content/
├── Source/
├── Plugins/
│   └── Neocortex/
│       ├── Neocortex.uplugin
│       └── Source/
└── YourProject.uproject
```
**2. Enable the Plugin**
   - Open your project's .uproject file in a text editor 
   - Add the plugin to the Plugins array (create it if it doesn't exist):
```json 
{
  "FileVersion": 3,
  "EngineAssociation": "5.x",
  "Plugins": [
    {
      "Name": "Neocortex",
      "Enabled": true
    }
  ]
}
```
**2. Enable the Plugin**
   - Open your project's .uproject file in a text editor
   - Add the plugin to the Plugins array (create it if it doesn't exist):
```json
{
  "FileVersion": 3,
  "EngineAssociation": "5.x",
  "Plugins": [
    {
      "Name": "Neocortex",
      "Enabled": true
    }
  ]
}
```
**3. Add Plugin Dependency**
   - Open your project's Source/YourProject/YourProject.Build.cs and add "Neocortex" to your module dependencies:
```text
PublicDependencyModuleNames.AddRange(new string[] 
{ 
    "Core", 
    "CoreUObject", 
    "Engine", 
    "InputCore",
    "Neocortex"  // Add this line
});
```
**4. Generate Project Files**
   - Right-click your .uproject file and select Generate Visual Studio project files (Windows) or Generate Xcode project (macOS).
**5. Compile the Project**
     - Open the generated solution/project file and build your project. The plugin will compile automatically.
     - Alternatively, use the command line:
     
Windows:
```text
"C:\Program Files\Epic Games\UE_5.x\Engine\Build\BatchFiles\Build.bat" YourProjectEditor Win64 Development "path\to\YourProject.uproject"
```

macOS:
```text
/Users/Shared/Epic\ Games/UE_5.x/Engine/Build/BatchFiles/Mac/Build.sh YourProjectEditor Mac Development "path/to/YourProject.uproject"
```
**6. Verify Installation**
   - Launch your project in the Unreal Editor. Navigate to Edit > Plugins and verify that Neocortex appears in the installed plugins list with a checkmark.

---
## Quick Start 

### API Key Setup
To start using the Neocortex Unreal SDK, you need to initialize it with your Neocortex API key. You can create a new API key from the [Neocortex Dashboard](https://neocortex.link/dashboard/api-keys) by going to the API Keys page.
- Create a new API key and copy it
- Open your Unreal project
- Go to `Edit > Project Settings > Game > Neocortex`
- Paste the API key in the API Key field and save the project

### Run Sample Scene
1. Open one of the Neocortex sample maps located at `Plugins/Neocortex/Content/Samples` for example `SampleChat.umap`
2. In the outliner, find the `BP_DemoNeoAgent` actor and select it
3. Select the NeocortexSmartAgent component and set the Project ID to your desired Neocortex project
4. Play the scene and interact with the agent using the text chat UI or voice input button

---

## Mobile Platform Setup

### Android
1. Enable permissions in Config/DefaultEngine.ini:
   [/Script/AndroidRuntimeSettings.AndroidRuntimeSettings]
   bEnablePermissionsSupport=True
2. Add microphone permission to your Android APL file (create YourProject/Build/Android/APL_armv7.xml if it doesn't exist):
```xml 
<?xml version="1.0" encoding="utf-8"?>
<root xmlns:android="http://schemas.android.com/apk/res/android">
    <init>
        <setBool result="bSupported" value="true"/>
    </init>
    <androidManifestUpdates>
        <addPermission android:name="android.permission.RECORD_AUDIO"/>
    </androidManifestUpdates>
</root>
```

#### iOS
Add to Config/DefaultEngine.ini:
```ini 
[/Script/IOSRuntimeSettings.IOSRuntimeSettings]
bEnableMicrophoneUsageDescription=True
MicrophoneUsageDescription="This app needs microphone access for voice recording"
```

#### Desktop (Windows/macOS)
No code changes needed. Users must grant microphone access through OS privacy settings:
Windows: Settings > Privacy > Microphone
macOS: System Preferences > Security & Privacy > Microphone
The OS handles permission dialogs automatically on desktop platforms.

--- 
## Troubleshooting

### Plugin not appearing in Editor:
- Ensure the .uplugin file exists in Plugins/Neocortex/
- Regenerate project files
- Check YourProject.uproject has the plugin enabled

### Compilation errors:
- Verify Neocortex is added to your .Build.cs file 
- Clean and rebuild the solution 
- Check Unreal Engine version compatibility (5.0+)

### Microphone recording fails:
- Check OS-level microphone permissions
- On mobile, ensure manifest/plist entries are correct
- Review logs for permission warnings

## Updating the Plugin
To update to the latest version:
```text 
cd YourProject/Plugins/Neocortex
git pull origin main
```
Then regenerate project files and recompile.
