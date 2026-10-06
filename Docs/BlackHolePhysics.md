# 黑洞受力与表现

左键的单次释放、30 米地面落点校验、8 秒寿命和数量升级规则保持不变。球体半径默认 65 厘米，底部离真实地面 45 厘米；不显示吸引范围光罩或亮环。材质为近黑色不透明表面，采用较低粗糙度与窄幅边缘反光，外观不随吸引范围升级膨胀。黑球仅为视觉表现，场 Actor 与特效 Actor 均关闭碰撞，球体网格也是 `NoCollision`；人物可以穿过、走到正下方并离开。吸力中心单独保留服务器校验过的地面落点，不采用升高后的球心，也没有球体接触或反弹约束。右键按住时持续生成以角色为中心、半径 6.5 米的斥力，松开、切换装备或能量耗尽后解除。斥力显示半透明紫色半球罩，底部沿角色脚下地表切面放置，随角色移动并采用星球径向上方向。半球没有底盖、物理碰撞或寻路影响；内外两侧均能看见，主体透明度较低、轮廓稍强。蓝图可通过 `bShowRepulsionBoundary` 隐藏它。

受力由服务器在现有 Mass 怪物移动系统中处理。每个黑洞单独登记吸力，右键斥力以施法角色登记；所有有效来源同时相加。吸力采用软化的距离平方反比模型，在外边界平滑衰减。斥力允许目标中心浅入可见边界 30 厘米，再启动向外的弹簧推力；范围内新出生的目标无需先跨越边界，下一次 0.05 秒受力刷新即可响应。触发状态跨刷新保留，完成推出后在外侧解除，避免每一帧反复开关。弹簧自然静止位置为边界外 12 厘米，采用 `2.2 × sqrt(刚度)` 的强阻尼吸收推出惯性，抑制远距离过冲；外侧仅保留 22 厘米的有限制动带。阻尼可以产生反向制动力；它与吸力、AI 推进一起积分，不通过改写坐标把怪物传送到边界。AI 的追击加速度继续参与运动，无法直接覆盖受力造成的惯性。

速度按照 `v += (吸力 + 斥力 + 接触力 + AI 推进 - 阻尼) × dt` 积分。每次 Mass 移动更新分为不超过 1/120 秒的小步，单次累计最多 0.25 秒；速度上限 18 米/秒、净场加速度上限 240 米/秒²，避免多场叠加产生无界速度。角色站在黑洞中心附近时，吸力与斥力的平衡自然形成边界停留；没有指定怪物停留坐标或人为锁定圆周。释放右键后，吸力仍会继续改变速度。

怪物被吸向黑球正下方的地面，在已有空间网格中与相邻个体发生弹簧接触，每次最多检查 32 个邻居，避免所有怪物挤成一个点。它们不会撞在黑球外壳上形成空心环。地形扫掠仍由 Actor 根碰撞处理，撞到障碍会反弹。可见的立方体通过通用聚居地敌人接口接收受力碰撞事件，表现短暂倾斜和挤压回弹；接口事件不决定位移。每只怪物的碰撞表现最短间隔 0.2 秒。

继续复用 Mass 实体、共享寻路与感知预算：每帧最多 80 次移动、24 次索敌、64 次可见性查询。只有受力的个体提升到最多 0.05 秒一次移动，不增加每个怪物的服务器 Tick。玩家、死亡目标不加入星际场目标列表；原有 Boss 位移抵抗规则保留。场伤害、斥力伤害的间隔独立于受力刷新，持续按住右键不会在伤害冷却期间失去保护。

斥力推出时的怪物移动扫掠临时忽略本次有效施法者，完成移动后立即恢复原有忽略列表；防止怪物出生或被吸到角色胶囊内部后无法脱离。既有忽略设置不被清掉，地形和其他障碍继续碰撞。200 个体测试包含一只与施法者胶囊重叠出生的怪物，验证它能被推出、普通碰撞忽略状态不会残留。

小步积分与阻尼设计参考 [Epic 物理子步进说明](https://dev.epicgames.com/documentation/unreal-engine/physics-sub-stepping-in-unreal-engine)和 [NVIDIA 刚体动力学说明](https://nvidia-omniverse.github.io/PhysX/physx/5.4.0/docs/RigidBodyDynamics.html)。实现是服务器 Mass 的地表约束受力求解，不是为每只怪物开启独立 Chaos 刚体模拟。

## 修改文件

- `Source/space/Components/JTSStellarTargetComponent.h/.cpp`：场来源、合力、阻尼、惯性、无实体球约束的吸力积分。
- `Source/space/Systems/JTSPlanetEnemyFragments.h`：配置碰撞半径、记录碰撞表现冷却。
- `Source/space/Systems/JTSPlanetEnemySubsystem.h/.cpp`：Mass 受力移动、有限邻居接触、地形反弹。
- `Source/space/World/JTSPlanetSettlementEnemy.h`、`JTSMoonCubeEnemy.h/.cpp`：通用受力表现接口、立方体碰撞反馈复制。
- `Source/space/Components/JTSStellarWeaponComponent.h/.cpp`：每 0.05 秒刷新持续斥力，伤害冷却独立；输入与心跳使用相同的相机射线。
- `Source/space/Weapons/JTSBlackHoleField.h/.cpp`、`JTSStellarEffectActor.h/.cpp`：球体参数复制、升高球体与地面吸引点分离、关闭场与特效碰撞、紫色半球罩。
- `Source/space/Items/JTSStellarWeaponCatalog.h`、`Content/Space/Data/Weapons/DA_StellarWeaponCatalog.uasset`：球半径、离地间距、斥力刚度、浅入深度和推出偏移。
- `Content/Space/Blueprints/Weapons/Stellar/BP_StellarBlackHoleFX.uasset`、`Content/Space/Materials/Weapons/StellarFX/M_StellarVoidCore.uasset`：小球网格与黑色材质。
- `Content/Space/Meshes/Weapons/StellarFX/SM_StellarRepulsionDome.uasset`、`Content/Space/Materials/Weapons/StellarFX/M_StellarRepulsionDome.uasset`、`SourceArt/StellarEffects/SM_StellarRepulsionDome.obj`：平底半球网格和双面紫色透明材质。
- `Tools/Art/configure_stellar_repulsion.py`、`configure_black_hole_physics.py`、`configure_stellar_combat_effects.py`：独立斥力资产配置与已有配置脚本同步。
- `Source/space/Tests/JTSBlackHoleForceTests.cpp`、`JTSStellarWeaponTests.cpp`、`JTSPlanetCrowdAITests.cpp`：合力、持续按键、边界平衡、无实体球限制、玩家穿过与离开、步长稳定和 200 怪物受力预算。

## Editor 与验证

资产已保存，无需人工连接蓝图。重新开始游戏，装备暗核心与黑洞配件，用左键在地面释放；保持右键，观察紫色半球罩内的怪物浅入后被推到边界附近，松开后罩子消失、怪物继续被黑洞吸引。尺寸、浅入深度与推出偏移在武器目录资产中调整；罩子开关在 `BP_StellarBlackHoleFX` 默认属性中调整。

斥力与无碰撞黑球调整后，`spaceEditor Win64 Development` 编译通过。44 项自动测试通过、0 失败，4 项既有普通物品定义缺失警告；黑洞专项无警告。从中心、1 米和 6.18 米处出生的独立目标均立即向外受力、最终停在 662 厘米，测试中最远距离也为 662 厘米；浅入阶段无力、触发后完成推出和持续通道中途出生均通过。吸力与斥力共同作用的平衡位于 657.27 厘米，最低距离 617.58 厘米，符合浅入后推出规则。场的无碰撞状态、人物扫掠穿入黑球与离开、地面吸力点与抬高球心分离、怪物自由进入球体投影范围均通过。200 个 Mass 个体受力测试保持全部工作预算，最近距离约 636.2 厘米，Mass 平均耗时约 1.132 毫秒。CPU 数值是无渲染自动化测试，不代表四客户端渲染帧率。报告：`Saved/RepulsionDomeTests/index.json`；构建日志：`Saved/Logs/RepulsionDomeBuild.log`。

首次黑球版本的 D3D 实景验证使用月球关卡与 200 只立方体怪物：短按左键生成一个黑洞，能量从 100 降至 70，切回普通栏后黑洞仍存在并造成伤害；仅球体可见，场壳和轨道环均隐藏。将测试角色移到球体下方并持续输入右键，最初贴近角色的怪物被推出，约 3 秒后最近怪物距离为 619.33 厘米，能量降至 48.18；黑洞在 8 秒寿命后消失。测试借助临时 Python 调用现有输入 RPC，不包含四客户端网络压力测试。实景图：`Saved/BlackHoleSphereRuntime.png`；日志：`Saved/Logs/BlackHolePhysicsRuntime.log`。

最终紫色半球罩与无碰撞黑球版本已完成 D3D 实景核对：角色可与黑球的视觉体积重叠，罩子沿地面切面包围角色，场壳和亮环继续隐藏。200 怪物场景中，保持右键约 3 秒后最近活怪距离为 627.93 厘米，符合允许浅入约 30 厘米的边界；最初与角色重叠的近身怪物也能退出。实景图：`Saved/RepulsionDomeRuntime.png`；日志：`Saved/Logs/RepulsionDomeRuntime.log`。此测试用临时 Python 提交持续输入 RPC；高分辨率截图可能造成超过心跳期限的停顿，因此测试视角在截图后继续提交保持输入。正式规则、心跳校验与服务器权限未因此放宽。测试已停止，编辑器重新打开到正常关卡，无需手工恢复蓝图或设置。
