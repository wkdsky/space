# 项目架构与联机

本文记录当前采用的系统边界。玩法说明见 [Gameplay.md](Gameplay.md)，月球内容与参数见 [Moon.md](Moon.md)，编译及资产操作见 [Development.md](Development.md)。

## 世界与流程

当前主流程为 `L_Entry → L_PreLaunchLobby → L_EarthLaunchPrototype → L_SpaceWorld`。SpaceWorld 是持续存在的宇宙玩法空间，由当前 Planet 的地表控制器启动对应玩法。星球切换使用世界管理与 Streaming，不把每个星球设计成依次旅行的独立章节地图。

这是 1–4 人共享的 Expedition。单人也使用同一套服务器规则；Listen Server 与后续 Dedicated Server 共用权威逻辑。服务器拥有阶段、出生、飞船创建及跨地图旅行，Earth 到 SpaceWorld 使用 seamless ServerTravel。开始远征后关闭普通加入；不支持房主迁移。

## 职责分配

| 层级 / 入口 | 当前职责 |
| --- | --- |
| `AJTSGameplayGameModeBase` 及派生 GameMode | 准入、准备、阶段、玩家出生、飞船创建、旅行；具体类和参数由 Blueprint 配置 |
| `AJTSGameState` | 复制公共阶段、截止时间、当前飞船和星球状态 |
| `AJTSPlayerState` | 复制准备、房主、颜色、登船及个人远征状态；持有个人仓库、星际装备与成长数据 |
| `UJTSExpeditionSubsystem` | 服务器跨关卡快照、房主存档、恢复玩家与飞船物资 |
| `UJTSOnlineSessionSubsystem` | UI 使用的唯一 OnlineSubsystem 包装；创建、查找、加入和结束会话 |
| `UJTSVoiceSubsystem` | Provider 语音接入、音量及空间策略；音频传输和编解码由 Provider 提供 |
| `AJTSSpaceWorldManager` | 星球注册、当前星球、Streaming、旅行与宇宙状态 |
| `AJTSPlanetAnchor` | 真实星球中心、表面碰撞查询、径向坐标与着陆所需几何 |
| `AJTSPlanetLandingManager` / `AJTSPlanetArrivalAnchor` | 首次到达、配置的飞船出生点、安全下船及重生查询 |
| `AJTSSpacecraftActor` 及飞行 / 包络 / 支撑 / 表现组件 | 驾驶权限与状态、运动、近地安全、足垫支撑、支架和灯光表现 |
| `AJTSMoonSurfaceController` 及其他 SurfaceController | 具体星球的资源、聚居地、地标及地表玩法启停 |
| `UJTSPlanetEnemySubsystem` | 服务器 Mass 实体注册、统一模拟、感知 / 路线预算与身体分离 |
| Character、库存 / 战斗组件、物品 Actor | 玩家移动、输入、交互、装备、战斗和物品自身行为 |

`UJTSGameInstance` 保留应用入口与本地设置职责；远征快照和房主存档归 `UJTSExpeditionSubsystem`。当前快照保存阶段、地图 / 星球标识、飞船类型与物资、玩家背包、仓库、星际装备 / 能量及成长数据，不能据此假定所有关卡 Actor 的动态状态都会持久化。

## 权威与复制

客户端通过所属 PlayerController、Pawn 或组件的 owner RPC 提交意图。服务器重新验证当前阶段、身份、距离、视线、物品实例、材料、冷却及目标状态后，才执行交互、库存 / 装备修改、伤害、资源生成、AI、登船和驾驶。

一艘共享飞船最多四名乘员、一个驾驶员。只有当前驾驶员能发送不可靠飞行意图，移动结果由服务器复制。个人背包与个人商店仓库分别保存；飞船资源属于共享 Expedition。

怪物采用 MassEntity 与 Actor 混合结构：Entity / Fragment 保存模拟状态，World Subsystem 集中调度，Actor 保留碰撞、命中身份、表现和复制。立方体与蚂蚁共用宿主；服务器不运行每只怪物的行为 Actor Tick。客户端快照插值只负责展示，不进行独立 AI 或伤害结算。当前不是完整的 Mass Processor chunk 批处理实现。

## 球面规则与配置边界

当地向上为 `normalize(Actor位置 - Planet中心)`，重力朝星球中心。地表放置、坡度、移动、攻击范围及起落都依据真实 Mesh Surface，不使用固定 World-Z。跨三角面时，部分朝向采用连续径向上方向，落点仍来自真实碰撞表面。

C++ 负责规则、数据结构、通用接口与运行行为。Blueprint / Data Asset / 关卡实例负责 GameMode 选择、玩家与飞船类、星球参数、网格、材质、声音、地标和生成区域。新增内容优先复用聚居地敌人模板 `AJTSPlanetEnemySettlement` 与球面资源范围模板，使用 Component、Subsystem、Interface 组合职责。

当前普通物品使用 `UJTSItemDefinition + FJTSItemInstance`，近战、远程、采矿和星际能力分别由现有组件 / 目标接口处理。旧方案中提出的通用 Action / Effect 路由、独立 Mining / Resistance / Requirement 组件尚未落地，不作为现有系统入口。

源码按现有 `Modes / Core / Systems / World / Player / Components / Ships / Items / Interaction / Weapons / UI / Animation` 职责维护。开发约束以根目录 [AGENTS.md](../AGENTS.md) 为准。
