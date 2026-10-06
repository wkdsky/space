# 星际武器手持模型

2026-10-06：模型已全部替换为圆球核心与短杖，当前配置和验证见 [StellarScepters.md](StellarScepters.md)。以下保留早期手持模型接入记录。

原有实现只有三种代表攻击特效，选中星际武器时会隐藏普通武器，人物手上缺少模型。奖池已定义十二种合法组合，但武器目录只登记三种，导致火核心＋爆炸等组合显示激活光芒却被 Tab 跳过。本次补齐十二种组合的目录和不同的低多边形手持原型，并接入现有装备、握持和网络同步系统。

## 资产与配置

| 武器 | 模型 | 外形 | 模型内枪口（厘米） |
| --- | --- | --- | --- |
| 喷射 | `SM_StellarJet` | 粗喷口、双侧罐体及散热装甲 | `(49, 0, 13)` |
| 爆炸 | `SM_StellarExplosion` | 宽口发射管和滚筒 | `(49, 0, 13)` |
| 治愈 | `SM_StellarHealing` | 医疗标记和双环导流装置 | `(37, 0, 13)` |
| 冷冻 | `SM_StellarFreezing` | 长管透镜和散热翼片 | `(70, 0, 12)` |
| 聚焦 | `SM_StellarFocus` | 长管、导轨、透镜和瞄准镜 | `(70, 0, 12)` |
| 塑造 | `SM_StellarShaping` | 剑柄、护手和发光剑刃 | `(82, 0, 0)` |
| 效应 | `SM_StellarEffect` | 三环光场发射装置 | `(38, 0, 13)` |
| 扩散 | `SM_StellarDiffusion` | 三管分流枪口 | `(49, 0, 13)` |
| 影子 | `SM_StellarShadow` | 层叠环形暗场装置 | `(42, 0, 13)` |
| 黑洞 | `SM_StellarBlackHole` | 环形发射器、支架和悬浮核心 | `(42, 0, 13)` |
| 实例 | `SM_StellarInstance` | 箱式构造器和指令面板 | `(40, 0, 13)` |
| 分解 | `SM_StellarDisassembly` | 双叉分解器 | `(52, 0, 13)` |

- 模型位于 `Content/Space/Meshes/Weapons/Stellar/`。这是实际导入的 StaticMesh 资产，不再以普通武器方块作为星际武器的手持显示。
- 共享材质位于 `Content/Space/Materials/Weapons/`：`M_StellarHeld`、`M_StellarHeldCore`、`MI_StellarHeldShell`、`MI_StellarHeldPanel`。金属外壳、面板与核心分为三个材质槽。
- `DA_StellarWeaponCatalog` 的各武器条目配置 `HeldMesh`、`HeldGripTransform`、`HeldMuzzleTransform`、`CoreMaterialSlot`、`bMeleePresentation` 和 `HeldCarryRotation`。核心材质颜色来自现有奖池的核心颜色，默认握柄中心为模型原点，+X 是枪口或剑尖朝向。
- OBJ 源文件与材质槽定义位于 `SourceArt/StellarWeapons/`。`Tools/Art/generate_stellar_held_models.py` 可重建源模型；`import_stellar_held_models.py` 在 Unreal Python 中重新导入十二个模型；`configure_stellar_held_models.py` 对齐奖池、目录与模型并保留已有攻击定义。

## 职责与行为

- `JTSWeaponVisualComponent` 复用现有手部锚点、姿态跟随与握持偏移，负责显示选中的模型。切回普通栏时恢复普通武器；攀爬时收起，结束攀爬或恢复人物显示时重新显示选中的星际武器。模型不继承导入骨架的缩放。
- `JTSStellarWeaponComponent` 复制当前核心与配件的组合标识。远端玩家据此选择模型，无需复制个人背包内容。攻击判定仍由原有服务器规则负责。
- `JTSAnimInstance` 将已选中的星际武器纳入持枪与瞄准状态，避免沿用普通栏中近战武器或空手的姿势。
- 选中组合的圆形和菱形显示白色外圈，普通栏取消选中高亮；激活的核心颜色光团仍保留。按 1–9 返回普通栏时恢复普通栏高亮。
- 喷流和聚焦光束的视觉起点改为手持模型的枪口；缺少可见枪口时保留原起点。服务器命中检测与射程不变。
- 模型、美术材质和握持参数由数据资产配置，C++ 未硬编码这些项目模型的路径。

当前可攻击的形态仍为喷射、聚焦和黑洞。其余九种的选择与手持显示已经接入，目录明确标为 `PresentationOnly`，服务器不执行攻击也不消耗能量，未把这些形态映射成另一种攻击。爆炸弹、治愈、冷冻、光剑等具体攻击规则仍需按设计方案后续实现。

## 修改文件

- `Source/space/Items/JTSStellarWeaponCatalog.h`
- `Source/space/Animation/JTSAnimInstance.cpp`
- `Source/space/Components/JTSStellarWeaponComponent.h/.cpp`
- `Source/space/Components/JTSWeaponVisualComponent.h/.cpp`
- `Source/space/Tests/JTSStellarWeaponTests.cpp`
- `Source/space/UI/SJTSStellarLoadoutView.cpp`、`JTSPrototypeHUDWidget.cpp`
- `Content/Space/Data/Weapons/DA_StellarWeaponCatalog.uasset`
- 上述十二个模型、四个材质、OBJ 源文件及三个美术工具脚本。

## Editor 操作

无需重建玩家或 Widget Blueprint。运行重新编译后的项目，在同列圆形和其下方菱形槽装备正确组合，使用 Tab 选择后即显示对应武器；按 1–9 返回普通物品栏。

扩展模型时在 `DA_StellarWeaponCatalog` 中选择 StaticMesh 并调整握柄、枪口和核心材质槽；可继续使用当前的攻击特效 Blueprint。

## 验证记录

- `spaceEditor Win64 Development` 编译成功，未使用 Live Coding。
- 模型缩略图已检查，导入后轴向、厘米尺寸和 Shell / Panel / Core 材质槽顺序已确认。
- `JTS.Stellar.HeldWeaponPresentation` 使用真实奖池、目录与模型资产，逐一检查合法组合均登记在目录、Tab 能选中各组合、模型唯一且可见、握柄/枪口位置、骨架缩放隔离、攀爬收起及恢复、返回普通栏、无个人背包数据的模拟代理显示，以及未完成攻击的形态不消耗能量。
- 最终 13 项回归全部通过，0 失败，新增模型测试无警告。已有两项测试仍报告普通武器数据资产缺失的警告。
- PIE 使用实际玩家 Blueprint，通过 Enhanced Input 子系统注入 `CycleStellarAction` 验证完整输入处理和服务器选择路径：火＋爆炸选中核心槽 1，显示 `SM_StellarExplosion`、正确持枪动画、橙色核心材质和两格白色选中外圈；返回普通武器时恢复普通手持模型。自动化映射测试另行确认此动作绑定 Tab、1–9 返回普通栏。
- 测试报告：`Saved/StellarHeldWeaponTests/index.json`。远端显示由模拟代理测试覆盖，未在本次验证中启动完整多客户端联机测试。
- 构建日志：`Saved/Logs/StellarHeldWeaponBuild.log`。
- 编辑器检查日志：`Saved/Logs/StellarHeldWeaponPreview.log`。
