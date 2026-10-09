# 月球蚂蚁 ECS 与星际武器状态

目的：让月球蚂蚁与立方体敌人共用服务器 Mass 实体管理，支持现有星际武器命中、状态控制及模型表现。

职责：`UJTSPlanetEnemySubsystem` 继续拥有实体创建、统一更新和释放；`FJTSPlanetAntSimulation` 只处理蚂蚁活动规则和真实球面移动；`AJTSMoonAntActor` 保留复制、模型、命中身份、血量和尸体掉落。没有新增平行的敌人管理系统。

依赖：现有 `UMassEntitySubsystem`、`PlanetAnchor`、地表转向组件、聚居地模板和 `UJTSStellarTargetComponent`。巢穴实例、蚂蚁模型、活动参数及武器效果资产继续由 Blueprint / Data Asset 配置，运行时代码没有关卡或资源路径。

## 实体数据与更新

- 每只服务器蚂蚁注册独立 `FMassEntityHandle`，复用立方体的绑定、移动、导航和身体 Fragment，并增加蚂蚁配置与活动 Fragment。
- 出洞、游走、受击、逃跑、钻回、活动周期、距离带和避障记忆属于实体数据。服务器关闭蚂蚁 Actor Tick；客户端 Tick 只做移动插值与视觉更新。
- 坡度限制依然相对当地径向重力，路径段检查真实 Planet Mesh。实体持有独立绕行方向与持续时间，沿用此前墙角脱离逻辑。
- 自然钻回在所有实体更新完成后销毁；武器击杀、巢穴清理和关卡退出会释放 Mass 身份。击杀仍产生原有尸体和经验奖励。
- 九个巢穴继续复用当前补充模板，现存量上限、动态生成间隔、活动范围和模型参数保持在原有数据资产中。

## 武器与 Buff

蚂蚁沿用原生 `StellarTarget` 组件和可见性命中球。射线武器命中球触发同一目标的伤害与状态，范围武器也能发现它；模型组件仍只负责显示。

统一移动处理读取状态组件的速度系数，冰冻 / 定身停止自主游走，减速按原规则降低速度，吸引 / 排斥经过原有力场积分再投影到球面。外力按其实际方向检查整个路径段，不受普通游走的转向记忆影响，并继续遵守坡度及静态障碍限制。火焰、光灼烧、腐蚀、冰冻使用武器配置的 `BP_StellarTargetStatusFX`，覆盖实际可见的 Static Mesh / Skeletal Mesh 并跟随身体。死亡与状态到期沿用现有清理规则。

没有为了演示 Buff 改高生产蚂蚁血量：致命命中仍会正常击杀。真实武器回归测试临时提高测试对象血量，以便观察持续效果。

光束命中复用 `JTSStellarCombat::TraceBeam`：原球形光束扫掠若先擦到静态地形，会补查准星中心射线；仅当中心射线先命中可存活的小型目标、命中体半径不超过光束半径两倍时，采用该精确命中。完整墙体仍会挡住中心射线，动态阻挡和普通敌人的原扫掠顺序保留。此规则用于解决小型贴地目标被光束宽度提前碰地遮蔽的问题，无蚂蚁类或月球关卡特例。

## 修改文件

- `Source/space/Systems/JTSPlanetAntFragments.h`：蚂蚁实体配置及活动状态。
- `Source/space/Systems/JTSPlanetAntSimulation.h/.cpp`：独立活动规则与带 Buff 的球面移动。
- `Source/space/Systems/JTSPlanetEnemySubsystem.h/.cpp`：共用注册、更新和释放流程。
- `Source/space/Components/JTSPlanetSurfaceSteeringComponent.h/.cpp`：支持实体拥有避障状态。
- `Source/space/World/JTSMoonAntActor.h/.cpp`：复制与展示桥接，移除服务器 Actor 行为 Tick。
- `Source/space/Tests/JTSMoonAntTestHabitat.h`、`JTSMoonAntECSTests.cpp`、`JTSStellarWeaponTests.cpp`：真实球面、生命周期、移动控制、真实武器命中与模型 Buff 回归。
- `Source/space/Weapons/JTSStellarCombat.h/.cpp`、`Source/space/Components/JTSStellarWeaponComponent.cpp`、`JTSStellarAbilityCombat.cpp`：通用贴地小型目标精确光束命中，覆盖聚焦、冰刺与腐蚀射线。
- `Source/space/Components/JTSStellarAbilityComponent.cpp`、`JTSStellarSupportComponent.cpp`：世界销毁后清理计时器时检查 World，避免重复自动化世界回收时空指针崩溃。
- `Tools/Validation/moon_ant_ecs_pie.py`：真实关卡实体与展示 Actor 一致性观察。

## Editor 操作与验证

蚂蚁沿用 `BP_MoonAnt` 父类及巢穴配置，无需重新放置巢穴或修改玩家装备。已通过 Unreal MCP 重新编译 `BP_MoonAnt` 和 `BP_MoonAntNestEntrance`，检查无警告；新模块已加载，项目资产已保存。编辑器停留在 `L_SpaceWorld`，验证用 Play 已停止，后台 CPU 节流和 Python 远程执行恢复原设置。使用时直接打开该关卡并 Play。

`spaceEditor Win64 Development` 完整编译及 DLL 链接成功，未使用 Live Coding。以下自动化测试覆盖本次能力：

- `JTS.Moon.Ants.MassRegistrationMovementAndCleanup`
- `JTS.Moon.Ants.MassStatusMovementControls`
- `JTS.Stellar.Targets.MassAntWeaponHitAndModelBuffs`

本次三个新增测试全部通过，第二次独立运行也全部通过。测试实际加载 `BP_MoonAnt` 与武器目录，验证 270 个实体注册、移动、死亡掉落 Actor、成批自然钻回清理，以及减速、冻结停止、控制到期恢复和外力不受旧转向记忆干扰。真实武器测试覆盖火焰、光灼烧、冰冻、腐蚀与模型覆盖材质；完整墙体阻挡伤害及状态。

真实 `L_SpaceWorld` 标准 PIE 验证通过：

- 45 秒 ECS 观察中，活蚂蚁数量始终等于有效实体数量，服务器蚂蚁 Actor Tick 数为 0，285 个不同蚂蚁发生移动（包含补充生成的新个体）。最终 267 个活 Actor 对应 267 个实体，符合自然轮换期间每巢约 30 只的目标。
- 160 秒生态观察覆盖自然活动周期及人为移除 15 只后的补充。九巢最终均为 30；数量超过上限和违规坡度样本均为 0。近／中／远实测 229／37／4，只数按外圈递减，最远约 41.2 米。单巢数量 30 → 15 后，名义出生间隔 1 → 0.23 秒，随后恢复到 30。
- 关卡中实际玩家的 `FreezingTube` 命中真实蚂蚁，目标仍有 Mass 身份，出现实际模型覆盖材质和 8 个冰晶。只在该 PIE 临时提高目标血量；资产中的生产血量仍为 3。截图为游戏视口原始输出，没有修改图像或保存运行时相机。

验证证据：

- [完整编译与自动化结果](../SourceArt/MoonAntNests/moon_ant_ecs_automation_validation.json)
- [实体和 Actor 一致性](../SourceArt/MoonAntNests/moon_ant_ecs_pie_validation.json)
- [160 秒生态观察](../SourceArt/MoonAntNests/moon_ant_ecology_pie_validation.json)
- [实际关卡冰冻状态](../SourceArt/MoonAntNests/moon_ant_live_buff_validation.json)
- [冰冻模型截图](../SourceArt/MoonAntNests/Moon_Ant_ECS_Frozen.png)

完整 `JTS.Moon + JTS.Stellar` 自动化为 55/57 通过。本次新增测试、蚂蚁坡度／墙角和全部 Stellar 测试通过；两项现有立方体断言未通过，未修改其测试或生产物理参数：

- `JTS.Moon.BodyCollision.RuntimeDenseCurvedSurface`：断言单步最多 5.001 cm，实际未改动的配置允许 `min(900/60, 45) = 15 cm`。实测 15 cm，最终残余重叠为 0，贴地误差和稳定性断言通过。
- `JTS.Moon.CrowdPhysicalFieldAndBudgets`：最小玩家距离及工作预算断言通过，但初始与玩家重叠的那个立方体未满足测试单独要求的最终距离大于 600 cm。

2026-10-09 后续修复了 `DA_Item_MoonAntCorpse` 缺失警告：该 Data Asset 是可选的物品定义覆盖，项目当前没有这份资产；尸体道具仍用于拾取及飞船有机质转换。物品查询现在先检查对象或包是否存在，缺失时直接使用已有的原生默认定义，已有 Data Asset 仍优先加载，真实加载错误仍正常报告。

完整 `spaceEditor Win64 Development` 编译通过。在重新启动的编辑器进程中，尸体拾取／有机质转换、Mass 注册清理、状态控制、模型 Buff 及普通武器测试共 5/5 通过，日志中的该缺失资产警告为 0。新回归测试实际验证死亡掉落、落地可拾取、背包增加一具尸体、提交后移除尸体并增加一份共享有机质，同时确认 Rock 仍使用其 Data Asset。详见 [尸体物品修复验证记录](../SourceArt/MoonAntNests/moon_ant_corpse_item_validation.json)。无需重新配置资产，编辑器已加载新模块。未执行多客户端联机验证。

架构参考：[Epic MassEntity 概述](https://dev.epicgames.com/documentation/unreal-engine/overview-of-mass-entity-in-unreal-engine?lang=en-US)。本项目继续使用已有立方体的 World Subsystem 作为 Mass 统一更新宿主。
