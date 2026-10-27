# 2026-10-27 主界面表现修正

本次针对人物错位、飞船穿模、手部姿势、待机僵硬和天空激光不明显进行修正。

## 实现与文件

- `FrontEnd_Models.blend`、`Animations/A_FE_Idle.fbx`：通过 Blender MCP 原创重做 12 秒循环。脚前后错开，膝盖微屈，双肘不对称弯曲，手指自然收拢；加入呼吸、重心变化和轻微转头。
- `Tools/Art/frontend_pose_polish_blender.py`：可重复生成该动画。导出时使用独立的厘米骨架，保留原始米制 Blender 文件。
- `Tools/Art/frontend_animation_basis_unreal.py`：将新动画转换到现有 Root=100 的骨骼单位。独立 `BCS_FE_Presentation` 采用保真压缩，保留细小的重心变化；不修改玩法动画或共享骨架。
- `FrontEnd_Threat.blend`、三个 `SM_FE_Threat*` 模型、`Textures/T_FE_ThreatHeatCloud.png`：通过 Blender MCP 原创制作圆润激光前端、光晕、等离子弧与 1024×1024 程序热云纹理。
- `Tools/Art/frontend_presentation_polish_unreal.py`：通过 Unreal MCP 导入上述素材、配置材质、创建大厅展示子类并校准关卡。入口人物与平台共用落点；飞船起落架距离地坪约 1 cm，与后方门框/墙体保留约 2.9 m 间隔。补齐基地远景地面。
- `Content/Space/FrontEnd/Maps/L_Entry`、`L_PreLaunchLobby`：保存站位、镜头和激光配置。全身镜头保留脚底；激光前端悬于建筑上方，白热核心外有红橙色大气热晕。
- `Content/Space/FrontEnd/Blueprints/BP_FE_LobbyAstronaut`：大厅专用展示角色；Earth GameMode 继续选择玩法角色，进入游戏后恢复玩法动画。
- 大厅胶囊高度由展示子类的实际 Mesh 相对偏移和平台顶面计算，避免沿用入口预览偏移而让鞋底陷入平台。
- `frontend_import_impl.py`、`frontend_maps_unreal.py`：重建流程接入本次修正，避免再次导入或重建地图时恢复旧站位。

## 来源

新动画、激光网格和热云纹理均为本项目原创。宇航员沿用此前原创服装及兼容骨架，飞船沿用项目原始 Twinfork 的展示改编版本。

能量效果的明暗和色彩层次参考[《星球大战：原力觉醒》官方图集](https://www.starwars.com/films/star-wars-the-force-awakens-gallery)及[Starkiller Base 官方资料](https://www.starwars.com/databank/starkiller-base)。影视图片仅供观察，未复制到游戏贴图、模型或发行内容；来源记录在 `sources.json`。

## Editor 操作

1. 打开 `L_Entry`，单人 PIE 可查看主菜单及五种服装。打开宇航员页面切换全身和面部镜头。
2. 在地图中的 `FE_MainPresentation` / `FE_LobbyPresentation` 配置镜头，在 `FE_CrewPad_*` 配置站台，在 `FE_TwinforkDisplay` 配置停机位置。
3. `FE_AlienThreat_*` 和 `FE_ThreatHeatedAtmosphere` 控制远景激光与热云。维持蓝天和正常运转的基地，激光不触地。
4. 重新制作素材时，先经 Blender MCP 运行 `frontend_pose_polish_blender.py` 和 `frontend_threat_polish_blender.py`，再由 Unreal MCP 执行 `presentation_polish/import`。刷新素材清单，通过门槛后，对 entry 与 lobby 分别执行 `presentation_polish/scene`。

## 验证

实际引擎截图、循环姿势采样、四人大厅与 travel、编译和存档恢复记录见 `presentation_fix_validation.json`。本次未修改 C++；关闭编辑器后使用现有 `spaceEditor Win64 Development` 配置正常构建，结果为 Succeeded（目标已是最新）。
