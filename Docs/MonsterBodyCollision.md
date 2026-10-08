# 聚居地怪物身体防重叠

## 接入与范围

现有月球立方体怪物由 `JTSPlanetEnemySubsystem` 中的 Mass 实体统一移动，`JTSPlanetEnemySettlement` 负责生成和数量补充。身体是简化 Box 查询体，模型和弱点不参与身体分离；原生身体忽略其他 WorldDynamic 怪物。过去的方向软避让不保证站立或完全同点时分离；黑洞运动还用弹簧接触加速度处理重叠。

现在仍使用原来的服务器 World Tick 模拟阶段：移动意图/外力积分 → 地表及环境扫掠得到预期位置 → 身体分离迭代 → 一次提交最终 Actor 位置 → 原有攻击判定。没有新增固定时钟、独立怪物 Tick、寻路、跟随、队形或选敌规则。原来的方向软避让保留；重复的怪物接触弹簧加速度移除，黑洞自身的引力和斥力继续使用原有积分。

本次接入现有 Mass 聚居地怪物（当前立方体怪物）。月球蚂蚁是另一个资源活动 Actor 的生命周期，未修改其独立移动路径；本次不提供自由三维飞行怪物的球体求解。

## 求解

- 每个身体保存服务器生命周期内单调唯一的 ID、位置、参与状态、半径、高度、逆质量及层。默认逆质量 1，静止与攻击不退出占用集合，零逆质量只接收自身既有移动意图，不接受分离修正。
- 使用星球分组的二维切平面投影空间哈希，真实碰撞测试仍在两者共同的径向水平平面进行。投影压缩距离；邻格范围用双方半径、skin 和高度的上界计算，格子小于直径时自动扩大查询，不固定只查一圈。高度与层检查排除上下层、不同层和不同星球；远离同一地表半球的投影候选会排除。
- 只处理较小 ID → 较大 ID 的组合。同点时以双方 ID 的哈希生成确定性水平法线，交换 ID 时方向反转。逆质量按比例分担重叠深度，双零权重保留可诊断重叠。
- 每轮读取同一份完整位置快照，累计所有修正，统一施加后重新建网格。每只身体跨全部迭代的修正路径受 `min(MaxCorrectionSpeed × DeltaSeconds, MaxCorrectionPerStep)` 限制；不把修正写入持续加速度或下一帧主动移动速度。
- 修正先探测真实 mesh 地表，再用根查询形状及现有响应过滤扫掠墙体、玩家等环境。注册怪物互相从此扫掠中过滤，避免引擎与自定义求解重复推开。简单形状直接扫掠，其他根组件保留组件扫掠。环境约束后的实际位置参与下一轮；没有空间时不瞬移、不删除身体，报告残余重叠。
- 圆形占用半径至少覆盖原生根组件在径向水平面的包围范围，计算预期旋转后八个角部的投影，立方体倾斜时仍覆盖其角落；高度至少覆盖根组件。配置值小于该下限时按下限处理。

## 生命周期

`RegisterEnemy` 对同一 Actor 返回现有实体，避免重复注册。`OnDestroyed` 即时注销，普通死亡沿用已有的立即注销/关闭碰撞与 0.8 秒延迟销毁设计；模拟阶段也清理无效或死亡实体。碰撞禁用后不参与，重新启用后恢复；站立、攻击仍参与。

当前生成流程使用 Spawn/Destroy，没有对象池。保留 `UnregisterEnemy` → `RegisterEnemy` 的释放/重新获取接口，重新获取分配新 ID，旧句柄不再有效；释放过的活 Actor 可以再次进行聚居地初始化。没有新增对象池或改变死亡复活规则。

## 参数与调试

敌人 Blueprint 的 `Behavior` 可配置 `CollisionRadius`、`CollisionHalfHeight`、`CollisionInverseMass`、`CollisionLayer`、`bBodyCollisionEnabled`。运行时 C++ 可调用 `SetBodyCollisionEnabled`。

World Subsystem 的 `BodySeparationSettings` 可由 Blueprint 配置，或使用 `DefaultGame.ini` 的 `[/Script/space.JTSPlanetEnemySubsystem]` 配置段：

```ini
BodySeparationSettings=(Iterations=4,CellSize=160,Skin=1,Tolerance=0.25,Relaxation=0.8,MaxCorrectionSpeed=900,MaxCorrectionPerStep=45)
```

单位为 cm、cm/s。迭代上限 12；默认每步 4 轮，60Hz 时全部修正累计最多 15cm，长帧也不超过 45cm。被包围时转向还会按拥挤程度让出追击方向，避免下一步再走回重叠。通过 `GetBodySeparationStats` 查询参与数、实际迭代数、候选检查数、残余对数、最大残余深度、每步最大修正和 CPU 耗时。

在权威主机控制台输入 `jts.EnemyBody.Debug 1` 显示实际圆形范围、高度线及统计；`jts.EnemyBody.Debug 0` 关闭。使用项目既有的 Unreal DrawDebug/屏幕诊断方式；不绘制模型部位的精细碰撞。

Development 自动化测试代码还提供 `jts.EnemyBody.TestSpawn <Planet Actor 名称> <数量> <地表 X> <Y> <Z>`，用于临时 PIE 中同点生成静止身体。该测试夹具不修改 AI 默认值或资产，测试 Actor 随 PIE 结束销毁；不应在需要保留的游戏会话中使用。

无需手动修改关卡或重新接蓝图。重新编译后启动新 PIE 即生效。保持现有返巢、追击边界、攻击规则；身体有真实占用后，拥挤返巢区域可能需要等待，这次没有通过修改目标或队形去绕过该物理约束。

## 修改文件

- `Source/space/Systems/JTSGroundBodySeparation.h/.cpp`：独立数值分离、网格、设置及诊断。
- `Source/space/Systems/JTSPlanetEnemyFragments.h`：身体配置/数据及预期移动快照。
- `Source/space/Systems/JTSPlanetEnemySubsystem.h/.cpp`：移动阶段接入、环境约束、参与生命周期及调试。
- `Source/space/World/JTSMoonCubeEnemy.cpp`：已释放活 Actor 的重新初始化。
- `Source/space/Tests/JTSGroundBodySeparationTests.cpp`：确定性、不同体型/高度/层、密集与有界修正。
- `Source/space/Tests/JTSPlanetCrowdAITests.cpp`：真实 Actor/mesh/墙体、生命周期、相向运动与既有回归的物理占用条件。

## 验证

2026-10-06，使用项目现有 `spaceEditor Win64 Development` 配置完成普通编译（关闭 Editor，不使用 Live Coding），结果成功，日志：`Saved/Logs/BodySeparationBuild.log`。

最终 `JTS.Moon` 自动化回归共 15 项，全部成功，0 警告、0 失败，报告：`Saved/BodySeparationVerifiedTests/index.json`，日志：`Saved/Logs/BodySeparationVerifiedTests.log`。

- 完全同点两只怪物：稳定分离、无 NaN；双方逆质量均为零时安全保留残余。
- 200 个数值身体同点生成：有限模拟步后残余 0；60Hz 最大单步累计修正 5cm，稳定尾段位移 0；输入顺序反转仍得到相同位置。
- 200 个真实 Actor 在倾斜曲面 mesh 上同点生成：残余 0，最大地表高度误差约 0.001cm，稳定尾段速度 0；全部保持身体参与。
- 相向移动、静止/攻击状态、不同半径、小于直径的网格、不同高度/层、侧面星球重力、墙体阻挡、长帧 8cm 修正上限全部通过。
- 注册去重、碰撞禁用/恢复、销毁、死亡及释放后重新初始化/注册通过；项目没有真实对象池，本次验证其释放/重新获取接口。
- 200 个分散身体的网格候选检查为 1424 次。200 个外力控制怪物的完整 AI Tick 平均约 7.97ms；200 个巡逻/返巢连续移动约 5.05ms。均为本机 headless CPU 测试，不代表 GPU 帧率或长期帧率压测结果。

实际带画面的月球关卡：200 只从完全相同的位置生成，初始重叠 19900 对，约 10.1 秒后残余 0；最大单步累计修正约 8cm，稳定后最大速度 0，最大地表高度误差约 0.0015cm。结果记录于 `Saved/BodyCollisionRuntime.json`，碰撞范围截图为 `Saved/BodyCollisionDebug.png`。

本次未进行多人网络专项验证和长时间帧率压测；测试临时 Actor、摄像机与调试显示在 PIE 结束后移除，关卡和资产没有为测试保存修改。
