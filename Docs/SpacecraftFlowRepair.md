# 飞船关卡配置修复

## 修改文件

- `Content/Space/Maps/L_EarthLaunchPrototype.umap`
- `Content/Space/Ships/TwinforkR1/BP_Twinfork_R1.uasset`
- `Content/Space/Blueprints/Ships/BP_Spacecraft.uasset`
- `Source/space/Components/JTSSpacecraftFlightMovementComponent.cpp`
- `Source/space/Components/JTSSpacecraftPresentationComponent.cpp`
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`
- `Tools/Editor/spacecraft_flow_mcp.py`
- `Tools/Editor/repair_spacecraft_flow.py`
- `Tools/Art/inspect_spacecraft_mounts.py`
- 本文档。

## 原因与修改

- 地球发射关卡仍使用旧 `BP_Spacecraft`，没有起落架接触点。发射会将它的类写入 Expedition snapshot，太空关卡优先恢复该类，绕过配置完整的 Twinfork。
- 地球关卡改为 `BP_Twinfork_R1`，以四个展开脚垫的高度放置，地面 Z=0、船根 Z=192 cm；出生点移到船侧，避免落入船体。
- 旧 `BP_Spacecraft` 保留资产路径并继承 `BP_Twinfork_R1`，同步原生组件和四脚配置，使旧 snapshot 的类引用仍能恢复完整飞船。
- Twinfork 船体及附属碰撞不再允许 Character Step Up，阻止步行直接踏上船体。
- Presentation 在尚未进入 SpaceWorld 的发射阶段保持支腿展开，进入太空后继续按飞行/降落状态收放。
- 火星等大星球的速度倍率曾使正常下降超过 `MaxCaptureDescentSpeed`，在地表边界反复返回 `TooFast`。地表跟随阶段的下降现在先限制到安全接管速度；侧向速度检查、支脚验证及障碍检查继续生效。
- 前照灯沿用驾驶员服务端验证和复制状态。两盏灯随完整船体组件挂接；菜单到地球、太空的实际输入链路另行验证。

## Editor 操作

通过 Unreal MCP 运行 `Tools/Editor/spacecraft_flow_mcp.py` 注册的工具完成编辑。
`run(step="apply")` 应用蓝图与地球关卡配置；`inspect` 检查关卡及兼容蓝图。
无需手工重新挂接灯光或更换关卡实例。

## 验证

报告保存在 `Saved/SpacecraftFlow/`。`spaceEditor Win64 Development` 正常编译通过。
六项自动化测试通过，无警告或错误：PreflightGear、LandedHeadlights、AutomaticLandingControls、TakeoffLandingCycle、MarsLandingMap、PossessionAndDisembark。
`menu_route.json` 记录菜单到火星并恢复驾驶权的完整流程；`descent.json` 记录真实火星上约 2.67 秒完成自动接管及四脚落地。
`hotkeys.json` 记录实际 L 关闭/开启前照灯、R 下船后保留照明。`earth_validation.json` 记录脚垫高度全部为 0 cm，胶囊从船侧横移时被船体挡住，所有有效船体碰撞禁止踏上。
`blender_mounts.json` 是通过 live Blender MCP 对船体原始网格和灯罩挂接坐标的检查。
PIE 测试使用正常 Session、大厅开始、地球登船/发射和跨关卡恢复流程，测试中补充燃料并提前结束收集计时。
用户原有 SaveGames 在验证前备份，验证结束后恢复。
