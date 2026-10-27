# 主界面重制交付记录

## 画面与素材

正式入口：`/Game/Space/FrontEnd/Maps/L_Entry`。正式组队大厅：`/Game/Space/FrontEnd/Maps/L_PreLaunchLobby`。

最终方向为地球晴朗白昼。前景是可定制宇航员，后方为项目 Twinfork 飞船、维修设施和照常作业的机器人。蓝天中露出外星激光柱的前端和能量环，悬在高空，没有触地爆炸或灾难警报。机器人和蒸汽采用 60 秒循环；减少动态选项暂停此循环。天空威胁保持悬停，不在菜单中真正击中地球。

`asset_manifest.json` 逐项记录 223 个源文件及 SHA256；`asset_gate.json` 记录先准备素材再制作正式界面的门槛。`README.md`、`sources.json`、`External`、`Fonts` 保存来源、改编说明与许可。新增激光来自 `FrontEnd_Threat.blend`，三个模型完全原创。

## 页面与流程

- 主菜单 → 四格存档 → 新建或继续的建队设置 → 连接状态 → 组队大厅。
- 加入队伍 → 按房间码搜索或浏览公开房间 → 选择房间、填写可选密码 → 连接 → 大厅。
- 大厅 → 所有人（含房主）准备 → 房主出发 → 三秒倒计时、关闭加入 → 服务器前往新游戏或存档地图。
- 宇航员 → 眼睛、嘴型、款式、头盔、上装、下装各独立五选一 → 草稿预览 → 应用保存或取消恢复。
- 设置 → 显示、音量、语音设备、鼠标、辅助选项；显示修改有 15 秒确认与恢复。自定义画质显示为“自定义”，取消或超时会恢复每项画质与分辨率比例。界面缩放范围 80–100%。
- 存档删除、踢出、离队、退出均有确认。大厅修改外观会取消准备。键位页提供操作说明，暂不提供重新绑定。

## 修改位置与职责

| 位置 | 内容 |
|---|---|
| `Content/Space/FrontEnd` | 可编辑 Widget Blueprint、地图、角色、动画、材质、图标、字体、音频、原始许可副本 |
| `Content/Space/Blueprints/Player/BP_JTSPlayer_Casual_2` | 选择五款外观共用目录与兼容角色动画 |
| `Source/space/UI/FrontEnd` | 通用菜单路由、按钮事件、设置模型、外观数据类型与 HUD |
| `Source/space/Components/JTSAvatarAppearanceComponent.*` | 应用目录中的网格、独立面部图案和三段配色 |
| `Source/space/Systems/JTSLocalProfileSubsystem.*` | 本机外观与操作、声音、辅助偏好 |
| `Source/space/Player/JTSPlayerState.*`、`JTSPlayerController.*` | 外观复制、服务器校验、准备与出发意图 |
| `Source/space/Modes/JTSPreLaunchLobbyGameMode.*`、`Core/JTSGameState.*` | 大厅摆位、倒计时、复制状态与服务器 travel |
| `Source/space/Systems/JTSExpeditionSubsystem.*`、`JTSOnlineSessionSubsystem.*` | 存档版本迁移、排除大厅快照、成功建房后选择存档 |
| `Source/space/World/JTSEntryPresentationStage.*`、`JTSPreLaunchLobbyStage.*` | 编辑器配置镜头、预览角色和队员位置 |
| `Config/DefaultEngine.ini`、`DefaultGame.ini` | 新启动地图、Widget 类、打包地图与许可 staging |
| `Tools/Art/frontend_*` | Blender MCP 与 Unreal MCP 的源素材制作、导入、UMG、地图配置和验证脚本 |

C++ 不包含具体角色网格、场景 Actor、材质或音频路径。款式、调色与预览动画由 `DA_FE_AvatarCatalog` 配置，角色 Blueprint 选择 `AvatarCatalog`。关卡中的 Stage 实例配置镜头与队员站位。

## 编辑器使用

1. 打开项目即可进入新入口；也可在 Content Browser 打开上述两张正式地图。
2. 单人流程从入口 PIE 开始。四人联机验证使用 Listen Server；发布用的大厅保留构建版本校验。
3. 编辑 `Widgets/WBP_FE_Main` 和 `WBP_FE_Lobby` 调整布局，编辑 `Data/DA_FE_AvatarCatalog` 调整款式、配色与缩略图。
4. 修改入口地图的 `FE_MainPresentation` 镜头视图；修改大厅 `FE_LobbyPresentation` 的镜头和 PresentationOffsets。
5. 调整远景 `FE_AlienThreat_*` 的位置与比例。它们无碰撞、无伤害逻辑；只负责菜单叙事。
6. 正常 Build `spaceEditor Win64 Development`，避免 Live Coding。修改 Widget 类结构后重启编辑器再验证。

## 验证范围

结果与证据见 `validation_report.json`，界面截图位于 `QA/`。完整测试原始数据保留在 `Saved/FrontEndQA`。

EOS/Steam 真实账号、跨机器网络、语音收发及打包后的平台认证需要对应服务环境；本次不把 NULL/PIE 验证等同于这些服务已验收。语音继续使用现有 provider 所属实现，没有新增音频传输或写入凭证。

Presentation corrections (2026-10-27): see [PRESENTATION_FIX.md](PRESENTATION_FIX.md) and `presentation_fix_validation.json`.
