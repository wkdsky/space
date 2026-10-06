# 星际核心与配件加点界面

## 行为

- 点击核心即可查看材料并升级，不要求武器组合激活；备用格、圆形格和灰色菱形格中的核心使用同一规则。原有联机封格保持只读。
- 核心窗口为 480 × 280 的设计尺寸，沿用项目 DPI，并仅在空间不足时缩小，不再放大填满屏幕。只显示核心点数、固件材料、“加 1 点”和关闭按钮。
- 每次升级由服务器扣除材料、永久提高一级，增加一个核心点数。核心没有草案、应用或重置入口；材料不足、满级或请求等待中时按钮禁用。
- 配件只有与正上方圆形格中的正确核心形成有效组合时，才能打开分配窗口。是否已选中该武器不影响加点资格；激活但零预算的配件可以查看，不能增加点数。
- 配件移位、核心移位/更换导致组合失效，或物品被删除时，已打开的窗口自动关闭。备用格、错配和封格中的配件不能打开分配页。
- 配件保留原有分配草案和调整操作，可用点数由对应核心提供。界面与服务器都校验激活状态；未激活配件连全零分配/重置请求也会被拒绝。
- 材料显示与装备升级规则一致：结余固件、可用装备格中的旧存档固件、普通背包里的旧存档固件；封格和伪造普通物品标签不计入。商店核心沿用商店物品栏材料规则。

## 修改文件

- `Source/space/UI/JTSStellarAttachmentDialog.h` / `.cpp`：点击资格、小核心窗口、按钮状态、组合失效自动关闭。
- `Source/space/UI/JTSStellarLoadoutPanel.cpp`：核心/配件点击提示。
- `Source/space/UI/JTSShopWidget.cpp`：未激活配件提示。
- `Source/space/Components/JTSStellarLoadoutComponent.h` / `.cpp`：可用固件查询，服务端要求配件有效激活。
- `Source/space/Player/JTSPlayerState.cpp`：商店未组合配件禁止修改分配。
- `Source/space/Tests/JTSStellarWeaponTests.cpp`：未激活核心升级、单次扣料、配件配对、预算限制、窗口拦截、旧存档材料查询回归。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：未组合商店配件禁止重置的回归断言。

## 编译与验证

- UE 5.8 / `spaceEditor Win64 Development` 编译通过：`Saved/Logs/StellarProgressionBuild.log`。
- 星际武器、商店物品栏及响应式布局等 36 个用例通过（35 个首次通过；新增窗口用例修正测试夹具的 LocalPlayer Outer 后单独通过）。4 个既有资源加载警告未修改。
- 扩展回归另有 `JTS.Spacecraft.PossessionAndDisembark` 失败，单独复测仍失败：下船角色的 MovementMode 未达到 `MOVE_Walking`。本次没有修改下船、地表投影或角色运动逻辑，保留该问题记录。
- 报告：`Saved/StellarProgressionTests/index.json`、`Saved/StellarProgressionAccessTests/index.json`、`Saved/StellarProgressionDisembarkCheck/index.json`。
- D3D 实际画面验证完成（1650 × 1080 截图）：点击未激活的备用格暗核心，点数由 0 → 1，固件由 2 → 1；同一窗口切换到激活配件时显示对应核心的 1 点预算；移走配件后窗口自动关闭，再次点击不能打开。验证过程中没有保存 Blueprint 或数据资产改动。
- 截图：`Saved/StellarCoreCompact.png`、`Saved/StellarCoreAfterPoint.png`、`Saved/StellarActivatedAttachment.png`。运行记录中最后一次完整验证以 `STELLAR_UI_QA_COMPLETE` 结束：`Saved/Logs/StellarProgressionRuntime.log`。

## Editor 操作

无需修改 Blueprint 或资产配置。加载编译后的模块后，在游戏中点击星际核心即可升级；配件先正确配对激活再点击。
