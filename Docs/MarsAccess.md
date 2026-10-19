# 火卫一着陆与火星外部干预

多天体共同可见及系统内转场的比例修正见 [CelestialCoherence.md](CelestialCoherence.md)。

配置保存在 `L_SpaceWorld`，继续使用同一个长期存在的 SpaceWorld。

## 火卫一

- 保留原模型、位置和天文坐标。火卫一最大外径由约 60 m 调到约 180 m（线性尺寸 3 倍），仍是紧凑的小天体，为飞船落脚及少量剧情活动留出空间。配置脚本按目标半径 9000 cm 设置绝对尺寸，重复执行不会继续放大。
- `SM_MarI` 启用 Complex As Simple 网格碰撞与双面几何，地面查询依据实际表面。
- 新增 `MarI_PlanetAnchor`，绑定原 `MarI_Background` 网格，接入现有星球注册、径向重力、地表包络和着陆流程。压缩空间条目改为引用 Anchor，删除原背景成员身份，避免重复控制表现。
- GameMode Blueprint 配置 `BP_PhobosSurfaceController`，复用被动地表控制器，不生成月球资源或生物。
- 地表凹凸仍受四脚支撑、坡度、脚垫粗糙度和悬架行程约束。没有通过放宽全局规则伪造着陆成功。
- 火卫一 Anchor 的 `Show On Star Chart`、`Show Screen Marker` 均关闭：不出现星图联系点、名称或屏幕标记；进入近地范围后只保留通用 `LOCAL / NEAR SURFACE` 状态，不将名称补回导航。实体仍参与碰撞、径向重力、着陆及遮挡。

## 火星拦截剧情

目的：在飞船即将进入火星近地状态时，让玩家感受到外部智能介入姿态和航线控制。

职责：新增 `JTSSpacecraftInterferenceComponent` 管理有限时长的接管阶段；现有飞行组件负责移动、完整船体碰撞和实际速度；HUD 和已有镜头后处理负责提示。组件没有独立 Tick，由已有服务端飞行更新调用。

依赖：关卡 `SpaceWorldManager.CompressedSpace.Bodies[].Interference` 配置、该天体已有近地边界、飞船飞行组件。C++ 不识别 Mars 标签，不引用具体关卡 Actor 或资产路径。

| 阶段 | 当前配置 | 表现 |
| --- | --- | --- |
| 触发 | 近地球壳外 180 m | 整段路径检测，提前截住高速或长帧飞行 |
| 制动 | 0.65 s | 平滑衰减入向速度，制动距离受安全余量限制 |
| 转向 | 2.8 s | 原地转向、姿态缓入缓出，朝径向外侧转向，小幅滚转后回正 |
| 外推 | 3.0 s / 160 m | 外部力驱动，平滑加减速滑行，主引擎保持怠速 |
| 恢复 | 0.55 s | 信号衰减提示，停车后归还输入 |

- 正面接近约调头 180°；侧飞、倒飞接近也确保船头最终指向远离火星的方向。
- 保留惯性参考姿态，不使用固定 World-Z；保留现有镜头，不切换 ViewTarget。
- 接管期间服务端拒绝驾驶输入和着陆请求；客户端收到复制的剧情阶段与飞船真实运动。`OnPhaseChanged` 可供 Blueprint 接入进一步音效、灯光或特效。
- HUD 显示未知场、外部姿态指令、强制离开和恢复操控。修正旧导航面板隐藏剧情警告的问题。
- 每次靠近都保持封锁，没有可突破的冷却窗口；离开和外侧掠过不触发。
- 结束时清空旧输入和残留速度，新驾驶输入正常生效；目标销毁时也释放控制。

设计参考：《Elite Dangerous》[外星防御场与接近限制](https://www.elitedangerous.com/news/galnet/assault-taranis)。制动、原地调头、滑行及归还操控曲线为本项目的具体实现。

## Editor 操作

已应用并保存，无需手动补装碰撞或组件。打开 `L_SpaceWorld` 直接运行。

调节剧情：选择 `SpaceWorldManager` → `CompressedSpace` → `Bodies` → 火星条目 → `Interference`。关闭 `Enabled` 可暂时解除封锁。距离使用关卡物理厘米，与 HUD 天文导航距离不同。

调节火卫一：选择 `MarI_PlanetAnchor`，`Planet | Navigation` 中控制星图和屏幕标记。当前两项均关闭。尺寸由 `configure_mars_access.py` 的 `PHOBOS_GAMEPLAY_RADIUS_CM` 配置，并同步真实网格和 Anchor 半径。

复现配置：停止 PIE，经项目 Unreal MCP 调用 `SpaceDebugAuthoringTools.run(operation="mars_access_configure")`。该入口只更新相关关卡实例、网格碰撞、被动控制器 Blueprint 和 GameMode 配置，不重建场景。

## 验证

- `spaceEditor Win64 Development` 编译成功，无新增编译警告；关闭编辑器编译，不使用 Live Coding。
- 12 项相关回归通过，零错误、零警告：外部接管、自适应速度场、反重力巡航、起降循环、火卫二关卡着陆、第三人称操控及六项地表包络。报告：`Saved/SpaceDebug/MarsAccessAutomation/index.json`。
- `ExternalFlightInterference` 覆盖长帧跨越、静止转向、安全余量、输入覆盖、归还操控、重复接近和外侧掠过。
- `Saved/SpaceDebug/phobos_landing_native.json`：180 m 外径下，96 个覆盖整个球面的地表采样中有 20 个位置能通过完整飞船支撑检查（各位置最多测试 12 个朝向）。随后用服务器驾驶 RPC 正常下降，四脚接触后完成着陆。
- `Saved/SpaceDebug/mars_interference_native.json`：实际关卡、持续提交推进加速输入，完整经过制动、转向、外推和恢复。285 个轨迹样本，最终朝外点积 1.0、速度归零，全程未进入近地球壳；HUD 剧情提示已在编辑器画面中确认。
- `Saved/SpaceDebug/phobos_exit_takeoff.json`：真实地表安全离船、重新登船及正常起飞均成功。
- 本次尺寸与导航调整：`spaceEditor Win64 Development` 编译成功；导航、外部接管、自适应速度场三项回归通过，零错误、零警告，报告 `Saved/SpaceDebug/PhobosRevisionAutomation/index.json`。
- `Saved/SpaceDebug/phobos_hidden_navigation.json`：实际关卡导航快照中火卫一星图和屏幕标记开关均关闭、标记不具备显示资格、实体碰撞保留；其他已有星图目标仍正常列出。近地 HUD 保留不带天体名称的真实高度读数。
- 当前实机验证使用单个 PIE 实例；多人网络延迟和多客户端画面尚未实测。接管判定、运动及输入屏蔽都由服务器执行，阶段通过组件复制，位置沿用现有飞船移动复制。

## 本次修改文件

- `Source/space/Components/JTSSpacecraftInterferenceComponent.h`、`.cpp`：新增可配置的外部接管组件。
- `Source/space/Components/JTSCompressedSpaceComponent.h`：天体条目增加接管配置。
- `Source/space/Components/JTSSpacecraftFlightMovementComponent.h`、`.cpp`：路径拦截及复用船体碰撞的接管移动。
- `Source/space/Ships/JTSSpacecraftActor.h`、`.cpp`：组件装配、服务端输入限制和镜头/推进表现。
- `Source/space/UI/JTSPrototypeHUDWidget.cpp`：分阶段接管提示。
- `Source/space/World/JTSPlanetAnchor.h`、`Source/space/UI/JTSCelestialNavigationLibrary.cpp`：星球导航可见性配置及快照传递；HUD 近地模式也遵守该配置。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：接管回归。
- `Content/Space/Maps/L_SpaceWorld.umap`：火卫一 Anchor 和火星接管参数。
- `Content/Space/Environment/MarsSystem/MarI/SM_MarI.uasset`：真实表面碰撞。
- `Content/Space/Blueprints/Planets/Phobos/BP_PhobosSurfaceController.uasset`：火卫一被动地表配置。
- `Content/Space/Blueprints/Modes/BP_SpaceWorldGameMode.uasset`：注册火卫一地表控制器。
- `Tools/Editor/configure_mars_access.py`、`Tools/Editor/configure_space_debug.py`：定向配置与实机验证入口。
- `Docs/MarsAccess.md`、`Docs/AdaptiveFlight.md`：配置、验证报告及原说明的更新入口。
