# 美术资产与源文件

## 目录职责

`Content` 保存 Unreal 使用的导入资产；`SourceArt` 保存可编辑源文件、导出文件、配方、授权和精选预览。`Tools/Art` 与 `Scripts` 保存维护工具。临时摄像机位置、检查报告、逐次 QA 输出放在 Saved，不作为源美术的一部分提交。

| SourceArt 目录 | 保留内容与用途 |
| --- | --- |
| FrontEnd | 前端模型、动画、字体、清单、外部资源与授权 |
| MarsDestruction | 火星碎裂原型、ScorchedFractureV3 源文件与运动数据 |
| MarsSystem | 当前星系参考图与建模提示 |
| MoonSurfaceV3 | 月面独立附着模块、材质、配方及实际几何 JSON |
| MoonResources | 资源模型、纹理与清单 |
| MoonCreaturesV3 | 四类生物骨骼、动画、材质、预览 |
| MoonAntNests / SkeletonAstronaut | 原始表面数据、洞口与尸体来源、授权 |
| MirrorBoss / SpireColony | Boss、巢塔、士兵及预览 |
| MiningPickaxe | 采矿镐源文件、配置与参考 |
| TwinforkR1 / ShipRefit | 基础船型、改装、机构、扫描头、声音 |
| ShipDocks / ShipAnalysis | 船坞与分析设备来源 |
| StellarWeapons / StellarEffects | 星际装备模型与特效 |

`References` 保留设计参考，`Previews` 保存精选成品预览。自动备份 `.blend1` 等可删除，但保留对应 `.blend`。PNG、JSON 或旧版本文件不能仅凭扩展名 / 名称判断无用：导入器可能读取纹理、表面三角形、UV、布局、规格和运动数据。

FrontEnd 的 sources / manifest、字体许可、SkeletonAstronaut 的原作者与许可仍需随源资产保留。外部模型即使存在字节相同的副本，只要现有工具或来源记录仍引用该路径，也保留其可重建性。

## 月球来源边界

保留 `moon_crater_source.json`、`moon_crater_foot.json`、`moon_reference_geometry.json`、`moon_crater_fit.json`、`centered_corpse_fit.json` 以及 EarthenEntrances / RiftlipSeams 中真正的几何数据。它们与可随时重跑生成的检查报告不同。

早期洞口源文件和原始放置数据不作为当前关卡坐标权威。当前固定遭遇为七个 Habitat，位置以 SpaceWorld 已放置实例为准。旧重建工具不能覆盖原始月球地形，不能把历史布局重新写回关卡；详见 [月球约束](Moon.md)。

## 火星毁灭演出

当前保留的 ScorchedFractureV3 是独立预览 / 媒体演出资产。源入口为 `SourceArt/MarsDestruction/ScorchedFractureV3`，导入内容位于 `/Game/Space/Environment/MarsDestructionPreview` 下；正式播放文件为 `Content/Movies/MarsDestruction.mp4`。

V3 manifest 当前为 revision 10：40 秒影片、60 fps 播放输出、30 fps 骨骼源采样，187 个实体、188 根骨骼。运动、碎裂位置和裂纹覆盖由源 JSON / Blender 文件描述。旧 POC、PlateDriftV2 的对应预览资产仍存在，因此保留其源文件，不把仍可导入的原型全部清掉。

可维护入口包括 `Tools/Art/build_mars_scorched_fracture.py`、`import_mars_scorched_fracture.py` 和 `configure_mars_cinematic_media.py`。最后一个用于独立编辑器流程，可能退出编辑器；不要直接在有未保存工作的用户会话中执行。

这些资产的存在不表示 SpaceWorld 已经接入星球毁灭、权限演出或多人同步。编辑器预览、影片播放和运行时共享玩法必须分别验证。SourceArt 中与正式影片字节相同的副本无需重复保存。

## 整理规则

保留模型编辑源、实际导入源、配方、几何、授权、参考和精选预览。删除可再生自动备份、一次性分析报告、旧截图与重复成片；移动预览时同步更新生成脚本的输出路径。重新运行历史工具前先检查输入是否存在、输出目录是否正确，以及是否会覆盖当前关卡。
