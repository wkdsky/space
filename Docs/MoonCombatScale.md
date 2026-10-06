# 月球 200 只立方体怪物与星际武器强化

月球立方体聚居地的目标为 **200 只存活怪物**。服务器每秒检查一次，最多补充 20 只；死亡立即释放人口名额，全灭后约 10 秒恢复。刷新沿用真实球面、日照与聚居地模板，并避开玩家 8 米。聚居地分布半径改为 28 米、最小间距 1.45 米。离开月球关闭聚居地时，清理敌人并停止补充。

这是持续维持目标人口的机制，受真实可用地表、日照和玩家避让约束；若暂时没有合法出生点，下一秒继续尝试，避免在玩家身上强行生成。

本次加强的是已经具备攻击逻辑的喷射、聚焦、黑洞。其余九个组合仍保留已有手持模型，攻击模式仍为 `PresentationOnly`。

| 基础参数 | 调整前 | 调整后 |
|---|---:|---:|
| 喷射射程 / 全角 | 6 米 / 35° | 18 米 / 50° |
| 喷射直接伤害 / 燃烧 | 300 / 40 每秒 | 460 / 60 每秒 |
| 聚焦射程 / 单次伤害 | 30 米 / 56 | 55 米 / 84 |
| 聚焦基础穿透目标 | 2 | 5，逐次衰减 |
| 聚焦基础跳链 | 0 | 1，9 米内 |
| 聚焦命中宽容半径 | 射线 | 18 厘米球形扫掠 |
| 黑洞最远放置 / 吸引半径 | 20 米 / 4 米 | 30 米 / 9 米 |
| 黑洞直接伤害 | 150 每秒 | 220 每秒 |
| 黑洞吸引强度参数 / 斥力半径 | 260 / 3 米 | 550 / 6.5 米 |

喷射与聚焦保留原能量消耗及服务器校验。黑洞已改为单次释放，默认消耗 30 能量、持续 8 秒；释放距离仍为 30 米，服务器必须在射程内命中合法地面，超距或无落点不扣能量、不生成，也不替换已有黑洞。数量升级每 2 个有效点增加 1 个同时存在上限，默认 1 个、最高 6 个，满额时替换最早一个。详细规则见 [黑洞单次释放](BlackHoleCasting.md)。范围攻击最多处理 200 个目标，喷射先筛选锥形范围再应用命中上限，避免侧面敌人挤掉有效目标。地形会阻挡攻击；敌人身体不会遮住喷射或黑洞对后排的范围伤害。聚焦穿透、跳链显示独立光束，跳链从上一只敌人出发。

表现采用蓝图配置的网格、材质和原创合成音效：喷射双层流动喷焰、聚焦光晕与命中闪光。黑洞现为略微离地的黑色光泽球体，已隐藏大范围外壳和旋转亮环；持续斥力显示半透明紫色半球罩，怪物浅入后被推至边界附近，强阻尼抑制远飞。吸力与斥力同时参与 Mass 的速度积分和碰撞求解，详细规则见 [黑洞受力与表现](BlackHolePhysics.md)。持续攻击通道复用本地特效 Actor；每个独立黑洞由复制的场 Actor 管理自己的本地特效，同一怪物的连续伤害合并为固定寿命飘字，降低群体攻击时的 Actor 和 UI 生成开销。

设计参考：[Warframe 官方光束武器调整](https://www.warframe.com/uk/patch-notes/pc/22-13-3)。借鉴持续喷射穿过敌群、较长射程和明确命中反馈，按本项目的千点怪物生命与能量规则重新配平。没有复用其美术或音频资产。

## 聚居地警报与群体 AI

月球实例使用 `BP_MoonCubeEnemy` 配置：以聚居地为中心的普通警戒进入半径 35 米、解除追击半径 45 米。感知 65 米仅决定视线检测的候选距离，不能让普通怪物在 35 米警戒区外主动索敌。怪物出生后立即进入首轮索敌队列，200 只分帧处理。

玩家从警戒区外攻击时，开启独立的受击追击：以聚居地为中心最多 90 米，且从本轮开始最多 15 秒；距离或时间任一超限立即清除目标并锁定返巢。重复伤害、持续看见玩家、其他怪物的警报、换攻击者均不能延长本轮截止时间。共享警报的晚加入怪物继承原截止时间，不能从加入时重新计时。返巢途中受击不会打断返巢，回到中心 6 米内后解除返巢状态，并等待 3 秒再接受新目标。3.8 米/秒追击速度、距离和时间均由蓝图配置。

每只怪物记录已经结束的受击追击轮次。即使聚居地旧警报尚未过期，返巢后的怪物也不会重新加入该轮追击；普通警戒仍可在冷却后恢复。

看见玩家或受到攻击会共享聚居地警报，其他怪物追向最后报告的位置；未受击的普通警报不会授予扩大追击范围。近战命中依然要求各自视线。失去目标、玩家死亡/登船/换星球或超过本轮边界后，解除目标并返巢。

继续使用现有服务器 Mass host，无每怪物服务器 Actor Tick、无客户端独立 AI。每帧最多 24 次目标扫描、64 次视线查询、80 次方向决策、48 次局部障碍探测；共享路线另有 64 次几何查询及 1024 次积分扩展的预算。所有怪物的位移、转向、受力与碰撞每帧更新，不受方向决策预算限制。每帧复用敌群射线忽略列表，避免每条射线重新遍历敌群；对最多四个玩家比较距离，再做视线检查。轮转队列保证公平，目标偏好与记忆减少频繁换目标。

这是 MassEntity 混合架构：数值状态使用 Entity/Fragment，World Subsystem 集中调度，Actor 负责碰撞、表现和网络复制。当前调度仍按 Entity Handle 访问，尚未迁移到按 chunk 批处理的 Mass Processor；不能据此宣称已经获得完整 ECS 批处理性能。

2026-10-06 已取消位置的距离分级更新；原频率参数仅用于较重的方向决策，所有怪物每帧连续移动，客户端使用有界快照缓冲插值，身体碰撞、弱点与模型保持同一变换。近邻使用三维空间桶，只检查相邻桶且每次最多 32 个候选，施加球面切向分离及拥挤减速。地表使用短距离径向探测，仍然落在真实 mesh 上；悬浮方向使用连续径向上方向，避免网格面法线引发跳动。移动碰撞继续阻挡障碍。详情与新验证见 [敌群连续移动](MoonCrowdSmoothMotion.md)。

遇到障碍时，以「星球＋聚居地＋目标玩家」为键共享反向 Dijkstra 场；无障碍处直接追击。网格在聚居地切平面的径向投影中定义，节点落在真实球面 mesh 上，检查地形坡度、净空、相邻通行和对角切角。几何与代价计算跨帧处理，最多缓存 8 个场，每个场不超过 16384 个单元；移动目标的新积分完成前保留上一份路线。局部避障先响应，路线随后接管绕行。动态物理障碍由局部探测及实际移动碰撞处理；静态路线缓存不宣称支持动态场景全局最优重规划。

设计参考：[Valve《Left 4 Dead AI》](https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf) 的局部避障；[《Supreme Commander 2》开发者的共享流场说明](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter23_Crowd_Pathfinding_and_Steering_Using_Flow_Field_Tiles.pdf) 的共享积分、缓存和分帧预算。按本项目真实球面和 1–4 人服务器规则实现。

## 修改位置

- `Source/space/World/JTSPlanetEnemySettlement.h/.cpp`：通用服务器人口补充、存活统计、出生点避让与关闭清理。
- `Source/space/World/JTSMoonCubeEnemy.h/.cpp`、`UI/JTSFloatingDamageActor.h/.cpp`：合并连续伤害飘字。
- `Source/space/Items/JTSStellarWeaponCatalog.h`、`JTSStellarProgression.cpp`：可配置范围、穿透、跳链参数与升级说明。
- `Source/space/Components/JTSStellarWeaponComponent.cpp/.h`：真实攻击判定、地形遮挡与效果同步。
- `Source/space/Player/JTSCharacter.h/.cpp`：真实星球上的攻击眼高沿径向重力偏移，修复默认世界 Z 轴眼高在球面侧面的错误。
- `Source/space/Weapons/JTSStellarEffectActor.h/.cpp`：复用的分层特效、旋转环、跳链、声音及径向方向。
- `Source/space/Systems/JTSPlanetEnemyFragments.h`、`JTSPlanetEnemySubsystem.h/.cpp`、`JTSPlanetCrowdNavigation.h/.cpp`、`JTSPlanetEnemyPursuit.h/.cpp`：预算、警报、追击状态规则、分级移动、近邻避让及共享球面路线。
- `Content/Space/Blueprints/Enemies/BP_MoonCubeEnemy.uasset`、`Tools/Art/configure_moon_cube_ai.py`：月球 AI 项目配置。
- `Source/space/Tests/JTSPlanetCrowdAITests.cpp`、`JTSSettlementPopulationTests.cpp`、`JTSStellarWeaponTests.cpp`：群体 AI、人口与攻击回归验证。
- `Content/Space/Maps/L_SpaceWorld.umap`、`L_SpaceWorld_Authoring.umap`：月球聚居地实例配置。
- `Content/Space/Data/Weapons/DA_StellarWeaponCatalog.uasset`：三种武器平衡配置。
- `Content/Space/Blueprints/Weapons/Stellar/BP_Stellar{Jet,Focus,BlackHole}FX.uasset`：特效及声音选择。
- `Content/Space/Materials/Weapons/StellarFX/`、`Meshes/Weapons/StellarFX/`、`Audio/Weapons/Stellar/`：新表现资产。
- `Tools/Art/generate_stellar_combat_effects.py`、`configure_stellar_combat_effects.py`、`SourceArt/StellarEffects/`：可重复生成、配置的资产来源。

## Editor 操作与验证

关卡实例、蓝图和资产已保存，无需人工连接。使用重新编译的编辑器，重新开始游戏后前往月球；在星际商店使用全套 debug 按钮，装备火核心＋喷射、光核心＋聚焦、暗核心＋黑洞，按 Tab 选择已激活组合。鼠标左键攻击；黑洞右键使用斥力，喷射右键收束喷流并延长射程。

Build：`spaceEditor Win64 Development`，关闭编辑器后构建，禁用 Live Coding。

2026-10-05 验证：`spaceEditor Win64 Development` 编译成功。34 项自动回归通过、0 失败，覆盖星际攻击、选择和能量、200 只人口补充、四玩家索敌、固定追击时限、返巢及旧警报隔离、共享球面障碍路线、径向攻击眼高、普通武器与攀爬相关行为。报告仍有项目既有的普通物品定义资产缺失警告。

200 只怪物的 180 帧无渲染服务器 AI 测试：平均 0.731 ms，P95 1.245 ms。统计仅包括敌人 Subsystem 本身，不包含渲染、音频、网络和客户端复制开销，不代表四客户端帧率基准。

实际 D3D SM6 PIE 已确认 200 只生成、警戒区外不主动索敌、进入警戒区后发现玩家、持续攻击产生死亡与补充、三种持有模型切换，以及修复后的喷焰与黑洞范围表现。聚焦武器的长光束、命中、跳链组件及实际伤害也已检查。材质生成脚本检查每条节点连接并使用实际输入名称，避免丢失输入造成默认材质替代。游戏预览截图位于 `Saved/MoonJetRuntime.png`、`Saved/MoonBlackHoleRuntime.png`，测试临时状态在结束 PIE 时清除。

自动测试输出保存在 `Saved/MoonCombatScaleTests/index.json` 和 `Saved/Logs/MoonCombatScaleTests.log`。
