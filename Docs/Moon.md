# 月球内容与生成配置

月球玩法运行在 `L_SpaceWorld`，由 `BP_MoonSurfaceController` 与 `PDA_MoonSurfaceGameplay` 配置。资源使用球面范围模板，怪物使用已有聚居地模板；所有生成、采集、伤害和补充由服务器执行。

## 七类资源

资源模型位于 `/Game/Space/Meshes/Resources/Moon`，Blueprint 选择矿点模型变体，物品 Data Asset 选择拾取外观。金属内部沿用 `Ore` 枚举，显示为 Metal；水晶是独立物品与飞船材料。

| 类型 | 完整采集产量 | 采矿工作量 | Blueprint 默认比例 |
| --- | --- | ---: | ---: |
| 小石头 | 1 石头，直接拾取 | 0 | 30% |
| 中石头 | 2 石头 | 5 | 15% |
| 大石头 | 6 石头 | 24 | 15% |
| 中石头 + 金属 | 2 石头 + 2 金属 | 10 | 15% |
| 大石头 + 金属 | 6 石头 + 6 金属 | 30 | 10% |
| 中石头 + 水晶 | 2 石头 + 2 水晶 | 10 | 10% |
| 大石头 + 水晶 | 6 石头 + 6 水晶 | 30 | 5% |

矿点按真实 Mesh Surface 放置与径向朝向，模型按包围盒居中，基础尺寸小 / 中 / 大约 32 / 100 / 200 cm，保留随机缩放。混合矿一次生成两类掉落；任一生成失败时回滚整笔掉落并保留矿点。普通采矿与工程分解共用产量事务，矿点变体和采集状态复制。

资源交互标记使用顶部中心的单行 `[E] Rocks`；散落金属为 `[E] Metal`，水晶为 `[E] Crystal`。标记位置按当地球面方向及实际物理包围盒计算。

## 区域、密度与重叠

`PDA_MoonSurfaceGameplay → Moon Resource Spawn Settings` 保存基础半径、随机种子、最小间距、地标排除、产量和七类资源勾选。**密度、各类百分比与覆盖优先级保存在每个关卡 Spawner 实例上。** PDA 基础参数刷新不会覆盖实例分布。

当前默认覆盖半径 2000 cm、密度 0.8 个 / 100 m²，单一无遮挡区域目标为 10 个对象。目标数量按球面有效面积四舍五入；间距、地标排除和合法表面可能降低实际数量。密度为 0 或所有类型取消勾选时不生成。

修改一个比例会保留输入值，其余已勾选类型按原比例分配剩余值，合计 100%；其余比例全为 0 时均分。仅一种类型时固定 100%。取消类型勾选立即隐藏对应比例并停止生成该类。

同一 Planet 的多个 Spawner 按覆盖并集生成，重叠位置只属于一个区域：

- 七类比例与密度全部相同时，忽略优先级、只生成一份。
- 任一比例或密度不同时，由 `Overlap Priority` 数字较小的区域胜出，完整采用其密度与比例。
- 相同优先级随机确定覆盖顺序，固定种子可复现；不会自动偏向高密度。
- 零密度区域胜出时，重叠部分不生成；非重叠部分保留各自配置。

生成前统一清理旧资源，重叠区扣除的数量不会转移到未重叠部分。相交有效面积用 4096 个等面积样本估计，所有区域共享最小间距检查。蓝色连续覆盖圆环贴合真实月面，宽度为 4 个屏幕像素，仅 Editor 显示；PIE 清空 / 隐藏，打包剔除。

配置步骤：

1. 打开 PDA，设置基础半径、种子、资源种类、产量及排除距离。
2. 在 `L_SpaceWorld` 选中或复制 `BP_MoonResourceSpawner`，将 `Coverage Planet` 指向 Moon PlanetAnchor，移动实例设置覆盖中心；需要独立半径时开启 `Override Coverage Radius`。
3. 在 `Moon / Resources / Distribution` 设置 `Resource Density (per 100 m2)`、`Resource Ratios`，并设置 `Overlap Priority`。
4. `Resource Gameplay Data` 可以选独立 PDA；为空时使用 SurfaceController 的默认 PDA。同一地表关卡内指向当前 Planet 的实例会自动初始化。
5. 保存 PDA 和关卡后 Play。模型变体在 `BP_MoonResourceActor → Moon / Presentation`，小石头 / 水晶拾取外观在对应 `DA_Item_*` 调整。

## 九个蚁巢与尸体地标

当前关卡有 `Moon_AntNest_01` 至 `09` 九个独立巢穴，沿陨石坑内部山脚分布；SurfaceController 的 `PlacedMoonAntNests` 绑定这些实例。已放置巢穴保留独立位置与形状，运行时只初始化活动，不再围绕旧尸体锚点随机搬动它们。

巢穴复用 `AJTSMoonAntNestActor` 补充模板。每巢目标 30 只，补充间隔 0.12–1 s，按现存比例 `r` 使用 `lerp(min, max, r³)` 并加小幅扰动；满额暂停，死亡 / 钻回释放名额并加快补充。生命周期为钻出、游走、受击、逃跑与钻回，地表活动 90–150 s。

| 蚂蚁参数 | 当前配置 |
| --- | --- |
| 近 / 中 / 远出生权重 | 65% / 25% / 10% |
| 对应距离带 | 0.6–9 m / 9–28 m / 28–60 m |
| 游走 / 返回限制半径 | 60 / 70 m |
| 最大地表坡度 | 相对径向重力 35° |
| 生产血量 | 3 |

出生、目标及沿途都检查真实表面坡度。`JTSPlanetSurfaceSteeringComponent` 处理短段探测、绕行记忆及卡住重选，不自行 Tick。蚂蚁的活动与状态移动由 `JTSPlanetAntSimulation` 在公共 Mass 宿主更新，Actor 保留生命、命中、模型、复制及尸体掉落。星际减速、冻结 / 定身和吸引 / 斥力作用于同一实体移动，仍受地形与障碍约束。

`EntranceShape` 程序网格组件负责洞口、岩石边缘、材质及 Visibility 查询碰撞，无 Tick。每个实例可调 Width / Height、Profile Power、Floor Roundness、Lean、Irregularity、Seed、Rim Width 与 Wall Slope；明确点击 `Randomize Entrance` 才重随机，重开关卡 / PIE 不重新随机。运行时改形状由服务器复制参数。当前包含细缝、偏斜、橄榄形及不规则开口。

中心地标 `Moon_Corpse_SkeletonAstronaut` 通过 `PlacedCorpseLandmark` 绑定，使用贴合坑底多折面的 `SM_SkeletonAstronaut_Corpse_CraterCenter`。该静态尸体独立于玩家可用 Skeletal Mesh；巢穴位置由各实例决定，尸体不承担巢穴定位。

Editor 调整人口与活动带在 PDA，坡度 / 探测在 `BP_MoonAnt → SurfaceSteering`，入口形状在关卡巢穴的 `EntranceShape`。搜索 `Moon_AntNest_` 或尸体标签后按 F 可查看；无需重建巢穴。

## 立方体聚居地与群体移动

月球立方体聚居地目标 200 只存活怪物，每秒最多补 20 只；出生避开玩家 8 m，分布半径 28 m、间距 1.45 m，并检查真实表面 / 日照。没有合法点时后续重试。退出月球关闭地表玩法后清理敌人及补充计时器。

普通警戒以聚居地为中心，进入 35 m、解除 45 m；65 m 感知距离只用于候选。区外受击开启最多 90 m / 15 s 的独立追击轮次，伤害、视线、换攻击者或后加入的共享警报不延长截止时间。超限锁定返巢，中心 6 m 内解除后等待 3 s；返巢中受击不打断。追击速度 3.8 m/s，参数在 `BP_MoonCubeEnemy`。

所有实体每世界帧连续移动 / 转向 / 碰撞；重方向决策、索敌、视线和路线计算分帧预算执行。障碍路线按星球 + 聚居地 + 目标玩家共享球面投影流场，客户端使用有界快照缓冲插值，碰撞 / 弱点 / 模型保持同一变换。

身体分离使用服务器位置约束、空间哈希和多轮修正，静止 / 攻击中的身体仍占用空间。修正受速度、单步上限、真实地表和环境扫掠约束，不产生下一帧持续推力。当前默认四轮、900 cm/s、每步最多 45 cm，60 Hz 下累计最多 15 cm；没有空间时保留可诊断残余重叠。该地面身体求解主要接入当前立方体聚居地，不将其当作自由三维飞行求解器。

在权威主机控制台使用 `jts.EnemyBody.Debug 1` 查看身体范围和统计，`0` 关闭。行为参数在敌人 Blueprint，求解参数由 `JTSPlanetEnemySubsystem.BodySeparationSettings` 配置。

## 美术来源与可用角色

`BP_JTSPlayer_SkeletonAstronaut` 是可选玩家 Blueprint，沿用 `Casual_2_Skeleton` 和 `ABP_JTSPlayer_Casual_2`。在实际使用的 Earth / SpaceWorld GameMode Blueprint 中选择 Default Pawn Class 才会启用，不因存在该资产自动替换当前玩家。

骷髅来自 KayKit Character Pack: Skeletons，宇航服来自 Quaternius Ultimate Space Kit；原授权、来源地址和源文件记录保留在 `SourceArt/SkeletonAstronaut/LICENSE_KayKit.txt`、`LICENSE_Quaternius.txt` 和 `sources.json`。资源 Kenney 模型使用项目中的副本。Blender / FBX / OBJ 和授权属于可编辑来源，保留在 SourceArt / SourceAssets，配置工具在 Tools/Art。
