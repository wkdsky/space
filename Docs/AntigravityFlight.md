# 反重力速度跃迁与航程

> 当前宽范围衰减、多天体融合及火星 / 火卫一无碰撞近地见 [AdaptiveFlight.md](AdaptiveFlight.md)。下文保留旧方案记录。


2026-10-18。适用于当前 `L_SpaceWorld` 与 `BP_Twinfork_R1`。

近地范围外以 100 倍关卡运动速度为自由航行上限，进入星球标记距离后按[接近减速规则](CruiseApproach.md)衰减至近地 NAV 速度的 3 倍，进入近地后恢复近地速度。反重力引擎直接响应速度指令：按键即达到目标速度，松开所有移动键即停止，反向和横移也直接响应。Shift 仍提供推进加速。近地保留精细操纵速度，不再因重力在静止时产生下坠；球心径向仍用于地表、人物重力和飞船近地参考系。

| 关卡运动 | 普通 | Shift |
| --- | --- | --- |
| 近地前进 | 42 m/s | 75.6 m/s |
| 标记距离外的自由航行前进上限 | 4.2 km/s | 7.56 km/s |

上表近地速度为月球/小天体基准。地球碎块区域现按完整几何尺度适配，普通/Shift 前进约为 101.5 / 182.7 m/s，详细参数、框选及近地范围修正见 [地月航行适配](CruiseApproach.md#地月近距离航行与碎块区域适配)。

这些是压缩关卡中的运动速度。HUD 的 NAV 速度仍由共同导航坐标映射的方向导数计算，保持与导航距离的实际推进一致，不能把表中关卡速度直接当成星际导航速度。科幻巡航不采用真实航天器的惯性和长时间加减速模型。

职责与依赖保持在既有组件中：`FlightMovementComponent` 处理服务器速度指令；`CompressedSpaceComponent` 查询每帧线段首次进入的玩法星球泡泡。后者截断本帧的高速位移，在边界内 2 cm 处切换近地速度，避免低帧率或高速穿越整个泡泡。它不增加 Tick，也不创建玩法星球。参数由飞船 Blueprint 配置，天体坐标和光照由关卡配置。

重新调整压缩布局：地月中心距为 25 km，月球至火卫二约 205 km，地球至火卫二约 218 km。导航中的天文距离保持原配置。增加接近减速前，纯自由航行段预计地月约 3–6 秒、月球与火卫二约 27–49 秒（Shift / 普通）；这些历史航程不包含新减速区、起飞和近地操纵。火星及两卫星内部的相对偏移保持，以保留从火卫二看到火星的角度。地月/火星系统航线均避开火星背景模型。

## 火卫二光照

[NASA](https://science.nasa.gov/mars/moons/deimos/) 描述火卫二具有暗色表面材质；[火星参数表](https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html)给出火星轨道平均太阳辐照约为地球的 43.1%。暗材质不意味着阳面没有光。火星反照光可由太阳照射、火星反照率、角大小及相位估算，不能代替太阳直射。

当前默认落点表面法线与太阳来光方向点积约为 -0.61，属于背阳面。原关卡的太阳间接光和 SkyLight 强度均为零，火星也没有提供反照光，因此此处的表面接近全黑。这是当前静态落点及场景光照配置的结果，不能推导为整个火卫二永远漆黑。

关卡增加一个朝向火卫二、来自火星方向的局部暖色 SpotLight，模拟火星反照光。它覆盖当前火卫二近地区域、投射阴影、无 Tick，不改变全局曝光或把夜面整体照成白昼。为保证夜面地形和飞船可读性，强度由美术放大，属于游戏表现近似，并非严格的光度重建。未增加碰撞或背景天体玩法。

## 修改与复现

- `Source/space/Components/JTSSpacecraftFlightMovementComponent.h/.cpp`：可配置巡航倍率、即时反重力响应和边界速度切换。
- `Source/space/Components/JTSCompressedSpaceComponent.h/.cpp`：高速线段的首次泡泡进入查询。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：速度跃迁、松键停止、反向、推进加速、长帧入泡泡回归。
- `Tools/Editor/configure_independent_space.py`：压缩位置和原生飞行验证。
- `Tools/Editor/configure_antigravity_flight.py`、`configure_space_debug.py`：MCP 关卡及 Blueprint 配置入口。
- `Content/Space/Ships/TwinforkR1/BP_Twinfork_R1.uasset`、`Content/Space/Maps/L_SpaceWorld.umap`：保存后的具体配置。

已通过 Unreal MCP 在停止 PIE 后执行 `SpaceDebugAuthoringTools.run(operation="antigravity_configure")` 并保存。需要重做配置时仍使用此入口，它先更新布局，再配置飞船及火星反照光。通常直接运行关卡即可。

## 验证记录

- `spaceEditor Win64 Development` 项目编译成功，32.04 秒，未使用 Live Coding。日志：`Saved/SpaceDebug/antigravity_build.log`。
- 六项自动化回归全部通过，零错误、零警告：`AntiGravityCruise`、`IndependentCompressedSpace`、`MarIILandingMap`、`SixAxisInertialFlight`、`TakeoffLandingCycle`、`ThirdPersonFlightControls`。记录：`Saved/SpaceDebug/antigravity_automation.json`。
- PIE 使用生产飞行组件与服务器输入 RPC，在 1 倍时间流速下实际飞行；飞行段没有改写飞船位置。推进加速航程实测：火卫二 → 月球 27.15 秒、月球 → 火卫二 27.11 秒、火卫二 → 地球 28.65 秒、地球 → 月球 3.23 秒。这些是离开源近地区域至进入目标近地边界的航程，不包含起降。574 个采样点无距离倒退、目标缩小或可见性异常。记录：`Saved/SpaceDebug/antigravity_native.json`。
- 月球与火卫二均完成双玩家镜头验证：55° / 150° FOV 下同距离同时显示标记，乘客转开再转回正常，超出固定距离时即使相机靠近也不显示，进入近地范围后均清除。记录：`Saved/SpaceDebug/independent_markers_e2e.json`。
- 同位置、同曝光的火卫二夜面补光对比：`Saved/SpaceDebug/Antigravity_Deimos_Light_off.png` 与 `Antigravity_Deimos_Light_on.png`。关闭时地面接近全黑，开启后可辨认暖色地形，天空和火星亮度保持一致。截图使用临时观察相机，不保存或修改玩家相机。
- 完整重启编辑器后再次运行关卡，确认默认人物和飞船仍在火卫二地表、巡航倍率 100、即时响应开启、静止速度为零，暖色反照光已保存。记录：`Saved/SpaceDebug/antigravity_persistence.json` 与 `Antigravity_Deimos_Saved.png`。PIE 已停止，验证期间的临时相机、镜头和玩家状态均已清除。

## 高速镜头跟随修复

原飞船 SpringArm 开启了位置延迟，追赶速度为 14，且最大滞后距离未限制。100 倍巡航后，这种世界空间位置平滑会使镜头支点落后飞船数百米，飞船在画面中变小或离开画面。此前的航程探针暂停了 Actor Tick，因此只能验证航程，不能作为正常驾驶镜头的验证。

`JTSSpacecraftActor.cpp` 在构造及激活驾驶镜头时均关闭 SpringArm 位置延迟，镜头支点随船体平移，兼容旧 Blueprint 中保存的位置延迟值。转动平滑、观察角度、滚轮缩放和原有切换视效继续使用现有逻辑。生产 `BP_Twinfork_R1` 同步保存关闭位置延迟的配置。

`AntiGravityCruise` 回归新增镜头实际支点检查，覆盖近地、瞬间进入巡航、推进加速、急停、反向及长帧进入目标泡泡。MCP 验证入口 `antigravity_camera_input_*` 通过驾驶者的服务器 RPC 持续提交飞行输入，保留飞船 Actor Tick 与组件 Tick，不创建观察相机、不改写 ViewTarget。

本次 `spaceEditor Win64 Development` 编译成功（19.35 秒）；`AntiGravityCruise`、`InterplanetaryCruise`、`TakeoffLandingCycle`、`ThirdPersonFlightControls` 四项全部通过，零错误、零警告。两名本地玩家的 PIE 实测包含 233 个采样点，最高关卡速度 7.56 km/s，镜头支点与船体最大误差约 0.0000033 cm。驾驶镜头与飞船的最大距离 18.45 m（原有 18 m 摇臂及偏移），乘客镜头保持随其附着人物移动；两者均没有切换到独立观察相机。覆盖两次出泡泡、一次入泡泡、巡航推进加速、横移、反向及各阶段停车。记录：`Saved/SpaceDebug/camera_follow_build.log`、`camera_follow_automation.json`、`camera_follow_e2e.json`、`CameraFollow_Cruise.png`、`CameraFollow_Stopped.png`。这项验证检查乘客的位置跟随，不评估其已有机身遮挡和独立观察布局。

Editor 配置已通过 MCP 执行 `antigravity_camera_configure` 保存到 `BP_Twinfork_R1`。直接运行当前 `L_SpaceWorld` 即可，无需手动修改镜头。此次没有调整天体布局、导航距离、速度或光照参数。

## 刚起飞时的方向控制

起飞净空阶段原先将整条速度向量替换成垂直抬升，直到整个船体超过周围最高坑沿。因此驾驶者的 W/S/A/D 输入虽然已经到达服务器，仍会在移动组件中被覆盖。即时反重力模式又在每帧先应用目标速度，使自动抬升的加速重新开始，延长了这段等待。

`JTSSpacecraftFlightMovementComponent.cpp` 现在保留驾驶者的切向速度，将自动抬升叠加到径向分量，并在反重力模式下直接使用起飞抬升速度。起飞期间仍预测实际行进方向的地形，执行包络修正及完整船体碰撞检查；坑壁实际挡住横向路径时，另行检查向上脱离路径，避免连抬升也被斜向碰撞阻断。自动抬升完成之前无需等待解锁方向键。

`CraterTakeoffClearance` 回归从相同低空姿态检查四个方向的首帧响应，覆盖普通加速与即时反重力两种模式，并检查松键停止横向运动、船体净空及越过坑沿。MCP 入口 `antigravity_camera_takeoff_forward/reverse/left/right` 在同一次调用中登船、开始起飞并提交驾驶者 RPC 输入，用于检查实际关卡的初始飞行帧；飞行过程中不改写船体位置或暂停 Actor Tick。

`JTSSpacecraftActor::CanTraverseLandingSegment` 同时校验路径终点的完整船体姿态，避免 Sweep 接触容差允许的终点被静态重叠检查判为穿入。该检查复用现有净空查询，不增加 Tick 或新的玩法系统。

本次修改文件：

- `Source/space/Components/JTSSpacecraftFlightMovementComponent.cpp`：起飞方向输入、即时抬升、贴壁脱离。
- `Source/space/Ships/JTSSpacecraftActor.cpp`：完整船体的路径终点净空检查。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：四方向首帧、松键停止和深坑脱离回归。
- `Tools/Editor/configure_antigravity_flight.py`：MCP 起飞输入与镜头观测入口。
- `Docs/AntigravityFlight.md`：原因、实现和验证记录。实际关卡验证驱动保存在 `Saved/SpaceDebug/validate_takeoff_input.py`。

最终 `spaceEditor Win64 Development` 编译成功（29.93 秒），未使用 Live Coding。九项自动化回归全部通过，零错误、零警告：六项 `SurfaceEnvelope` 回归、`TakeoffLandingCycle`、`AntiGravityCruise`、`ThirdPersonFlightControls`。月球及火卫二实际 PIE 起飞各验证前进、后退、左移、右移，共八个场景全部通过；提交输入后约 0.027–0.042 秒内即检测到对应方向位移，松键后横向停止，飞船 Actor Tick 保持启用，镜头支点跟随误差小于 0.00001 cm。记录：`Saved/SpaceDebug/takeoff_input_build.log`、`takeoff_input_automation.json`、`takeoff_input_e2e.json`。

Editor 无需手动配置；已重新打开当前关卡并停止验证 PIE，直接运行 `L_SpaceWorld` 即可。
