# 骷髅宇航员角色

已将免费骷髅与宇航服组合成一个抽象、低模的 Skeletal Mesh，并通过 Unreal MCP 导入项目。

## 素材来源与授权

- 骷髅：Kay Lousberg / KayKit Character Pack: Skeletons 1.0，使用免费版 `Skeleton_Warrior` 的头、下颌和眼睛。原作者页面：https://kaylousberg.itch.io/kaykit-skeletons 。原始文件来自作者仓库：https://github.com/KayKit-Game-Assets/KayKit-Character-Pack-Skeletons-1.0 。授权：CC0 1.0，允许修改、个人与商业使用。
- 宇航服：Quaternius / Ultimate Space Kit，使用 `Astronaut_RaeTheRedPanda.blend` 的服装和颈环，移除原动物头部和眼睛。原作者页面：https://quaternius.com/packs/ultimatespacekit.html 。文件来自该页面链接的 Google Drive 免费下载目录。授权：CC0 1.0，允许修改、个人与商业使用。
- 下载目录中的 Quaternius `License.txt` 标题沿用 “Ultimate Platformer Pack”，文件正文为 CC0；原样保留，并由 Ultimate Space Kit 官方页面上的 CC0 标识交叉确认。
- 授权原文保存在 `SourceArt/SkeletonAstronaut/LICENSE_KayKit.txt` 和 `LICENSE_Quaternius.txt`；实际源文件下载地址和 SHA-256 记录在 `sources.json`。

## 新增资产

| 资产 | 路径 |
| --- | --- |
| 合并骨骼网格 | `/Game/Space/Characters/SkeletonAstronaut/SK_SkeletonAstronaut` |
| 物理资产 | `/Game/Space/Characters/SkeletonAstronaut/SK_SkeletonAstronaut_PhysicsAsset` |
| 六个材质实例 | `/Game/Space/Characters/SkeletonAstronaut/Materials/MI_SA_*` |
| 可用玩家 Blueprint 变体 | `/Game/Space/Blueprints/Player/BP_JTSPlayer_SkeletonAstronaut` |
| 可编辑 Blender 源文件 | `SourceArt/SkeletonAstronaut/SkeletonAstronaut.blend` |
| Unreal 导入源文件 | `SourceArt/SkeletonAstronaut/SK_SkeletonAstronaut.fbx` |

玩家变体由 `BP_JTSPlayer_Casual_2` 复制后仅调整 Mesh 与材质覆盖，继承其输入、交互、装备、联机和角色配置。没有更改现有 GameMode 的玩家选择。

## 实现

- 保留完整现有 `Casual_2_Skeleton`，80 个骨骼名称、顺序、父级与原玩家一致，原 Skeleton 资产没有被修改。
- 宇航服按目标骨骼比例重新变形和蒙皮，手指权重映射到现有手指骨骼；骷髅头及头盔绑定 `Head`。
- 骷髅被衣服遮住的身体未额外保留，避免不可见网格开销。
- 新增多边形头盔后壳、开放面罩边框、背包和胸前信号面板。开放面罩使骷髅脸清晰可见。
- 米白、石墨灰、橙色、骨白、青色与金属灰采用六个材质实例，复用项目已有的 `M_StellarHeld` 参数化材质。
- 3,256 个源顶点、5,534 个三角面；所有顶点都有权重。T Pose 总高约 2.10 米，包含加大的卡通头盔。
- 修正了原玩家骨架左右 Foot 显示尾端长度不同造成的靴子拉伸：两只靴子使用相同尺寸，长度均约 27.3 cm。小腿末端对应实际 Foot 关节，保持脚踝连接；现有玩家 Skeleton 资产不变。
- 直接沿用 `ABP_JTSPlayer_Casual_2`，无需 IK Retargeter 或另一套游戏动画。
- 用 Unreal MCP 创建并分配对应 Physics Asset。

## Editor 使用步骤

1. 在 Content Browser 搜索 `BP_JTSPlayer_SkeletonAstronaut`，打开即可检查 Mesh、原动画蓝图和配置。
2. 需要让游戏玩家使用该外观时，打开 `BP_EarthGameMode` 和 `BP_SpaceWorldGameMode`，在 Class Defaults 中把 `Default Pawn Class` 设为 `BP_JTSPlayer_SkeletonAstronaut`；Compile、Save。关卡若使用另一个 GameMode Override，应在实际使用的 GameMode 中同样设置。
3. PIE 中切换第三人称，检查移动、拳击和装备表现。服装颜色可在 `MI_SA_*` 中调整 `BaseColor`、`Roughness` 和 `Metallic`。

## 验证与编译结果

- Unreal MCP 成功导入并保存 Mesh、Physics Asset、六个材质实例和新玩家 Blueprint。
- Blueprint 使用 warnings-as-errors 编译通过。
- 在 Unreal 中断言新旧 Mesh 使用同一个 Skeleton、玩家变体使用新 Mesh、原动画蓝图、无旧材质覆盖、Physics Asset 和材质均已分配。
- 导出项目的实际 `Idle_Neutral`、`Walk`、`Run`、`Punch_Left`、`Punch_Right` 五个动作，在 Blender 对新网格进行姿势抽样渲染，已视觉检查。骨骼参考矩阵最大误差约 `9.31e-6`，未发现异常拉伸或骨骼错位。
- Unreal 资产缩略图也已确认导入后的模型和材质可渲染。
- 原始检查数据：`assembly_report.json`、`unreal_validation.json`、`animation_qa.json`。五个动作截图和站姿截图在同一源文件目录中。
- 没有改动 C++，因此未运行 C++ Build；尚未进行该角色作为受控 Pawn 的完整 PIE / 联机回归。

## 重建

`Tools/Art/skeleton_astronaut_fetch.py` 下载固定来源；`export_skeleton_astronaut_reference.py` 在 Unreal 内导出当前玩家参考骨架；`build_skeleton_astronaut.py` 在 Blender 生成合并模型和站姿截图。

Unreal MCP 使用 `SkeletalMeshTools.import_file` 导入 FBX 时指定现有 `Casual_2_Skeleton`，不导入动画、材质或纹理。材质实例、玩家变体、Physics Asset 通过 MCP 配置。`validate_skeleton_astronaut_unreal.py` 导出实际动作并验证配置，`render_skeleton_astronaut_animation_qa.py` 在 Blender 渲染动作截图。

本次辅助导出临时启用了绑定 `127.0.0.1`、TTL=0 的本地 Editor Python Remote Execution，完成后恢复关闭。`skeleton_astronaut_ue.py` 仅连接本仓库项目；如要重跑辅助导出，应在编辑器临时打开同一配置，结束后关闭。
