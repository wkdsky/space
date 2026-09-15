# Jump to Space Multiplayer Implementation Report

## Implemented

- The game now uses one server-authoritative, 1–4 player `Expedition` model.  A listen host and a future dedicated server use the same GameMode, GameState, PlayerState, character, inventory, spacecraft, and surface-gameplay rules; single player is simply an expedition with one player.
- Added replicated global state in `AJTSGameState` (phase, server-time Earth deadline, launch fuel target, admission state, active spacecraft, planet, and expedition id) and replicated player state in `AJTSPlayerState` (ready, host, avatar colour, status, and boarded state).  PlayerState copies its project state through seamless travel.
- Moved authoritative cross-world snapshot/save ownership from `UJTSGameInstance` to `UJTSExpeditionSubsystem`.  Snapshots contain version/id, map/planet/phase, shared spacecraft class/storage, player state, and host save UTC ticks.  Only a server/listen host writes saves.
- Added `UJTSOnlineSessionSubsystem` as the only project wrapper around OSS identity/session operations.  It supports NULL/LAN fallback, EOS-ready settings, account login fallback, create/search/join/resolve/leave/destroy, build-version and password validation, join-code collision refusal, admission updates, and network/travel failure recovery.
- Added a native C++ front end on `/Game/Space/Maps/L_Entry`: Continue, Start, Join, Settings, Credits, Quit; host form; join-code search; error/status messaging; lobby; and session details.  The lobby has four fixed slots, ready/avatar controls, join-code copy, password indicator, host start/close-joining/kick actions, and local mute controls.
- Earth now starts as a lobby, starts collection only after server validation that every active player is ready, spawns resources once on the server, uses a replicated server deadline, evaluates fuel/boarding on the server, and closes normal joining after start.
- Character interaction, pickups, carry inventory, equipment, health, melee damage, crafting, and dynamic gameplay actor lifecycle were changed to client-intent/server-validation paths with replicated outcomes.
- The shared spacecraft now replicates movement, shared storage, up to four occupant seats, and one driver.  Only the driver sends unreliable flight intent; the server applies flight simulation.  Passengers never create competing possession.
- Earth-to-SpaceWorld captures the host snapshot and uses seamless `ServerTravel`; clients follow the server.  SpaceWorld reuses the common ship, initializes a planet surface controller once, passes all active players through the surface context, and gates Moon resources/nests/enemies to server creation.
- Added `UJTSVoiceSubsystem` over Unreal VoiceChat/EOSVoiceChat: provider-owned transport, lobby-voice session setting, input/output device choices, mute/talking state, stable PlayerState identity mapping, proximity/occlusion volume fallback, optional provider positional hook, and disabled-by-default radio scaffold.  No custom voice codec or transport was added.
- Reworked local pause/leave behavior: Escape is local UI only and does not globally pause an expedition.  Host loss saves when possible, notifies remaining clients with “The host left the expedition.”, returns to the front end, and deliberately performs no host migration.
- Static review removed gameplay authority dependence on `Player 0`/`GetFirstPlayerController`; the only remaining `OpenLevel` is the host's initial `?listen` entry into Earth.  No `.uasset` or `.umap` file was changed.

## New files

- `Source/space/Core/JTSExpeditionTypes.h`
- `Source/space/Player/JTSPlayerState.h/.cpp`
- `Source/space/Systems/JTSExpeditionSaveGame.h`
- `Source/space/Systems/JTSExpeditionSubsystem.h/.cpp`
- `Source/space/Systems/JTSOnlineSessionSubsystem.h/.cpp`
- `Source/space/Systems/JTSVoiceSubsystem.h/.cpp`
- `Source/space/Modes/JTSGameplayGameModeBase.h/.cpp`
- `Source/space/Modes/JTSMainMenuGameMode.h/.cpp`
- `Source/space/UI/FrontEnd/JTSFrontEndHUD.h/.cpp`
- `Source/space/UI/FrontEnd/JTSFrontEndRootWidget.h/.cpp`
- `Source/space/UI/FrontEnd/JTSMainMenuWidget.h/.cpp`
- `Source/space/UI/FrontEnd/JTSHostExpeditionWidget.h/.cpp`
- `Source/space/UI/FrontEnd/JTSJoinExpeditionWidget.h/.cpp`
- `Source/space/UI/FrontEnd/JTSSettingsWidget.h/.cpp`
- `Source/space/UI/JTSLobbyWidget.h/.cpp`
- `Source/space/UI/JTSSessionDetailsWidget.h/.cpp`
- `Source/spaceServer.Target.cs`
- `Config/DefaultEngineEOS.example.ini`

## Major modified files

- `Config/DefaultEngine.ini`, `space.uproject`, and `Source/space/space.Build.cs`: front-end boot map, GameMode mappings, Online/Voice plugins, OSS defaults, EOS example integration, net rates, and module dependencies.
- `Core/JTSGameState.*` and `Core/JTSGameInstance.*`: replicated expedition state versus local-only preferences split.
- `Modes/JTSEarthGameMode.*`, `Modes/JTSSpaceWorldGameMode.*`, `World/JTSSpaceWorldManager.*`, and `World/JTSPlanetLandingManager.cpp`: authoritative flow, seamless travel, common arrival ship, and one-time surface initialization.
- `Player/JTSCharacter.*`, `Player/JTSPlayerController.*`, `Interaction/InteractionComponent.*`, and `Components/JTSCarryComponent.*`, `JTSHealthComponent.*`, `JTSMeleeComponent.*`, `JTSPlayerEquipmentComponent.*`: validated RPC and replication boundaries.
- `Ships/JTSSpacecraftActor.*` and `Components/JTSSpacecraftFlightMovementComponent.cpp`: shared four-seat vehicle authority and replicated flight/storage.
- `World/JTSMoon*`, `World/JTSResourceSpawnArea.cpp`, and `Items/JTS*PickupActor.*`: server-only dynamic gameplay spawning/destruction and local-only visual perspective handling.
- `UI/JTSPrototypeHUD.*` and `UI/JTSPrototypeHUDWidget.*`: replicated HUD values plus local-only session/pause actions.
- `AGENTS.md`: durable multiplayer architecture rules.

## Final compile result

- `spaceEditor Win64 Development`: succeeded using UE 5.8 `Build.bat` after the final source changes.
- `space Win64 Development`: succeeded using UE 5.8 `Build.bat` after the final source changes.
- `spaceServer Win64 Development`: the installed UE 5.8 binary distribution rejects Server Targets before project compilation with `Server targets are not currently supported from this engine distribution.`  `spaceServer.Target.cs` remains in the project for a source-built/Server-capable UE installation; no project C++ error was reported for that attempt.
- Unreal Editor, PIE, Standalone play, packaged-game launch, and automated gameplay tests were not run, as required.

## External prerequisites before internet multiplayer can work

- Create/configure an EOS product, sandbox, deployment, client id, client secret/artifact credentials, and the required session/lobby/P2P/voice permissions in the Epic Developer Portal.
- Copy the relevant values from `Config/DefaultEngineEOS.example.ini` into a local platform override such as `Config/Windows/WindowsEngine.ini`; never commit secrets.
- Select EOS as `DefaultPlatformService` only for that configured build.  The checked-in NULL provider remains the LAN/local-development fallback.
- For a dedicated deployment, use a UE distribution that supports Server Targets and provide allocator/deployment/persistent-save infrastructure.

## Intentional current limitations

- No host migration: when a listen host leaves, the expedition ends and clients return to the front end.
- No normal mid-expedition join; joining is accepted only while the lobby is open.
- No dedicated allocator/matchmaking backend yet; the code path is structured for it but player-hosted sessions are the active path.
- Radio is a disabled future capability until an authenticated provider channel exists.
- Advanced per-speaker DSP/reverb remains a provider extension; baseline voice uses provider volume/positional capabilities when available.

## Editor steps

1. In the Blueprint derived from `AJTSEarthGameMode`, set **Post Earth Space World Level** to `/Game/Space/Maps/L_SpaceWorld`.  Do not rely on the legacy `MoonLevel` fallback.
2. If any existing map has a World Settings GameMode override, keep Earth on `AJTSEarthGameMode` and SpaceWorld on `AJTSSpaceWorldGameMode`; the checked-in map-prefix configuration already supplies these defaults.
3. In the SpaceWorld configuration, verify the intended `PlanetAnchor` has the expected `PlanetId` (the native default surface definition expects `Moon`) and configure/override surface controller classes and assets in Blueprint as needed.
4. Configure EOS locally using the example file above before testing internet P2P/lobby voice.  Native C++ UI needs no newly created UMG asset; Blueprint/UMG assets may later replace its styling.
5. Ensure Entry, Earth, and SpaceWorld are included in packaging/cook settings for packaged builds.

代码改造已完成，等待玩家首次实际体验反馈。
