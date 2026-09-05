# Jump To Space 项目清理扫描报告

## 扫描范围与方法

- 扫描目录：`Source/space`、`Content/Space`。
- 补充检查：`Content/M_TestGround.uasset`，因为它位于项目 Content 根目录且名称明确带有测试语义。
- 引用数量定义：定义文件自身不计，统计源码、配置和可读 Unreal 包名中出现该类/资产的外部文件数量；地图中的序列化对象按地图文件计为引用。
- 本报告生成于修改前；此阶段未删除、移动或重命名任何源码和资产。

## Class

| Class / Interface | 文件路径 | 父类 | 被引用数量 | 当前用途 | 建议 |
|---|---|---|---:|---|---|
| `UJTSCarryComponent` | `Source/space/Components/JTSCarryComponent.h/.cpp` | `UActorComponent` | 5 | 玩家携带资源、与资源和飞船交互 | KEEP |
| `UJTSMoonWrappedActorComponent` | `Source/space/Components/JTSMoonWrappedActorComponent.h/.cpp` | `UActorComponent` | 4 | Fake Moon 周期坐标与渲染设置 | KEEP |
| `UJTSPlanetGravityComponent` | `Source/space/Components/JTSPlanetGravityComponent.h/.cpp` | `UActorComponent` | 2 | 旧球形 Moon 径向重力兼容组件；Fake Moon 中旁路 | KEEP（兼容） |
| `UJTSGameInstance` | `Source/space/Core/JTSGameInstance.h/.cpp` | `UGameInstance` | 3 | 跨地图玩家设置与远征资源状态 | KEEP |
| `AJTSGameMode` | `Source/space/Core/JTSGameMode.h/.cpp` | `AGameModeBase` | 4 | Earth 资源搜集、发射准备和结果流程 | KEEP |
| `AJTSGameState` | `Source/space/Core/JTSGameState.h/.cpp` | `AGameStateBase` | 11 | Earth/Moon 章节阶段状态 | KEEP |
| `AJTSMoonGameMode` | `Source/space/Core/JTSMoonGameMode.h/.cpp` | `AGameModeBase` | 9 | Moon 探索规则和补给消耗 | KEEP |
| `UInteractable` | `Source/space/Interaction/IInteractable.h` | `UInterface` | 1 | Blueprint 交互接口包装 | KEEP |
| `IInteractable` | `Source/space/Interaction/IInteractable.h` | 接口契约 | 5 | 正式 Interaction 接口实现 | KEEP |
| `UInteractionComponent` | `Source/space/Interaction/InteractionComponent.h/.cpp` | `UActorComponent` | 2 | 玩家附近交互目标检测与执行 | KEEP |
| `AJTS_TestInteractableActor` | `Source/space/Interaction/JTS_TestInteractableActor.h/.cpp` | `AActor`, `IInteractable` | 0 | 仅用于验证 IInteractable 的立方体测试演员 | DELETE（确认无引用后） |
| `AJTSResourcePickupActor` | `Source/space/Items/JTSResourcePickupActor.h/.cpp` | `AActor`, `IInteractable` | 3 | Earth 资源拾取 | KEEP |
| `AJTSMoonPlanetActor` | `Source/space/Planets/JTSMoonPlanetActor.h/.cpp` | `AActor` | 3 | 旧球形 Moon 视觉/径向重力兼容 | KEEP（暂不删除） |
| `AJTSCharacter` | `Source/space/Player/JTSCharacter.h/.cpp` | `ACharacter` | 6 | 玩家移动、交互、携带和登船 | KEEP |
| `AJTSPlayerController` | `Source/space/Player/JTSPlayerController.h/.cpp` | `APlayerController` | 3 | 玩家控制器与输入准备 | KEEP |
| `AJTSSpacecraftActor` | `Source/space/Ships/JTSSpacecraftActor.h/.cpp` | `AActor`, `IInteractable` | 9 | 飞船资源、登船和 Fake Moon 周期表示 | KEEP |
| `UJTSMoonWrapSubsystem` | `Source/space/Systems/JTSMoonWrapSubsystem.h/.cpp` | `UWorldSubsystem` | 3 | Moon 2D 周期坐标数学与配置 | KEEP |
| `UJTSCircularProgressWidget` | `Source/space/UI/JTSCircularProgressWidget.h/.cpp` | `UWidget` | 2 | 发射/登船进度显示 | KEEP |
| `AJTSPrototypeHUD` | `Source/space/UI/JTSPrototypeHUD.h/.cpp` | `AHUD` | 2 | 原型 HUD 创建 | KEEP |
| `UJTSPrototypeHUDWidget` | `Source/space/UI/JTSPrototypeHUDWidget.h/.cpp` | `UUserWidget` | 2 | Earth/Moon 原型 HUD 呈现 | KEEP |
| `AJTSMoonFakeWorldActor` | `Source/space/World/JTSMoonFakeWorldActor.h/.cpp` | `AActor` | 8 | Moon 循环地图配置、MPC 驱动、World Bend | RENAME → `AJTSMoonWorldActor` |
| `AJTSMoonLoopGroundActor` | `Source/space/World/JTSMoonLoopGroundActor.h/.cpp` | `AActor` | 2 | 3×3 可回收 Fake Moon 地面环 | KEEP |
| `AJTSResourceSpawnArea` | `Source/space/World/JTSResourceSpawnArea.h/.cpp` | `AActor` | 2 | Earth 资源生成区域 | KEEP |
| `AJTSTestWrappedSphereActor` | `Source/space/World/JTSTestWrappedSphereActor.h/.cpp` | `AActor` | 1 | 仅验证 Moon wrapping 与材质赋值 | DELETE（先清理地图实例） |

### 非 UCLASS 类型

- `EJTSAvatarColor`、`EJTSGameplayPhase`、`EJTSFailureReason`、`EJTSResourceType` 是正式状态/资源枚举，KEEP。
- `SJTSCircularProgress` 和 `FSpaceModule` 是实现辅助类型，不是独立测试 gameplay 类。

## Map

| 当前名称 | 当前用途/内容 | 建议名称 | 建议 |
|---|---|---|---|
| `Content/Space/Maps/Test/L_TestGameplay.umap` | Earth 原型：`PlayerStart`、`JTSSpacecraftActor`、`JTSResourceSpawnArea`、3 个 `JTSResourcePickupActor`，并使用 `BP_TestGameMode` | `L_EarthLaunchPrototype` | RENAME；保留全部内容 |
| `Content/Space/Maps/Test/L_MoonFakeTest.umap` | Fake Moon 原型：`JTSMoonFakeWorldActor`、`JTSMoonLoopGroundActor`、飞船、PlayerStart、FakeMoon 材质/MPC；另含 1 个 `JTSTestWrappedSphereActor` 测试实例 | `L_MoonPrototype` | RENAME；删除测试球体实例后保留章节内容 |
| `Content/Space/Maps/Test/L_MoonTest.umap` | 旧球形 Moon：`JTSMoonPlanetActor`、`JTSSpacecraftActor`、`PlayerStart`，地图使用 `JTSMoonGameMode`，因此具备玩家/飞船/Moon gameplay | 暂不改名 | KEEP；待与正式 Moon 原型合并前不得删除 |

## Content 资产

- `Content/Space/Materials/FakeMoon/M_JTSFakeMoon_Master.uasset`：KEEP，正式 Fake Moon WPO 材质基础。
- `Content/Space/Materials/FakeMoon/MI_JTSFakeMoon_Ground.uasset`：KEEP，正式 Moon 地面材质。
- `Content/Space/Materials/FakeMoon/MI_JTSFakeMoon_Prop.uasset`：KEEP，正式 Moon 物件材质。
- `Content/Space/Materials/FakeMoon/MI_JTSFakeMoon_Ship.uasset`：KEEP，正式 Moon 飞船材质。
- `Content/Space/Materials/FakeMoon/MPC_JTSFakeMoon.uasset`：KEEP，正式 MPC。
- `Content/Space/Test/BP_TestGameMode.uasset`：KEEP；虽位于 `Test` 目录，但被 Earth 地图直接引用并承载 Earth 原型配置，不是无引用验证垃圾。
- `Content/M_TestGround.uasset`：扫描到 0 个外部引用，属于旧测试材质；建议 DELETE。

## 关键结论

1. `AJTS_TestInteractableActor` 没有源码、配置或地图引用，可以删除两个源文件。
2. `AJTSTestWrappedSphereActor` 目前仍被 `L_MoonFakeTest.umap` 的一个关卡演员引用；必须先清理该地图实例，再删除两个源文件。
3. `AJTSMoonFakeWorldActor` 已承担正式 Moon World 初始化、MPC、World Bend 和 Loop Ground 配置，建议只改名为 `AJTSMoonWorldActor`，不改变逻辑。
4. `AJTSMoonPlanetActor` 同时被 `UJTSPlanetGravityComponent` 和 `L_MoonTest.umap` 引用；它是旧方案兼容层，当前不满足“无引用可删除”条件，暂时 KEEP。
5. `L_MoonTest.umap` 不是单纯 WPO/材质验证地图，不能在未合并玩家、飞船和 Moon 内容前删除。

## 最终执行报告

### 1. 删除文件列表

- `Source/space/World/JTSTestWrappedSphereActor.h`
- `Source/space/World/JTSTestWrappedSphereActor.cpp`
- `Source/space/Interaction/JTS_TestInteractableActor.h`
- `Source/space/Interaction/JTS_TestInteractableActor.cpp`
- `Content/M_TestGround.uasset`（确认无外部引用的旧测试材质）
- 同时从 `L_MoonPrototype.umap` 删除 1 个 `JTSTestWrappedSphereActor` 测试实例；未删除任何章节地图。

### 2. 重命名文件列表

| 原文件 | 新文件 | 说明 |
|---|---|---|
| `Source/space/World/JTSMoonFakeWorldActor.h` | `Source/space/World/JTSMoonWorldActor.h` | 仅重命名正式 Moon World 配置类 |
| `Source/space/World/JTSMoonFakeWorldActor.cpp` | `Source/space/World/JTSMoonWorldActor.cpp` | 逻辑保持不变 |

已同步更新 `JTSMoonWrappedActorComponent`、`JTSMoonWrapSubsystem`、`JTSMoonLoopGroundActor` 及兼容提示中的类名引用。`L_MoonPrototype` 中的实例对象名和 Actor Label 也从 `JTSMoonFakeWorldActor` 整理为 `JTSMoonWorldActor`。

### 3. Map 重命名列表

| 原地图 | 新地图 | 处理 |
|---|---|---|
| `Content/Space/Maps/Test/L_TestGameplay.umap` | `Content/Space/Maps/Test/L_EarthLaunchPrototype.umap` | 保留全部 Earth 原型内容 |
| `Content/Space/Maps/Test/L_MoonFakeTest.umap` | `Content/Space/Maps/Test/L_MoonPrototype.umap` | 删除测试球体实例后保留全部 Moon 原型内容 |
| `Content/Space/Maps/Test/L_MoonTest.umap` | — | 保留，等待后续合并 |

`Config/DefaultEngine.ini` 的 `EditorStartupMap` 与 `GameDefaultMap` 已指向 `L_EarthLaunchPrototype`。

### 4. 保留的 Earth 章节内容

`L_EarthLaunchPrototype` 保留 `PlayerStart`、玩家默认流程、`JTSSpacecraftActor`、`JTSResourceSpawnArea`、3 个 `JTSResourcePickupActor` 以及 Earth 发射准备所需的 `BP_TestGameMode` 配置；没有删除 `L_TestGameplay` 中的章节内容，只完成地图包改名。

### 5. 保留的 Moon 章节内容

`L_MoonPrototype` 保留 `JTSMoonWorldActor`、`JTSMoonLoopGroundActor`、玩家出生点、飞船、Fake Moon 地面和周期世界配置；Fake Moon 材质目录与 MPC 全部保留。`L_MoonTest` 仍保留旧球形 Moon 的玩家、飞船、`JTSMoonPlanetActor` 和 `JTSMoonGameMode` gameplay，未在合并前删除。

### 6. `JTSMoonPlanetActor` 处理结果

保留 `Source/space/Planets/JTSMoonPlanetActor.h/.cpp`。它仍被 `UJTSPlanetGravityComponent` 和 `L_MoonTest.umap` 引用，属于旧球形 Moon 兼容层，不满足“无引用可删除”条件；仅更新弃用提示指向 `JTSMoonWorldActor`，没有改变 gameplay 逻辑。

### 7. 是否存在残留引用

- 在 `Source/space`、`Content/Space`、`Config/DefaultEngine.ini` 和已保存地图资产中，未发现 `JTSMoonFakeWorldActor`、`JTSTestWrappedSphereActor`、`JTS_TestInteractableActor`、`L_TestGameplay` 或 `L_MoonFakeTest` 的正式引用。
- 无界面资产核查确认三张地图均可加载；Moon 原型中的 World actor 类和实例名均为 `JTSMoonWorldActor`，测试球体不存在。
- `ProjectCleanupReport.md` 的扫描快照和改名记录会保留旧名称作为历史说明；`Saved/` 下的编辑器日志/用户历史属于生成缓存，不计入运行时资产引用，本次临时脚本与验证缓存已清理。

### 8. Build command

```text
D:\Software\UE_5.8\Engine\Build\BatchFiles\Build.bat spaceEditor Win64 Development D:\projects\space\space.uproject -WaitMutex -NoHotReloadFromIDE
```

### 9. Build result

- 结果：`Succeeded`。
- 类重命名后的完整编译执行 9 个 action 并成功链接 `UnrealEditor-space.dll`。
- 最终改名和引用清理后的增量编译再次成功（`Target is up to date`，0 个 action）。
- 构建日志中的 UBA 端口绑定提示为非致命警告；没有编译错误。
- 资产重存/地图改名通过无界面 `UnrealEditor-Cmd` Python commandlet 完成；仅做资产加载、保存和静态核查。
- 未启动 Unreal Editor 图形界面。
- 未运行 PIE。
- 未进行 Gameplay 测试或 Runtime 测试。
