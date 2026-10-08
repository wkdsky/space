# 星际武器完整能力与高举施放

2026-10-08。参考项目《太空合作生存武器系统设计方案》，补齐原来九种仅有持握表现的组合。保留喷射、聚焦、黑洞已有玩法与最新黑洞参数。

## 武器行为

以下为零加点基准，伤害由服务器结算。六项有效加点分别参与伤害、范围、穿透、状态、特殊能力和消耗/冷却等计算。

| 配件 | 左键 | 右键 | 零加点基准 |
| --- | --- | --- | --- |
| ExplosionTube 爆炸 | 从高举核心发射扫掠碰撞的爆炸球 | 蓄满 0.8 秒后松开，释放强化球与残留火场 | 500/次，14 能量；强化 2.5 倍，30 能量、8 秒冷却 |
| HealingTube 治疗 | 瞄准治疗区域，恢复队友生命和体力 | 优先治疗自己 | 每秒恢复最大生命 6%、最大体力 10%，18 能量/秒 |
| FreezingTube 冻结 | 穿透冰矛，积累寒冷并冻结、碎裂 | 角色周围冰雹，只减速 | 冰矛 100/次、0.2 秒间隔；冰雹 100 DPS；22/26 能量/秒 |
| ShapingTube 塑形 | 前方光刃扇形斩击 | 短格挡窗口；蓄满 1.2 秒松开释放环斩 | 400/次、8 能量；环斩 2.5 倍、25 能量、6 秒冷却 |
| EffectTube 效果 | 光耀区域伤害与短定身 | 为附近队友提供光盾 | 340 DPS、26 能量/秒；光盾最大生命 20%、5 秒、25 能量 |
| DiffusionTube 扩散 | 穿透腐蚀光束，支持有限分叉、死亡传染 | 保持高举施放与瞄准姿势 | 90/次、0.125 秒间隔，加 100 腐蚀 DPS、22 能量/秒 |
| ShadowTube 暗影 | 近地腐蚀领域，减速和削甲 | 暗盾；吸收伤害为施放者有限回能 | 380 腐蚀 DPS、24 能量/秒；暗盾最大生命 15%、5 秒、22 能量 |
| InstanceTube 实例 | 制造跟随、索敌、射击的可受伤机器人 | 引爆自己制造的机器人 | 每次制造 18 能量、2 秒间隔；射击 140/次，自爆 650 |
| DisassemblyTube 分解 | 持续锁定后战斗分解或工程分解 | 切换战斗/工程筛选 | 战斗锁定 0.8 秒、伤害预算 1100、28 能量；工程锁定 1.5 秒 |

治疗溢出产生水盾、紧急治疗、冻结碎裂范围、爆炸分裂及残留时长、光刃扇形范围、腐蚀穿透/分叉/传染、机器人数量/生命/寿命/自爆、分解连锁/冲击/回能均接入对应有效加点。

分解只对显式实现 `JTSDisassemblyTarget` 的工程对象生效。现有资源节点接入该接口，沿用原有掉落事务，总产量保持不变。任务对象、玩家、飞船、未接入接口的对象不会被工程分解。掉落事务失败时退回本次能量。

## 表现与职责

- `JTSStellarWeaponComponent` 保留装备身份、owner RPC、视线方向、有效输入及施放姿势验证；`JTSStellarAbilityComponent` 执行新技能、支付、蓄力、冷却、锁定和持久对象清理。
- `JTSStellarSupportComponent` 统一治疗、护盾和格挡；`JTSStellarTargetComponent` 统一寒冷、腐蚀、减速、定身、削甲及状态表现。通用生命和体力组件提供结算入口。
- 爆炸球、残留场、机器人分别由独立 Actor 负责移动/范围结算/索敌。伤害和状态由服务器管理，移动与必要状态复制到客户端；纯表现 Actor 在本地生成，Dedicated Server 不生成美术表现。
- Blueprint/数据资产选择 FX、网格、材质、颜色、音效与基准参数。C++ 不硬编码项目资源路径。

所有武器继续使用高举权杖姿势。直线攻击从实际核心位置出射；爆炸球从抬高的施放点飞向视线落点；冰雹从地表上空约 9 米处沿星球重力方向向下落，不显示核心发射光环或光束；其他持续技能保留核心施放光环，地面法阵和腐蚀领域沿真实星球的径向朝上。光刃为核心投射的能量刃，不依赖挥舞旧近战模型。

新增极坐标 UV 圆盘/弧刃、动画范围材质、柔和深度交接、菲涅尔护盾、机器人材质以及九套原创合成音效。敌人状态改用独立的目标表现 Blueprint：火焰挂在头部，冰晶挂在四肢/身体边缘，中毒/腐蚀在实际身体表面形成绿色污块，各有局部颜色沾染，支持同时显示和独立到期。详见 [冰雹与敌人状态表现](StellarStatusPresentation.md)。护盾随角色移动并在耗尽、过期或死亡时清理。

## 共同限制

- 治疗不复活；所有来源共享每秒最大生命 20% 的治疗预算，紧急治疗共享 20 秒冷却。
- 同类护盾取较强值；光/暗/水盾合计不超过最大生命 60%。每次暗盾施放共享最多 5 能量返还。
- 精英单次硬控制最多 1 秒并共享 4 秒抗性窗口；Boss 不冻结或定身。碎裂/死亡传染不无限递归。
- 爆炸分裂每个敌人最多两次附加命中；残留火场最多两个；机器人最多九个；穿透、连锁和范围查询均有数量限制。
- 换武器不重置技能冷却。失效装备取消残留场/机器人；有效换装后的机器人最多继续 8 秒；失效装备的飞行弹最多保留 0.5 秒。取消机器人不会触发免费自爆。
- 遮挡、无能量、无效装备和中断锁定不产生免费攻击；同帧计时器补偿调用的零时间差不会错误停止持续施放。

## 修改文件

新增：

- `Source/space/Components/JTSStellarAbilityComponent.h/.cpp`、`JTSStellarAbilityCombat.cpp`、`JTSStellarSupportComponent.h/.cpp`。
- `Source/space/Weapons/JTSStellarCombat.h/.cpp`、`JTSStellarProjectile.h/.cpp`、`JTSStellarAreaField.h/.cpp`、`JTSStellarDrone.h/.cpp`。
- `Source/space/Interaction/JTSDisassemblyTarget.h`。
- `Tools/Art/generate_stellar_abilities.py`、`configure_stellar_abilities.py`；`SourceArt/StellarEffects` 的两个 OBJ 和九个 WAV。
- `Tools/Validation/stellar_mcp_preview.py`：按需注册的 PIE 专用 Unreal MCP 预览工具，只修改本次运行状态，不保存关卡或装备。
- `Content/Space` 下九个 FX Blueprint、五个材质、两个范围网格和九个音效资产。

接入修改：`JTSStellarWeaponCatalog.h`、`JTSStellarWeaponComponent.cpp`、`JTSStellarTargetComponent.h/.cpp`、`JTSHealthComponent.h/.cpp`、`JTSStaminaComponent.h/.cpp`、`JTSStellarLoadoutComponent.h/.cpp`、`JTSCharacter.cpp`、`JTSPlanetEnemySubsystem.cpp`、`JTSStellarEffectActor.h/.cpp`、`JTSMoonResourceActor.h/.cpp`、`JTSPrototypeHUDWidget.cpp`、`JTSStellarWeaponTests.cpp`、`DA_StellarWeaponCatalog.uasset`。

## Editor 操作

资产已配置并保存，无需手动连蓝图。打开重新编译的项目，运行 `L_SpaceWorld`，通过飞船星际商店已有的 `DEBUG 全套物品` 获取装备，将匹配核心/配件装入相邻格，按 Tab 选择后测试左右键。蓄力技能需松开右键才能释放。

需要重建这些表现资产时，先运行生成脚本，再在已编译的 Editor Python 环境运行 `Tools/Art/configure_stellar_abilities.py`。基准伤害/范围/消耗在 `DA_StellarWeaponCatalog` 调整；具体美术在对应 `BP_Stellar*FX` 与 `M_StellarAbility*` 调整。

## 编译与验证

- `spaceEditor / Win64 / Development` 编译成功，未使用 Live Coding；日志：`Saved/Logs/StellarAllAbilitiesBuild.log`。
- `JTS.Stellar` 共 33 项自动化测试通过、0 失败。其中两项已有普通物品缺失资产警告（AssaultRifle、RailPistol）；报告：`Saved/Automation/StellarAbilities/index.json`。
- 通过 Unreal MCP 运行 `JTS.Moon.CrowdAcquisitionAndBudgets` 与 `JTS.Moon.CrowdEveryFrameMotion`，两项均通过且无警告，验证 AI 移动/感知接入没有破坏现有群体行为；结果：`Saved/StellarCrowdMcpResults.json`。
- 新测试覆盖十二组合真实玩法、新技能伤害/消耗、蓄力/格挡、护盾/治疗共享上限、冻结/抗控制、腐蚀传染、机器人切换/清理、分解锁定/筛选、实际资源产量、墙体遮挡及径向范围渲染。
- 使用 Unreal MCP 在 `L_SpaceWorld` 的实际 PIE 玩家上通过已有服务器装备接口切换九种新武器，检查高举姿势、元素颜色、光束与范围场、护盾、机器人、自爆和蓄力释放。稳定施放时右腕沿角色径向上方约 87.28 cm，上下臂方向点积约 1；自疗范围环位于脚下。
- 在正常 D3D12/SM6 Editor 中重新生成并编译五个材质，修复 UE 5.8 节点输入名称导致的 ComponentMask 未连接问题，确认未再出现材质编译失败或灰色默认材质。原始游戏截图位于 `Saved/Stellar{Explosion,Healing,Freezing,Shaping,Radiance,Diffusion,Shadow,Instance,Disassembly}Raised.png`，蓄力爆炸实际命中 1250、扣除 30 能量。
- 多人规则通过服务器世界及多来源状态测试验证；未进行四客户端实机性能和网络延迟测试。
