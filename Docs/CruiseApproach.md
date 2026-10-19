# 自由航行接近减速

> 当前速度算法与参数见 [AdaptiveFlight.md](AdaptiveFlight.md)；下文保留旧方案的排查和验证记录。


适用于 `L_SpaceWorld` 和 `BP_Twinfork_R1`，2026-10-18 第二次调校。

目的：在星球刚能看出是小圆盘时开始标记和减速，避免自由航行中突然放大。旧参数虽然在数值上衰减，但火卫二的 +Z 接近段在正常游戏时间内不到一帧，不能据此认定视觉过渡平缓。

职责与依赖：现有 `CompressedSpaceComponent` 根据注册天体、压缩坐标、标记范围和独立近地泡泡计算速度；现有飞行组件在服务器应用实际速度。`PlanetAnchor` 保存每个星球的曲线配置，具体数值保存在关卡；飞船 Blueprint 保存公共回退值和边界倍率。没有新增系统、Tick 或 C++ 具体天体引用。

## 各星球配置

| 关卡天体 | 开始标记及减速的导航中心距 | 初始视直径约值 | 指数强度 | 放大速度上限 |
| --- | ---: | ---: | ---: | ---: |
| 月球 Moon | 200,000 km | 1.00° | 2.5 | 10°/s |
| 火卫二 MarII | 2,400 km | 0.69° | 3 | 12°/s |
| 地球 EarthFragments | 620,000 km | 约 1.18°（碎块真实外缘） | 0.85 | 20°/s |

视直径仅用于离线调校这些距离，不参与运行时触发。参考 1080 高、70° 垂直视野约为 9–16 像素，已能区别于背景星点。实际投影受窗口和镜头设置影响，触发范围保持固定。

`Planet|Navigation` 中 `NavigationMarkerRangeKilometers` 为标记与减速共用的固定船体到中心阈值；`CruiseApproachExponentOverride` 是独立曲线强度，0 使用飞船回退值 4；`CruiseApproachExpansionDegreesPerSecond` 限制已触发范围内的几何视直径增长速度，0 关闭此补充保护。每个星球保留自己的近地球形泡泡。火星、火卫一仍是背景，不创建标记、减速区、碰撞或着陆玩法。

## 速度及边界规则

- 不检查镜头朝向、FOV、屏幕占比或遮挡。同船驾驶者和乘客使用同一阈值；转镜头不改变飞船速度。
- 只对实际运动方向正在接近的天体限速。扩大后的地球范围会覆盖月球附近，驶离或切向经过不会因此被身后的天体限速。标记显示仍只按固定距离。
- 压缩坐标在不同方向的比例不同。沿当前位置的星球径向求标记距离对应的关卡外半径，计算外半径到近地半径的进度 `p`，从 0 到 1。
- 保留指数式 `w = (exp(-k*p) - exp(-k)) / (1 - exp(-k))`。每个天体分别配置 `k`；较大的值使高速更早降低、接近末段更平缓。不能把火卫二的强度直接套给地球或月球。
- 目标 NAV 为 `3*NearNAV + (CruiseNAV - 3*NearNAV)*w`，再按当前位置、实际运动方向的导航比例换回真实运动速度。前后、横移、升降、速度档和 Shift 都使用对应近地输入作为基准。
- 单纯提高指数不能解决压缩坐标的方向差异：火卫二某些方向可在几米内完成放大，另一些方向却会长时间显示为小圆盘。保留较平缓指数，在触发范围内再按船体位置、实际视觉缩放及模型半径计算视直径变化率，降低真实推进速度。该保护完全不使用屏幕、FOV 或玩家镜头；同船乘客共享相同运动。
- 放大保护不低于同输入 3 倍近地 NAV 的下限；启用大星球尺度适配时，还受下述实际速度托底约束。因此末段若速度下限要求更快，允许超过表中放大速率。压缩坐标转换带可能使 NAV 读数局部变化，额外保护针对真实几何放大速度，不伪造 HUD 数字。
- 近地外最低为同输入近地 NAV 的 3 倍；进入近地后为正常近地速度。远处保持原巡航能力，松键立即停止。
- 移动后同步实际反重力速度，HUD 始终由实际导航坐标导数计算。大星球的实际速度托底可能使 NAV 局部上升，不把仪表单调性作为牺牲操作体验的目标。碰撞修正和近地速度覆盖具有优先权。
- 自由航行接近泡泡时缩短模拟步长，并在配置放大保护的天体附近，将单次模拟的几何放大增量控制在约 0.25° 内，避免平坦坐标带末端的骤变跨过导数采样；复用原有移动、碰撞及镜头逻辑。
- 多个正在接近的减速范围重叠时取较低上限。C++ 不移动天体、不更换视觉中心或观察相机。

## 修改文件及 Editor 操作

- `Source/space/World/JTSPlanetAnchor.h`：每个星球可覆盖曲线强度和几何放大速率。
- `Source/space/Components/JTSCompressedSpaceComponent.h/.cpp`：按天体读取强度，限定接近方向，保留现有指数和 NAV 换算，补充几何放大保护。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：独立曲线、回退值、关闭曲线及驶离检查，保留原移动回归。
- `Tools/Editor/validate_cruise_approach.py`：通过 MCP 配置关卡，原生飞行逐帧记录视直径，以及多方向纯查询调校。
- `Content/Space/Maps/L_SpaceWorld.umap`：三个 PlanetAnchor 的独立范围和强度。
- `Content/Space/Ships/TwinforkR1/BP_Twinfork_R1.uasset`：确认并保存公共回退强度 4、近地外倍率 3。

Editor 中停止 PIE 后，通过 Unreal MCP 执行 `SpaceDebugAuthoringTools.run(operation="antigravity_approach_configure")`，仅保存三个天体的范围、强度、放大速率及飞船公共回退参数，不重新配置布局或灯光。可手动在三个 PlanetAnchor 的 `Planet|Navigation` 调整以上三个字段。不要使用会改写完整布局的旧 `antigravity_configure`。

验证输出：`Saved/SpaceDebug/cruise_approach_tuning_build.log`、`approach_tuning_automation.json`、`approach_tuning_e2e.json`、`approach_tuning_directions.json`。真实飞行使用驾驶者服务器 RPC，保留生产 Actor Tick、原生镜头和正常游戏时间；纯查询积分仅用于预估，不能替代实际飞行验证。

## 上一轮验证记录（本轮调整前）

- `spaceEditor Win64 Development` 编译成功，85.15 秒，未使用 Live Coding。
- 七项回归全部通过，零错误、零警告：`CruiseApproachSlowdown`、`AntiGravityCruise`、`IndependentCompressedSpace`、`CelestialNavigation`、`TakeoffLandingCycle`、`ThirdPersonFlightControls`、`SurfaceEnvelope.CraterTakeoffClearance`。
- 11 条原生飞行路线通过，共 5,896 个游戏帧采样，全为 1 倍游戏时间、持续 Shift 前进。验证固定触发距离、近地外 3 倍下限、实际几何增长、正常入泡泡、松键立即停止及镜头跟随。逐帧最大视直径增量约 1.472°；几何增长限制在 3 倍下限优先时允许提高。

| 路线 / 初始来向 | 到近地耗时 |
| --- | ---: |
| 月球 +Z | 6.46 s |
| 火卫二 +Z | 3.95 s |
| 地球 +Z | 14.25 s |
| 火卫二，从月球方向 | 4.31 s |
| 月球，从火卫二方向 | 9.66 s |
| 地球，从火卫二方向 | 12.00 s |
| 火卫二 −X | 4.70 s |
| 月球 −Y | 4.72 s |
| 地球 −X | 10.55 s |
| 月球，从地球方向 | 23.23 s |
| 刚离开月球近地区域 → 地球近地 | 36.51 s |

最后一项从月球近地球外开始，避免把位于月球另一侧的地球远程标记边界当成有效起点。其余项目从目标标记边界外 10 m 的瞬态初始探针开始，不代表完整地表到地表航程。

三个天体另完成六个轴向和两个其他星球来向的纯查询扫描。地球 +X 的完整标记球穿过月球泡泡，报告显式标注该路径，不将此积分视为可直达地球的实际飞行。火卫二各方向预估接近段收敛至约 3.94–4.71 秒。

飞船 Actor Tick 保持开启，SpringArm 位置延迟关闭，镜头支点最大误差约 0.0000037 cm。`ApproachTuning_Deimos.png` 为正常原生飞行后停车的编辑器截图，确认星球、名称方框及船体同时显示。天体中心、导航位置及近地高度与调校前一致，火星和火卫一仍无碰撞、无玩法导航。PIE 已停止并恢复临时性能设置，直接运行当前关卡即可。

## 地月近距离航行与碎块区域适配

复用现有组件，无新增系统。`PlanetAnchor` 保存可编辑参数，`CompressedSpaceComponent` 捕获完整几何尺度并计算接近与近地速度，飞行组件继续只在服务器应用输入和移动。HUD 复用已有碎片包络数据进行投影。

- 地球 Actor 经旋转后的包围半径约 173,193 cm，但 67 块碎片的真实最远顶点约 122,638 cm。近地球形区域改为真实碎片包络的最远点加原有近地高度，半径从约 1,909 m 收紧至 1,403 m；不再额外覆盖约 506 m 的低速空区。
- 地球框选投影已有凸包顶点，跟随其几何旋转和压缩表现比例，保留 UI 原有 8 像素留白。其他普通星球继续使用球体轮廓。框选触发仍按船体到天体的固定导航距离，镜头/FOV 不改变速度。
- 近地倍率 `clamp((max(1, 完整半径 / 参考半径))^指数, 1, 上限)`。本关卡参考半径 21,000 cm、指数 0.5、上限 3。参数为 `Planet|Flight` 的 `NearFlightReferenceRadius`、`NearFlightSizeExponent`、`NearFlightMaxSpeedMultiplier`；参考半径为 0 时不启用适配。始终使用捕获的完整几何半径，远处缩小的 Mesh 不影响操控。
- 月球和火卫二维持 1 倍，地球约 2.417 倍。普通前进、Shift 前进、倒退、横移、升降和限速档位均使用同一近地倍率。普通前进约 101.5 m/s、Shift 约 182.7 m/s；月球仍为 42 / 75.6 m/s。这些是关卡实际运动速度，NAV 仍由真实导航坐标导数计算。
- 地球接近强度从 1.5 改为 0.85，放大速率上限从 12.5 改为 20°/s；接近末段的 3 倍下限以目标星球适配后的同输入近地速度为基准。进入近地的当帧立即应用目标星球倍率，避免暂用出发星球的速度。
- 原生实飞发现仅抬高 NAV 下限，地球末段普通/Shift 实际速度仍会低至约 44.9 / 80.8 m/s，低于其自身近地速度。因此对近地倍率大于 1 的星球，在距近地边界最后一个完整几何半径内，以 `SmoothStep` 渐入实际速度下限；边界处衔接到同输入 3 倍适配近地速度。两端斜率为零，不在标记刚出现时强行提速；月球与火卫二保持原曲线。这个实际速度下限与 NAV 下限一起优先于放大限速，避免压缩比例造成低速爬行。
- 完整船体扫掠、碎块安全包络及松键立即停车继续生效。

Editor 停止 PIE 后，通过 MCP 执行 `SpaceDebugAuthoringTools.run(operation="antigravity_balance_configure")` 保存本轮关卡配置；或在三个 PlanetAnchor 的 `Planet|Flight` 设置上述参数，并在地球 `Planet|Navigation` 设置 0.85 / 20。该入口只调整飞行参数，不改天体位置、资源、光照或飞船 Blueprint。验证入口为 `antigravity_balance_native` / `antigravity_balance_status`，结果写入 `Saved/SpaceDebug/planet_flight_balance_native.json`。

涉及文件：`JTSPlanetAnchor.h/.cpp`、`JTSCompressedSpaceComponent.h/.cpp`、`JTSSpacecraftFlightMovementComponent.cpp`、`JTSCelestialNavigationTypes.h`、`JTSCelestialNavigationLibrary.cpp`、`JTSSpacecraftRegressionTests.cpp`、`configure_antigravity_flight.py`、`validate_cruise_approach.py`、`validate_planet_flight_balance.py`、`L_SpaceWorld.umap`，以及本说明。

### 本轮验证结果

- `spaceEditor Win64 Development` 最终编译通过，39.61 秒，未使用 Live Coding；日志 `Saved/SpaceDebug/planet_flight_balance_final_build.log`。
- 11 项相关自动化全部通过，零错误、零警告：新增 `PlanetAdaptiveFlight`、`FragmentNavigationBounds`，以及 `CruiseApproachSlowdown`、`AntiGravityCruise`、`IndependentCompressedSpace`、`CelestialNavigation`、`TakeoffLandingCycle`、`ThirdPersonFlightControls`、`SurfaceEnvelope.CraterTakeoffClearance`、`FragmentClearance`、`FragmentAssetFlight`。记录 `planet_flight_balance_automation.json`。
- 正常游戏时间下完成 12 段原生飞行，7,666 帧采样，无错误。使用实际驾驶者服务器 RPC，不关闭 Actor Tick，不替换驾驶镜头；镜头支点最大偏差约 0.00000193 cm。每段松键后核对立即停止，转移路线的导航距离持续接近目标。

| 离开出发星球近地区域 → 进入目标近地区域 | 普通 | Shift |
| --- | ---: | ---: |
| 月球 → 地球 | 35.67 s | 19.80 s |
| 地球 → 月球 | 45.23 s | 25.12 s |

计时起点在出发星球近地边界外 10 m，终点为目标近地边界内 2 cm，不包含起降。月球→地球 Shift 较上一轮 36.51 秒明显缩短。仅从目标标记边界外 10 m、沿 +Z 来向的接近段，地球 / 月球 / 火卫二 Shift 分别为 6.20 / 6.24 / 3.91 秒；不同来向的压缩坐标不同，不将这一组当成地月完整航程。

碎块内部开阔间隙另以原生输入验证普通前进 101.50、Shift 182.69、横移 50.75、倒退 43.50、升降 67.66 m/s。每项均测量真实位移和速度，包含停车检查。截图 `PlanetFlightBalance_EarthFrame.png` 确认正常驾驶镜头下框选贴合碎块分布，保留船体与原 HUD。完整记录 `planet_flight_balance_native.json`；纯查询积分 `planet_flight_balance_estimates.json` 仅供参考。

关卡参数已通过 `antigravity_balance_configure` 保存，PIE 验证结束后停止。直接运行 `L_SpaceWorld` 即可，无需手工修改 Blueprint；复现或继续调校时使用上述配置入口。修改文件清单见上一段，`AntigravityFlight.md` 同步说明新的地球近地速度。

最后完整关闭并重新打开 Editor，确认磁盘中地球 2.41659 倍、月球/火卫二 1 倍及各自接近参数仍正确；记录 `planet_flight_balance_persistence.json`。当前 Editor 已打开 `L_SpaceWorld`，PIE 已停止。
