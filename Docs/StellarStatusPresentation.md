# 冰雹与敌人状态表现

2026-10-08。按战斗可读性和状态共存要求调整表现，伤害、减速、冻结、抗控制和腐蚀结算沿用已有服务器规则。

| 效果 | 附着与运动 | 颜色与形状 |
| --- | --- | --- |
| 冰雹 | 角色周围范围上空约 9 米，沿当前星球重力向地表下落；24 颗交错循环，落点查询真实静态表面 | 明亮冰蓝棱晶、短拖尾、落地碎冰环；右键无核心光环和发射光束 |
| 燃烧 / 光灼烧 | 头顶集中 5 簇上升、摆动的火舌及余烬；优先使用 Blueprint 配置的头部 socket | 橙红火焰、金黄热芯，头部橙红沾染；光灼烧偏金黄 |
| 冰冻 | 优先跟随四肢 sockets，没有对应 socket 时附着身体 8 个边缘位置 | 冰蓝晶体、亮蓝边缘结霜，保留躯干可见面积 |
| 中毒 / 腐蚀 | 实际身体模型表面分布不规则污块，每个主要侧面约 3 处，随模型移动和转动 | 暗绿中心、黄绿边缘，覆盖局部而非全身染绿 |

现有腐蚀状态使用中毒/腐蚀的绿色污块视觉。武器射线和领域自身仍使用各自配置的颜色。

## 参考与取舍

- 冰雹参考《流放之路》[Firestorm / Icestorm 官方重做说明](https://www.pathofexile.com/forum/view-thread/2934377)：借鉴从上空落下、清楚的冲击落点和受控的风暴数量。本项目采用固定数量的实例网格，无逐颗伤害 Actor。
- 火焰、冰霜、毒污的形状与局部颜色区分参考《暗黑破坏神 IV》[视觉特效设计说明](https://news.blizzard.com/en-us/article/23746639/diablo-iv-quarterly-updatedecember-2021)中的战斗可读性、色彩/亮度层级原则。头部/边缘/躯干的分区是针对本项目和用户要求的设计；网格与材质均为本项目原创。
- 共存参考《Warframe》开发团队的[冻结与其他状态兼容性说明](https://forums.warframe.com/topic/1119134-cold-status-effects-how-they-block-new-status-effects-coming-changes/)：冻结后仍允许其他状态作用，并各自按持续时间结束。本项目分别复制冻结、燃烧、光灼烧和腐蚀标记，局部材质分别读取三个权重。

## 职责与文件

- `Source/space/Weapons/JTSStellarStatusEffectActor.h/.cpp`：新增本地表现 Actor，选择敌人的主体网格；管理头部火焰、边缘冰晶、身体表面染色及清理。依赖目标 Actor、Blueprint 的网格/材质/sockets，不计算伤害，不复制美术 Actor。
- `JTSStellarTargetComponent.h/.cpp`：原有状态结算组件接入独立表现类型，补充燃烧的复制通知；死亡、到期和状态类别切换时正确清理。传播会携带状态表现配置。
- `JTSStellarWeaponCatalog.h`：每个武器定义提供 `TargetStatusEffectClass`；攻击入口 `JTSStellarWeaponComponent.cpp`、`JTSStellarAbilityCombat.cpp`、`JTSStellarAreaField.cpp`、`JTSStellarProjectile.cpp` 使用该配置。
- `JTSStellarEffectActor.h/.cpp`：冰雹实例与向下轨迹、拖尾、真实表面落点。敌人状态从该通用攻击表现类移出，原护盾跟随功能改为明确的 `UpdateOwnerShield` 入口，`JTSStellarSupportComponent.cpp` 对应接入。
- `Tools/Art/configure_stellar_status_effects.py`：生成两种原创网格、三个状态材质、`BP_StellarTargetStatusFX`；更新 `BP_StellarFreezingFX` 与 `DA_StellarWeaponCatalog`。`configure_stellar_abilities.py` 重建末尾调用该脚本。
- `Source/space/Tests/JTSStellarWeaponTests.cpp`：加入向下/任意径向冰雹与三层状态同时显示、独立到期、死亡清理测试。
- `Tools/Validation/stellar_status_visual_preview.py`：在不保存的空白编辑器世界中，以四只真实 CubeEnemy 渲染单状态与三状态共存，并渲染冰雹。

所有网格和材质由 Blueprint 选择，C++ 未写入项目资产路径。状态表现使用实例网格，只有带状态的敌人创建本地 Actor，按 30 Hz 更新；Dedicated Server 不创建表现 Actor。

## Editor 操作

资产已配置并保存，无需手工连接蓝图。打开重新编译的项目，运行 `L_SpaceWorld`，使用飞船商店的 `DEBUG 全套物品` 装备对应武器。

1. 冻结武器右键：确认冰雹由上空落下；左键积累冻结，确认敌人边缘冰晶。
2. 喷射武器或残留火场：确认火焰挂在头部，并在停止攻击后的燃烧持续期保持可见。
3. 扩散/暗影武器：确认身体表面出现绿色污块；多人分别施加不同元素以查看三种状态同时显示。
4. 替换为有四肢的敌人模型时，在 `BP_StellarTargetStatusFX` 中配置 `HeadSocket` 和 `IceSockets` 为模型实际的头部、手脚 socket 名称；无 socket 的 CubeEnemy 自动使用身体顶部及边缘。

重建本次资产：在已编译的 Editor Python 控制台执行 `py D:/projects/space/Tools/Art/configure_stellar_status_effects.py`。调整颜色、socket 和表现资产在 `BP_StellarTargetStatusFX`；冰雹高度、速度与数量在 `BP_StellarFreezingFX`。

## 验证

- `spaceEditor / Win64 / Development` 编译成功，未使用 Live Coding：`Saved/Logs/StellarStatusBuild.log`。
- `JTS.Stellar` 35 项通过、0 失败；两个原有普通武器缺失资产警告仍存在：`Saved/Automation/StellarStatus/index.json`。
- 新测试验证冰雹沿任意径向向下、无核心发射效果；三种状态同挂，毒污独立到期，全部到期/死亡恢复原材质。
- D3D12/SM6 实际材质渲染结果存放于 `Saved/StellarStatusQA/`。
- 未进行四客户端实机性能和网络延迟测试。
