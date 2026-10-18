> Current space architecture: [Independent planet bubbles](IndependentSpaceBubbles.md). Shared-sky rules, old positions and old distance values below are historical and superseded.

# 天体导航与画面标记

目的：让每位玩家从自己的相机辨认自由航行中的可导航星球。火星和火卫一是纯背景，既不出现在星盘上，也不显示画面名称角框。

职责：PlanetAnchor 保存关卡配置；CelestialNavigationLibrary 收集只读天体快照和计算投影、天体遮挡；CelestialMarkerWidget 只绘制本地玩家标记；既有 CruiseNavigationWidget 绘制星盘和地表距离。依赖现有 SpaceWorldManager 的注册表、本地天空隐藏与近地表切换规则，以及各玩家 LocalPlayer 的实际投影矩阵。没有新增 Actor Tick、碰撞查询、RPC 或天体玩法。

关卡为月球、地球、火卫二配置名称。火卫二的 BackgroundNavigation 数组保留火星、火卫一的 Actor 引用和模型局部半径，供天体遮挡计算使用；ShowOnStarChart、ShowScreenMarker 均关闭，背景默认也关闭这两个开关。条目必须属于既有 CelestialBackgroundActors，不能借导航配置注册玩法星球。两个背景继续无碰撞、重力、起降点；地月组与火星组三者的天空隔离和缩放规则保留。C++ 按配置过滤，不硬编码星球 ID。

画面标记由固定的飞船到星球中心距离触发，同船驾驶员、乘客共用该判断。PlanetAnchor.NavigationMarkerDistance 由关卡配置：月球 265000 cm、火卫二 145000 cm（压缩场景距离，不是星盘上的天文航程）；地球配置为 150000 cm。先在现有缩放规则下，按 90° 水平 FOV、16:9、盘面直径约占短边 5% 估算，再保存为固定值。月球实测约 263189 cm、火卫二约 144655 cm，过程保存在 Saved/SpaceDebug/marker_distance_calibration.json。运行时不按画面占比、镜头距离、FOV 或上一次显示状态开关标记。旧 MinimumScreenDiameterFraction、SizeHysteresisFraction 已移除；ScreenDiameterFraction 只作诊断。

标记为四个细线角框和名称底板，不填充星球内部。各玩家自己的投影矩阵只决定框的位置、大小与是否位于视野内，支持 FOV、DPI、分屏和限制宽高比。不绘制镜头背后、画外、另一天体遮住或已被本地天空逻辑隐藏的天体。导航进入原有 NEAR SURFACE／地表距离模式后清空画面标记，曲线下方居中显示对应星球名。

星盘继续按船体方向显示月球、地球、火卫二三个导航目标。星盘标签错开摆放，底部显示联系天体数量。乘客 HUD 从乘坐的共享飞船获取导航；飞行操作提示仍限驾驶员。HUD 加入所属玩家屏幕，避免各本地玩家共用整张视口画布。

参考 [Star Citizen 官方入门指南的分阶段飞行 HUD](https://support.robertsspaceindustries.com/hc/en-us/articles/360025028633-Getting-Started-in-the-Verse)与[官方量子旅行指南的目的地／遮挡标记](https://support.robertsspaceindustries.com/hc/en-us/articles/360019449994-How-to-Quantum-Travel)。角框、名称与触发规则为本项目的实现。

修改文件：

- Content/Space/Maps/L_SpaceWorld.umap
- Source/space/World/JTSCelestialNavigationTypes.h、JTSPlanetAnchor.h、JTSSpaceWorldManager.h/.cpp
- Source/space/UI/JTSCelestialNavigationLibrary.h/.cpp、JTSCelestialMarkerWidget.h/.cpp
- Source/space/UI/JTSCruiseNavigationWidget.h/.cpp、JTSPrototypeHUD.cpp、JTSPrototypeHUDWidget.h/.cpp
- Source/space/Tests/JTSSpacecraftRegressionTests.cpp
- Tools/Editor/configure_celestial_navigation.py、configure_space_debug.py
- Tools/Art/configure_mars_system.py（说明更新）、Docs/MarsBackgrounds.md、本文档

Editor 操作由 Unreal MCP 完成并保存，无需手动重配。需要重新配置时，在停止 PIE 后通过既有 SpaceDebugAuthoringTools.run 执行 configure_navigation。视角诊断命令均仅操作 PIE 临时状态，不写入关卡：navigation_view_add_passenger、navigation_view_board、navigation_view_cruise、navigation_view_range_<PlanetId>_<cm>、navigation_view_cameras_distance/distance_back/close/zoomout/offaxis、navigation_view_near_airborne；inspect_navigation 输出实际距离、固定触发距离、距离资格与每位玩家的标记快照。

验证记录见 Saved/SpaceDebug/celestial_navigation_automation.json、marker_distance_e2e.json 和 Navigation_Distance_*.png。JTS.Spacecraft.CelestialNavigation 覆盖固定距离配置传递、从共享飞船测距、边界包含、超距拒绝、宽 FOV 小投影保留资格，以及背景默认排除、缩放、隐藏、碰撞/注册表不变、背面/天体遮挡和地表模式清空；同时保留本地天空隔离、地月成对缩放、巡航与火卫二起降回归。

2026-10-18 最终验证：spaceEditor / Win64 / Development 编译成功（73.20 秒），上述五项自动回归全部通过，错误、警告均为零。Unreal MCP 已保存固定距离配置，火星、火卫一的两个显示开关仍为 false。星盘仍只有地球、月球、火卫二三个目标。

PIE 同一共享飞船的驾驶员、乘客验证月球和火卫二，共 14 个场景快照：在各自固定距离的 90% 处，两人分别用 55° 和 150° 水平 FOV，火卫二投影约 13.51% / 1.88%、月球约 13.61% / 1.90%，均显示标记；乘客转出视野为 1/0，转回恢复 1/1；镜头实际拉远和旋转到侧面时仍为 1/1。飞船移到固定距离的 102%，即使观察相机移近到盘面很大，标记仍为 0/0；进入各自近地表模式后均为 0/0。截图为 Navigation_Distance_MarII/Moon_TwoFOV.png、Navigation_Distance_MarII/Moon_OffAxis.png。观察相机和隐藏飞船仅用于 PIE 验证，不写入关卡。

最终编辑器检查确认两个背景仍为 NoCollision、无 Tick，玩法注册表仍为三颗，原有地表和背景位置保持一致。PIE 已停止，关卡已保存，无需手动 Editor 操作。早期按占比触发的导航快照以及含火星角框的验证已被最新需求替代，不代表当前配置。
