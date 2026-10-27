# Jump to Space 主界面素材与制作记录

本目录保存可编辑源文件、导出文件、来源许可、导入脚本产物与验收证据。`asset_gate.json` 是正式界面制作前的素材验收；`asset_manifest.json` 列出每项源文件和 SHA256。

## 视觉与素材

- 工业卡通：炭灰、象牙白、航天橙，配冷色工作灯。地球蓝天白昼发射基地，前景宇航员，后景项目共享飞船。外星激光柱前端悬在远处高空，基地仍正常运作。
- Blender 原创：五套同骨架宇航服；五种眼睛、五种嘴型；基地建筑模块、塔架、地形、维修道具与机器人；图标、九宫格界面、标志、插画、程序纹理、八段展示动画、十八段 PCM 音频。
- `FrontEnd_Models.blend` 是可编辑场景。`PoseQA` 和 `UI/T_Thumb_*` 保存姿态检查与款式缩略图。
- Unreal 原创：参数材质、调色实例、灯光、四个镜头、60 秒维修机器人/蒸汽循环、场景背景静帧。`L_FrontEndArtLab` 保留独立素材检验场景。

## 外部来源与改编

| 来源 | 许可 | 实际使用与修改 |
|---|---|---|
| [Poly Haven: Hangar Concrete Floor](https://polyhaven.com/a/hangar_concrete_floor)，Dimitrios Savva | CC0 | 仅地面纹理来源；冷色重调、降低对比与法线幅度、提高粗糙度、抑制 AO。原文件在 External，游戏使用 Textures 中的改编文件。 |
| [Adobe Source Han Sans SC 2.005R](https://github.com/adobe-fonts/source-han-sans/releases/tag/2.005R) | SIL OFL 1.1 | 重命名 JTS UI Sans，调整数字与标点字宽，保留中文覆盖。许可与修改说明见 Fonts。 |
| [Quaternius Ultimate Modular Characters](https://quaternius.com/packs/ultimatemodularcharacters.html) | CC0 | 仅骨架兼容性参考。新宇航服网格、权重、面部与展示动画原创。 |
| 项目 SourceArt/TwinforkR1 | 项目原始资产 | 制作展示副本，焊接、重算法线、倒角、重新配色，删除玩法内部部件。 |
| 用户截图 | 仅设计参考 | 未提取截图像素、标志或模型。 |

原始 URL、许可副本、文件校验值见 `sources.json` 和 `External/`。所有在线美术来源均经过适配处理。

## 正式页面与流程

主菜单 → 四格存档 → 新建/续玩建队设置 → 连接状态 → 四人大厅 → 三秒出发倒计时 → 由服务器前往新游戏或保存的玩法地图。

加入队伍支持公开浏览、房间码、密码和分页。大厅支持准备、房主出发、复制码、关闭加入、语音静音、队员静音/踢出与离队确认。单人沿用同一条远征流程。

宇航员页支持款式、眼睛、嘴型、头盔/上装/下装颜色六项独立五选一，以及随机、默认、草稿预览、应用保存。局部偏好属于 LocalProfile；复制状态属于 PlayerState。打开大厅外观页会取消准备。

设置含显示模式、分辨率、质量、垂直同步、帧率，15 秒保留/恢复；音乐/音效/语音与设备；鼠标灵敏度、垂直反转、操作说明、界面缩放、减少背景动态。键位说明暂不提供重新绑定。

## 编辑器与重建

1. 使用 `frontend_blender_client.py` 连接 Blender MCP，制作模型、图形、音频和动画源文件。
2. 通过 `frontend_art_mcp.py` 注册项目 Unreal MCP 工具，执行 import、lab、validate。
3. 运行 `frontend_asset_manifest.py`，通过素材门槛后才执行 UI/正式地图配置。
4. `frontend_build_ui_unreal.py` 通过 Unreal UMG Toolset 创建可编辑 Widget Blueprint；`frontend_maps_unreal.py` 从独立场景配置正式入口和大厅。
5. 样式布局在 `/Game/Space/FrontEnd/Widgets`，款式与调色在 `Data/DA_FE_AvatarCatalog`，镜头与位置在地图 Stage 实例。运行时 C++ 不硬编码这些资产。

构建、PIE 操作与多人验证结果以本目录最终报告为准。

蓝天与悬空激光的补充制作：`frontend_sky_blender.py` 生成原创晴空纹理；`frontend_threat_blender.py` 制作激光前端、光晕和能量环，源文件为 `FrontEnd_Threat.blend`；`frontend_daylight_unreal.py` 导入并配置材质。正式地图使用白昼灯光、远景悬空激光和日常维修循环，激光没有地面撞击或伤害行为。

发行许可副本保存在 `Content/Space/FrontEnd/Legal`，已配置 NonUFS staging。最终实现、编辑器操作和验证范围见 `IMPLEMENTATION.md` 与 `validation_report.json`；界面截图见 `QA/`。

Presentation corrections and authoring steps: [PRESENTATION_FIX.md](PRESENTATION_FIX.md).
