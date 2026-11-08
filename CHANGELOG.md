# Changelog
All notable changes to the Neocortex Unreal SDK are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.4.0] - 2026-11-09

Moves the SDK to the Neocortex API v3 and adds chat lines, group conversations, quizzes, actions with targets, streamed speech, lip sync, ready-made chat UIs, editor tools and a new set of samples.

### Added
- **Chat lines**: replies arrive as lines, each with its emotion (`OnChatLineStarted`, `OnEmotionChanged`, `OnComposingNextLine`, `OnReplyFinished`). `ChatLinesMode` picks a voice per line, one clip for the reply, text only, or the whole reply at once. Voiced modes check your credits first. Input sent while the character speaks is queued.
- **Group conversations** with `UNeocortexGroupDirector`: up to three characters in one conversation. The cast decides who answers, or `SendTo` addresses one; `ContinueScene` lets them talk among themselves; `SendAudio` takes the player's voice; characters can join and leave while it runs.
- **Quizzes** with `UNeocortexQuizAgent` and `UNeocortexQuizUI`: a character hosts a quiz from a question set, for one player or several, with typed answers, tapped choices, hints, passes, scores, and `Resync` after a lost turn.
- **Actions with targets**: several actions per reply, each with a `TargetId`; `RegisterAction(Keyword, Handler)` on the agent, for instant or timed actions; `ActionTrigger` decides when they run; `OnActionsCompleted` and `OnUnhandledAction`.
- **Streamed speech**: the voice starts playing while the rest of the line is still being generated (`bStreamSpeech`, on by default) on Windows, Linux and Android. Other platforms play whole clips.
- **Streamed replies** (experimental): `bStreamReplies` and `OnTextDelta`, so the first line is spoken while the reply is still being written.
- **Lip sync** with `UNeocortexFaceAnimator`, on ARKit style mouth morph targets, timed to the voice.
- **Perception**: interactables carry an id and an optional character id, are sent nearest first and capped (`MaxPerceivedEntities`), and the agent sends its own position.
- **Conversation Flows**: `FlowState` on every reply.
- **History**: `RequestChatHistory(Limit, Before)` pages through past conversations with a cursor; `bLoadHistoryOnStart` restores them when play begins.
- **Errors you can act on**: `OnRequestError` carries `FNeocortexRequestError` with the status, the API's code and checks such as `IsRetryable` and `IsOutOfCredits` (also in Blueprints through `UNeocortexFunctionLibrary`). Lookups, history and audio retry once on their own. Requests time out (30 s for messages, 20 s for audio, 15 s for lookups), and `Abort` cancels a whole voice turn.
- **Chat widgets built in code**: `UNeocortexChatUI` and `UNeocortexGroupChatUI`, at the bottom centre or on the left, with bubbles and initial avatars, a typing bubble, a text field and voice controls with push to talk; the parts `UNeocortexChatTranscript`, `UNeocortexTextInput` and `UNeocortexVoiceInput` work on their own. No widget asset needed. Shared colours, text, pills, toggles and dropdowns live in `NeocortexUI`.
- **Editor tools**: Tools > Neocortex shows your key, account, characters and question sets, with Copy, and opens the samples; the Smart Agent and the Quiz Agent get a picker for character and question set ids in the Details panel.
- **Account and usage**: `GetAccount`, `GetUsage`, `GetCharacters` and `GetQuestionSets` on `UNeocortexService`, and `UNeocortexUsageGate`, which caches usage, raises credit and cap events, and gates features with `CanUseSmartNPC`.
- **Samples**: seven maps and a launcher, starring Cora with lip sync and idle, talking, thinking, walking, dance and jump animations. Each map holds its characters, camera and shapes as actors and shows a Widget Blueprint screen next to it, built from widgets that look the same in the UMG designer as in game. Each screen has a demo brief with steps to try; the group sample lets characters leave and join. Pick a character, a question set and a microphone in the launcher and every sample uses them. `Scripts/build_samples.py` rebuilds the maps, the screens and Cora headlessly.
- **Action queue**: `UNeocortexActionQueue` runs a reply's actions one at a time for Blueprint handlers that finish with `NotifyActionFinished`.
- **Voice detection**: `StartListening` on the microphone recorder waits for the player to speak and sends the recording once they pause, with `VoiceThreshold`, `MaxWaitTime`, `LeadIn`, `GetLevel`, `GetWaitProgress`, `OnSpeechStarted` and `OnSpeechEnded`. The chat UI uses it whenever it has a microphone, unless push to talk is on.
- **Voice capture on by default**: the plugin turns on Unreal's voice capture itself, so the microphone works without editing any ini file or restarting.
- **Microphone permission**: `UNeocortexMicrophonePermission` requests it on Android at runtime.
- **Right to left text**: chat messages use Unreal's own shaping and flow direction, so Arabic and Hebrew render correctly. The chat panel can apply a font override to every message.

### Changed
- All requests go to `https://api.neocortex.link/v3`.
- `FNeocortexChatResponseData` gains `Lines`, `Actions` (`FNeocortexChatAction` values; `GetActionNames()` gives the keywords), `FlowState`, `SpokenResponse`, `CharacterId` and `Name`. `Action` is the first action's keyword.
- `OnChatHistory` passes `FNeocortexChatHistoryEntry` values and the cursor to the next page.
- `FNeocortexRequestError` has `Status`, `Code`, `Message`, `bTimedOut` and `Url`.
- A failed chat request is no longer sent again on its own; check `IsRetryable` to decide.
- Perception is on by default (`bIncludeAllInteractables`).

### Removed
- `FNeocortexChatMessage`: use `FNeocortexChatHistoryEntry`.
- The **Transcription Language** setting, the agent's `Language` and the language argument of `AudioToText` and `AudioToAudio`: speech is transcribed in the character's own language.
- The old sample maps and Blueprints, replaced by the new samples.

## [0.3.0]
- Initial public Unreal SDK: text/audio chat, transcription, chat history, sessions, interactables,
  microphone capture, chat UI widgets, event logging.
