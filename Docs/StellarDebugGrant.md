# 星际商店 DEBUG 全套物品

星际商店页面顶部新增 `DEBUG 全套物品` 按钮。每次点击，服务器从飞船配置的 `StellarLootTable` 中读取核心和配件，每种物品尝试加入商店物品栏一个。当前配置为 5 种核心、12 种配件，共 17 件。

- 不发放升级固件，也不发放其他非核心、非配件材料；重复的物品表行按 ItemId 去重。
- 仅填充玩家自己的 30 格商店物品栏空位，不覆盖已有物品，不直接装备，不消耗飞船资源。
- 空位不足时保留已发放部分，提示加入数量和未加入数量。再次点击会再次尝试发放整套。
- 按钮通过玩家控制器可靠 owner RPC 提交请求。服务器验证当前远征飞船、人物、终端距离和地表可交互状态，再修改现有复制物品栏。
- 使用现有物品实例与拖拽标识；发放后可直接拖到人物星际栏，无需遥感揭晓动画。

## 修改文件

| 文件 | 作用 |
| --- | --- |
| `Source/space/UI/JTSShopWidget.h/.cpp` | 按钮、点击请求、等待状态与发放结果提示；沿用终端响应式缩放 |
| `Source/space/Player/JTSPlayerController.h/.cpp` | 服务器请求与客户端结果 RPC、当前飞船验证 |
| `Source/space/Ships/JTSSpacecraftActor.h/.cpp` | 数据表筛选、逐个填充空位、结果计数和终端权限验证 |
| `Source/space/Tests/JTSSpacecraftRegressionTests.cpp` | `JTS.Spacecraft.StellarDebugGrant` 回归测试 |

没有新增独立游戏系统或更改 Blueprint 配置。

## Editor 操作

使用编译后的编辑器运行 `L_SpaceWorld`，靠近飞船按 `E` 打开终端，切换到星际商店，点击页面顶部的 `DEBUG 全套物品`。无需重新连接蓝图。

## 验证

- `spaceEditor / Win64 / Development` 完整编译通过，未使用 Live Coding；日志：`Saved/Logs/StellarDebugGrantBuild.log`。
- `JTS.Spacecraft.StellarDebugGrant`、`JTS.Spacecraft.ShipLockerAndStellarLoot`、`JTS.Spacecraft.StellarFirmwareDrag` 共 3 项自动化测试通过，0 项失败。报告：`Saved/StellarDebugGrantTests/index.json`。
- 新测试覆盖当前配置全量物品、重复行去重、固件及其他材料排除、立即可用的唯一物品实例、保留已有物品、空位不足和满栏、资源不扣除、未直接装备、当前飞船 RPC 验证、越界距离和客户端角色拒绝。
- 原有物品栏、固件拖拽测试存在项目既有普通武器数据资产缺失警告，测试使用现有原生后备定义通过。
- PIE 实测按钮位置、字号与商店栏显示；触发实际按钮已绑定的 `OnClicked` 委托，经 owner RPC 后首次加入 17 件，第二次填剩余 13 格并提示 4 件未加入，满栏后提示 0 / 17、17 件未加入。没有模拟成功的物理鼠标点击，也未开展双客户端网络实测。
- 预览保留首次发放的一套 17 件物品；清理了重复点击测试产生的额外物品，没有修改数据资产或关卡配置。日志：`Saved/Logs/StellarDebugGrantPreview.log`。
