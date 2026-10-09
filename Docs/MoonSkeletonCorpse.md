# 月球骷髅宇航员尸体

`L_SpaceWorld` 中的环形洼地使用骷髅宇航员尸体。巢穴改造已移除 `Moon_CorpseAnchor_A`，地表控制器通过 `PlacedCorpseLandmark` 保留这具独立尸体。九个独立蚂蚁巢穴沿环形山内侧山脚分布，详见 [MoonAntNestEntrances.md](MoonAntNestEntrances.md)。

## 当前中心摆放

通过真实山脚闭合环拟合盆地中心，再投影到实际月面。`Moon_Corpse_SkeletonAstronaut` 的横向中心误差约 0.0017 cm，实例使用 `SM_SkeletonAstronaut_Corpse_CraterCenter`。中心处跨六个低模折面，新的静态躺姿连续适应实际折面高度；靴子最近接触间隙为左 0.30、右 0.33 cm，头部 1.66 cm、背部 1.27 cm、骨盆 0.78 cm。原绑定玩家骨骼的 `SK_SkeletonAstronaut` 保留原状。

源文件与配置工具为 `SourceArt/MoonAntNests/CenteredMoonCorpse.blend`、`SM_SkeletonAstronaut_Corpse_CraterCenter.fbx`、`Tools/Art/fit_centered_moon_corpse.py` 和 `configure_moon_ant_ecology.py`。新的数量及游走规则见 [MoonAntEcology.md](MoonAntEcology.md)。下文保留早期原始躺姿、锚点和五巢穴配置及验证记录，当前关卡已改用上述独立中心尸体和九巢穴配置。

## 早期配置记录

- `BP_MoonSkeletonAstronautCorpse` 继承现有 `AJTSMoonCorpseActor`，选择 `SM_SkeletonAstronaut_Corpse` 与现有六种角色材质。
- 关卡放置 `Moon_Corpse_SkeletonAstronaut`，月球地表控制器通过 `PlacedCorpseLandmark` 引用复用它；`MoonCorpseClass` 配置为同一 Blueprint，供未放置实例时生成使用。
- `CorpseSurfaceAnchor` 继续引用 `Moon_CorpseAnchor_A`。尸体自身使用真实月面法线和模型最低点计算离地间隙，该 Blueprint 配置为 0.3 cm。
- 重做躺卧姿势，让骨盆与双脚下沉、脚踝随腿一起调整。两个靴子尺寸一致，头盔、背包与身体保持完整。
- 坑底跨越三个低模折面，锚点的切向朝向已调整，使头、背、骨盆与双脚的最近接触点距实际可见月面约 0.33～1.99 cm；模型没有穿入月面。
- `PDA_MoonSurfaceGameplay` 将虫巢外圈半径配置为 600 cm，复用现有五虫巢生成模板，以同一尸体位置为中心；虫子的生成与活动仍由虫巢负责。
- 原七块基础几何体、原型材质覆盖和相关尸体表现代码已移除。保留服务器权威、移动复制、蚁巢分布中心和资源排除区域的既有职责。

这是同一个已绑定玩家骨骼的角色的静态躺卧版本：从项目已有 Idle 动画姿势出发，调整四肢并烘焙成单个静态网格，避免尸体持续播放动画或运行物理。原 `SK_SkeletonAstronaut` 和玩家角色 Blueprint 继续可用。免费素材来源与授权见 [SkeletonAstronaut.md](SkeletonAstronaut.md)。

## 编辑器查看

打开 `/Game/Space/Maps/L_SpaceWorld`，在 World Outliner 搜索 `Moon_Corpse_SkeletonAstronaut`，选中后按 **F**。完整角色可直接在关卡中查看。相关材质和尸体类型通过 Blueprint 调整；当前尸体以关卡实例和控制器引用配置。

## 可重建文件

- `Tools/Art/build_skeleton_astronaut_corpse.py`：生成躺卧模型、FBX 和姿势预览。
- `Tools/Art/inspect_moon_corpse_placement.py`、`analyze_moon_corpse_crater.py`、`center_moon_corpse_anchor.py`：导出当前真实月球、拟合坑沿和移动锚点。
- `Tools/Art/moon_corpse_mcp.py`：可选的 Unreal MCP 场景配置与 PIE 验证工具；不会随游戏运行。
- `analyze_moon_corpse_heading.py`、`validate_moon_corpse_visual_grounding.py`：对实际月面 LOD0 三角形检查切向朝向与可见模型接触。
- `validate_moon_corpse_layout.py`：在 PIE 中验证五个虫巢围绕唯一尸体生成。
- `SourceArt/SkeletonAstronaut/SkeletonAstronaut_Corpse.blend` 与 `SM_SkeletonAstronaut_Corpse.fbx`：可编辑源文件和导入源。

最终配置与检查结果存于 `SourceArt/SkeletonAstronaut/moon_corpse_corrected_layout.json`、`moon_corpse_visual_grounding.json`、`moon_corpse_pie_validation.json` 与 `moon_corpse_nest_validation.json`。近景和坑全景为 `Moon_Corpse_InLevel.png` 与 `Moon_Corpse_CraterLayout.png`。

原尸体替换所需的 `spaceEditor Win64 Development` 编译已成功；本次模型、贴地和位置修正没有新增 C++ 修改。尸体、玩家变体和月球地表控制器 Blueprint 均通过 warnings-as-errors 编译检查。

标准 PIE 中完成初始月球降落与地表初始化：连续四次请求复用同一具关卡尸体，根部离真实月面 0.3 cm，朝向与月面法线一致，只有 `SceneRoot` 与 `CorpseMesh` 两个组件。最终朝向下生成五个虫巢，距离尸体约 2.16～3.58 米，观察到十只活动虫子。未执行多人客户端联机测试。结束验证后已停止 PIE 并保存关卡。
