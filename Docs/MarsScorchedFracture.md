# Mars 破坏动画：修订 10

本次修正漏光在 7 秒后切回旧效果、主板块提前错开释放、20 秒后慢速段卡顿三个问题。第一套 Mars 模型负责完整状态和整个破坏过程，第二套残骸只在 33.5 秒全黑期间替换。独立电影场景保留项目原有星空。

## 直接查看与 Editor 操作

- 完整视频：`SourceArt/MarsDestruction/ScorchedFractureV3/Mars_Scorched_Fracture_V3_Unreal.mp4`，1920×1080、60 fps、40 秒、无音轨。
- 局部检查：同目录 `Mars_Leakage_4to12_Review.mp4` 覆盖 7 秒之后及同步爆开；`Mars_LateMotion_20to32_Review.mp4` 检查后半段运动。
- 分镜：同目录 `Unreal_Storyboard_V3.jpg`。
- 游戏视频副本：`Content/Movies/MarsDestruction.mp4`，H.264、yuv420p、每秒一个关键帧，文件头前置。
- Editor 资产目录：`/Game/Space/Environment/MarsDestructionPreview/ScorchedFractureV3/`。
- 打开 `L_MarsScorchedFracture_Preview`，点击 Play，自动播放一次 40 秒，结束后保留第二套残骸。
- 打开 `LS_MarsScorchedDestruction`，编辑动画、材质、灯光、相机和黑屏。
- 视频使用 `FMS_MarsDestruction`、`MP_MarsDestruction`、`MT_MarsDestruction`、`M_MarsDestruction_UI`。双击 Media Player，打开 File Media Source 可检查视频；用于 UMG 时把 UI 材质指定给 Image，在现有剧情流程调用 Media Player 的 Open Source。
- File Media Source 保存相对路径 `./Movies/MarsDestruction.mp4`，启用预缓存。发布打包时在电影配置中包含 `MarsDestruction`，并确认没有排除 Movies；本轮没有进行整游戏打包。

当前仍是独立电影关卡及可播放媒体资产，尚未接入正式 SpaceWorld 的剧情触发、实际玩家飞船和多人同步。预览关卡 Play 使用实时序列；Media Player 播放离线视频。

## 漏光连续性与同步释放

所有阶段使用同一套暖色裂隙光提取、深度遮挡、有限传播长度及平滑光束末端，不再在 7 秒后切回原先的长光束样式。散射中心固定在原星球中心，没有跟随板块移动的光束卡片。

`CrackLeakage` 强度按 4 秒 0.22、5 秒 0.48、6 秒 0.78、7 秒 1.0、8 秒 1.25 持续增强，爆开后 8.5 秒 0.70、9 秒 0.40、10 秒 0.18、11 秒 0.10、14 秒 0.03、17 秒 0.005、19 秒归零。后段额外补偿裂口扩大导致的光源暴露面积增加，避免参数下降而画面反而更亮。球心点光同样在 8 秒达到峰值，之后衰减到 19 秒。只衰减辐射，固定软体积不收缩，没有带清楚球面边界的发光球。

8 秒之前主板块没有刚体位移或旋转。先用裂沟 Morph、断口局部退让和原地壳的小岩片向内塌落表现撕裂，随后所有实体在 8 秒共同开始向外释放。保留大小不同带来的速度差、轻微转动、指数减速和 30% 持续漂移项，至全黑前仍然向外移动。

## 卡顿原因与输出方式

对旧版 Unreal 资产在 20–21 秒按 60 Hz 比较原始姿态与压缩后姿态，原始大板块连续运动，而默认 ACL 压缩/抽帧后的代表板块在 60 个采样间隔中有 56 个没有位移，随后跳变。原始单步约 0.023–0.025 个局部厘米，压缩后跳变可达约 0.402 个局部厘米，关卡实例还会放大 40 倍。旧版 MP4 的 30 fps 容器及不同图像帧没有消除这个源动画问题。

本次只为该电影动画创建 `ABS_MarsCinematic`：Least Destructive 压缩、保留全部源帧、线性插值、禁用 Frame Stripping。电影实例始终更新骨骼且禁用 Update Rate Optimization，不改变项目全局动画压缩设置。

`validate_mars_animation_sampling.py` 对 187 个实体在 20–21、25–26、31–33 秒按 60 Hz 比较 RAW 和 COMPRESSED 姿态，要求每个间隔均移动、单步运动与原始姿态相符。同时检查所有主实体在 8 秒前保持原位、8 秒后同时释放。

视频由 Unreal Movie Render Queue 按固定时间步长渲染 2400 张 1080p 图像，逐张编码为 60 fps，没有通过重复截图或视频补帧提高标称帧率。骨骼源曲线按 30 Hz 创作，以线性插值在 60 Hz 求值，材质、照明与后处理也在各输出时刻重新渲染。离线渲染耗时不会改变视频中的运动时间。

## 实体几何与时序

187 个闭合实体、188 根骨骼、90,764 三角面、两种岩体材质。126 块窄岩片从原地壳中真实切出并从母块扣除；没有复制地表。分块保留原体积约 99.86%，其中 19 块完全位于内部，内部光散去后仍有实体岩石。4 秒时窄岩片径向塌落至少约 0.0658R。

三条 Morph 分别负责深沟 `TectonicRift`、不规则断口退让 `RiftOpening` 和后期边缘烧蚀 `ScorchedRim`。最大沟深约 0.1107R，局部断口位移约 0.0713R，烧蚀损失体积约 1.80%。表面保留从内部根部向外推进的稀疏分叉热流、亮黄热核、橙红前缘与暗红冷却余热。

| 时间 | 表现 |
| --- | --- |
| 0–约 0.8 秒 | 完整地表 |
| 约 0.8–4 秒 | 初裂漏光、窄岩片向内塌落、裂沟加深 |
| 4–8 秒 | 裂隙漏光持续增强，主板块保持原位 |
| 8 秒 | 内部光最强，所有主板块同时开始向外释放 |
| 8–19 秒 | 大块慢、小块稍快，内部光逐渐衰减，热流从内向外推进 |
| 19–32 秒 | 可见内部实心碎块，地表炭化，持续减速外移 |
| 32–33 秒 | 渐黑，运动继续 |
| 33–34 秒 | 全黑，33.5 秒替换第二套残骸、碰撞及预览相机 |
| 34–35.5 秒 | 淡入残骸视角 |
| 35.5–40 秒 | 第二套探索残骸，保留星空 |

第二套 107 块原始残骸按实体体积估计收拢比例，统一缩放 1.65243；没有用其炸开后的外包尺寸限制第一套模型的外移。

## 修改文件与重建

| 文件 | 本次内容 |
| --- | --- |
| `Tools/Art/build_mars_scorched_fracture.py` | 同时释放、取消主板块提前撬动、连续漏光曲线、修订 10 与视频帧率 |
| `Tools/Art/import_mars_scorched_fracture.py` | 统一漏光 shader、专用动画压缩资产、实例更新配置 |
| `Tools/Art/render_mars_scorched_unreal.py` | 真正逐帧 60 fps 输出，正确区分序列帧和输出帧 |
| `Tools/Art/assemble_mars_scorched_unreal_movie.py` | 编码、前后段重复帧检查、黑屏检查与游戏视频副本 |
| `Tools/Art/assemble_mars_scorched_unreal_storyboard.py` | 新时序分镜与当前报告归档 |
| `Tools/Art/validate_mars_scorched_fracture.py` | 同步释放、提前静止、持续减速及原几何检查 |
| `Tools/Art/validate_mars_scorched_runtime.py` | 连续 Play 检查到 8 秒的增强及爆开后的衰减 |
| `Tools/Art/validate_mars_animation_sampling.py` | 新增实际压缩姿态逐步检查 |
| `Tools/Art/validate_mars_leakage_render.py` | 实际渲染画面中星球轮廓外的暖光增强、爆开后衰减检查 |
| `Tools/Art/configure_mars_cinematic_media.py` | 检查 Unreal 媒体解码为 1080p、60 fps、40 秒 |
| `SourceArt/MarsDestruction/ScorchedFractureV3/` | blend、FBX、数据、视频、分镜及报告 |
| `Content/Space/Environment/MarsDestructionPreview/ScorchedFractureV3/` | 更新的动画、材质、序列、关卡、压缩及媒体资产 |
| `Content/Movies/MarsDestruction.mp4`、本说明 | 游戏视频与制作记录 |

重建顺序：Blender 后台执行 build 和几何验证；Unreal commandlet 执行导入及姿态采样验证；完整 Editor 执行连续 Play 验证；Editor 带 `-EnablePlugins=MovieRenderPipeline -RenderOffscreen` 执行 MRQ；Python 执行视频及分镜组装；Editor 执行媒体解码验证。

MRQ 请求文件 `Saved/MarsDestruction/cinematic_render_request.json` 的 start/end 使用序列显示帧率 30，即完整片段为 0–1200；60 fps 输出为 0–2399。当前输出目录 `Saved/MarsDestruction/offline_r10`。所有当前报告均为修订 10：几何、姿态采样、MRQ、连续 Play、视频、媒体；实际数值见同目录 JSON。

没有修改 C++，无需 C++ 编译。Blender MCP 本机连接不可用，使用 Blender 后台脚本制作与验证，再用独立 Unreal 进程导入和渲染。没有修改正式 SpaceWorld 及无关玩法。

正式剧情接入仍由既有服务器规则决定开始时间和最终状态；客户端播放共同时间的视频，在全黑期间切换已配置的探索残骸与实际飞船。最终状态归 Expedition snapshot 管理，资产和镜头引用由 Blueprint 配置。

## 参考范围

此前参考 [ILM Jedha Destruction 制作资料](https://history.siggraph.org/wp-content/uploads/2022/09/2017-Talks-Moloney_Rogue-One-A-Star-Wars-Story-Jedha-Destruction.pdf)的地形变形与分层碎屑编排、[ILM《星际迷航》](https://www.ilm.com/vfx/star-trek/)的多层破坏效果，以及 [SideFX Pyro Lookdev](https://www.sidefx.com/docs/houdini/pyro/lookdev.html)的热核、前缘和冷却颜色层次。动画是按游戏镜头编排的受限破坏，没有进行行星自引力或真实温度模拟。
