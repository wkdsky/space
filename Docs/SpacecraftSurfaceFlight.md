# 飞船地形包络、贴地飞行与起落

近地飞行使用真实星球 Mesh 外的一层平滑安全包络。普通飞行保持在包络上方；向下运动将穿过包络下沿时，服务器才检查四脚支撑并接管着陆。不能着陆就把下降限制在包络边界。没有固定准备高度，也没有等待支架展开后才开始下降的阶段。

## 设计依据与职责

[No Man’s Sky 的 Atlas Rises 官方说明](https://www.nomanssky.com/atlas-rises-update/)介绍了低空飞行辅助，让飞船能够在地表上方巡航。[Star Citizen 的官方 IFCS 设计](https://robertsspaceindustries.com/en/comm-link/engineering/13951-Flight-Model-And-Input-Controls)将飞行意图与执行控制区分，并使用辅助控制处理减速和反向推力。[Elite Dangerous 官方 Horizons 指南](https://d1wv0x2frmpnh.cloudfront.net/elite/website/assets/English-PlayersGuide_v2.00-Horizons.pdf)要求地面能够支撑整个起落架占地范围。这里采用这些设计目标；以下包络算法是本项目的实现，未声称复现这些游戏的内部算法。

目的：高速通过起伏地形时保持安全距离与连续姿态，并以实际包络越界区分飞行和着陆。

职责与依赖：

- `SurfaceEnvelopeComponent` 无 Tick、无输入、无移动。依赖 `PlanetAnchor` 的真实 Mesh 径向查询，以及飞船实际尺寸，生成包络位置、切面法线和跟随状态。服务器计算，并复制状态供驾驶客户端使用。
- `FlightMovementComponent` 负责沿包络运动、姿态、向下越界尝试及起落动作。
- `LandingSupportComponent` 负责真实足垫、支架行程、稳定性、障碍与有界落点修正。没有 LandingSite 授权。
- `PresentationComponent` 负责支架展开、回收和足垫接地表现；`SpacecraftActor` 负责驾驶者 RPC 权限、输入锁和状态转换。

Blueprint 配置参数及支架资产；C++ 没有硬编码月球坐标、关卡 Actor 或飞船资产路径。初始停放仍由开发者拖动 `BP_JTSPlanetArrivalAnchor` / `SpacecraftArrivalPoint` 确定。

## 安全包络

以真实星球中心为基准，在球面邻域采样实际地形：9×9 周围网格、8 个密集机身占地样本，以及沿当前运动方向的三条密集预测走廊。前进、后退、侧移都使用当前实际速度建立走廊。邻域随速度扩展；采样覆盖周围而非只检查船头。普通飞行最多 209 条地形射线每次刷新，单艘共享飞船默认 0.1 秒刷新一次。起飞净空阶段使用最大配置半径，增加 17×17 远处网格并保留局部网格，静止时最多 459 条射线；坑底静止时也能提前检查周围坑沿。

真实地表若包含多个碰撞组件，`PlanetAnchor` 会比较所有有效命中，选择从外侧射入时最近的表面，避免先命中坑底组件后遗漏同一射线上的坑壁。

对地形样本半径 `rᵢ`，使用带圆角的山肩：

```text
dᵢ = max(球面弧长 - 机身占地半径, 0)
hᵢ = rᵢ - tan(14°) × (sqrt(dᵢ² + 圆角距离²) - 圆角距离)
包络半径 = 平滑上包络(hᵢ) + 完整机身净空 + 平地间距 + 起伏余量
```

平滑上包络使用稳定的 log-sum-exp，始终不低于它包含的最高样本。直接平均地形会切进山峰，此处最高点由圆角山肩向前、向后和侧面延伸。其梯度给出连续切面法线，限制山肩相对径向的倾斜程度。起伏余量根据附近 Mesh 偏离局部地面平面的程度增加。飞行包络按沿切面放平的机身计算，防止当前俯仰变化反过来抽动包络高度。

| Twinfork 参数 | 保存值 |
| --- | --- |
| 平地间距 | 100 cm，另含已有机身净空 |
| 正常跟随偏移 | 包络上方 20 cm |
| 山肩最大坡度 | 14° |
| 山肩圆角距离 | 600 cm |
| 平滑上包络宽度 | 5 cm |
| 邻域半径 | 36–120 m，随速度扩展 |
| 预测时间 | 1.8 s |
| 起伏额外间距上限 | 4 m |
| 查询刷新间隔 | 0.1 s |
| 跟随进入/主动上升释放高度 | 包络上方 3 m / 7 m |
| 支架展开 | 0.65 s，与下降同时进行 |
| 有界水平修正速度 | 3.5 m/s |
| 选点修正/偏航范围 | 2 m / ±8° |

近地只使用包络切面这一种姿态目标。相机可继续观察地面，鼠标仅把投影到切面上的方向用于偏航；相机俯仰、键盘俯仰和翻滚不会与贴地姿态竞争。越过山肩、地面暂时下降不会解除跟随。主动 Space 上升到释放高度才回到自由姿态；远离真实地面或失去当前星球时也释放。自由飞行保持现有六轴操作。

沿切面保留巡航速度，使用预测位置检查下一步包络净空，并使用适应帧长的高度响应回到跟随偏移。正常回落可使用飞船升降能力；已低于包络的恢复先消除向下速度，再限制向外恢复速度。重力和上下方向始终来自星球径向，不使用固定 World-Z。

中心地面射线经过裂口而失效时，已建立的包络继续使用周围真实地形样本延续高度保护。着陆查询仍要求真实地面及四脚支撑，不能用这层延续的包络授权着陆；整个采样邻域都失去地形时结束跟随。

完整机身与飞行碰撞代理都执行扫掠/旋转净空检查。射线预算有限，对未被采样捕获的极小尖峰、独立岩石和其他障碍，完整机身扫掠会限制运动；不会承诺任意细小障碍或任意尺寸山脉都能保持最高速度。包络不是可见的新 Mesh，也不更改地形碰撞。

## 环形坑起飞与碰撞脱困

地面起飞在服务器上先完成垂直净空阶段：目标半径为当前采样邻域最高真实地形半径，加完整船体净空、平地间距、起伏余量和跟随偏移，并且阶段中只允许提高目标。短按 Space 后即使松开，也会完成抬升；W/A/S/D、Shift 和下降意图不会提前带飞船冲向坑壁。达到目标后恢复普通包络飞行，继续按 Space 则正常爬升。着陆中断仍使用原有脱离行为。

已重叠的飞船恢复时固定姿态，每步限制为不超过 25 cm 且受恢复速度约束。碰撞代理和完整船体分别检查：移动必须沿每个现有重叠的脱离法线，途中和终点不能增加穿入深度，同时扫掠检查其他障碍。船头插进垂直坑壁时可以按 S 后退；向墙内继续推进、后方新障碍或同时顶住另一侧墙壁仍会阻止移动。恢复不依赖中心地面射线，也不限于径向向上。

新增回归覆盖倾斜径向的高坑沿、短按 Space 后立即前进/侧移/加速、实际月球 Mesh 与 Twinfork、强制船头重叠后的后退，以及后方障碍拒绝。当前修复验证见 [环形坑回归记录](Validation/spacecraft_crater_recovery_tests.json)。

本次 `spaceEditor Win64 Development` 编译成功；22 项飞船回归全部通过（21 项普通成功，1 项原有的预期着陆取消警告）。实际月球坑内起飞后前进超过 60 m，没有船体重叠；强制船头穿入同一真实坑壁后，后退约 64.6 m 并恢复无重叠飞行。本次为服务器侧真实物理查询自动化验证，尚未做多客户端联机复测。

## 着陆与中断

1. Ctrl 先作为正常下降意图使用。在包络上方不会按旧的固定 12 m 高度开始自动着陆。
2. 服务器预测本次向下运动会穿过包络时，检查真实地面、四个足垫完整平面、独立支架行程、机身净空、支撑多边形与重心。预测检查发生在提交越界移动前，避免无效落点已经钻到包络下面。正常跟随山肩抬升不作为着陆意图。
3. 支撑无效或速度过高，保持 Flying 并限制下降在边界；HUD 显示实际失败原因。成功则固定一个合法目标，最大修正 2 m / ±8°，立即展开支架，同时平移、调姿和下降。
4. 客户端 W/A/S/D、俯仰、偏航、翻滚、加速输入全部锁住；服务器也拒绝这些 RPC 意图。相机观察仍可用。只有 Space 请求中断。支架展开完毕前保留最后 20 cm 接地余量。
5. 每 0.2 秒复检目标支撑，移动时检查完整机身扫掠。四脚真实接地和机身姿态通过最终检查后才进入 Landed。不能用根节点高度代替四脚支撑。
6. Space 中断后恢复 Flying，并允许从包络下方向外脱离；真实 Ctrl 按键状态保留，必须松开后才能重新接管。起飞也使用脱离保护。

高山包络的准入高度由包络决定，自动着陆不再叠加旧的固定高度门槛。手动兼容查询仍保留原参数。足垫及稳定性配置详见 [真实地形着陆](SpacecraftTerrainLanding.md)。

## Editor 与验证

参数已保存到 `BP_Twinfork_R1`，Equipped 派生 Blueprint 继承。无需迁移关卡或调整初始停放位置。调整步骤：

1. 打开 `BP_Twinfork_R1`，选 `SurfaceEnvelopeComponent → Settings` 调整间距、山肩、预测和跟随参数。
2. `FlightMovementComponent → Flight / Landing` 调整下降、修正和接地速度；`PresentationComponent → GearDeploymentDuration` 调整展开时间。
3. PIE 中先 Space 起飞，W/A/S/D 与 Shift 测试贴地巡航；松开水平推力降低速度，再按 Ctrl 尝试穿过包络。着陆中测试 W/S 无效与 Space 中断。

配置脚本：[configure_surface_envelope.py](../Tools/Validation/configure_surface_envelope.py)。实际月球 PIE 验证脚本：[spacecraft_surface_pie.py](../Tools/Validation/spacecraft_surface_pie.py)。脚本通过普通驾驶者 RPC 提交意图，仅暂停 Actor 的相机/输入心跳 Tick；移动组件、支架组件、物理和星球查询正常运行。原生自动化单独验证真实相机耦合与客户端输入锁。

验证记录：[参数](Validation/spacecraft_surface_configuration.json)、[自动化](Validation/spacecraft_surface_tests.json)、[真实月球 PIE](Validation/spacecraft_surface_pie.json)、[Blueprint 编译](Validation/spacecraft_landing_blueprints.json)、[关卡与初始锚点](Validation/spacecraft_landing_editor.json)。此轮为 server/standalone 验证，尚未运行多客户端网络集成。

UE 5.8.2 的 `spaceEditor Win64 Development` 编译成功。19 项 `JTS.Spacecraft` 测试通过（18 项普通成功，1 项因主动移除地面的预期取消日志带 warning），无失败；四个相关 Blueprint 以 warnings-as-errors 编译成功。真实平面 Mesh 的船体额外净空为 110.44 cm；相机持续向下及持续俯仰输入均没有让贴地姿态往返切换，高速山脊测试保持安全推进。

最终实际月球 PIE 达到 75.4 m/s，巡航全程保持包络跟随，经过约 166 m，最小船体离地净空约 3.98 m（起伏月球地形）。进入着陆即下降和展开支架；服务器拒绝飞行轴及加速输入；Space 中断后持续按 Ctrl 不会重新接管。再次着陆约 1.95 秒完成，最终四脚验证有效，可见足垫接地点误差均小于 0.001 cm。低帧率遥测按移动后的真实位置重新求包络，避免把上一帧的位置误当作当前边界。

关卡最终无未保存内容，PIE 已停止；Moon 与 MarII ArrivalAnchor 的位置、旋转及子飞船出生点保持原值，旧 LandingSite 为 0，已有 9 个蚁巢保留。

## 修改文件

- 新增 `Components/JTSSpacecraftSurfaceEnvelopeComponent.h/.cpp`。
- 更新 `Components/JTSSpacecraftFlightMovementComponent.h/.cpp`、`JTSSpacecraftLandingSupportComponent.h/.cpp`、`JTSSpacecraftPresentationComponent.h/.cpp`。
- 更新 `Ships/JTSSpacecraftActor.h/.cpp`、`Core/JTSExpeditionTypes.h`、`UI/JTSPrototypeHUDWidget.cpp`。
- 更新 `Tests/JTSSpacecraftRegressionTests.cpp`、`JTSSpacecraftLandingSupportTests.cpp`。
- 配置 `Content/Space/Ships/TwinforkR1/BP_Twinfork_R1.uasset`；新增两个验证脚本，更新旧 `spacecraft_landing_pie.py` 入口，新增本文件/验证 JSON。
