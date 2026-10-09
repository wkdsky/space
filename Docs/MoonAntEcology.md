# 月球蚂蚁数量与球面游走

本次目标：把独立尸体移到九巢穴环形区域的真实几何中心，让每巢保持约 30 只蚂蚁，并阻止爬陡坡和墙角反复转向。

## 实现与配置

复用既有 `AJTSMoonAntNestActor` 聚居地模板和蚂蚁的钻出、游走、受击、逃跑、钻回状态。巢穴增加可选的 `bMaintainPopulation`，以数据资产中的现存数量上限作为补充目标；现存比例为 `r` 时，补充间隔为 `lerp(min, max, r³)`，附带小幅时间随机扰动。到达上限后暂停出生；蚂蚁死亡或钻回时通知原巢穴，数量损失可以立即缩短待执行补充计时器。该模式不再叠加随机出生失败概率，所有出生仍由服务器执行。

`PDA_MoonSurfaceGameplay` 的配置为每巢 30 只，补充间隔 0.12～1 秒，地表活动周期 90～150 秒。出生权重近／中／远为 65%／25%／10%，距离分别为 0.6～9 米、9～28 米、28～60 米。游走半径 60 米，返回巢穴的限制半径 70 米。各只蚂蚁保留其出生距离对应的活动带，避免所有远处蚂蚁很快汇回入口。出生位置、沿途抽样和游走目标都检查真实月球表面坡度。

新增 `UJTSPlanetSurfaceSteeringComponent` 只负责真实球面移动，不自行 Tick。它依赖 `PlanetAnchor` 的地表查询，用表面法线与当地径向重力比较坡度，配置上限 35°；短段采样避免跳过悬崖，前方 65 cm 探测与 12 cm 身体半径扫掠检查阻挡。绕行保持选定方向 1.25 秒，优先处理阻挡，再恢复平滑目标转向。卡住超过 0.6 秒时重选游走目标。蚂蚁现已迁入共享 Mass 实体更新，服务器不再运行每只蚂蚁的 Actor Tick；避障记忆由实体保存，详见 MoonAntECS.md。

尸体中心由已提取的真实山脚闭环拟合，横向误差约 0.0017 cm。中心地形跨多个低模折面，单独生成 `SM_SkeletonAstronaut_Corpse_CraterCenter` 静态躺姿，连续贴合这些折面；可绑定玩家的原骷髅宇航员模型保持原样。原生导入网格已检查：左右靴子接触间隙约 0.30／0.33 cm，头部 1.66 cm、背部 1.27 cm、骨盆 0.78 cm。

## 机制参考

- [Reynolds：Steering Behaviors for Autonomous Characters](https://www.red3d.com/cwr/steer/gdc99/)：保存游走方向状态、前方避障以及避障优先级。本实现将这些思路用于球面陡坡与实体阻挡。
- [Valve：The AI Systems of Left 4 Dead](https://cdn.akamai.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf)：用受控的空间与时间随机性配置游戏群体。本项目采用按巢穴数量缺口反馈的补充规则，公式与参数由本项目制定。

## 修改文件

- `Source/space/Components/JTSPlanetSurfaceSteeringComponent.h/.cpp`：通用坡度限制、路径段探测、绕行状态。
- `Source/space/World/JTSMoonAntActor.h/.cpp`：调用移动组件、活动带、失效目标重选、生命周期通知。
- `Source/space/World/JTSMoonAntNestActor.h/.cpp`：数量反馈、出生可达性和间距、清理重入保护。
- `Source/space/Tests/JTSMoonAntTraversalTests.cpp`：径向重力下的坡度限制和墙角脱离回归测试。
- `Tools/Art/center_moon_corpse_in_basin.py`、`fit_centered_moon_corpse.py`、`configure_moon_ant_ecology.py`、`validate_moon_ant_ecology_pie.py`：中心摆放、姿势源模型、资产参数配置和运行观察。
- 关卡、蚂蚁 Blueprint、入口 Blueprint、月球数据资产与中心尸体网格由编辑器工具应用和保存。

## 当前验证状态

关闭两个重复项目编辑器后，项目现有 `spaceEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 完整编译及链接成功，记录为 `Saved/MoonAntEcology/Build-spaceEditor.log`。重开唯一编辑器后已应用并保存关卡、数据资产和 Blueprint 配置。

`JTS.Moon.Ants.RadialSlopeLimit` 与 `JTS.Moon.Ants.EscapesBlockedCorner` 两项原生自动化测试均通过，没有错误或警告。结果为 `SourceArt/MoonAntNests/moon_ant_traversal_tests.json`。九个巢穴的独立配置与真实山脚入口贴合复查通过，洞口距真实表面约 0.25 cm。中心尸体贴地检查再次通过，两只靴子最近接触间隙分别约 0.30、0.33 cm。

160 秒标准 PIE 观察通过，覆盖第一轮自然钻回／补充周期。最终九巢穴均为 30 只，整个采样没有超过上限或越过 35° 坡度限制。自然交替时允许短暂低于目标，十秒快照最低为 27，只要仍持续补充便不强行瞬间刷满。546 个不同蚂蚁实例观察到持续位移。

移除一个巢穴的 15 只后，活跃数量由 30 降至 15，计算补充间隔从 1 秒缩短为 0.23 秒；下一次十秒快照已恢复 30。密度采样近／中／远为 219／47／4，最远约 44 米，形成入口密集、向外递减的活动分布。完整结果为 `SourceArt/MoonAntNests/moon_ant_ecology_pie_validation.json`。

蚂蚁、巢穴入口和尸体三个 Blueprint 的 warnings-as-errors 编译检查均通过。另一项 PIE 复查确认只有九个已放置巢穴，没有旧随机巢穴，位置与复制配置保留；只有一具尸体，运行期间与中心配置位置误差为 0。中心全景截图为 `SourceArt/MoonAntNests/Moon_Corpse_CraterCenter.png`。未执行多客户端联机测试。

Editor 无需手动配置。可搜索 `Moon_Corpse_SkeletonAstronaut` 查看中心尸体，或搜索 `Moon_AntNest_` 查看独立巢穴；数量和范围在 `PDA_MoonSurfaceGameplay` 配置，坡度与探测在 `BP_MoonAnt` 的 `SurfaceSteering` 配置。后续需完整编译时先退出全部该项目的编辑器，避免同一关卡与模块被重复实例占用。
