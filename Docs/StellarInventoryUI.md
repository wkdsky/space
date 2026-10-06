# 人物物品栏与星际装备栏调整

## 实现内容

### 尺寸与分辨率修正（2026-10-04）

- 项目原先已有 UE 默认的最短边 DPI 曲线；问题在于格子固定为 48 高、文字固定为 10 号、格宽始终按 9 格分摊，以及菜单没有避开装备区。此次修正布局与字号，不重复叠加 DPI 缩放。
- 普通格统一使用 96 × 88 的 1080p 基准尺寸、18 号文字和 8 点格间距。按实际可见格数居中，格数少时仍保持相同大小。半透明格、编号和名称保留。
- 圆形与菱形半径从 28 提高到 38，同一列垂直对齐。橙色备用格的中心与普通栏中心对齐。两栏共同计算位置；窄画面一起左移或缩小，预留底部和左右边距。
- 飞船菜单按可用屏幕和装备区顶部计算边界，菜单底部与装备区至少留 24 个逻辑单位。使用 1320 × 720 的设计尺寸，由 ScaleBox 按剩余空间缩放。
- 菜单采用更深的背景与清晰的字号层级：标题 32、商品名称 24、价格和主要按钮 18、辅助文字至少 16。按钮单行排版，长名称省略，完整名称与说明放入悬浮提示。商品及仓库悬浮说明使用 18 号文字。
- 新增 `FJTSGameUILayout` 仅计算 UI 边界，依赖 UMG 已完成 DPI 换算的逻辑尺寸；HUD 和终端共用它。没有新增 Gameplay 系统或改变背包解锁数量。
- 当前普通枪械的弹药数字移到对应的普通物品格下方，切换普通武器时位置随格子变化；换弹期间数字保留，并显示进度条。能量型普通枪械的状态也归到对应格下方，星际能量仍使用原位置。
- `Tab` 从普通武器切入时选中最左侧已激活的星际组合，后续按键依次切换并循环。只选择可用圆形槽与其正下方菱形的正确组合；错误配对、备用槽、未完整装备和封闭列均不参与。没有可用组合时保留普通栏选中状态。
- 使用星际武器时，`1–9` 返回对应已解锁的普通格，包括空格和原先选中的同一格。切回普通格后再按 `Tab`，重新从第一个激活的星际组合开始。普通背包解锁数量不变。
- `Z` 根据当前装备处理：普通枪械主动换弹，已选星际武器继续切换星际武器。`T` 保留为普通枪械换弹的备用键。换弹继续提交给现有服务器 RPC。星际切换通过可靠的 owner RPC，由服务器根据最新装备状态逐次计算，避免连续按键依赖尚未同步的客户端选中状态。

设计参考：[FFXIV 热键栏布局和尺寸调整](https://na.finalfantasyxiv.com/blog/002170.html)、[No Man’s Sky 库存界面改进](https://www.nomanssky.com/waypoint-update/)、[Xbox 文字可读性建议](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/101)、[Unreal DPI 缩放机制](https://dev.epicgames.com/documentation/en-us/unreal-engine/dpi-scaling-in-unreal-engine)。采用统一格尺寸、清晰层级、拖拽管理和安全边距；没有新增玩家自定义 HUD 缩放设置。

### 装备与拖拽规则

- 普通人物物品栏只保留半透明格子、编号和物品名称，去掉整块底板、标题及常驻抓墙提示。
- 星际装备栏放在普通人物物品栏右侧。1 / 2 / 3 / 4 名远征玩家分别显示 4 / 3 / 2 / 1 组圆形与下方灰色菱形，另有一个左下方橙色备用槽。
- 星际槽只接收核心或配件。允许自由摆放、交换位置；只有核心位于圆形，兼容配件位于其正下方灰色菱形，才激活该列武器。备用槽和跨列组合不参与激活。
- 每个正确组合都显示覆盖上下两格及间隙的一团柔和光晕。火 / 水 / 光 / 暗 / 药剂核心分别采用橙红、蓝、金、紫、绿；颜色配置在现有 `DA_StellarAllianceLoot` 的核心条目 `ActivationColor` 中。
- 飞船终端删除重复的人物星际槽，保留并扩大 30 格商店物品栏。星际商品可在商店仓库内移动，也可拖到人物星际栏，装备可拖回商店仓库。
- 拖动星际物品时，普通人物格变灰。非法放置保留源物品；结束、取消拖拽及关闭终端会恢复普通格颜色。
- 服务器拒绝将星际商品拖入、交换进或通过“取到角色背包”送入普通人物物品栏。旧存档中的人物背包星际物品仍可迁入空的星际槽，旧组合武器可拆为一组上下槽；已封闭的装备仍保留，人数减少后恢复。

## 修改文件

| 职责 | 文件 |
| --- | --- |
| HUD 与终端共用的安全边界计算 | `Source/space/UI/JTSGameUILayout.h/.cpp` |
| 分辨率和栏位数量边界验证 | `Source/space/Tests/JTSResponsiveUILayoutTests.cpp` |
| 普通枪械 Z 换弹与星际快捷键分派 | `Source/space/Player/JTSCharacter.h/.cpp` |
| 槽位规则及配对 | `Source/space/Items/JTSStellarLoadoutTypes.h/.cpp` |
| 核心颜色配置 | `Source/space/Items/JTSStellarLootTable.h/.cpp`、`Content/Space/Data/Shop/DA_StellarAllianceLoot.uasset` |
| 服务器装备操作 | `Source/space/Components/JTSStellarLoadoutComponent.cpp` |
| 星际切换意图和服务器顺序选择 | `Source/space/Components/JTSStellarWeaponComponent.h/.cpp` |
| 服务器商店至人物背包限制 | `Source/space/Player/JTSPlayerState.cpp` |
| 形状布局、连续光晕、命中检测 | `Source/space/UI/SJTSStellarLoadoutView.h/.cpp` |
| 人物星际槽交互 | `Source/space/UI/JTSStellarLoadoutPanel.h/.cpp` |
| 普通人物栏布局及灰色反馈、提示删除 | `Source/space/UI/JTSPrototypeHUDWidget.h/.cpp` |
| 商店仓库布局及拖拽 | `Source/space/UI/JTSShopWidget.h/.cpp` |
| 鼠标整理模式和 UI 拖拽状态 | `Source/space/Player/JTSPlayerController.h/.cpp`、`Source/space/Player/JTSCharacter.cpp` |
| 回归验证 | `Source/space/Tests/JTSStellarWeaponTests.cpp`、`Source/space/Tests/JTSSpacecraftRegressionTests.cpp` |

新 Slate 视图仅负责绘制、布局及命中检测，依赖现有星际装备组件与物品数据表；UMG 面板负责鼠标交互，现有服务器组件负责规则与复制。未新增独立游戏系统。

## Editor 操作

无需重建 Widget Blueprint 或重新连接蓝图。

1. 使用重新编译后的编辑器运行 PIE。
2. 游戏中按 `Tab` 循环选择激活的星际武器，按 `1–9` 返回对应普通格。按 `I` 打开鼠标整理模式，拖动星际槽内物品；按 `I` 或 `Esc` 返回游戏操作。飞船终端内 `Tab` 仍用于切换商店页面。
3. 打开飞船终端，星际商品先留在右侧 30 格商店仓库，拖到画面底部人物星际栏即可装备。普通商品仍可取到普通人物背包。
4. 需要调整光晕颜色时，打开 `DA_StellarAllianceLoot`，修改对应核心条目的 `ActivationColor`。透明颜色会使用稳定的默认颜色。

## 编译与验证

- `spaceEditor / Win64 / Development` 编译成功，未使用 Live Coding。
- `JTS.UI.ResponsiveInventoryLayout`、`JTS.Character.NormalWeaponReloadShortcut`、`JTS.Stellar` 全套及 `JTS.Spacecraft.ShipLockerAndStellarLoot`、`JTS.Spacecraft.StellarFirmwareDrag` 共 12 项自动化测试通过，0 项失败。
- 布局自动化覆盖 1280×720、1280×800、1920×1080、2560×1440、3840×2160、2560×1080、1440×1080，每种分辨率检查 1–9 个普通格及 1–4 组星际栏，共 252 种组合。检查边缘留白、栏位互不重叠、菜单与装备区间隔、格尺寸以及橙色格对齐。
- 在 720p、1080p 和 2K 的 PIE 窗口中实际检查 HUD 与飞船菜单。普通名称可读、格子未越界、菜单不覆盖装备区、DEBUG 按钮保持单行。Windows 150% 缩放下，2K 窗口实际游戏视口为 2551×1443，DPI 为 1.3361；4K 与宽屏完成自动化边界验证，未进行实机截图检查。
- 覆盖人数对应槽位数、同列上下配对、错误位置及配件、服务器类型限制、空/已占用普通格拒绝、仓库移动、旧组合武器拆分、重复拖拽防复制和原有星际武器行为。
- 普通换弹测试覆盖开枪扣弹、Z 主动换弹、服务器计时器补满弹夹、满弹夹拒绝重复换弹、普通枪械按 Z 不误选已有星际组合，以及星际状态下 Z 不触发普通枪械换弹。
- `JTS.Stellar.ActivatedWeaponKeyboardSelection` 通过真实键位映射和绑定的输入回调验证：普通枪械按 Tab 进入首个激活组合、不误换弹、跳过错误/反向/缺件组合、单个组合保持选中、多个组合依次循环、数字键返回同一普通格或空格、1–9 全部返回对应已解锁普通格、未解锁数字键不误切、无可用组合保留普通状态、多人封闭列排除，以及非游戏阶段阻止快捷键。
- 前次 PIE 实测：磁轨手枪开一枪后，`11 / 12` 位于其格子下方；按 `Z` 出现黄色换弹进度，完成后显示 `12 / 12`。切到第二格合金短刃时隐藏，切回第一格时恢复。
- 本次 PIE 实测：飞船终端内 Tab 从普通商店切到星际商店；游戏中 I 打开/关闭整理模式，整理期间数字键被阻止，关闭后数字键恢复普通栏选择；没有激活组合时 Tab 保持普通武器和游戏鼠标模式。多个激活组合循环与 1–9 返回由自动化测试验证，未在实机窗口额外填充装备或开展双客户端网络测试。更新后的游戏预览保持开启，无需蓝图操作。
- 最终画面：[弹药位置](../Saved/ResponsiveInventoryUI/Ammo-under-slot.png)、[飞船终端与底部栏](../Saved/ResponsiveInventoryUI/Terminal-responsive.png)。
- 其中 2 项测试存在项目既有 `DA_Item_RailPistol`、`DA_Item_PowerHammer`、`DA_Item_AssaultRifle` 数据资产缺失警告；物品库使用现有原生后备定义，测试仍通过。本次未修改这些资产引用。
- 最新构建日志：`Saved/Logs/StellarKeyboardBuild.log`。
- 最新自动化报告：`Saved/StellarKeyboardTests/index.json`。
- 实机检查日志：`Saved/Logs/StellarKeyboardPreview.log`；前次布局及弹药检查：`Saved/Logs/ResponsiveInventoryUIPreview.log`、`Saved/Logs/ResponsiveInventoryUIPreviewFinal.log`。
