# 反重力速度跃迁与航程

2026-10-18。适用于当前 `L_SpaceWorld` 与 `BP_Twinfork_R1`。

近地范围外立即切换为 100 倍的关卡运动速度。反重力引擎直接响应速度指令：按键即达到目标速度，松开所有移动键即停止，反向和横移也直接响应。Shift 仍在巡航倍率之上提供推进加速。近地保留精细操纵速度，不再因重力在静止时产生下坠；球心径向仍用于地表、人物重力和飞船近地参考系。

| 关卡运动 | 普通 | Shift |
| --- | --- | --- |
| 近地前进 | 42 m/s | 75.6 m/s |
| 自由航行前进 | 4.2 km/s | 7.56 km/s |

这些是压缩关卡中的运动速度。HUD 的 NAV 速度仍由共同导航坐标映射的方向导数计算，保持与导航距离的实际推进一致，不能把表中关卡速度直接当成星际导航速度。科幻巡航不采用真实航天器的惯性和长时间加减速模型。

职责与依赖保持在既有组件中：`FlightMovementComponent` 处理服务器速度指令；`CompressedSpaceComponent` 查询每帧线段首次进入的玩法星球泡泡。后者截断本帧的高速位移，在边界内 2 cm 处切换近地速度，避免低帧率或高速穿越整个泡泡。它不增加 Tick，也不创建玩法星球。参数由飞船 Blueprint 配置，天体坐标和光照由关卡配置。

重新调整压缩布局：地月中心距为 25 km，月球至火卫二约 205 km，地球至火卫二约 218 km。导航中的天文距离保持原配置。纯自由航行段预计地月约 3–6 秒、月球与火卫二约 27–49 秒（Shift / 普通）；起飞、近地操纵、停车另计。火星及两卫星内部的相对偏移保持，以保留从火卫二看到火星的角度。地月/火星系统航线均避开火星背景模型。

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
