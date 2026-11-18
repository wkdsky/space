# 月球玩法与配置

## 入口与地形约束

当前玩法位于 `/Game/Space/Maps/L_SpaceWorld`。月球 PlanetAnchor 的真实表面半径约 15000 cm；Gameplay Data 入口为 `/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay`。参数还可能由关卡实例覆盖。

保留原始月球 Mesh、顶点、碰撞和主坑轮廓。洞口、裂缝、采食斑、巢塔、石块等全部是独立附着物，按区域随机生成并贴合真实表面。不能通过切割、合并、替换月球模型来制作生态，也不能把外围附着物烘成固定地形。

尸体主坑附近保留当前 **七个**巢穴及其位置；仅这七个洞口允许摧毁。更新规则时同步遵守 [AGENTS.md](../AGENTS.md)，不把旧九巢穴布局重新导入关卡。

## 资源区域模板

资源范围使用球面区域模板，在真实月面采样。重叠区域先比较资源比例与密度：相同配置合并、去重；不同配置由较低的 OverlapPriority 决定归属。不能为每个资源区新增一套独立随机生成逻辑。

| 类型 | 岩石 | 金属 | 晶体 | 采集工作量 | 默认权重 |
| --- | --- | --- | --- | --- | --- |
| 小石块 | 1 | 0 | 0 | 直接拾取 | 30 |
| 中石块 | 2 | 0 | 0 | 5 | 15 |
| 大石块 | 6 | 0 | 0 | 24 | 15 |
| 中石块与金属 | 2 | 2 | 0 | 10 | 15 |
| 大石块与金属 | 6 | 6 | 0 | 30 | 10 |
| 中石块与晶体 | 2 | 0 | 2 | 10 | 10 |
| 大石块与晶体 | 6 | 0 | 6 | 30 | 5 |

通用模板默认密度为每 100 m² 0.8 个，半径 2000 cm 的区域约 10 个目标。表中是模板基准，不代表每个已配置区域的独立覆盖值。生成数量由有效球面面积决定；失败采样不会凭空补发物品，收纳失败需保留可回收实物。

当前资源与生物活动会通过资源斑、采食点和聚居地关联。新增星球资源区复用已有分布数据与模板，资源美术入口在 `SourceArt/MoonResources`。

## 当前生态

当前生态使用 MirrorSkimmer、Riftlip、SpireWorker、SpireSoldier 及各自的 Habitat / 活动区域。聚居地配置生成与恢复、资源关联、居民上限和活动范围；生物负责移动、攻击、受击、死亡和尸体表现。布点依赖球面表面与坡度，不能直接用世界水平面散布。

生物系统同时使用实体数据、集中模拟和表现 Actor，不应把它概括为“所有敌人都由完整 Mass Processor 驱动”。具体接触伤害、攻击时序和尸体由当前类与 Blueprint 决定。

旧 MoonAntNest / CubeCluster 工具与资源仍可能作为历史来源存在，不代表它们是当前主生态。当前 Data Asset 中 `MoonAntNestCount = 0` 是关闭旧随机蚁巢生成，**不能为了七巢穴约束把它改成 7**；七个固定洞口是已放置的生态 Habitat。

## 七巢穴遭遇

当前固定洞口标签为 `Moon_Nest_01` 至 `Moon_Nest_07`，由 `JTSNestBossEncounter` 的 Nests 引用配置。遭遇 ID 为 `MoonCorpseCraterLeader`，Boss Blueprint 为 `/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MirrorBoss`。

七个洞口全部封堵 / 摧毁后触发 Boss 唤醒流程。入口操作还需要满足服务器交互与物种识别条件；外围随机生成洞口不属于可摧毁名单。通用生态组件保持可配置，不把本关卡数量硬编码到 Runtime C++。

Boss 被击败后产生火星权限卡奖励。远征快照保存已封堵入口、遭遇击败和重要物品标识，恢复时避免重复奖励。验证应覆盖入口权限、最后一个洞口触发、Boss 死亡、奖励拾取与读档，而非只检查 Actor 数量。

## 美术与工具

- `SourceArt/MoonSurfaceV3`：附着物、模块配方、材质、洞口与裂缝独立几何数据。
- `SourceArt/MoonCreaturesV3`：四类生物源模型、动画、材质与预览。
- `SourceArt/MirrorBoss`、`SourceArt/SpireColony`：Boss 与巢塔 / 士兵源资产。
- `SourceArt/SkeletonAstronaut`、`SourceArt/MoonAntNests`：尸体、原始表面参考和早期洞口来源。
- `Tools/Art/mirror_boss_editor.py`：当前七巢穴遭遇的编辑器配置入口；含旧九巢穴到七巢穴的迁移兼容。

历史构建脚本可能覆盖当前参数、使用旧布局或切割地形，不能作为一键重建当前关卡的步骤。执行前按 [开发文档](Development.md) 检查影响范围；本次整理没有重新生成或移动关卡中的七个洞口。
