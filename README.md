# Neocortex Unreal SDK

**Prerequisites:**

- Unreal Engine 5.0 or higher (tested on 5.0 and 5.8)
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
**4. Turn On Voice Capture** (for microphone input)
   - Unreal's voice capture is off by default. Add this to your project's `Config/DefaultEngine.ini` and restart the editor:
```ini
[Voice]
bEnabled=True
```
**5. Generate Project Files**
   - Right-click your .uproject file and select Generate Visual Studio project files (Windows) or Generate Xcode project (macOS).
**6. Compile the Project**
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
**7. Verify Installation**
   - Launch your project in the Unreal Editor. Navigate to Edit > Plugins and verify that Neocortex appears in the installed plugins list with a checkmark.

---
## Quick Start

### API Key Setup
Create an API key on the API Keys page of the [Neocortex Dashboard](https://neocortex.link/dashboard/api-keys), then either:
- open **Tools > Neocortex**, paste the key and press **Save and refresh**: the window then shows your account, characters and question sets, each with **Copy id**; or
- go to `Edit > Project Settings > Game > Neocortex` and paste it in **API Key**.

### Run the Samples
1. Open the map `Plugins/Neocortex/Content/Samples/Maps/NeocortexSamples` and press Play.
2. Above the cards, pick one of your characters, the question set for the quiz and your microphone; every sample uses them.
3. Open a sample. **Back to Samples** (or Escape) returns to the launcher.

| Sample | What it shows |
|---|---|
| 1. Text Chat | Type to Cora; each line of the reply drops in and is voiced in its own emotion. The conversation picks up where it left off; **New Chat Session** starts afresh. |
| 2. Audio Chat | Pick a microphone and just speak: the character listens, hears when you pause and answers out loud. Turn on **Push-to-Talk** at the bottom right to talk while holding the hand button. |
| 3. Actions | Speak or type: ask Cora to dance or jump; stacked actions run in order. Needs DANCE and JUMP actions on the character. |
| 4. Interactables | Cora perceives the shapes on the floor and walks to them. Needs a GO_TO_LOCATION action. |
| 5. Group Chat | Three characters share one conversation; the toggles at the top make two of them join or leave, which starts the next round. Needs a Pro or Team key and at least two characters. |
| 6. Usage Gating | Every message goes through the usage gate first; the usage log on the left shows the account, usage and gate events. |
| 7. Quiz Show | Cora hosts a quiz for one to four players from the question set picked in the launcher; pick who replies, then type or tap an option. |

The samples star Cora, with lip sync and animations: she idles, thinks while a reply is on its way, talks while she speaks, walks, dances and jumps. Each screen has a demo brief on the right with the steps to try.

Everything a sample uses is in its map and next to it, ready to open and change:
- **The scene:** Cora and the other characters (`ANeocortexSampleCharacter`, with their Smart Agent, voice, lip sync and animations as components), the camera, the shapes and the group seats are actors in the map.
- **The screen:** each map shows a Widget Blueprint next to it, `WBP_` followed by the map name (for example `WBP_1_TextChat`). Open it in the UMG designer to see the frame, the brief, the chat and the buttons, and move or change them; the widgets draw themselves, so the designer shows the screen as it looks in game.
- **The logic:** a C++ actor per sample (`ANeocortexTextChatSample` and the others in `Samples/NeocortexSamples.h`) connects the screen to the characters by widget name.

---

## Feature Guide

### Smart Agent basics
Add a **NeocortexSmartAgent** component to an actor and set its **Project ID** (the Details panel has a picker listing your characters). Then from C++ or Blueprint:

- `SendMessage(Text)`: text in; the reply arrives on `OnChat` and, in a chat lines mode, line by line.
- `SendMessageForAudio(Text)`: text in, reply with speech. In a chat lines mode the agent voices the reply itself.
- `SendAudioForAudio(WavData)`: the full voice in, voice out flow (`OnTranscribed` shows what was heard).
- `TranscribeBytes(WavData)`: speech to text only.
- `RequestChatHistory(Limit, Before)`: a page of the conversation on `OnChatHistory`, with the cursor to the next, older page.
- `StopSpeaking()`, `Abort()`, `ClearSessionId()`.

```cpp
Agent->ProjectId = TEXT("your-character-id");
Agent->OnChatLineStarted.AddDynamic(this, &AMyActor::HandleLine);      // show Line.Text
Agent->OnEmotionChanged.AddDynamic(this, &AMyActor::HandleEmotion);    // drive animation
Agent->OnRequestError.AddDynamic(this, &AMyActor::HandleError);
Agent->SendMessage(TEXT("Hello!"));
```

### Chat lines
A reply is split into lines, each with its own emotion; their text joined is the whole reply. **Chat Lines Mode** on the Smart Agent decides how they are delivered:

| Chat Lines Mode | What you get | Cost |
|---|---|---|
| `PerLineAudio` *(default)* | Each line voiced in its own emotion, in order | About 1 audio credit **per line** |
| `SingleAudio` | Lines drop in while one clip voices the whole reply | 1 audio credit |
| `Text` | Lines drop in with an emotion each, `OnComposingNextLine` before the next | No extra cost |
| `Off` | The whole reply at once on `OnChat`; nothing is played for you | No extra cost |

The voiced modes check your credits first: a low balance voices the reply with one clip, an empty one shows text only. They play on the agent's **AudioComponent**, or create one on the owner. In every mode but `Off`, input sent while the character is still speaking is queued and sent when the reply ends. Blank, punctuation only and emoji only lines are never sent for speech.

### Streamed speech and streamed replies
With **Stream Speech** on (the default), speech plays as it is synthesized, so the voice starts sooner; the next line is fetched while the current one is heard. **Stream Replies** (experimental) streams the reply itself: `OnTextDelta` fires as text is written and the first line starts before the reply is finished. Both work on Windows, Linux and Android and fall back to whole clips and whole replies elsewhere.

### Actions
A reply can trigger several actions, in order. Each `FNeocortexChatAction` has a `Name` (the keyword authored on the character) and a `TargetId`, the interactable it applies to.

```cpp
// Instant: done when the handler returns.
Agent->RegisterAction(TEXT("WAVE"), [this](const FNeocortexChatAction& Action) { PlayWave(); });

// Timed: call Finish when done; the next action waits for it.
Agent->RegisterAction(TEXT("DANCE"), [this](const FNeocortexChatAction& Action, TFunction<void()> Finish)
{
    PlayDance(MoveTemp(Finish));
});
```

Blueprints use **Register Action** with an event. **Action Trigger** decides when the actions run: when the reply arrives, as speech starts, or after the reply is spoken. Actions without a handler raise `OnUnhandledAction`; `OnActionsCompleted` fires once all have run. The **NeocortexActionQueue** component still works for Blueprint handlers that need `NotifyActionFinished`.

### Interactables and perception
Add a **NeocortexInteractable** component to anything a character should know about. Give it a **Name** and **Properties** (for example `color: red`). Its **Id** is derived from the actor unless you set one, and actions point at it through `TargetId`. Each message carries the interactables nearest the agent (up to **Max Perceived Entities**, within **Search Radius** when **Use Radius Filter** is on) and the agent's own position.

### Errors, timeouts and retries
`OnRequestError` passes an `FNeocortexRequestError` with the HTTP `Status`, the API's `Code` and a readable `Message`. Branch on what happened, in C++ or with the Blueprint checks in `UNeocortexFunctionLibrary`:

```cpp
if (Error.IsOutOfCredits()) { ShowUpgradeHint(); }
else if (Error.IsRetryable()) { RetryLater(); }
```

Other checks: `IsUnauthorized`, `IsForbidden`, `IsRateLimited`, `IsSpeechFailure`, `IsTranscriptionFailure`. Requests time out after 30 seconds for messages, 20 for audio and 15 for lookups. Lookups, history and audio are sent once more after a retryable failure; a message never is. `Abort()` cancels a whole voice turn without reporting an error.

### Group conversations
**NeocortexGroupDirector** runs one conversation for up to three Smart Agents and has each agent speak its own turn in order. A cast of two or more needs a Pro or Team API key.

```cpp
Director->AddAgent(Cora);
Director->AddAgent(Guard);
Director->Send(TEXT("Hello you two"));             // the cast decides who answers
Director->SendTo(Guard, TEXT("Open the gate"));     // one answers
Director->ContinueScene();                          // they talk among themselves
```

`OnSpeaker` fires before each character speaks, `OnTurnStarted` and `OnTurnFinished` bracket a turn, and `RemoveAgent` lets a character leave while the scene runs.

### Quizzes
**NeocortexQuizAgent** runs a quiz hosted by one of your characters from a question set (create one under Question Sets in the dashboard). Call `Begin()`, then hand over what the player did with `Say(Text)`, `Choose(OptionId)` or `PassTurn()`. Question order, retries, hints, scoring and when the run ends are decided on the server. Listen to `OnQuestionChanged` to set the scene, `OnUpcomingQuestion` to prepare the next one, `OnExpectingChanged` to open input, `OnResult` and `OnQuizFinished` for scores. `SayAs` and `ChooseAs` name the player in a game with several.

### Lip sync
Add **NeocortexFaceAnimator** next to a Smart Agent (or set its **Quiz Agent**). It moves ARKit style mouth morph targets (`jawOpen`, `mouthFunnel`, `mouthPucker` and the rest, any casing) on the owner's skeletal meshes, from the words being spoken and timed to the voice, and rests when the voice stops. `PlayExpression(Shape, Weight)` holds a shape on top of speech.

### Ready-made UI
The chat widgets are built in code, so no widget asset is needed:

```cpp
UNeocortexChatUI* Chat = CreateWidget<UNeocortexChatUI>(PlayerController, UNeocortexChatUI::StaticClass());
Chat->SetAgent(Agent);
Chat->SetMicrophone(MicrophoneRecorder);   // optional: adds the voice controls
Chat->AddToViewport();
```

The chat UI shows the conversation as bubbles over the scene, each with the speaker's initial, a bubble of dots while a reply is on its way, and under it a text field, voice controls or both (`bShowTextInput`). **Layout** puts it at the bottom centre or on the left over a fading backdrop; `ConversationHeight`, `PlayerName` and `InputHint` fit it to your game. With a microphone it listens on its own and sends what the player says once they pause; `SetPushToTalk(true)` swaps that for a button to hold while speaking. The voice controls show the microphone level, a microphone or a clock for whose turn it is, and the silence countdown as a ring. `UNeocortexGroupChatUI` does the same for a director (`SetDirector`), with each speaker's name, and `UNeocortexQuizUI` is a whole quiz show screen (`SetQuiz`). The parts work on their own too: `UNeocortexChatTranscript`, `UNeocortexTextInput` and `UNeocortexVoiceInput`. For your own screens, `UNeocortexLabel`, `UNeocortexPillButton`, `UNeocortexToggle` and `UNeocortexDropdown` give the same look; drop any of these into a Widget Blueprint and set them in the Details panel. The colours, Arial text, pills, toggles and dropdowns come from `NeocortexUI` in `UI/NeocortexUIStyle.h`, laid out in pixels on a 1920 x 1080 screen, and the icons are the images in `Extras/UI`, which every packaged game that uses the plugin includes. Turn off **Auto Send** on the chat UI to decide yourself what happens with each message (`OnSubmitted`). The Blueprint widgets (`NeocortexChatPanel`, `NeocortexTextChatInput`, `NeocortexAudioChatInput`) are still there for your own layouts.

### Voice detection
`UNeocortexMicrophoneRecorderComponent` can listen for the player instead of recording on a button:

```cpp
Microphone->OnWavReady.AddDynamic(this, &AMyActor::HandleSpeech);   // what the player said, as WAV
Microphone->StartListening();                                        // call again for the next answer
```

A level above **Voice Threshold** starts a recording, keeping **Lead In** seconds from before it so a soft first word is not cut. After **Max Wait Time** of quiet the recording is sent and listening stops. `GetLevel` and `GetWaitProgress` drive meters, and `OnSpeechStarted` and `OnSpeechEnded` mark the player's turn.

### Account and usage
Read-only calls on `UNeocortexService` that never cost a credit: `GetAccount` (tier, owner email, credits, next refresh), `GetUsage(PlayerId, CharacterId)` (credit status and per player and per character usage), `GetCharacters` and `GetQuestionSets`. **UNeocortexUsageGate** turns usage into events and a yes or no:

```cpp
UNeocortexUsageGate* Gate = UNeocortexUsageGate::CreateUsageGate(this);
Gate->OnLowCredits.AddDynamic(this, &AMyActor::OnLow);
Gate->OnCreditsEmpty.AddDynamic(this, &AMyActor::OnEmpty);

// Cheap to call before every message; cached within MinRefreshInterval (default 30 s). Fails open.
if (Gate->CanUseSmartNPC(TEXT(""), Agent->ProjectId))
{
    Agent->SendMessage(Message);
}
```

### Right to left languages
Set **Writing Direction** = `RightToLeft` on the **NeocortexChatPanel** widget. Text uses Unreal's built-in shaping, so Arabic and Hebrew join and order correctly. The panel can also apply a **Message Font Override** to every message.

### Push to talk and microphone permission
Use **NeocortexAudioChatInput** with `bPushToTalk` enabled and a bound `RecordButton`: hold to record, release to send. On mobile, request the microphone permission early with `UNeocortexMicrophonePermission::RequestMicrophonePermission()` (no effect on desktop and iOS).

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
- Make sure `[Voice] bEnabled=True` is in `Config/DefaultEngine.ini` (see Installation step 4), then restart the editor
- Check OS-level microphone permissions
- On mobile, ensure manifest/plist entries are correct
- Review logs for permission warnings

## Rebuilding the Samples
The sample maps and Cora ship ready to use. To rebuild them, for example after changing a sample, enable the **Python Editor Script Plugin** in your project and run:
```text
UnrealEditor-Cmd.exe YourProject.uproject -ExecutePythonScript="<path to>/Plugins/Neocortex/Scripts/build_samples.py" -unattended -nullrhi
```
It imports `Extras/Cora` and its animations into `Content/Samples/Cora`, makes the checker floor, and writes the maps to `Content/Samples/Maps`, replacing what it made before.

## Updating the Plugin
To update to the latest version:
```text 
cd YourProject/Plugins/Neocortex
git pull origin main
```
Then regenerate project files and recompile.
