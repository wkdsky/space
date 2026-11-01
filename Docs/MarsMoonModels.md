# 火卫模型替换与火星默认着陆（2026-10-30）

本次配置取代旧文档中“火卫二默认出生”和“火星禁止进入”的关卡设置，仍使用同一个长期存在的 SpaceWorld。

## 模型和资产

- 火卫一来自 `MarI 3d model/asteroid_rock_3d_model.fbx` 及同目录 `.fbm` 中的 BaseColor。覆盖 `/Game/Space/Environment/MarsSystem/MarI/SM_MarI` 和对应贴图，旧几何不再保留。保持原世界中心和 9000 cm 最大包围盒半径，保留径向重力、真实表面碰撞与原有被动地表能力。
- 火卫二来自 `MarII 3d model/rock_3d_model.fbx` 及同目录 `.fbm` 中的 BaseColor。新资产统一放在 `/Game/Space/Environment/MarsSystem/MarII/`，包括 `SM_MarII`、`T_MarII_BaseColor`、`M_MarII_Surface`。保持原模型的世界包围盒中心和最大外径。
- Blender MCP 检查两份 FBX 均有 UV、没有边界边或非流形边。检查与渲染使用独立场景，不修改输入 FBX。预览：`Saved/MoonModels/new_moon_models.png`。
- 删除旧 `ThirdParty/MarII/MarII__2_`、未使用的 `ThirdParty/Environment/UltimateSpace/Meshes/MarII` 与独占的旧材质/贴图。`M_Cave_Interior` 仍被既有 `Stairs_74EF54B8` 网格引用，因此保留这一共享材质；它不是旧天体模型。

## 火卫二与火星

`L_SpaceWorld` 和 `L_SpaceWorld_Authoring` 均配置火卫二 PlanetAnchor、LandingAnchor 与 ArrivalAnchor，使用新模型的真实复杂碰撞、径向重力和四脚着陆拟合，保留飞行接近、降落与下船能力。火卫二 `bShowScreenMarker=False`、`bShowOnStarChart=False`，仅隐藏场景标记和导航图标记，不影响玩法注册。主关卡压缩空间条目引用火卫二 Anchor 并开启 NearFlight；GameMode 配置 `BP_MarIIPassiveSurfaceController`，不生成月球资源或生物。旧 LandingZone 与旧模型不恢复。

主关卡将既有火星外观接入 `Mars_PlanetAnchor`，球心使用模型包围盒中心。火星启用 Complex As Simple 和双面表面查询，使用径向重力。关卡压缩空间条目引用新 Anchor，并关闭原火星禁入干预。`BP_MarsPassiveSurfaceController` 复用通用被动控制器，只配置 PlanetId=Mars，不生成月球资源或生物。

火星 `bShowScreenMarker=True`、`bShowOnStarChart=True`，场景标记和导航图沿用现有显示距离、遮挡及飞行状态规则。

`Mars_LandingAnchor` 位于真实网格日照侧，通过现有 TwinFork 四脚支撑拟合；没有放宽起落架、坡度、净空或着陆规则。`Mars_ArrivalAnchor` 的飞船查询点沿球心径向位于地表上方，人物由原有船外净空查询放置。当前地表点约为 `(12277952.069, 12479433.734, 12202869.867)` cm，配置留在关卡实例中。

Debug 菜单目的地为 Moon 与 Mars；InitialPlanet=Mars，主机本地 Debug Default 也设为 Mars。远征恢复仍使用原有存档优先规则。

## 通用旅行修正

远处星系会临时禁用地表碰撞。Debug 目的地校验现在检查配置的地表 Actor；LandingManager 在校验实际地表及抵达变换前恢复表现缩放和碰撞。失败时恢复当前飞船对应的表现。该修正不包含天体名称、资产路径或具体实例引用，仍走原有主机与拥有者 RPC 校验。

## 文件与编辑器操作

- 资产和关卡：`Content/Space/Environment/MarsSystem/{MarI,MarII,Mars}`、`Blueprints/Planets/{Mars,MarII}/BP_*PassiveSurfaceController`、`Blueprints/Modes/BP_SpaceWorldGameMode`、`Maps/L_SpaceWorld`、`Maps/L_SpaceWorld_Authoring`。
- 通用逻辑：`World/JTSPlanetLandingManager.cpp`、`World/JTSSpaceDebugTravelManager.cpp`。
- 回归：`Tests/JTSSpacecraftRegressionTests.cpp` 的地图着陆用例改为 `JTS.Spacecraft.MarsLandingMap`；`Ships/JTSSpacecraftActor.h` 同步测试友元名称。
- 工具：`Tools/Art/replace_mars_moon_models.py`、`mars_moons_mcp.py`，以及现有模型源路径、Debug 配置和 `Tools/Editor/configure_celestial_navigation.py`。

配置已通过 Unreal MCP 保存，无需补做手工导入或绑定。打开 `/Game/Space/Maps/L_SpaceWorld` 并 Play 即可。需要调整火星默认落点时，移动 `Mars_LandingAnchor` 和 `Mars_ArrivalAnchor`，然后重新执行真实表面和四脚支撑验证。

验证报告保存在 `Saved/MoonModels/`：`blender_geometry.json`、`validate.json`、`cleanup.json`、`pie_probe.json`、`pie_status.json`、`deimos_landing_pie.json`、`automation_results.json` 和 `build.log`。

## 验证结果

- `spaceEditor Win64 Development` 正常完整编译成功，未使用 Live Coding。
- 两张地图均确认火卫二可降落且隐藏两类导航标记；旧网格和独占资产已删除。火卫一、火星、火卫二各 96/96 个方向的真实地表查询通过；火星和火卫二的配置落点均为四脚接触。
- PIE 通过正式 PlayerController RPC 验证火星默认落地、保存默认值、火星→月球→火星往返、火星登船和起飞。重新启动 PIE 后仍默认在火星，菜单显示 Moon/Mars，Mars 的 Default 带星号。
- PIE 将已登船的测试飞船放置在火卫二真实地表上方后，通过正式 `ServerSetFlightInput` 持续下降，原生自动捕获与着陆流程在约 3 秒后成功完成，四脚接触、无着陆失败；正式下船 RPC 成功，人物当前星球为 MarII。此测试不向 Debug 菜单添加火卫二。
- Unreal MCP 自动化测试 `JTS.Spacecraft.CelestialNavigation`、`JTS.Spacecraft.MarsLandingMap` 与 `JTS.Debug.TravelAuthority` 共 3/3 通过，零错误、零警告。地图用例包含实际地表降落、登船、下船及人物胶囊净空检查。本次修正只更改资产与 Python 配置，无新增 C++ 编译需求。
- 通过 Blender MCP 渲染检查新源模型，并通过 Unreal MCP 视口复核两颗火卫的新外观。
