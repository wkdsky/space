# SpaceWorld 调试旅行与物品栏

`L_SpaceWorld` 首次默认将人物和同一艘共享飞船放在火卫二（`MarII`）。
月球、火卫二继续使用各自已有的 ArrivalAnchor，飞船按真实 Mesh 地表和停靠支撑检测停放，人物使用船外净空检测落点。

## 操作

- 按住左/右 Alt：临时显示鼠标，停止移动与视角输入；松开恢复游戏。拖动中松开 Alt 会取消拖动，不丢弃物品。原有 I 键整理模式仍保留。
- 下方物品栏：左键拖到另一格移动/交换；右键装备。腰灯装备后点亮。
- 将物品拖出物品栏松开：将整格物品丢到人物附近的真实地表。掉落生成失败时，物品仍留在背包。拖到 Debug 菜单上取消丢弃；原有星核栏/飞船商店拖拽继续使用对应交互。
- 右下角 Debug 菜单：点“月球 / Moon”或“火卫二 / Deimos”传送队伍和飞船。已经登船的玩家保留座位，船外人物移动到船旁地表。人物、物品与飞船仓储不会重建。
- 每行 Default：只设置下次直接进入 SpaceWorld 的默认星球，不立即传送；星号标记当前选择。

Default 使用主机本地 `Saved/SaveGames/JumpToSpaceDebugPreferences.sav`，与四个远征存档分开。正常从其他地图旅行或恢复远征时，优先保留远征的星球状态。

联机旅行与 Default 仅主机可以操作；客户端通过自己的 PlayerController RPC 提交请求，服务器验证主机身份、配置的目的地和真实地表。旅行请求间隔至少一秒。Shipping 构建关闭调试旅行菜单和请求。

## 职责与修改文件

- `World/JTSSpaceDebugTravelManager.h/.cpp`：关卡目的地配置、主机校验、默认选择及状态复制，无 Tick。
- `UI/JTSSpaceDebugWidget.h/.cpp`：右下角小面板，低频刷新；项目星球名称和 ID 不写入 C++。
- `Modes/JTSSpaceWorldGameMode.h/.cpp`：默认出生选择及地表流程初始化；旅行不会重复恢复背包或发放初始物品。
- `World/JTSPlanetLandingManager.h/.cpp`：移动现有共享飞船及人物，复用真实地表、停靠支撑和船外人物落点检测。
- `World/JTSSpaceWorldManager.h/.cpp`：瞬时到达前恢复远处天体的真实几何尺寸，避免用缩小后的表现模型作落点检测。
- `Player/JTSPlayerController.h/.cpp`、`UI/JTSPrototypeHUDWidget.h/.cpp`：Alt 鼠标与物品栏鼠标事件。
- `Components/JTSInventoryComponent.h/.cpp`：服务器装备、交换和丢弃；校验物品实例 ID，拒绝过期拖拽。
- `Items/JTSWorldPickupActor.cpp`：掉落绑定人物当前星球，按球面重力放置；船边丢弃检查实际碰撞体。
- `Systems/JTSExpeditionSubsystem.h/.cpp`、`Systems/JTSExpeditionSaveGame.h`：主机本地 Debug 偏好持久化。
- `Tests/JTSSpaceDebugTests.cpp`：过期物品请求、交换/装备、失败丢弃不丢物、有效世界掉落及主机权限测试。
- `Content/Space/Maps/L_SpaceWorld.umap`：InitialPlanet 改为 MarII，新增 `Debug/Space_DebugTravel` 配置 Actor。
- `Tools/Editor/configure_space_debug.py`：通过 Unreal MCP 配置关卡、检查 PIE、调用正式拥有者 RPC。

## Editor 步骤

关卡配置已经保存，无需额外手工设置，直接打开 `L_SpaceWorld` 并 Play。

调整目的地时选择 `Debug/Space_DebugTravel`，编辑 Destinations 的 PlanetId 与 DisplayName；相应 PlanetAnchor 必须存在真实地表、ArrivalAnchor 以及 GameMode 的 SurfaceGameplayControllers 定义。调整出生点应修改相应 ArrivalAnchor 的人物/飞船组件，不在 C++ 写坐标。

备份和验证输出位于 `Saved/SpaceDebug/`。执行原生自动化测试：`JTS.Debug.InventoryMouseTransactions`、`JTS.Debug.TravelAuthority`。

## 验证结果（2026-10-17）

- `spaceEditor Win64 Development` 编译通过，`git diff --check` 通过。
- `JTS.Debug.InventoryMouseTransactions`、`JTS.Debug.TravelAuthority`、`JTS.Spacecraft.PossessionAndDisembark` 三项自动化测试通过，无测试警告或错误。
- Unreal MCP 驱动实际 PIE 界面，验证右键装备、物品栏交换、拖出生成地表掉落、点击旅行及 Default。界面截图：`Saved/SpaceDebug/DebugMenu.png`。
- 四名本地玩家月球 ↔ 火卫二往返：人物使用独立净空落点，始终保留原来的一艘共享飞船和物品；登船旅行保留驾驶座，抵达后可以离船。批量落点提前预留胶囊空间，避免多个玩家选择同一备用位置。
- 非主机旅行请求被拒绝；默认星球分别设为月球和火卫二后，重新打开编辑器/PIE 能按保存值出生。交付时默认值为火卫二。
- 以上多人实测使用四个本地 PlayerController；未进行外部机器联网测试。
