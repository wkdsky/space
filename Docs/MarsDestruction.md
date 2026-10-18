# Mars 破坏动画：缓慢板块分离 V2

> 当前方案已更新为 [焚烧与真实分裂 V3](MarsScorchedFracture.md)。本文件保留 V2 的制作记录；新版从第一套模型真实切割，黑屏后再显示第二套残骸。

适用场景：玩家在飞船上或远处观看，结束后再接近残骸。当前交付为 Blender 动画、FBX、预览视频和独立 Unreal 演示关卡。正式 Expedition 触发、联机状态和探索碰撞尚未接入。

## 本次修正

上一版错误地把末态放大到完整模型的 1.3 倍，让额外地壳飞出 1.3–2.4 个半径，并用短促闪光和冲击环强化快速抛射。V2 已去除这些内容，重新制作运动，而非单纯降低播放速度。

原有 107 块残骸缓慢向外分离：大块慢、小块稍快，释放后速度持续下降。终点严格回到第二套模型原本的相对布局。没有第二批飞远的地壳，也没有额外扩大的残骸范围。

## 影像参考与物理边界

没有火星从内部爆开的真实拍摄影像。本轮下载、抽帧查看了以下原始资料，参考文件保存在 Saved/MarsDestruction/references，不作为游戏素材分发。

| 资料 | 参考内容与适用边界 |
| --- | --- |
| [NASA：月球形成模拟](https://www.nasa.gov/solar-system/collision-may-have-formed-the-moon-in-mere-hours-simulations-reveal/)；[杜伦大学研究组与视频](https://astro.dur.ac.uk/ICC/giant_impacts/) | 大块物质保持整体、拉伸分离，部分回落、重聚或进入轨道。是巨型碰撞数值模拟，不是星球内部爆炸实拍，也不直接照搬其高能喷射。 |
| [NASA：Amery 冰架开裂卫星序列](https://science.nasa.gov/earth/earth-observatory/rifting-and-calving-on-the-amery-ice-shelf-145680/) | 参考裂缝张开、整块分离的形态和尺度感。这是跨多日的卫星序列，水流、浮力和风与真空不同，不借用其阻力模型。 |

本版是物理启发的预制运动，不是天体自引力求解器。“大块慢、小块稍快”按尺寸配置，没有根据真实密度、压力和质量求解。速度衰减表现分离受到约束，不解释为太空空气阻尼。真实自引力还可能导致回落；为了保留可接近的固定末态，本版不制作回落。28 秒是游戏表现时间，不能当作真实火星尺寸下的物理时间。

## 素材检查

| 项目 | 完整 Mars | 破碎 Mars |
| --- | ---: | ---: |
| Mesh 对象 | 1 | 107 |
| 三角面 | 5,004 | 5,080 |
| 顶点 | 2,506 | 2,754 |
| 连通岛 | 2 | 107 |
| 原始材质 / 贴图 | 1 / 1 | 107 / 107 |
| 贴图尺寸 | 8192² | 32²–1024² |
| 缺失贴图 | 0 | 0 |

两套模型独立生成，拓扑和表面不对应，不能倒放拼合或直接使用常规 Morph Target。原始 FBX 和贴图未修改。游戏版本将完整贴图缩为 4096²，残骸贴图合成带扩边的 4096² 图集；近距离探索仍需验收 mip、压缩和清晰度。

## 动画与表面交接

| 时间 | 表现 |
| --- | --- |
| 0–2 秒 | 第一个模型完整显示，残骸藏在内部 |
| 2–12 秒 | 裂缝逐渐张开，大板块显露，两套表面完成遮罩交接 |
| 2–23/26 秒 | 小块约 21 秒、大块约 24 秒完成外移，持续减速 |
| 26–28 秒 | 保持第二个模型原本布局，准备静态末态接管 |

最大碎块径向行程为星球半径的 12%，最小碎块为 19%。这是从收拢起点走向原始终点，不是在第二套模型之外继续外扩。大块转角上限约 0.8°，小块上限 2.5°；末态恢复原始旋转、缩放与相对位置。

运动函数为 p(u) = 1 - (1-u)^2.4，u 为各碎块归一化时间，速度从释放起持续降低。每 3 帧采样并使用线性关键帧，避免 Bezier 插值产生额外加速和超调。

首尾拓扑不匹配仍需视觉交接。完整模型按残骸方向生成裂缝遮罩，原表面从裂缝向板块内部逐渐退出，原始残骸从内部显现。完整表面仅收缩 3% 编码交接进度，完全隐藏后再归零，没有另造地壳向外抛飞。这是美术近似，近看或慢放仍可能发现表面形状变化，不是真正可拼合的断裂。

第二 UV 通道保存交接数据。Unreal 材质从当前局部位置与 Pre-Skinned Local Position 的半径比读取源骨骼缩放，不依赖材质 Time；暂停和拖动动画时遮罩也能同步。FBX 导入翻转 V 坐标的影响已处理。

Pre-Skinned Local Position 通过 Vertex Interpolator 传入像素着色器，适配 UE 5.8 的 SM6 编译限制。材质槽结构逐项写回 Unreal 数组，并对预览组件显式绑定，避免保存后退回默认灰色材质。

## 文件与 Editor 操作

源文件在 SourceArt/MarsDestruction：

- Mars_Internal_Explosion_POC.blend：28 秒可编辑动画与阶段标记。
- SK_MarsExplosion_POC.fbx：1 个骨骼网格、109 根骨骼、2 个材质槽，840 帧 / 30 fps。
- SM_MarsDebris_Final.fbx：107 块原始残骸，UE 合并为一个静态网格、一个材质槽。
- T_MarsIntact_4K.png、T_MarsDebris_Atlas.png：游戏贴图。
- Mars_Explosion_Preview.mp4、Storyboard.jpg：新版视频与分镜，覆盖上一版同名预览。
- prototype_manifest.json、fragment_motion.json：版本、时长、末态约束和每块运动参数。
- Asset_Comparison.png、asset_audit.json、Mars_Asset_Inspection.blend：原始素材检查。

当前 Editor 占用了上一版动画文件，因此新版保存到独立目录：
/Game/Space/Environment/MarsDestructionPreview/PlateDriftV2/

1. 打开该目录中的 **L_MarsPlateDrift_Preview**，点击 Play；固定远处镜头每 28 秒循环。旧的 L_MarsDestruction_Preview 不代表新版。
2. 双击同目录的 SK_MarsExplosion_POC_Anim，可以暂停、拖动并检查遮罩和运动。
3. 使用 SM_MarsDebris_Final 核对末态。若目录未刷新，保存当前工作后重开 Editor。

演示关卡没有正式剧情触发与探索碰撞。生产版本应只播放一次，完成后停留在静态残骸。

## 成熟游戏参考与正式接入

| 参考 | 可借鉴的公开内容 |
| --- | --- |
| [Deep Rock Galactic Season 03](https://www.deeprockgalactic.com/season-03) | 合作破开陨石、取得内部物品的阶段反馈；官方未公开其模型替换实现。 |
| [Astroneer Jet Powered Update](https://blog.astroneer.space/p/the-jet-powered-update-patch-notes/) | 用炸药灯光提示引爆时机，借鉴远处可读的反馈。 |
| [Sea of Thieves SIGGRAPH 技术美术论文](https://history.siggraph.org/wp-content/uploads/2022/09/2018-Talks-Ang_The-Technical-Art-of-Sea-of-Thieves.pdf) | 触手离线形变与运行时播放，支持预制大型表现的思路；不是星球爆炸案例。 |
| [Epic Valley of the Ancient](https://dev.epicgames.com/documentation/en-us/unreal-engine/valley-of-the-ancient-sample-game-for-unreal-engine) | 预录破坏保证固定结果；本项目优先用刚性骨骼动画锁定指定末态。 |

以下为后续方案，尚未新增 Runtime C++ 系统。

目的：共享 Expedition 一次触发，所有玩家按同一服务器时间观看，结束后接近同一残骸。

职责：小型 UJTSPlanetDestructionComponent 管理服务器状态、开始时间和完成通知，GameMode 决定触发条件。Blueprint / 数据资产选择模型、动画、声音、时长、特效及末态碰撞，不在 C++ 硬编码资产路径或关卡实例。

依赖：已复制宿主 Actor、AJTSGameState 服务器时间、AJTSPlanetAnchor、AJTSSpaceWorldManager、现有 UJTSPlanetFragmentEnvelopeComponent、UJTSExpeditionSubsystem。

复制持久状态与开始时间，客户端按服务器经过时间采样，不逐块复制刚体，也不用一次性 Multicast 代替状态。Dedicated Server 只处理规则、时间和碰撞交接。到时统一启用固定末态，以 Mars 自身几何数据复用 FragmentEnvelope 能力，保持同一球心及径向规则。跨关卡结果进入 Expedition snapshot，SpaceWorld 不为完整和破碎 Mars 分别 Travel。

## 重建与验证

修改工具：Tools/Art/build_mars_destruction.py、assemble_mars_destruction_preview.py、import_mars_destruction_preview.py；新增 validate_mars_destruction.py。此前的 inspect_mars_destruction.py 继续保留。

用 Blender 后台运行 build_mars_destruction.py，追加 -- --render 可生成完整预览帧；随后运行 assemble_mars_destruction_preview.py 打包视频。用 Blender 后台运行 validate_mars_destruction.py 验证运动与首尾资产。

导入建议使用独立 UnrealEditor-Cmd -run=pythonscript -script=... -unattended -NullRHI -nowrite。已有 Editor 开启 MCP 时，追加进程参数 -ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False，避免争用端口。-nowrite 禁止写回 ini，不影响保存资产。

已验证：

- FBX 回读通过：109 根骨骼、2 个材质槽、840 帧 / 30 fps；静态末态 107 块、5,080 三角面。
- 全部碎块外移单调，释放后速度单调下降，尺寸越大初始速度越低；初始残骸顶点全部位于完整模型表面内部。
- 动画末态对静态末态、静态末态对原始第二套模型，最大顶点误差均约 1.1e-7 Blender 单位。
- Unreal V2 导入、28 秒时长校验和关卡保存通过，commandlet 退出码 0；有 4 条 EditorLevelLibrary 弃用提示。
- 独立 Unreal 模拟运行的 0、5、12、28 秒截图已检查，D3D SM6 无本次材质编译错误，检查进程正常退出。游戏截图分镜为 SourceArt/MarsDestruction/Unreal_PlateDrift_V2.jpg。
- 没有修改 C++，未运行 C++ 编译。联机同步、探索碰撞和性能验收尚未完成。

Blender MCP 此前没有响应，本轮仍通过独立 Blender 后台进程生成，未操作用户打开的 Blender 场景。
