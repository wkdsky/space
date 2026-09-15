# Jump to Space — 1–4 人正式多人联机架构改造执行规范

> **用途**：本文件直接交给 Codex。Codex 应把它视为一次完整工程改造的主任务规范，而不是建议清单。
>
> **目标项目**：Unreal Engine 5.8，项目名 `space` / 游戏名 **Jump to Space**。
>
> **最终目标**：把当前以单机逻辑为主的 Earth → SpaceWorld → Moon 原型，改造成可以长期作为正式版基础的 **1–4 人、服务器权威、统一 Session、共享主飞船、可从玩家托管 Listen Server 平滑迁移到 Dedicated Server** 的多人架构。
>
> **关键体验参考**：《BIG WALK》式的好友小队 / Join Code / 房主进度 / 距离语音，《How to Fish》式 1–4 人合作规模。但实现必须遵循 Unreal Engine 正式联网模型，而不是复制其他游戏的内部实现。

---

# 0. Codex 必须遵守的执行规则

这是整个任务最重要的部分。**先读完全文，再开始改代码。不要只做前几步。**

## 0.1 不允许中途测试，不允许打开 Unreal Editor

用户明确要求：

- **不要启动 `UnrealEditor.exe`。**
- **不要打开 UE Editor。**
- **不要做 PIE。**
- **不要做 Standalone Play。**
- **不要在每个阶段结束后要求用户打开 UE 测试。**
- **不要创建“请先测试这一阶段，通过后我再继续”的检查点。**
- **不要因为某个阶段完成就停下来询问用户。**
- **不要做自动化 Gameplay Test / Functional Test 来替代用户测试。**
- **不要为了测试去新建临时关卡、临时 Blueprint 或临时 UAsset。**
- 从 Phase 1 一直执行到最后一个 Phase，中间持续修改代码和配置。
- 所有实现全部完成以后，才允许进行一次**命令行 C++ 编译验证**；编译不是 Gameplay 测试。
- 如果最终命令行编译报错，直接继续修编译错误，直到能编译或确认是外部环境问题。
- **即使最终编译成功，也不要打开 UE 运行游戏。**
- 最终真正的联机体验、操作流程和 Bug 由玩家自己运行游戏后检查，再进行下一轮修复。


## 0.2 不重新发明 Unreal 已经提供的底层网络

禁止：

- 自己实现 NAT Traversal。
- 自己实现可靠 UDP 协议。
- 自己实现角色移动网络预测。
- 自己实现语音 Codec / Opus 传输。
- 自己实现一套平行于 Unreal Replication 的 Actor 同步。

应使用：

- Unreal Actor / Component Replication。
- Unreal RPC。
- `ACharacter` + `UCharacterMovementComponent` 的已有网络移动能力。
- Online Subsystem（OSS）。
- OnlineSubsystemEOS + EOS P2P。
- Unreal Voice Chat Interface + EOS Voice Chat。
- `UWorld::ServerTravel` + Seamless Travel。

## 0.3 不把项目绑死到 EOS / Steam / Dedicated Server

玩法层和 UI 层不得直接调用 EOS API。

所有在线会话行为必须通过项目自己的薄封装：

`UJTSOnlineSessionSubsystem`

UI 只能表达：

- Create Expedition
- Join Expedition
- Leave Expedition
- Ready
- Invite / Join Code
- Session Details

UI 不应该知道当前底层是：

- Local
- NULL subsystem
- EOS Listen Server
- Steam
- Dedicated Server

## 0.4 遇到 UE 5.8 API 不确定时的处理方式

如果函数签名、Module 名、Plugin 名或 UE 5.8 实现细节不确定：

1. 先检查本机安装的 UE 5.8 Engine Headers / Plugin Source。
2. 以实际 UE 5.8 代码为准。
3. 不凭记忆硬写不存在的 API。
4. 不要求用户帮忙查。
5. 不因为外部 EOS ProductId 等凭据缺失而中断整个改造；这些属于最终外部配置。

---

# 1. 当前项目现状与本次改造边界

Codex 在改造前应先快速阅读当前源码，确认本文描述仍与仓库一致，但**不要重新设计一套完全不同的架构**。

当前主要目录：

```text
Source/space/
    Animation/
    Components/
    Core/
    Interaction/
    Items/
    Modes/
    Planets/
    Player/
    Ships/
    Systems/
    UI/
    World/
```

现有架构原则继续保留：

```text
GameMode
    游戏流程与规则

GameState
    全局可复制游戏状态

PlayerState
    单个玩家的网络身份与跨 Pawn 状态

Character
    玩家实体、移动、交互、装备

SpacecraftActor
    唯一共享主载具

SpaceWorldManager
    世界 / Planet / Travel 状态

SurfaceController
    当前星球地表玩法
```

## 1.1 当前已确认的主要单机假设

当前源码存在以下问题，本次必须处理：

- `AJTSGameState` 的 GameplayPhase / Earth Timer 等还不是正式 replicated state。
- `AJTSEarthGameMode` 多处使用 `GetFirstPlayerController()` / `GetPlayerPawn(World, 0)`。
- Earth → SpaceWorld 使用 `UGameplayStatics::OpenLevel()`，不是 multiplayer `ServerTravel()`。
- `UJTSGameInstance` 当前保存飞船跨关卡数据，这在多人客户端之间不是共享权威数据。
- `UInteractionComponent::TryInteract()` 当前在本地直接调用 Interact。
- Carry / Equipment 等核心组件不是完整服务器权威复制。
- `AJTSSpacecraftActor` 当前只支持一个 `BoardedPlayer`。
- 飞船自定义 MovementComponent 当前不是网络预测组件。
- Moon 多处逻辑围绕 `Player 0` / 单个 Player。
- `FJTSSurfaceGameplayContext` 当前只有 singular `Player`。
- SpaceWorld 初始 Landing 流程当前按单个玩家触发，有重复初始化 Moon gameplay 的风险。
- 当前绝大部分动态资源 / pickup / Moon 生物还没有完整 replication 规则。
- Pause / Restart / Return flow 仍按单机处理。

## 1.2 可以直接保留的重要基础

不要推倒这些已有设计：

- Earth → SpaceWorld → Current Planet 的总体世界结构。
- `ACharacter` 作为玩家 Pawn。
- `AJTSSpacecraftActor` 作为主飞船。
- `AJTSSpaceWorldManager`。
- `AJTSPlanetLandingManager`。
- `AJTSMoonSurfaceController`。
- 当前资源 / 装备 / 商店 / Moon 生物的具体玩法规则，只对网络权限与多人目标逻辑改造。
- `UJTSHealthComponent` 已经具备部分 replication 基础，应沿用。
- `AJTSPlanetLandingManager` 已有 `ArrivalSpacecraftByPlanet` / 单星球复用 ArrivalSpacecraft 的思路，应扩展而不是重写。

---

# 2. 最终多人模型：统一 Expedition

整个游戏从现在开始统一理解为：

> **一个由服务器权威运行的 1–4 人 Expedition。**

区别只在服务器运行在哪里。

```text
单人：
本机 Authority + 1 个玩家

当前试玩联机：
玩家 A = Listen Server + Client
玩家 B/C/D = Client
EOS P2P 负责互联网连通

以后正式 Dedicated：
Dedicated Server = Authority
1–4 个玩家 = Client
```

**Gameplay 规则只写一次。**

不允许维护：

```text
SinglePlayerGameMode
MultiplayerGameMode
```

两套玩法。

单人只是当前连接人数为 1。

---

# 3. 技术栈决定

## 3.1 当前采用

使用：

```text
Unreal Replication
+ Unreal RPC
+ OnlineSubsystem
+ OnlineSubsystemUtils
+ OnlineSubsystemEOS
+ SocketSubsystemEOS
+ EOS P2P
+ VoiceChat Interface
+ EOSVoiceChat
```

## 3.2 不把 Lyra CommonUser / CommonSession 作为硬依赖

Epic 的 CommonUser / CommonSession 设计值得参考，但当前项目仓库没有该插件源码，而且它通常来自 Lyra Sample。

本次：

- 不下载一大套 Lyra 到项目。
- 不为了一个 Session 封装引入 Lyra Experience 架构。
- 不让项目依赖一个仓库里不存在的 `CommonUser` Plugin。

项目自己实现一个**很薄的**：

```text
UJTSOnlineSessionSubsystem
```

其职责只封装 OSS Session / Identity / Join / Leave 等行为。

这不是 DIY 底层网络。

## 3.3 不以新的 Online Services v2 作为正式核心

UE 5.8 的 Online Services 仍有 Beta 标记。

当前正式实现优先采用成熟 OSS 接口。

必须通过 `UJTSOnlineSessionSubsystem` 隔离，因此未来替换成 Online Services v2 时不会改 UI 和 Gameplay。

---

# 4. 玩家完整流程

最终必须支持以下完整流程。

## 4.1 启动

程序启动后进入 Front End，而不是直接开始 Earth Collection。

主菜单：

```text
Continue Expedition
Start Expedition
Join Expedition
Settings
Credits
Quit
```

如果没有可继续存档：

- `Continue Expedition` Disabled 或隐藏。

## 4.2 Start Expedition

页面至少包含：

```text
Max Players: 1 / 2 / 3 / 4
Access:
    Join Code
    Friends + Join Code（provider 支持时）
    Invite Only（provider 支持时）
Optional Password
Create Expedition
Back
```

默认：

```text
MaxPlayers = 4
Access = Join Code
Password = empty
```

用户不需要选择：

```text
Listen Server
Dedicated Server
EOS
NULL
```

这些是内部策略。

## 4.3 Join Expedition

至少包含：

```text
Join Code input
Password input
Join
Back
Error / status area
```

Join Code 是当前最主要的试玩版邀请方式。

Provider 支持好友列表时，可以增加：

```text
Friends' Expeditions
Accept Invite
```

但不能让好友功能阻塞 Join Code 主路径。

## 4.4 Lobby

Lobby 使用同一套 Earth 关卡作为网络世界即可，不要求新建二进制 Lobby Map。

但此时：

- 不开始 60 秒计时。
- 不生成正式 Earth 资源。
- 不开始发射逻辑。
- 玩家在 Waiting/Lobby 阶段。

Lobby UI 固定 4 个槽位：

```text
Player Name
Host marker
Avatar Color
Ready state
Microphone / Talking state
Connection status（如果可获取）
```

Lobby 顶部或侧面：

```text
Join Code
Copy Code
Password Protected indicator
Players: N / Max
```

Host 可以：

```text
Start Expedition
Kick Player
Close Joining
```

所有玩家可以：

```text
Ready / Not Ready
Choose Avatar Color
Mute Player（本地行为）
Leave Expedition
```

开始条件：

- Host 发起。
- 至少有 1 个有效玩家。
- 所有当前有效玩家 Ready。

Host 也作为一个普通玩家参加 Ready 检查。

## 4.5 Start Expedition 后

Server：

1. 关闭中途加入。
2. Update Session phase。
3. `StartSession`（如当前 OSS 流程需要）。
4. 生成 / Reset 所有玩家。
5. 初始化唯一飞船。
6. 初始化 Earth Resources。
7. 开始 Earth 60 秒 Collection Timer。
8. Gameplay HUD 替代 Lobby HUD。

## 4.6 Earth

1–4 人同时：

- 移动。
- 捡资源。
- 使用各自 Carry Inventory。
- 向同一艘飞船装资源。
- 登上同一艘飞船。

所有资源变化必须由 Server 决定。

Earth 发射条件保留现有设计：

- Fuel 达到成功线 → 可以进入 SpaceWorld / Moon 流程。
- Fuel 未达到 → Earth Capture Failure。

登船规则本次定义成可配置：

```cpp
UENUM()
enum class EJTSEarthBoardingRequirement : uint8
{
    AllActivePlayers,
    AtLeastOne
};
```

默认：

```text
AllActivePlayers
```

Disconnected 玩家不计入 Active。

## 4.7 Earth → SpaceWorld

成功以后：

- Server 创建 expedition snapshot。
- 保存必要共享数据。
- 使用 `UWorld::ServerTravel()`。
- `bUseSeamlessTravel = true`。
- 所有连接 Client 自动跟随。
- 禁止客户端分别 `OpenLevel(L_SpaceWorld)`。

## 4.8 SpaceWorld / Moon

- 全队依旧只有一艘主飞船。
- 所有玩家被放置到同一个 Arrival Context。
- Moon Surface Gameplay **只初始化一次**。
- Moon 资源 / 巢穴 / 生物由 Server 创建。
- 所有客户端看到同一套结果。
- 不允许每个玩家各生成一份 Moon 资源或蚂蚁。

## 4.9 Esc 菜单

在线 Gameplay 按 Esc：

```text
Resume
Session Details
Settings
Leave Expedition
Quit to Desktop
```

Session Details：

```text
Join Code
Players
Mute / Unmute
Host actions（Kick 等）
```

**不调用全局 `SetPause(true)`。**

多人世界继续运行。

单人也尽量保持同样行为，避免两套流程。

## 4.10 Host 离开

第一版**明确不做 Host Migration**。

Listen Server Host 离开：

1. Host 尽可能保存 Expedition。
2. Session 销毁。
3. Clients 收到 session / network failure。
4. Clients 返回 Front End。
5. 显示：`Host left the expedition.`

以后 Dedicated Server 自然消除此限制。

## 4.11 第一版不支持 Mid-game Join

本次正式规则：

- Lobby 可以加入。
- Host Start 后 `bAllowJoinInProgress=false`。
- Earth Collection、SpaceFlight、MoonExploration 均不接受普通新玩家。

原因：

- 当前游戏已有较多状态需要网络化。
- 第一版先保证 1–4 个已组队玩家从 Earth 一直玩到 Moon。
- 架构必须为未来 reconnect / late join 留出接口，但不要在本次增加不必要复杂度。

---

# 5. Front End Map 策略：不创建新 UMap

本次禁止 Codex 生成二进制 `.umap`。

因此：

- `GameDefaultMap` 改为项目自己的空入口地图 `/Game/Space/Maps/L_Entry`。
- 新增 `AJTSMainMenuGameMode`。
- Global Default GameMode 在 Front End 使用 `AJTSMainMenuGameMode`。
- Main Menu GameMode 不生成 Gameplay Pawn。
- Main Menu HUD 使用纯 C++ UMG。

如果 Codex 检查 UE 5.8 安装目录后发现 Engine Entry Map 的 package name 与上述不同：

- 使用 UE 5.8 实际存在的最小 Engine Entry Map。
- 不创建新的项目 Map。
- 在最终报告中写明实际使用路径。

进入 Expedition：

- Host 初次进入 Earth 是一次正常非 Seamless 初始进入。
- Listen Server 使用 `L_EarthLaunchPrototype?listen`。
- Client 通过 Session resolved connect string + `ClientTravel` 首次连接。
- Earth → SpaceWorld 才使用 Seamless ServerTravel。

---

# 6. 新增目录和核心类型

建议新增：

```text
Source/space/
    Online/
    Voice/
    Network/
    UI/FrontEnd/
```

不要把所有代码继续塞进 `JTSPrototypeHUDWidget.cpp`。

---

# 7. 新增核心类

## 7.1 `AJTSPlayerState`

新增：

```text
Source/space/Core/JTSPlayerState.h
Source/space/Core/JTSPlayerState.cpp
```

职责：

- 玩家在 Expedition 中的网络身份。
- Ready。
- Avatar Color。
- Expedition player status。
- 是否 boarded。
- Host marker。
- 跨 Seamless Travel 保留的玩家轻量状态。

建议枚举：

```cpp
UENUM(BlueprintType)
enum class EJTSPlayerExpeditionStatus : uint8
{
    FrontEnd,
    Lobby,
    Active,
    Boarded,
    Dead,
    Disconnected
};
```

Avatar Color 至少扩为：

```text
Blue
Orange
Green
Purple
```

Replicated：

```text
AvatarColor
bReady
ExpeditionStatus
bBoarded
bIsHost
```

玩家名字继续使用 PlayerState 标准 PlayerName / OSS identity，不做重复数据源。

实现：

- `GetLifetimeReplicatedProps()`。
- `OnRep_*` / delegates 供 UI 更新。
- `CopyProperties()` / `OverrideWith()` 或 UE 5.8 正确的 PlayerState travel copy hook，用于 Seamless Travel 时复制项目自己的状态。

不要复制本地 mute 状态：

- Mute 是当前 Client 本地 Voice preference。

## 7.2 `FJTSExpeditionSnapshot`

新增：

```text
Source/space/Core/JTSExpeditionTypes.h
```

至少包含：

```text
Version
ExpeditionId
CurrentPhase
CurrentWorld / Planet identifier
Shared Ship Class identifier
Shared Ship Storage
Shared Supplies
Ship gameplay state needed across travel
World / procedural seed if currently meaningful
Per-player persistent lightweight state where necessary
Timestamp / save metadata
```

不要在 SaveGame 数据里保存：

- Raw Actor Pointer。
- `TObjectPtr<AActor>`。
- World-specific Actor references。

资源数据使用 SaveGame-friendly USTRUCT / enum / integer。

如果当前 `TMap<EJTSResourceType, int32>` 在 replication / SaveGame 上不好维护，可以增加：

```cpp
USTRUCT(BlueprintType)
struct FJTSResourceAmount
{
    EJTSResourceType Type;
    int32 Amount;
};
```

并通过数组作为稳定 serialization representation。

## 7.3 `UJTSExpeditionSubsystem`

新增：

```text
Source/space/Core/JTSExpeditionSubsystem.h
Source/space/Core/JTSExpeditionSubsystem.cpp
```

类型：

```cpp
UGameInstanceSubsystem
```

职责：

- 当前进程 Expedition runtime snapshot。
- Authority 在 Server 上维护跨 World 数据。
- ServerTravel 前 Capture。
- 新 World 初始化后 Restore。
- 向 SaveSubsystem 提供 snapshot。

注意：

- Client 也会拥有自己的 GameInstanceSubsystem 实例。
- **Client 的实例绝对不能成为 Gameplay authority。**
- Client 的 gameplay 展示从 GameState / replicated Actor 获取。
- Client Subsystem 只保存本地 UI / session metadata 时才可用。

## 7.4 `UJTSExpeditionSaveSubsystem`

新增：

```text
Source/space/Core/JTSExpeditionSaveGame.h/.cpp
Source/space/Core/JTSExpeditionSaveSubsystem.h/.cpp
```

职责：

- 现在：Listen Host 本地 `USaveGame` 存档。
- 未来：Dedicated Server 可替换 persistence backend。

接口至少：

```text
HasContinueSave()
LoadLatestExpedition()
SaveExpedition(const FJTSExpeditionSnapshot&)
Delete / invalidate save when explicitly requested
```

不要让 UI 直接读 `USaveGame`。

主菜单只问 SaveSubsystem：

```text
CanContinueExpedition?
```

自动保存点：

- Expedition 创建成功后。
- Earth → SpaceWorld ServerTravel 前。
- 关键共享飞船存储变化后可 debounce 保存（例如 2 秒聚合一次），不要每个资源变化都同步写磁盘。
- Host 正常 Leave / Quit 前。

客户端不保存权威 Expedition。

## 7.5 `UJTSOnlineSessionSubsystem`

新增：

```text
Source/space/Online/JTSOnlineSessionTypes.h
Source/space/Online/JTSOnlineSessionSubsystem.h
Source/space/Online/JTSOnlineSessionSubsystem.cpp
```

类型：

```cpp
UGameInstanceSubsystem
```

这是 UI 唯一可以访问的 Online Session 高层入口。

职责：

- Online identity state。
- EOS / NULL provider 获取。
- Create Session。
- Find Session。
- Join Code exact search。
- Join Session。
- Resolve connect string。
- Leave / Destroy Session。
- Update Session。
- Session error state。
- Host / Client session lifecycle。
- Network failure / travel failure cleanup 入口。

内部可以持有：

```text
IOnlineSubsystem*
IOnlineIdentityPtr
IOnlineSessionPtr
FOnlineSessionSearch
Delegate handles
```

禁止 UI 直接持有这些 OSS 类型。

### Hosting Mode

定义：

```cpp
UENUM()
enum class EJTSHostingMode : uint8
{
    Auto,
    Local,
    Listen,
    Dedicated
};
```

玩家 UI 只使用 `Auto`。

当前策略：

- 1 人且 Online provider 不可用 → Local。
- 多人互联网 → EOS Listen。
- Dedicated 先保留架构入口，当前若没有 allocator 则返回 `DedicatedUnavailable`，不能假装已经有服务器。

未来只修改 Subsystem / Hosting provider，不改菜单。

### Session 状态

定义稳定状态机，例如：

```text
Idle
Authenticating
Creating
HostingLobby
Searching
Joining
Connected
Leaving
Error
```

所有 async callback 必须：

- 清 delegate handle。
- 防重复完成。
- 处理 subsystem destroyed / world travel。
- 广播项目自己的 delegate 给 UI。

## 7.6 `UJTSVoiceSubsystem`

新增：

```text
Source/space/Voice/JTSVoiceTypes.h
Source/space/Voice/JTSVoiceSubsystem.h
Source/space/Voice/JTSVoiceSubsystem.cpp
```

类型：

```cpp
UGameInstanceSubsystem
```

详细见后面的 Voice 章节。

## 7.7 `AJTSGameplayGameModeBase`

新增：

```text
Source/space/Modes/JTSGameplayGameModeBase.h
Source/space/Modes/JTSGameplayGameModeBase.cpp
```

作为 Earth / SpaceWorld Gameplay GameMode 公共基类。

职责：

- `PlayerControllerClass = AJTSPlayerController`
- `PlayerStateClass = AJTSPlayerState`
- `GameStateClass = AJTSGameState`
- Gameplay HUD 配置
- `bUseSeamlessTravel = true`
- 通用 `PostLogin()` / `Logout()`
- Password / build compatibility `PreLogin()`
- Active player helpers
- Host role helper
- session loss / player leave common behavior

不要把 Earth / Moon 具体玩法放进这个基类。

## 7.8 `AJTSMainMenuGameMode`

新增：

```text
Source/space/Modes/JTSMainMenuGameMode.h
Source/space/Modes/JTSMainMenuGameMode.cpp
```

职责：

- Front End。
- 不生成 Character。
- 使用 FrontEnd HUD。
- 不运行 Earth Gameplay。

## 7.9 Front End UI

新增：

```text
Source/space/UI/FrontEnd/JTSFrontEndHUD.h/.cpp
Source/space/UI/FrontEnd/JTSFrontEndRootWidget.h/.cpp
Source/space/UI/FrontEnd/JTSMainMenuWidget.h/.cpp
Source/space/UI/FrontEnd/JTSHostExpeditionWidget.h/.cpp
Source/space/UI/FrontEnd/JTSJoinExpeditionWidget.h/.cpp
Source/space/UI/FrontEnd/JTSSettingsWidget.h/.cpp
Source/space/UI/FrontEnd/JTSModalDialogWidget.h/.cpp   (可选但推荐)
```

Lobby UI 推荐独立：

```text
Source/space/UI/JTSLobbyWidget.h/.cpp
Source/space/UI/JTSSessionDetailsWidget.h/.cpp
```

禁止继续把所有东西塞进 `JTSPrototypeHUDWidget`。

UI 原型可以纯 C++ `WidgetTree`。

暴露 style 参数，未来 Blueprint / UMG 可以替换。

复制 Join Code 使用平台剪贴板 API，例如 UE 5.8 可用的 `FPlatformApplicationMisc::ClipboardCopy` 或实际等价 API。

## 7.10 Dedicated Server Target

新增：

```text
Source/spaceServer.Target.cs
```

`TargetType.Server`。

这次不要求部署 Dedicated Server。

目的只是：

- 项目结构从现在开始 dedicated-ready。
- Gameplay 代码不得依赖本地渲染 / 本地玩家才存在。

如果 Launcher Binary Engine 对 Server Target 最终编译有限制，在最终报告里说明，不要为了通过它破坏项目。

---

# 8. 修改 `UJTSGameInstance`

当前 `UJTSGameInstance` 不应继续承担权威飞船跨关卡状态。

改造后职责：

```text
Local player preferences
Local front-end state
Local cached avatar preference（如果需要）
Access to Subsystems
```

迁移掉：

- authoritative spacecraft storage snapshot。
- authoritative ship class travel state。
- 任何认为“所有客户端 GameInstance 内容天然相同”的逻辑。

原有需要跨地图的权威数据移到：

```text
Server UJTSExpeditionSubsystem
```

再由新 World 的 replicated Actors / GameState 发给 Client。

---

# 9. 改造 `AJTSGameState`

`AJTSGameState` 是客户端观察 Expedition 全局 Gameplay 的主要入口。

必须真正 replicated。

至少 Replicate：

```text
GameplayPhase
FailureReason
EarthCollectionEndServerTime
ActiveSpacecraft reference
CurrentPlanet identifier / state needed by HUD
ExpeditionId（可只给 UI 使用）
```

## 9.1 Timer

不要让每个 Client 用自己的：

```cpp
World->GetTimeSeconds()
```

计算 Earth deadline。

Server 设置：

```text
EarthCollectionEndServerTime
```

Client HUD 使用：

```cpp
AGameStateBase::GetServerWorldTimeSeconds()
```

得到：

```text
Remaining = EndServerTime - GetServerWorldTimeSeconds()
```

这样不用每帧复制倒计时数字。

## 9.2 Setters

所有更改 GameplayPhase 等方法：

- 只能 Authority 调用。
- Client 不允许直接 Set。
- `OnRep` / multicast delegate 通知 HUD。

不要用 Multicast RPC 代替持久 replicated state。

---

# 10. 改造 `AJTSPlayerController`

当前 `StartGame()` 直接 `GetAuthGameMode()` 的单机方式必须修改。

## 10.1 Server RPC

至少提供：

```cpp
ServerSetReady(bool bReady)
ServerSetAvatarColor(EJTSAvatarColor Color)
ServerRequestStartExpedition()
ServerRequestBoardSpacecraft(AJTSSpacecraftActor* Spacecraft)
ServerRequestDisembarkSpacecraft(AJTSSpacecraftActor* Spacecraft)
```

具体 Interaction 仍尽量放在 InteractionComponent，不把所有 RPC 集中到 Controller。

## 10.2 Start Expedition

Client 点击 Start：

```text
UI
→ local PlayerController
→ ServerRequestStartExpedition
→ Server GameMode validates:
    caller is Host
    phase == Lobby
    all active players ready
→ Start Earth gameplay
```

Client 不直接拿 GameMode。

## 10.3 Pause / Game Menu

清除 Multiplayer Gameplay 中的：

```text
SetPause(true)
SetPause(false)
```

Esc 只：

- 切 Input Mode。
- 显示本地 Widget。
- 锁 / 解鼠标。

## 10.4 Restart / Return

普通 Client 不允许：

```text
Restart Current Level via OpenLevel
```

多人中：

- Restart Expedition 如果未来保留，只能 Host 请求 Server 做统一 restart/travel。
- `Return to Menu` = Leave Session + ClientTravel/Browse 到 Front End。
- Host Return = Save → Destroy Session → 回 Front End。

---

# 11. 临时玩家外观

用户明确允许其他玩家先使用柱状体。

当前 `AJTSCharacter` fallback 是 Cube。

改为：

```text
/Engine/BasicShapes/Cylinder.Cylinder
```

要求：

- 每个玩家按 `AJTSPlayerState::AvatarColor` 显示不同颜色。
- 颜色必须所有 Client 一致。
- 不要求新建 Material Asset。
- 可以使用 Dynamic Material Instance + parameter；如果 Engine basic material 不支持参数，则使用项目已有可 tint 材质或 C++ 创建简单 dynamic fallback 方式，具体以现有资产能力为准。
- 如果没有可靠 tint material，至少保留 replicated AvatarColor 数据；视觉颜色无法无资产完成时在最终报告说明，但不要因此中断全部改造。

之后换人物资产时：

- 只替换 Character Mesh / Anim / cosmetics。
- 不改 PlayerState / session / movement architecture。

---

# 12. Character 网络规则

`AJTSCharacter` 继续继承 `ACharacter`。

禁止为角色自己写位置 RPC。

使用 `CharacterMovementComponent` 现有 replication。

## 12.1 Boarded 状态

Replicate：

```text
BoardedSpacecraft
bIsBoarded
```

修改只能 Server。

`OnRep` 负责远端表现：

- Character collision。
- visible / hidden。
- attach / detach。
- local camera mode。

不要让 Client 自己认为已登船就立即永久改变权威状态。

## 12.2 Player Persistent State

因为 Character 在 Seamless Travel 后可能重新生成，不应把所有跨 World 状态只放 Character。

Server 在 Travel 前：

```text
Character Carry / Equipment
→ Capture into PlayerState persistent snapshot / ExpeditionSnapshot
```

新 Character spawn 后：

```text
PlayerState / Expedition state
→ Restore Character Components
```

---

# 13. Interaction：从本地交互改成 Server Authority

当前：

```text
TryInteract()
→ Execute_Interact(Target, Pawn)
```

必须改。

保留 Client 本地 Target Trace 用于：

- Prompt。
- Highlight。
- “Press E”。

真正交互：

```text
Client presses E
→ UInteractionComponent::ServerTryInteract(Target)
→ Server validates
→ Execute_Interact(Target, Pawn) on Server
→ State changes
→ Replication
```

## 13.1 Server validation

至少检查：

- Owner Pawn valid。
- Target valid。
- Target implements `IInteractable`。
- Target not pending kill。
- 距离 <= server interaction range + 小容差。
- 必要时 Line Of Sight。
- 当前玩家状态允许交互（未 dead / 未 boarded / 等）。

Server 重新验证，不接受 Client 声称“我已经在范围内”。

## 13.2 RPC 位置

`UInteractionComponent`：

- `SetIsReplicatedByDefault(true)`。
- Owner 必须是 replicated Pawn。
- `ServerTryInteract` 使用 Reliable。

低频玩家按键 RPC 用 Reliable 可以接受。

---

# 14. Carry / Inventory 网络改造

当前 `UJTSCarryComponent` 必须服务器权威。

## 14.1 数据表示

优先 replicate 有确定顺序的：

```text
TArray<EJTSResourceType> CarriedItems
```

如果当前 `TMap` 只是为了快速 aggregate：

- Client 可根据 replicated array rebuild aggregate。
- 或 replicated `TArray<FJTSResourceAmount>`。

避免依赖 unordered `TMap` 作为 UI ordering。

## 14.2 mutation

只有 Server 可以：

```text
Add
Remove
Clear
Transfer to ship
Drop
```

Client UI 根据 OnRep 更新。

不要让 Client inventory 与 Server inventory 双写。

---

# 15. Equipment 网络改造

`UJTSPlayerEquipmentComponent`：

Replicate：

```text
4 equipment slots
Selected slot index
```

只有 Server 可以：

- Add Equipment。
- Remove。
- Equip。
- Drop。

Drop：

- Pickup actor 只在 Server Spawn。
- Client 不 Spawn 自己的 pickup。

`OnRep` 更新 HUD / visible equipment。

---

# 16. Health / Melee

`UJTSHealthComponent` 已有 replication，保留并检查：

- Damage 只由 Authority 最终应用。
- Death replicated。

`UJTSMeleeComponent`：

- Client input 发起 Server attack request。
- Server 验证 attack cooldown / state。
- Server 做最终 hit validation 和 damage。
- Cosmetic animation 可以本地立即播放以减少输入延迟，但最终伤害不由 Client 决定。
- 不要信任 Client 传入 Damage amount。

如果现有 animation notify 在 Client 直接 ApplyDamage：

- 改为 Authority branch。
- Server attack window 驱动最终命中。

---

# 17. 所有 Pickup / Resource Actor

至少审计：

```text
JTSResourcePickupActor
JTSWorldPickupActor
JTSMoonResourceActor
JTSMoonAntCorpsePickupActor
JTSMoonCorpseActor
以及其它 runtime spawned interactable
```

要求：

- Gameplay 相关动态 actor：`bReplicates = true`。
- 需要移动同步时 `bReplicateMovement = true`。
- Spawn 只在 Server。
- Destroy 只由 Server。
- Resource depleted / collected 状态从 Server replicate。

如果静态 Level-placed Actor 只需 Server Destroy：

- Actor 必须 replicated，确保 destruction 对 Client 同步。

---

# 18. 唯一共享 Spacecraft 的正式多人模型

这是本次最重要的 Gameplay 改造之一。

`AJTSSpacecraftActor` 永远代表一个共享主载具。

不要现在实现每玩家个人飞船。

## 18.1 replication

构造：

```cpp
bReplicates = true;
bReplicateMovement = true;
```

合理配置 NetUpdateFrequency，例如：

```text
NetUpdateFrequency ≈ 30
MinNetUpdateFrequency ≈ 10
```

最终以实际 UE API / 当前移动需求为准。

## 18.2 Occupants

删除“只有一个 `BoardedPlayer`”作为唯一真相的设计。

新增可 replicated：

```cpp
UENUM()
enum class EJTSSpacecraftSeatRole : uint8
{
    Driver,
    Passenger
};

USTRUCT()
struct FJTSSpacecraftOccupantState
{
    AJTSPlayerState* PlayerState;
    EJTSSpacecraftSeatRole Role;
};
```

或 UE replication 更安全的等价结构。

最多 4 人。

至少维护：

```text
Occupants
DriverPlayerState
```

## 18.3 Earth boarding

Earth Collection 阶段：

- 玩家 Board 后作为 Passenger/Boarded state。
- 不需要因为 Board 就让 4 个 Controller 都 Possess ship。
- Character 变为 boarded representation。
- 飞船仍然是共享对象。

## 18.4 SpaceWorld driver

SpaceFlight 阶段：

- 同一时间最多一个 Driver。
- Driver 才真正拥有 flight input authority request。
- 默认选择规则：第一个在可驾驶阶段获得控制权的 boarded 玩家成为 Driver。
- 不假设 Host 永远是 Driver。
- Passenger 不 Possess 同一 Pawn。

为了避免一个 Pawn 被多个 PlayerController possession：

- Driver Controller 可 Possess Spacecraft。
- Passenger Controller 保留 / 使用自己的 passenger Pawn/camera actor strategy，或者 Character attach 到 spacecraft 并禁用 movement；选择对当前代码侵入最小的方案。
- Passenger camera 必须能正常看到世界。

如果当前 `AJTSCharacter` → `Spacecraft` Possession 流程很深，优先：

```text
Driver possesses Spacecraft
Passenger Character attached/hidden or seat-attached
```

不要做 multi-possession hack。

## 18.5 Driver transfer

本次不要过度设计自动驾驶席转移。

规则：

- Driver 正常离开并处于 grounded / disembark allowed 状态 → Driver cleared。
- 其他玩家可通过已有 Board / TakeControl 逻辑成为新 Driver。
- 不必在 Driver 离开瞬间静默强制把控制权给随机 Passenger。

为未来显式 `Take Controls` 交互保留接口。

---

# 19. Spacecraft Flight 网络方式

当前 `UJTSSpacecraftFlightMovementComponent` 不是 `CharacterMovement`，不要假设它自动拥有客户端预测。

本次第一版采用：

> **Server-authoritative ship simulation + replicated movement。**

## 19.1 Driver input struct

新增 compact struct，例如：

```cpp
USTRUCT()
struct FJTSSpacecraftInputState
{
    float Throttle;
    float Steering;
    float Pitch;
    float Yaw;
    bool bBoost;
    bool bLanding;
};
```

值 clamp。

## 19.2 flow

```text
Driver Client collects input
→ unreliable Server RPC with compact input state
→ Server validates caller == current Driver
→ Server movement component simulates
→ Actor replicated movement
→ all clients observe
```

高频 flight input：

- 使用 `Unreliable` RPC。
- 不要一帧发多个重复 RPC。
- 可按 input tick / fixed network update rate 发送。

## 19.3 不在本次自研 full client prediction

第一版不要造：

- rewind。
- reconciliation framework。
- custom saved moves。
- full NetworkPhysics prediction。

如果以后玩家实测高延迟驾驶手感不够，再只针对 Spacecraft 优化。

当前架构必须允许未来替换 movement networking，但不预先实现复杂预测。

---

# 20. Spacecraft Storage

飞船 Storage 是共享服务器数据。

不能继续以每个 Client `JTSGameInstance` 的 TMap 为真相。

建议在 `AJTSSpacecraftActor`：

- Server authoritative storage。
- Replicate compact snapshot / fast array / resource amount list。
- `OnRep` 更新所有客户端 HUD。

修改 Storage 的入口：

```text
DepositResource
ConsumeResource
Purchase / craft / fuel use
```

必须 Authority-only。

Travel 前：

```text
Spacecraft Storage
→ UJTSExpeditionSubsystem snapshot
```

Travel 后：

```text
snapshot
→ new Spacecraft Storage
```

---

# 21. Earth GameMode 改造

`AJTSEarthGameMode` 改为继承 `AJTSGameplayGameModeBase`。

## 21.1 BeginPlay

不要 BeginPlay 直接启动 60 秒。

初始：

```text
GameplayPhase = WaitingToStart / Lobby
```

由 Host Start 才开始。

## 21.2 StartEarthCollection

必须：

- Authority only。
- 对所有 Active Players 应用正确状态。
- 不使用 `GetFirstPlayerController()`。
- 一次性生成 Earth resources。
- 一次性找到 / 初始化唯一飞船。
- 设置 Server deadline。
- Session phase Update 到 EarthCollection。

## 21.3 FinishEarthCollection

不允许：

```cpp
GetPlayerPawn(World, 0)
```

而是：

- 遍历 `GameState->PlayerArray` 或 authoritative controller list。
- 判断所有 Active Player boarding 状态。
- 按 `EJTSEarthBoardingRequirement` 判定。
- 检查唯一共享 ship fuel/storage。

## 21.4 launch outcome

成功：

```text
Capture Expedition Snapshot
Save Host Snapshot
Update GameState = Launching
ServerTravel to L_SpaceWorld
```

失败：

- Server 设置失败原因和 phase。
- Client HUD 通过 replication 显示。
- 不让每个客户端各自算结果。

## 21.5 禁止 Earth GameMode 使用本地 GameInstance 作为网络数据源

GameInstance 可拿：

```text
UJTSExpeditionSubsystem
```

但所有修改必须是 Server context。

---

# 22. Seamless Travel

Earth → SpaceWorld 必须用 Unreal 正式 multiplayer travel。

`AJTSGameplayGameModeBase`：

```cpp
bUseSeamlessTravel = true;
```

Earth Server：

```cpp
GetWorld()->ServerTravel(SpaceWorldURL);
```

不要用：

```cpp
UGameplayStatics::OpenLevel(this, SpaceWorld)
```

来推进已经建立的多人 session。

保留 PlayerState / controllers 的必要状态。

如需 transition map：

- 可以保持 `TransitionMap` 为空，让 UE 使用默认空 transition world，避免创建新 `.umap`。
- 不要因为没有 transition asset 阻塞。

---

# 23. SpaceWorld GameMode 改造

当前 `JTSSpaceWorldGameMode` 不得再只对 `GetFirstPlayerController()` 做 Initial Landing。

流程应变为：

1. Server 在 World ready 后识别全部 Active PlayerControllers。
2. 找到 / 生成当前 Planet 共享 ArrivalSpacecraft。
3. 对每个玩家生成 / restore Character。
4. 在 Arrival anchor 周围使用 deterministic offsets 分布角色，避免完全重叠。
5. 所有玩家关联到同一个 Spacecraft。
6. 等必要 world context ready 后，只调用一次 Surface Gameplay Initialization。
7. 后续玩家 state restoration 是 per-player；SurfaceController initialization 不是 per-player。

`HandleStartingNewPlayer()` 可以注册新 Controller，但不要每调用一次就重新 spawn 整个月球资源。

因为第一版 Start 后禁止新普通玩家加入，所以重点是 Seamless Travel 后 1–4 个已有玩家全部正确恢复。

---

# 24. `JTSPlanetLandingManager`：保留“一星球一艘船”

现有 `ArrivalSpacecraftByPlanet` 思路继续使用。

必须保证：

```text
Planet A
→ exactly one shared arrival spacecraft for this Expedition
```

所有 PlayerController 映射到这个共享 ship。

不要：

```text
Player 1 → Ship A
Player 2 → Ship B
Player 3 → Ship C
```

`PlayerSpacecraft` 如果保留：

- 可以让多个 PlayerController value 都指向同一个 ship。

或重构为：

```text
CurrentSharedSpacecraft
```

加 occupants。

选择对现有代码侵入更小的方案。

---

# 25. Surface Gameplay Context 改为多人

当前 `FJTSSurfaceGameplayContext` singular `Player` 必须重构。

建议：

```cpp
USTRUCT()
struct FJTSSurfaceGameplayContext
{
    Planet;
    Spacecraft;
    GameplayData;
    TArray<...> Players;
};
```

或：

- Context 只要求 Planet + Spacecraft + GameplayData。
- SurfaceController 单独通过 `RegisterPlayer/UnregisterPlayer` 维护玩家。

推荐第二种更稳：

```text
InitializeSurfaceGameplay(Context)
    只初始化一次星球玩法

RegisterPlayer(Character)
UnregisterPlayer(Character)
```

这样以后 reconnect / late join 更容易。

`HasRequiredRuntimeActors()` 不应再依赖“必须有唯一 Player 0”。

至少要求：

```text
Planet valid
Spacecraft valid
GameplayData valid
```

玩家集合可以随后注册。

---

# 26. Moon Surface Controller 多人化

`AJTSMoonSurfaceController` 当前 singular `CachedPlayer` 改为玩家集合 / helper。

至少：

```text
RegisteredPlayers
GetActivePlayers()
GetNearestActivePlayer(Location)
```

`InitializeSurfaceGameplay()`：

- Server Only。
- 一次。
- 创建 landmarks / nests / resource setup / consumption timer。

Client 不生成第二份。

## 26.1 Player0 refs

逐个审计：

```text
JTSMoonSurfaceController.cpp
JTSMoonResourceSpawner.cpp
JTSMoonAntActor.cpp
JTSMoonAntCorpsePickupActor.cpp
JTSMoonLoopGroundActor.cpp
JTSMoonWorldActor.cpp
JTSMoonWrappedActorComponent.cpp
```

不要机械地把所有 Player0 都替换成一个全局数组。

先分类：

### Gameplay authority 逻辑

例如：

- resource spawn exclusion。
- ant target。
- damage。
- nest behavior。
- collision decisions。

必须使用 Server 的 Active Players 集合。

### Local visual / camera logic

例如如果某些 Moon wrap / presentation 逻辑本来就是：

- 每个客户端围绕自己的 local camera 做视觉偏移。

这种**可以**使用 local Player Pawn。

但要：

- 改名或注释明确 `LocalViewPawn`。
- 不让这个 local presentation 结果影响 Server gameplay state。

---

# 27. Moon Resource Spawner

要求：

- Resource spawning 只在 Server。
- 避让区域要检查所有 Active Player positions，而不是 Player0。
- 避让共享 Spacecraft。
- 使用 Server RNG 决定最终 spawned actors。
- Client 不自己根据相同 seed 再生成一套资源。

Seed 可用于存档 /重现，但**不能以“客户端也跑相同随机数”代替 replication**。

---

# 28. Moon Ant / Nest 网络化

## 28.1 Spawn

`AJTSMoonAntNestActor`：

- Spawn ant 只在 Authority。
- Spawn timer / random interval 只在 Authority。

## 28.2 Ant AI

`AJTSMoonAntActor`：

- AI state decisions Server only。
- Move / state Server authoritative。
- Actor replicated。
- 需要位置同步则 `bReplicateMovement=true`。
- Client 不各自执行独立 roaming / flee target 随机决策。

Target：

- 从所有 Alive + Active players 中选 nearest / 当前规则最合适者。
- 定期 retarget，不要每帧遍历所有玩家（最多 4 人，但仍保持合理）。

## 28.3 Damage / death

- Damage Server only。
- Health replicated。
- Death state replicated。
- Corpse / pickup Server spawn。

---

# 29. SpaceWorldManager replication

审计 `AJTSSpaceWorldManager`。

如果以下状态会影响 Client gameplay / HUD：

```text
TravelState
CurrentPlanet
SurfaceReady
CurrentLanding state
```

则：

- Server-owned setters。
- replicated / RepNotify。

不要让 Client 自己推进世界 travel state machine。

客户端只表现 replicated state。

---

# 30. Session 具体实现规范

`UJTSOnlineSessionSubsystem` 应使用 `NAME_GameSession` 或项目统一 Session Name。

## 30.1 Custom Settings

定义稳定 FName keys，例如：

```text
JTS_JOIN_CODE
JTS_BUILD_VERSION
JTS_PASSWORD_PROTECTED
JTS_EXPEDITION_ID
JTS_PHASE
JTS_SAVE_VERSION
```

另使用标准：

```text
SETTING_MAPNAME
```

所有需要搜索的 custom setting 设置合适的 `EOnlineDataAdvertisementType`，至少可通过 Online Service 搜索。

## 30.2 Session Settings

Lobby 创建时建议：

```text
bIsLANMatch = false for EOS
bShouldAdvertise = true
NumPublicConnections = MaxPlayers
bUsesPresence = true where provider supports
bAllowJoinViaPresence = true where provider supports
bAllowJoinInProgress = true while Lobby
bUseLobbiesIfAvailable = true
bUseLobbiesVoiceChatIfAvailable = true
```

具体字段以 UE 5.8 `FOnlineSessionSettings` 实际头文件为准。

Start Expedition 后：

```text
bAllowJoinInProgress = false
JTS_PHASE = EarthCollection
UpdateSession
```

## 30.3 Join Code

生成 6 位左右 human-readable code。

字符集避免：

```text
0 O
1 I L
```

例如只用：

```text
ABCDEFGHJKMNPQRSTUVWXYZ23456789
```

- uppercase。
- UI 自动 trim whitespace / hyphen。
- 搜索使用 exact match。
- 不把 raw IP 暴露给普通用户。

Find：

- `FOnlineSessionSearch`。
- QuerySettings 对 `JTS_JOIN_CODE` `Equals`。
- 额外校验 `JTS_BUILD_VERSION`。
- 如果找到多个同 code 结果，优先 exact build + joinable，并处理冲突而不是随机无提示加入。

## 30.4 Build Version

定义项目 multiplayer build string，例如来自：

- Project version + protocol constant。

不要只依赖显示版本。

增加：

```cpp
constexpr int32 JTS_NETWORK_PROTOCOL_VERSION = ...;
```

或等价。

不同 protocol：

- Join 时明确拒绝。
- `PreLogin` 再做服务器最终验证。

## 30.5 Password

Session 广播：

```text
JTS_PASSWORD_PROTECTED = true/false
```

不要广播明文密码。

Host runtime memory 保存 password / verification token。

Join Client 提交 password credential 到 connect URL option 或 UE 5.8 当前最合适的 prelogin options path：

```text
?JTSPassword=...
?JTSBuild=...
```

`AJTSGameplayGameModeBase::PreLogin()`：

- 验证 build。
- 验证 password。
- reject message 清晰。

如果直接 URL 传明文 password 会进入日志，优先做一个简单的 hash/token 表示，避免日志直接出现明文；但不要造复杂认证协议。

这只是小队房间密码，不是账户安全系统。

## 30.6 EOS Identity

Internet multiplayer 创建 / 加入 EOS session 前必须确保 local user 已登录。

实现顺序：

1. 如果 Identity 已 `LoggedIn` → 继续。
2. 尝试 `AutoLogin(0)`，允许开发/命令行 credentials。
3. 如果 AutoLogin 没有启动或失败，EOS provider 下使用 `FOnlineAccountCredentials("AccountPortal", "", "")` 调 `Login`。
4. 登录 callback 后继续原先 pending action。
5. NULL / Local backend 不强制 EOS login。

不要在 UI 里写 EOS login 细节。

UI 只看到：

```text
Connecting to online services...
Login failed...
```

## 30.7 Online unavailable

如果 EOS credentials 尚未配置：

- 单人 Local 仍应可以进入。
- 多人 Create/Join 返回清晰 `OnlineUnavailable`。
- 不 crash。
- 不无限 loading。

不要为了让代码“看起来能用”写假的 session success。

---

# 31. EOS 配置与 Plugin

修改 `space.uproject`，按 UE 5.8 实际可用 Plugin 名启用需要的 Online plugins。

预期包括（以安装的 UE 5.8 Plugin descriptor 为准）：

```text
OnlineSubsystem
OnlineSubsystemUtils / required subsystem support if pluginized
OnlineSubsystemNull
OnlineSubsystemEOS
SocketSubsystemEOS
VoiceChat
EOSVoiceChat
```

某些 Module 不是独立 `.uplugin`，不要因为名字出现在这里就盲目添加不存在的 Plugin。

**先检查 UE 5.8 Plugins 目录实际 descriptor。**

## 31.1 `space.Build.cs`

加入实际需要的 modules，例如：

```text
OnlineSubsystem
OnlineSubsystemUtils
VoiceChat
```

只有直接 include EOS-specific header 时才增加：

```text
OnlineSubsystemEOS
```

减少 gameplay files 对 EOS-specific header 的依赖。

## 31.2 EOS NetDriver

按 UE 5.8 官方 OSS EOS 配置使用：

```ini
[/Script/Engine.Engine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SocketSubsystemEOS.NetDriverEOSBase",DriverClassNameFallback="/Script/OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[/Script/SocketSubsystemEOS.NetDriverEOSBase]
bIsUsingP2PSockets=true
```

但必须先检查项目已有 `NetDriverDefinitions`，避免重复冲突。

## 31.3 EOS Credentials

**Codex 不得编造：**

```text
ProductId
SandboxId
DeploymentId
ClientId
ClientSecret
EncryptionKey
```

不要写假的看似真实字符串。

新增一个明确模板，例如：

```text
Config/DefaultEngineEOS.example.ini
```

包含注释和 placeholder。

如果当前 DefaultEngine 没有真实 EOS artifact config：

- 完成所有代码。
- 最终报告把“创建 EOS Developer Portal Product 并填入 credentials”列为外部前置步骤。
- 不因此停下来问用户。

## 31.4 DefaultPlatformService 与 fallback

项目必须能在 credentials 缺失时仍启动 Front End / 单机。

实现 Provider selection：

- 优先尝试指定 EOS subsystem `IOnlineSubsystem::Get(TEXT("EOS"))`。
- 如果不可用，允许 NULL / Local fallback。
- 互联网 Create/Join 若需要 EOS 而不可用，显示错误。

如果 UE 5.8 的 EOS NetDriver / subsystem 初始化要求 `DefaultPlatformService=EOS` 才能正确工作，则按实际 Engine Source 调整配置，但必须保留 Local fallback，不让前端直接 crash。

最终报告写清实际做法。

---

# 32. Voice Chat 总体目标

语音是正式架构的一部分，但不要自己传音频包。

目标分层：

```text
Tier 0 — EOS Lobby Voice transport
Tier 1 — 距离 / 遮挡音量模拟
Tier 2 — provider 支持时使用 positional 3D channel
Tier 3 — 未来真正环境 DSP / Radio / advanced acoustics
```

本次必须实现 Tier 0 + Tier 1 基础，并把 Tier 2 / Tier 3 的接口架好。

不要声称在没有实际 provider positional channel / audio pipeline 的情况下已经完成了完整 BIG WALK 声学效果。

---

# 33. EOS Lobby Voice

创建 EOS lobby session：

```text
bUseLobbiesIfAvailable = true
bUseLobbiesVoiceChatIfAvailable = true
```

EOS OSS 可以自动加入/离开 lobby voice。

通过 UE 5.8 EOS / VoiceChat 正式接口取得：

```text
IVoiceChatUser
```

不要直接依赖 EOS SDK raw C API。

`UJTSVoiceSubsystem` 负责：

- 获取 VoiceChatUser。
- 注册 talking / player added / removed / mute events。
- input device list。
- output device list。
- mic mute。
- player mute。
- output/input volume。
- UI callbacks。

---

# 34. Proximity Voice 基础

由于 lobby voice 与自定义 trusted positional channel 的能力不同，本次做 provider-independent baseline。

每个 Client 本地 `UJTSVoiceSubsystem` 以低频更新，例如：

```text
10 Hz
```

对每个 remote player：

1. 找到 Local Pawn 世界位置。
2. 找到 Remote Player 对应 Pawn 世界位置。
3. 计算距离。
4. 算 proximity gain。
5. 做一条或少量 Visibility / VoiceOcclusion trace。
6. 得到 obstruction multiplier。
7. 最终调用 `IVoiceChatUser::SetPlayerVolume(RemoteVoiceId, Volume)`。

默认可调参数：

```text
FullVolumeDistance ≈ 300 cm
MaxAudibleDistance ≈ 3000 cm
OccludedVolumeMultiplier ≈ 0.45
VoiceUpdateInterval ≈ 0.1 s
```

这些全部放在 config / settings struct，不散落 magic number。

距离曲线：

- FullDistance 内 1.0。
- 之后平滑下降。
- MaxDistance 后 0。

不要每 Tick 做 3 × 多条复杂 trace。

最多 4 人，10Hz 足够 baseline。

---

# 35. Voice Identity Mapping

EOS Voice Participant Name 与 `AJTSPlayerState` 必须有稳定映射。

在 PlayerState / VoiceSubsystem 做 helper，例如：

```text
GetOnlineIdentityString()
GetVoiceIdentityString()
```

优先来自：

```text
FUniqueNetIdRepl
```

不要用 display name 作为唯一 identity，因为名字可以重复。

Lobby UI 的 talking indicator：

- Voice callback → identity → PlayerState → UI slot。

---

# 36. Positional 3D Voice 扩展

UE `IVoiceChatUser` 提供：

```text
Set3DPosition(ChannelName, Position)
FVoiceChatChannel3dProperties
    MinDistance
    MaxDistance
    AttenuationModel
```

如果当前 EOS channel integration 实际支持可控 Positional Channel：

- 使用 provider capabilities。
- 定期 `Set3DPosition`。

但是 EOS Trusted Server 模式可能要求由可信服务器生成 channel credentials。

因此：

- 不用 insecure development token 当正式方案。
- 不伪造 trusted server。
- 当前 player-hosted 版本没有安全 token service 时，不强行写假 positional flow。

`UJTSVoiceSubsystem` 增加 capability：

```text
bSupportsProviderPositionalVoice
```

可用则启用；不可用则继续 Tier 1 distance volume baseline。

---

# 37. 环境声音干扰架构

未来目标：

```text
无遮挡：正常
距离远：衰减 + 空气感
隔墙：闷 / low-pass / 音量降低
洞穴：reverb
走廊：echo / reflections impression
```

UE VoiceChat interface 提供 mixed / unmixed PCM callbacks，例如：

```text
RegisterOnVoiceChatBeforeRecvUnmixedAudioRenderedDelegate
RegisterOnVoiceChatBeforeRecvMixedAudioRenderedDelegate
```

本次不要一上来实现完整自定义 PCM → ProceduralSoundWave pipeline，除非 UE 5.8 当前 API 可以非常干净且无风险地接入。

本次应：

1. 实现距离 + 遮挡音量 baseline。
2. 新增明确扩展接口 / strategy hook，例如：

```text
EJTSVoiceAcousticMode
ComputeProximityGain()
ComputeOcclusionGain()
ApplyProviderVoiceMix()
```

3. 给未来 PCM route 留封装位置。
4. 不把这些逻辑写进 Character。

最终报告中明确：

```text
Implemented now:
- voice transport
- mute/device/talk state
- distance gain
- obstruction gain

Future advanced acoustics:
- per-speaker low-pass
- reverb
- radio DSP
```

---

# 38. 对讲机架构

现在不要求对讲机成为可获得装备并完成全玩法，但架构必须预留。

定义：

```cpp
UENUM()
enum class EJTSVoiceRoutingMode : uint8
{
    Proximity,
    Radio
};
```

`UJTSVoiceSubsystem` 具备：

```text
SetRoutingMode
CanUseRadio
Transmit route abstraction
```

未来：

- Proximity channel。
- Radio channel。
- Push-to-talk / radio equip gating。
- `TransmitToSpecificChannels()`。
- Radio DSP：band-limit / compression / static / crackle。

当前：

- `bRadioFeatureEnabled=false` 或 provider capability gated。
- 不生成 fake trusted channel credentials。
- 不自己写 voice transport。

---

# 39. Settings

Settings 至少为 Voice 预留：

```text
Master voice output volume
Microphone input volume
Mute microphone
Input device
Output device
Push-to-talk / open mic preference（架构可留）
```

如果当前没有专门用户设置类，可以新增：

```text
UJTSGameUserSettings : UGameUserSettings
```

保存纯本地设置：

- voice device identifiers / preference。
- volumes。
- local avatar preference。
- 其它未来 graphics/control settings。

不要用 replicated PlayerState 保存设备名。

---

# 40. 主菜单 / Lobby UI 状态驱动原则

Widget 不自己猜网络状态。

UI 订阅：

```text
UJTSOnlineSessionSubsystem delegates
UJTSVoiceSubsystem delegates
AJTSGameState delegates
AJTSPlayerState RepNotify/delegates
```

Button enable/disable 根据状态机。

例如：

- `Creating` 时 Create button disabled。
- `Joining` 时 Join disabled。
- error 后恢复。
- 不允许连续 double-click 创建多个 Session。

所有 async 操作都要有 timeout/error pathway（OSS callback 为主；可以额外 watchdog，但不要过度复杂）。

---

# 41. Network Failure / Travel Failure

在 GameInstance / OnlineSessionSubsystem 注册 Engine network/travel failure delegate。

处理：

```text
Connection lost
Host left
Join failure
Travel failure
Session destroyed
```

流程：

1. 清理 voice。
2. 清理 OSS session / delegates。
3. reset transient Expedition client state。
4. 回 Front End。
5. Front End 显示可读错误。

不要 crash 或停在没有 Pawn 的空 world。

---

# 42. 玩家离开和 Kick

## Client Leave

```text
Local player requests leave
→ Voice cleanup
→ Destroy/End local session membership
→ ClientTravel Front End
```

## Host Kick

Host-only server action：

- 验证 target 非 host 自己。
- 让 target 客户端返回 FrontEnd，或使用 UE 正确网络 close/kick 方式。
- 更新 Session / lobby list。

不要让 Client 自己从 UI 直接调用 socket close hack。

## Host Leave

前文规则：

- save。
- destroy session。
- clients 返回 menu。
- no host migration。

---

# 43. Save / Continue Expedition

主菜单有 `Continue Expedition`，所以本次需要数据路径完整。

## 43.1 Listen Server

Host 本地存权威 save。

Clients 不拥有可继续该 Expedition 的 authority save。

## 43.2 Continue

Host 点击 Continue：

1. Load latest snapshot。
2. Create a **new multiplayer Session**。
3. 生成新的 Join Code。
4. 进入对应 world / lobby-like resume staging。
5. 新加入的朋友进入 Lobby / Resume lobby。
6. Host Start/Resume 后 restore gameplay。

第一版可以规定：

- Continue 仍先把大家聚到 Lobby state。
- 不允许朋友通过旧 Join Code 直接连接已经不存在的 Session。

## 43.3 Save location

Snapshot 记录当前主位置，例如：

```text
Earth
SpaceWorld/Moon
```

当前原型只需要可靠恢复目前已有章节。

不要为未来 Mars 预写大量空代码。

## 43.4 Dedicated future

将来：

```text
UJTSExpeditionSaveSubsystem
```

内部 backend 从：

```text
Local SaveGame
```

换成：

```text
Dedicated persistence / backend DB
```

GameMode / Spacecraft / UI 不改变。

---

# 44. 服务器权威总规则

以后任何 Gameplay 修改，都先问：

> 谁决定结果？

正式规则：

| 行为 | Client | Server |
|---|---|---|
| 本地输入 | 采集 | 接收意图 |
| Character movement | prediction | authority / correction |
| Interaction target prompt | 本地 | 不需要 |
| 是否允许拾取 | 请求 | 决定 |
| Inventory add/remove | 显示 | 决定 |
| Equipment | 请求/显示 | 决定 |
| Damage | 表现 | 决定 |
| Ant AI | 表现 | 决定 |
| Resource spawn | 不生成 | 生成 |
| Ship storage | 显示 | 决定 |
| Ship physics | 发送 driver input | 模拟 |
| Earth timer | 显示 server time | 决定 |
| Launch success | 显示 | 决定 |
| World travel | 跟随 | ServerTravel |
| Save | 不做权威存档 | Host/Dedicated 保存 |
| Voice playback mix | 本地 | Voice transport/session coordination |

---

# 45. Replication 频率与 RPC 原则

不要为了“联网”把所有函数都做 Reliable RPC。

## Reliable

适合：

- Interact button。
- Ready。
- Start Expedition。
- Board / Disembark。
- Inventory transaction request。
- Shop purchase。

## Unreliable

适合：

- 高频 Spacecraft input state。
- 纯 cosmetic transient events（如果丢一两个没关系）。

## Replicated Property

适合持久状态：

- phase。
- inventory。
- equipment。
- health。
- boarded。
- storage。
- ant state。

不要用 reliable multicast 每秒广播持久状态。

---

# 46. Ownership / RPC 注意事项

Codex 必须遵循 Unreal RPC ownership。

Client 只能可靠地从：

- 自己 owned PlayerController。
- 自己 possessed Pawn。
- 自己 owned replicated component。

发 Server RPC。

世界里的 Resource Actor 不应要求普通 Client 直接调用它的 Server RPC，因为 Client 通常不拥有它。

因此 Interaction flow 应从 owned：

```text
Character / InteractionComponent
```

向 Server 发 request，Server 再调用世界 actor。

---

# 47. 网络安全基础

这是合作游戏，不需要做反作弊系统，但不要把明显作弊入口留在协议层。

Server 不接受 Client 直接提交：

```text
GiveMeResource(1000)
ApplyDamage(9999)
SetFuel(999)
TeleportTo(...)
```

Server request 只表达意图：

```text
TryInteract(Target)
TryPurchase(ItemId)
TryBoard(Ship)
SetShipInput(...)
```

数值和合法性 Server 自己算。

---

# 48. 网络相关的现有文件修改清单

Codex 应至少审计并按本规范修改下列文件。

## Core

```text
Core/JTSGameInstance.h/.cpp
Core/JTSGameState.h/.cpp
```

新增：

```text
Core/JTSPlayerState.h/.cpp
Core/JTSExpeditionTypes.h
Core/JTSExpeditionSubsystem.h/.cpp
Core/JTSExpeditionSaveGame.h/.cpp
Core/JTSExpeditionSaveSubsystem.h/.cpp
```

## Modes

```text
Modes/JTSEarthGameMode.h/.cpp
Modes/JTSSpaceWorldGameMode.h/.cpp
Modes/JTSMoonGameMode.h/.cpp（如仍参与实际 runtime）
```

新增：

```text
Modes/JTSGameplayGameModeBase.h/.cpp
Modes/JTSMainMenuGameMode.h/.cpp
```

## Player

```text
Player/JTSCharacter.h/.cpp
Player/JTSPlayerController.h/.cpp
```

## Interaction / Components

```text
Interaction/InteractionComponent.h/.cpp
Components/JTSCarryComponent.h/.cpp
Components/JTSHealthComponent.h/.cpp
Components/JTSMeleeComponent.h/.cpp
Components/JTSPlayerEquipmentComponent.h/.cpp
Components/JTSSpacecraftFlightMovementComponent.h/.cpp
Components/JTSPlanetGravityComponent.*（只在需要 authority fix 时改）
Components/JTSMoonWrappedActorComponent.*（分类 local visual 与 gameplay）
```

## Ships

```text
Ships/JTSSpacecraftActor.h/.cpp
```

## World

至少：

```text
World/JTSSpaceWorldManager.h/.cpp
World/JTSPlanetLandingManager.h/.cpp
World/JTSPlanetSurfaceGameplay.h
World/JTSMoonSurfaceController.h/.cpp
World/JTSMoonResourceSpawner.h/.cpp
World/JTSMoonResourceActor.h/.cpp
World/JTSMoonAntActor.h/.cpp
World/JTSMoonAntNestActor.h/.cpp
World/JTSMoonAntCorpsePickupActor.h/.cpp
World/JTSMoonCorpseActor.h/.cpp
World/JTSMoonWorldActor.h/.cpp
World/JTSMoonLoopGroundActor.h/.cpp
```

## Items

```text
Items/JTSResourcePickupActor.h/.cpp
Items/JTSWorldPickupActor.h/.cpp
```

## UI

保留 Gameplay HUD：

```text
UI/JTSPrototypeHUD.*
UI/JTSPrototypeHUDWidget.*
```

但拆出 Lobby / Session / FrontEnd。

## Config / Build

```text
Source/space/space.Build.cs
Source/space.Target.cs（如需要）
Source/spaceServer.Target.cs (new)
space.uproject
Config/DefaultEngine.ini
Config/DefaultGame.ini（只在实际需要时）
Config/DefaultInput.ini（若 voice / menu input 确有需要）
Config/DefaultEngineEOS.example.ini (new)
AGENTS.md
```

---

# 49. Phase 执行顺序

**注意：Phase 是代码依赖顺序，不是测试检查点。**

Codex 必须连续执行。

---

## Phase 1 — Build / Plugin / Core skeleton

完成：

1. 检查 UE 5.8 实际 plugin/module 名。
2. 修改 `.uproject`。
3. 修改 `space.Build.cs`。
4. 新增 `spaceServer.Target.cs`。
5. 新增 Online / Voice / Network / UI FrontEnd 目录。
6. 增加 core enums / structs。
7. 增加 `AJTSGameplayGameModeBase`。
8. 增加 `AJTSPlayerState`。
9. 增加 `UJTSExpeditionSubsystem`。
10. 增加 Save subsystem。

不要编译，继续 Phase 2。

---

## Phase 2 — GameState / GameInstance authority split

完成：

1. `AJTSGameState` replicated state。
2. server time deadline。
3. RepNotify delegates。
4. 移除 GameInstance 的 authoritative ship snapshot responsibility。
5. 把跨-world authority 迁移到 ExpeditionSubsystem。
6. 保留本地 preference。

不要测试，继续。

---

## Phase 3 — Online Session subsystem

完成：

1. Provider abstraction。
2. EOS / NULL selection。
3. identity login。
4. create session。
5. join code generation / search。
6. join session。
7. resolve connection。
8. leave/destroy/update。
9. error state。
10. network/travel failure integration。
11. password/build validation data path。
12. EOS example config。

不要测试，继续。

---

## Phase 4 — Front End / Host / Join / Lobby UI

完成纯 C++ UI：

1. FrontEnd HUD。
2. Main menu。
3. Host form。
4. Join code form。
5. Settings basic page。
6. session status/error modal。
7. Lobby widget。
8. Session Details widget。
9. host/player buttons。
10. Join Code Copy。
11. boot map / MainMenuGameMode config。

不要打开 UE，继续。

---

## Phase 5 — Earth multiplayer flow

完成：

1. Earth starts in Lobby。
2. all-player ready state。
3. host start RPC。
4. resource generation server-only。
5. server timer。
6. all active players。
7. shared ship。
8. boarding requirement。
9. success/failure authority。
10. HUD reads GameState。

不要测试，继续。

---

## Phase 6 — Character / interaction / inventory / equipment / combat

完成：

1. Cylinder fallback。
2. Character replicated boarded state。
3. interaction RPC + validation。
4. carry replication。
5. equipment replication。
6. pickup authority。
7. health authority audit。
8. melee authority audit。
9. dynamic item actor replication。

不要测试，继续。

---

## Phase 7 — Shared spacecraft

完成：

1. ship replicate / movement replicate。
2. occupants array。
3. driver state。
4. up to 4 boarded players。
5. shared storage replication。
6. authority board/disembark。
7. server flight simulation。
8. driver unreliable input RPC。
9. passenger handling。
10. landing manager reuse one ship。

不要测试，继续。

---

## Phase 8 — Travel / snapshot

完成：

1. capture authoritative snapshot。
2. host save。
3. seamless server travel。
4. PlayerState carry-over。
5. Character persistent state capture/restore。
6. SpaceWorld restore shared ship state。

不要测试，继续。

---

## Phase 9 — SpaceWorld / Moon multiplayer

完成：

1. all players restored。
2. deterministic arrival offsets。
3. surface gameplay initializes once。
4. multi-player surface context。
5. resource spawning authority。
6. resource actor replication。
7. Moon ant/nest authority。
8. nearest / valid player targeting。
9. Player0 gameplay refs eliminated。
10. local visual Player0 refs explicitly isolated。
11. one shared ship remains。

不要测试，继续。

---

## Phase 10 — Voice

完成：

1. VoiceSubsystem。
2. EOS lobby voice hook。
3. devices。
4. mute。
5. talking indicator。
6. identity mapping。
7. proximity volume。
8. occlusion volume。
9. optional positional capability。
10. radio routing scaffold。
11. advanced acoustic hook。

不要测试，继续。

---

## Phase 11 — Leave / Host loss / Continue / cleanup

完成：

1. Client Leave。
2. Host Leave。
3. Kick。
4. network failure return。
5. travel failure return。
6. Continue Save path。
7. no host migration state/error。
8. no midgame join policy。
9. menu pause cleanup。

不要测试，继续。

---

## Phase 12 — Full static audit

在整个源码中搜索并逐项检查：

```text
GetPlayerPawn(.*, 0)
GetFirstPlayerController
GetPlayerController(.*, 0)
OpenLevel
SetPause
HasAuthority
SpawnActor
Destroy()
ApplyDamage
Interact
AddResource
RemoveResource
Possess
UnPossess
GetAuthGameMode
GameInstance spacecraft state
```

不是要求全部字符串为 0。

要求：

- Gameplay authority 不能依赖 Player0。
- Local presentation 可以明确依赖 local player。
- Multiplayer established session 内 World progression 不得用 local `OpenLevel`。
- FrontEnd initial host travel / client connect 可使用适当非 seamless travel。
- `SetPause` 不应暂停网络 gameplay。
- Spawn/Destroy gameplay actors authority only。

继续，不测试。

---

## Phase 13 — Update project architecture docs

修改 `AGENTS.md`，增加一个简短但明确的 Multiplayer Architecture 部分。

必须记录：

```text
Server Authority
GameState replicated global state
PlayerState player network state
No Player0 assumptions in gameplay
Session UI only talks to JTSOnlineSessionSubsystem
One shared spacecraft
ServerTravel between multiplayer gameplay worlds
Surface gameplay initializes once per planet
Dynamic gameplay spawns occur on server
Voice goes through JTSVoiceSubsystem
No host migration in current player-hosted version
```

不要把整个本文件复制进 AGENTS.md。

AGENTS 只写长期不变的规则。

---

# 50. 最终唯一一次命令行编译

**只有所有 Phase 都完成以后才做。**

规则：

1. 确认 Unreal Editor 没有运行。
2. 使用项目现有 UE 5.8 Build.bat / UnrealBuildTool。
3. 编译 `spaceEditor Win64 Development`。
4. 如果能支持，再编译普通 Game target；不要因为 Launcher Engine 不支持 Server Target 而反复折腾。
5. 修复实际 compile errors。
6. 不启动 Editor。
7. 不运行 PIE。
8. 不启动 packaged game。

如果最终 C++ compile 成功：

- 到此停止。
- 玩家之后自己体验。

如果 compile 无法执行是因为：

- 本地 Engine 路径不可用。
- 文件权限。
- Launcher binary 不支持 Server target。

则：

- 不删除已经完成的代码。
- 最终报告明确外部环境原因。

---

# 51. 最终静态验收清单

Codex 在结束前逐项自行检查代码，不需要玩家参与。

## Architecture

- [ ] 1–4 player architecture。
- [ ] 单人不是另一套 Gameplay。
- [ ] Listen / future Dedicated 共用一套 Gameplay。
- [ ] UI 不直接依赖 EOS。
- [ ] Online backend 封装在 `UJTSOnlineSessionSubsystem`。

## Session

- [ ] Host Create。
- [ ] Join Code generation。
- [ ] Join Code exact search。
- [ ] Password protected flag。
- [ ] Build version validation。
- [ ] Join Session / ClientTravel。
- [ ] Leave / Destroy。
- [ ] session state delegates。
- [ ] EOS unavailable error。
- [ ] Local single-player fallback。

## Lobby

- [ ] 4 fixed slots。
- [ ] Ready replicated。
- [ ] color replicated。
- [ ] Host start validation。
- [ ] Join Code copy。
- [ ] no gameplay collection before Start。

## GameState / PlayerState

- [ ] GameState global phase replicated。
- [ ] Earth deadline uses server world time。
- [ ] PlayerState exists and is configured in GameMode。
- [ ] Ready / host / boarded etc replicated。

## Earth

- [ ] no authoritative Player0 assumption。
- [ ] one shared spacecraft。
- [ ] resources spawn once on server。
- [ ] inventory server authority。
- [ ] launch result server authority。
- [ ] all required players boarding policy。

## Character / Items

- [ ] ACharacter movement uses UE built-in networking。
- [ ] temporary cylinder fallback。
- [ ] Interact server RPC validation。
- [ ] Carry replicated。
- [ ] Equipment replicated。
- [ ] pickups replicated / server destroyed。
- [ ] damage server authority。

## Spacecraft

- [ ] replicated actor。
- [ ] replicated movement。
- [ ] up to 4 occupants。
- [ ] exactly one driver max。
- [ ] shared storage replicated。
- [ ] server flight simulation。
- [ ] driver input intent only。
- [ ] passengers do not attempt multi-possession。

## Travel

- [ ] Earth → SpaceWorld uses `ServerTravel`。
- [ ] `bUseSeamlessTravel=true`。
- [ ] snapshot capture before travel。
- [ ] restore after travel。
- [ ] clients do not OpenLevel themselves。

## Moon

- [ ] surface initializes once。
- [ ] players registered as collection。
- [ ] resource spawn server-only。
- [ ] ant/nest AI server-only。
- [ ] gameplay queries consider all active players。
- [ ] local visual code isolated from authority。
- [ ] shared arrival spacecraft reused。

## Voice

- [ ] Voice subsystem exists。
- [ ] EOS lobby voice hookup。
- [ ] input/output devices。
- [ ] mute/unmute。
- [ ] talking state。
- [ ] stable voice identity mapping。
- [ ] distance gain。
- [ ] obstruction gain。
- [ ] positional capability hook。
- [ ] radio route scaffold。
- [ ] no custom codec/transport。

## UI / menu

- [ ] Front End exists without new binary map asset。
- [ ] Start / Join / Continue / Settings / Quit。
- [ ] Esc local only, no world pause。
- [ ] Session Details。
- [ ] failure messages return player safely to menu。

## Save

- [ ] Host owns Listen Expedition save。
- [ ] Client not authoritative save owner。
- [ ] Continue creates new session / new join code。
- [ ] shared ship snapshot no longer stored as authoritative client GameInstance data。

## Code hygiene

- [ ] no irrelevant massive refactor。
- [ ] no duplicate multiplayer system。
- [ ] no invented engine API without checking UE 5.8 headers。
- [ ] no hardcoded EOS credentials。
- [ ] no raw IP in normal UI。
- [ ] no `.uasset` / `.umap` modification。

---

# 52. Codex 最终要生成的实施报告

所有改造完成后，在项目根目录新增：

```text
MULTIPLAYER_IMPLEMENTATION_REPORT.md
```

内容只需要清楚，不要写成几十页。

至少包含：

## Implemented

- 新增的系统。
- 现有系统网络化结果。
- Session flow。
- Voice baseline。
- Travel。
- Save。

## New files

列出新增文件。

## Major modified files

列出核心修改文件。

## Final compile result

记录最终唯一一次命令行编译结果。

## External prerequisites before internet multiplayer can actually work

例如：

```text
EOS Developer Portal ProductId
SandboxId
DeploymentId
ClientId
ClientSecret / required artifact credentials
appropriate EOS permissions/settings
```

只列真实需要的项目。

## Intentional current limitations

明确：

```text
No Host Migration
No normal Mid-game Join
Dedicated allocation backend not implemented yet
Radio gameplay not enabled yet
Advanced per-speaker low-pass/reverb DSP remains future extension if only lobby voice path is available
```

这些不是“忘了做”，而是本设计明确边界。

## Editor steps

由于本任务明确不打开 Editor，理想结果应该很少。

如果某些美术/Blueprint asset 最终必须之后由人手设置：

- 只在这里列出。
- 不阻塞 C++ 完整改造。

---

# 53. 当前明确不做的事情

避免 scope creep。

本次不要：

- Host Migration。
- Matchmaking queue。
- Public server browser 大厅。
- MMO backend。
- 玩家个人飞船系统。
- Steam integration（架构兼容即可）。
- Cross-platform certification。
- Anti-cheat service。
- 自研 voice codec。
- 自研 NAT traversal。
- Full spacecraft client prediction。
- 新建大量美术 UMG assets。
- 重做 Moon 玩法内容。
- 重做角色美术。
- 给未来 Mars 写空框架。

---

# 54. 未来 Dedicated Server 的迁移边界

本次架构完成后，未来 Dedicated Server 应只需要主要增加：

```text
Dedicated server deployment
Server allocator / matchmaking backend
Persistent save backend
Possibly trusted voice channel credential service
```

不应该需要重写：

```text
Earth gameplay
Moon gameplay
Inventory
Interaction
Spacecraft authority
GameState
PlayerState
Lobby widgets
Start/Join UI structure
Voice UI
```

`UJTSOnlineSessionSubsystem` 的 Hosting backend 从：

```text
PlayerHosted Listen
```

变成：

```text
Request Dedicated Allocation
→ receive endpoint/session
→ join
```

UI 仍然只有：

```text
Start Expedition
Join Expedition
```

这就是本次架构的核心价值。

---

# 55. 参考的 UE 5.8 官方能力

Codex 如需要核对 API，优先查看本机 UE 5.8 headers，其次参考官方文档。

## Online Subsystem EOS

https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine

关键点：

- OSS EOS。
- EOS NetDriver。
- P2P sockets。
- Identity / Session。

## Online Subsystem Identity

https://dev.epicgames.com/documentation/unreal-engine/online-subsystem-identity-interface-in-unreal-engine

EOS Account Portal 登录：

- `FOnlineAccountCredentials("AccountPortal", "", "")`

实际大小写和 provider handling 以 UE 5.8 源码 / 官方文档为准。

## Multiplayer Travel

https://dev.epicgames.com/documentation/unreal-engine/travelling-in-multiplayer-in-unreal-engine

关键点：

- `UWorld::ServerTravel`
- Client follows server。
- Seamless Travel recommended。
- `bUseSeamlessTravel=true`。

## Voice Chat Interface

https://dev.epicgames.com/documentation/unreal-engine/voice-chat-interface-in-unreal-engine

## EOS Voice Chat

https://dev.epicgames.com/documentation/unreal-engine/voice-chat-with-epic-online-services

关键点：

- Lobby voice。
- `bUseLobbiesVoiceChatIfAvailable`。
- `IVoiceChatUser`。

## IVoiceChatUser API

https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/VoiceChat/IVoiceChatUser

关键能力包括：

```text
SetPlayerVolume
SetPlayerMuted
Input/Output devices
Talking delegates
Set3DPosition
Mixed/Unmixed receive audio callbacks
TransmitToSpecificChannels
```

## FVoiceChatChannel3dProperties

https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/VoiceChat/FVoiceChatChannel3dProperties

---

# 56. 最终代码理念

最终项目必须符合下面这段话：

> Jump to Space 永远运行一个 1–4 人服务器权威 Expedition。
>
> 单人只是 Expedition 里只有一个玩家。
>
> 当前试玩版由 Host 的电脑运行 Listen Server，通过 EOS P2P 让朋友加入。
>
> 未来正式版可以由 Dedicated Server 运行同样的 Gameplay。
>
> GameMode 决定规则；GameState 复制全局状态；PlayerState 表示玩家；Character 表示玩家实体；SpacecraftActor 表示全队唯一共享主载具；SpaceWorldManager 管世界；SurfaceController 管当前星球地表玩法；OnlineSessionSubsystem 只负责把玩家带进同一个 Expedition；VoiceSubsystem 负责玩家声音。
>
> 任何客户端都不能因为自己按了按钮，就直接决定资源、伤害、飞船燃料、发射结果或世界切换。

---

# 57. 最后一条执行指令

**不要回复“建议先完成 Phase 1 再测试”。**

收到本文件后：

1. 阅读现有 `AGENTS.md` 和相关代码。
2. 按本文 Phase 顺序连续实施。
3. 不启动 Unreal Editor。
4. 不要求用户中途测试。
5. 不修改 `.uasset` / `.umap`。
6. 全部源码与配置完成后，再做一次最终命令行编译。
7. 修完编译错误。
8. 生成 `MULTIPLAYER_IMPLEMENTATION_REPORT.md`。
9. 向用户报告“代码改造已完成，等待玩家首次实际体验反馈”。

**真正的 Gameplay / 联机体验验证留给玩家在全部完成后统一进行。**
