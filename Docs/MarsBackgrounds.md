> Current space architecture: [Independent planet bubbles](IndependentSpaceBubbles.md). Shared-sky rules, old positions and old distance values below are historical and superseded.

﻿# 火星与火卫一背景

L_SpaceWorld 的 Backgrounds/MarsSystem 包含 Mars_Background 和 MarI_Background，使用提供的 FBX 和颜色贴图。两者是纯背景：无碰撞、重力、起降点或地表玩法，不注册为玩法星球，也不出现在星盘或画面标记中。只读背景元数据仅用于天体遮挡等显示计算，见 [天体导航与画面标记](CelestialNavigation.md)。

火星半径 600 m，距火卫二中心约 4.15 km；火卫一轨道半径约 1.66 km，约为火星半径的 2.77 倍。这些距离采用相同压缩比例，对应 [NASA 的轨道数据](https://starchild.gsfc.nasa.gov/docs/StarChild/solar_system_level1/mars_satellites.html)。火卫二已有地表、位置和起降配置保留，从其落点看火星约 17.1°。

火卫一原先按真实尺寸比例显示，在出生点 1920px/90° 视角下只有约 1.1 像素，固定相位 55° 又使其与火星相隔 23.5°，亮面约 14%，实际画面几乎无法辨认。现将固定相位设为 -20°，轨道半径不变；火卫一单独使用 4 倍显示半径补偿，等效直径约 15.7 m。它从出生点看约 5.9 像素，与火星相隔约 13.4°，亮面约 39%。这是明确的显示尺寸补偿，不是天文尺寸比例，也没有缩短其轨道距离。参数为配置脚本中的 PHOBOS_DISPLAY_RADIUS_MULTIPLIER 和 PHOBOS_ORBITAL_PHASE_DEGREES。

## 本地航行

地球与月球属于一个本地空间；火星、火卫一与火卫二属于另一个。火卫二 PlanetAnchor 的 CelestialBackgroundActors 配置两个背景，CelestialBackgroundCompanion 配置火星。

SpaceWorldManager 复用地月的本地航路影响范围与可逆成对缩放计算。火卫二离开后按自己的航路进度缩小，靠近后恢复原有游戏尺寸；其远处盘面大小通过关卡中的 CompanionApparentSeparationCentimeters 配置。

两个背景在三者之间的本地航路内保留各自配置的显示尺寸，盘面随各自到飞船的距离自然增减。它们不再继承火卫二的离开比例。离开整个本地航路范围后，背景从各自中心计算远航缩放，并按原有角直径阈值隐藏。模型缩放保持原有包围盒中心，避免偏移轴心引起位置漂移。本地航行期间继续隔离另一个星系的天空。

所有更新沿用现有表现更新入口，没有新增 Actor Tick、轨道模拟或材质 WPO 缩放。玩法星球仍只有 Moon、MarII、EarthFragments。

## 配置与验证

- 地图：Content/Space/Maps/L_SpaceWorld.umap
- 参数与背景资产配置：Tools/Art/configure_mars_system.py
- 通用运行时：Source/space/World/JTSPlanetAnchor.h、JTSSpaceWorldManager.h/.cpp
- 航路回归：JTS.Spacecraft.LocalSkyIsolation，检查跨越自由航行阈值后火星持续变大、火卫二独立缩小及往返一致性，另外覆盖火卫一航路和离开整个星系。
- 实际飞船输入验证：Tools/Editor/probe_mars_local_flight.py，仅用于 PIE；结果写入 Saved/MarsSystem/local_flight.json。
- 火卫一画面测量：Tools/Editor/inspect_phobos_sky.py，结果为 Saved/MarsSystem/phobos_visibility_before.json 与 phobos_visibility.json。Saved/SpaceDebug/Phobos_Before.png、Phobos_After.png 均为 1600×900、90° 普通视角；Phobos_Detail.png 为 2° 放大诊断，不能代表肉眼效果。

关卡配置通过 Unreal MCP 完成，无需补做 Editor 步骤。后续调整可在 MarII_PlanetAnchor → Planet / Presentation 检查背景成员、背景伴星和远处视角参数。背景 Mobility 使用 Movable，碰撞保持 NoCollision。

2026-10-17 验证：spaceEditor Win64 Development 编译通过，LocalSkyIsolation、PairedCelestialPresentation、InterplanetaryCruise、MarIILandingMap 四项回归全部通过，无测试警告或错误。实际飞船输入完成火卫二到火星附近再返回的往返，共记录 487 个样本；去程跨过自由航行切换点，火星盘面持续增大，返回时火卫二恢复完整尺寸，全程没有显示地球或月球，火星碰撞保持关闭。完整结果见 Saved/MarsSystem/local_flight.json、local_flight_validation.json 和 local_sky_automation.json。

火卫一可辨认度调整后的实飞检查完成，250 个样本显示其普通视角投影由约 5.9 像素逐渐增大，沿途最大约 24.2 像素；没有被隐藏或启用碰撞，本地背景比例保持稳定，火星持续随接近增大，地月组没有出现。结果为 Saved/MarsSystem/phobos_flight.json、phobos_flight_validation.json，普通视角截图为 Saved/SpaceDebug/Phobos_Flight.png。

火卫一可辨认度调整记录：当时修改了 L_SpaceWorld.umap、Tools/Art/configure_mars_system.py、Tools/Editor/configure_space_debug.py、capture_space_sky.py、probe_mars_local_flight.py、新增 inspect_phobos_sky.py 和本文档，没有修改 C++ 或材质。之后的导航与画面标记包含 C++ 修改，其编译、关卡配置和验证结果见 [天体导航与画面标记](CelestialNavigation.md)。关卡通过 Unreal MCP 配置并保存，无需手动 Editor 操作。
