# 月球敌群连续移动

2026-10-06。问题源于逻辑位置的低频更新：近处追击 20Hz、远处追击 10Hz、返巢/巡游约 3–4Hz，每帧最多移动 80 只；主机没有表现插值。渲染帧率没有被降低，但逻辑位置停顿已经让画面上的敌人明显顿挫。另一个原因是地表三角面的法线不连续，悬浮偏移和朝向在跨面时会跳变。

## 实现

复用服务器 MassEntity/World Subsystem 和既有聚居地模板。所有存活实体每个世界帧都进行速度积分、地表贴合、转向和障碍碰撞扫掠，返巢与远处敌人也一样。每只仍保留自身碰撞体、弱点命中、血量与交互身份；没有降低绘制帧率、隐藏敌人或移除碰撞。

原 `MovementInterval`、`IdleMovementInterval`、`DistantMovementInterval` 序列化字段仅控制较重的方向决策刷新，保留蓝图兼容。索敌每帧最多 24 次、视线查询 64 次、方向决策 80 次，局部障碍与共享球面路径仍沿用预算、空间桶及缓存。集体位移不再受这个轮转队列限制，状态切换和受击会提前请求新的方向决策。

地表落点继续使用真实 mesh 的径向探测；悬浮偏移和朝向使用连续的星球径向上方向，避免三角面法线导致跳动。移动后的实际扫掠速度写回根组件，供原生 `FRepMovement` 同步使用。

客户端保留原 30Hz 网络发送上限，每个渲染帧从一个最多八份快照的缓冲中采样：位置采用速度约束的 Hermite 插值，旋转采用四元数插值，默认延迟 100ms。根碰撞、模型和弱点统一推进，避免原来只有可见模型滞后、身体碰撞仍跳动。客户端没有新增寻路或伤害规则。断流不会外推穿墙，重新进入相关范围或瞬移会清空旧轨迹。

## 参考

查看了 [Family Time 开发者页面](https://sgthale.itch.io/family-time) 及 [开发者公开回复](https://itch.io/profile/sgthale)。公开信息确认了大量狼子女的玩法、持续更新和作者对性能的重视，没有披露内部寻路、碰撞或渲染算法，因此不将其宣传视频当成本项目的性能证据，也不宣称复制了其实现。

客户端方法参考 [Glenn Fiedler 的 Snapshot Interpolation](https://gafferongames.com/post/snapshot_interpolation/)：网络采样率与画面更新率分开，以缓冲和速度连续插值处理包间隔抖动。路径部分继续使用此前已经实现的共享球面流场。

## 修改文件

- `Source/space/Systems/JTSPlanetEnemyFragments.h`：缓存方向决策，澄清刷新参数用途。
- `Source/space/Systems/JTSPlanetEnemySubsystem.h/.cpp`：分开预算方向决策与每帧连续运动；径向悬浮、真实扫掠速度及诊断。
- `Source/space/Systems/JTSReplicatedMotionBuffer.h/.cpp`：有界客户端快照插值工具，仅负责连续运动采样。
- `Source/space/World/JTSMoonCubeEnemy.h/.cpp`：接收原生运动复制并每帧推进完整根变换，保留碰撞与弱点。
- `Source/space/Tests/JTSPlanetCrowdAITests.cpp`：200 只每帧运动、返巢、包抖动与丢包、停下与瞬移回归；原预算断言改为限制方向决策。
- `Docs/MoonCombatScale.md`：更新过时的移动频率说明。

无需重新接线或修改关卡蓝图。使用重新编译的 Editor 开始新的 PIE，前往月球测试；既有 200 只补充规则、警戒半径及受击追击时限保持不变。`BP_MoonCubeEnemy` 的 `Behavior` 刷新参数现在只影响方向决策。

## 验证

- `spaceEditor Win64 Development` 编译成功；`Saved/Logs/CrowdSmoothBuild.log`。
- 月球相关 8 项测试全部通过、0 失败；`Saved/CrowdSmoothFinalTests/index.json`。覆盖 200 只每帧巡游/返巢无人为停帧和间隔位移跳变、群体索敌、真实球面绕障、黑洞与斥力、人口补充、普通武器命中及追击限制。
- 模拟 30Hz 网络采样与到达抖动、丢失一份快照，在 120Hz 表现采样下没有包间停帧；停下不外推穿墙，瞬移不拖尾。
- 200 只追击的无渲染服务器 AI：平均 2.613ms，P95 3.681ms；巡游/返巢平均 2.316ms，受力平均 3.019ms。统计仅包含敌人 Subsystem 的 CPU 工作，不等同于整体游戏帧率。
- 星际武器相关 22 项测试通过，0 失败；其中 2 项保留已有的普通武器测试资产警告。报告：`Saved/CrowdSmoothStellarTests/index.json`。合计 30 项相关回归通过。
- 实际宇宙关卡、1650×1080 带渲染的权威主机 PIE：200 只全部存在，每帧移动处理数均为 200。追击记录 150 帧、撤退记录 117 帧；每帧抽样 17 只，移动中的人为停帧计数均为 0，模型与碰撞体位置误差为 0。玩家离开追击范围后目标数量为 0。数据：`Saved/CrowdSmoothRuntimeSolo.json`；截图：`Saved/CrowdSmoothChase.png`。
- 上述带 Python 每帧诊断的 Editor 测试，追击世界帧间隔平均 19.99ms、P95 23.56ms；撤退平均 25.63ms、P95 29.10ms。AI 本身分别平均 5.09ms、10.48ms。这证明移除了低频位置停顿，不能据此宣称完整游戏已经稳定 60fps；Editor、诊断、渲染和密集接触均在这次测量中。
- 双窗口网络实机测试未完成：项目现有 PIE 自动连接缺少构建凭据，补齐凭据后又遇到 Lobby → SpaceWorld 的客户端跨图停留问题。没有为通过验证而修改生产联机准入规则。客户端插值目前由上述抖动/丢包自动化覆盖，真实双窗口网络观感尚待完成验证。
- 验证结束后已停止 PIE，恢复原来的单窗口启动设置、后台节流设置和 PIE seamless-travel CVar，Editor 保留在 `L_SpaceWorld`。
