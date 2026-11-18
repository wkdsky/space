# 火星与火卫一：Tripo3D 提示词及制作规格

这份规格只准备资产，不修改现有项目。当前场景风格参考为圆润轮廓、柔和着色的大面低模、稀疏的大陨石坑、克制的颜色块。

## 尺度与交付

| 项目 | 火星 | 火卫一 |
| --- | --- | --- |
| 完整模型最终显示尺寸 | 直径约600 m | 远景约2 m；近处可玩大小另行配置 |
| 场景距离 | MarII地表观察点到火星中心约2.1 km | 到火星中心约830 m |
| 完整模型建议预算 | 6000三角面，2K贴图 | 4000三角面，1K贴图 |
| 碎裂模型建议预算 | 总计18000三角面 | 总计12000三角面 |
| 分块目标 | 12大+18中+36小，约66块 | 8大+12中+20小，约40块 |
| 终态总体包络 | 原始直径的1.7倍，约1020 m | 原始最长轴的2倍，远景约4 m |
| 破坏动画 | 6秒，30 fps，不循环 | 8秒，30 fps，不循环 |

提示词中的面数、分块数、独立对象、封闭切面均为目标，不是Tripo保证。最终以导出模型检查为准。提示词不负责可靠的公制尺寸；在Blender/Unreal中校准。

Tripo公开的动画流程是自动绑骨和动作重定向，未找到可直接控制星体几何破碎、碎块轨迹、最终布局的官方功能。第三、第四条用于生成碎裂终态参考，配套过程动画在Blender中制作。用于正式动画的碎块应从同一份完整模型切出，才能保证形状与贴图连续。

## 1. 完整火星

```text
One complete 3D Mars planet for a stylized low-poly space exploration game. Use rounded silhouette, broad polygonal forms, softly shaded facets, restrained detail and clean color blocks. A near-spherical solid body with muted terracotta, dusty ochre and reddish-brown regions. Add a few broad shallow craters, one wide simplified canyon, a low shield-volcano plateau and small off-white polar caps. Keep terrain shallow relative to the sphere; every side must be finished. Matte nonmetallic rock, roughness about 0.85, subtle surface variation, neutral albedo without painted shadows. Target 6000 triangles, one UV atlas, one shared surface material. Closed watertight mesh with consistent outward normals; origin at planet center. Build geometry at convenient source scale; final in-game diameter is 600 meters. Isolated planet only: no pedestal, stars, other moons, text, rings, clouds, glow or scenery.
```

## 2. 完整火卫一

```text
One complete 3D Phobos, Mars's inner moon, for a stylized low-poly space exploration game. Use soft shading over broad geometric forms, rounded rocky silhouette and restrained surface detail. An irregular elongated potato-shaped solid body, axis proportions about 26:23:18, clearly not spherical. Muted warm gray, dusty taupe and charcoal-brown rock; darker and grayer than Mars. One large shallow Stickney-like crater, several smaller broad craters and a few sparse parallel grooves. Craters are closed depressions, not holes or caves. Matte nonmetallic material, roughness about 0.9, neutral albedo without painted shadows. Target 4000 triangles, one UV atlas and one shared material. Finish all sides, watertight solid geometry, outward normals, origin at body center. Use convenient source scale; final distant display size is about 2 meters. Isolated moon only: no Mars, rings, support, buildings, text, fire or scenery.
```

## 3. 火星破坏后的终态参考

```text
Static final debris state of one shattered Mars, matching the supplied intact Mars in silhouette, color regions, crater style and low-poly shading. An asymmetric 3D cluster of solid rock fragments around the original planet center, retaining a broken spherical composition with wide visible gaps. Aim for 12 large chunks, 18 medium chunks and 36 small shards. Large chunks retain curved terracotta crust, crater sections and canyon edges; exposed fracture faces are matte dark rust and charcoal rock. Pieces are thick closed solids, not thin shells. Each fragment must be a separate disconnected mesh with capped interior faces and outward normals; no connecting bridges. Use broad facets, restrained texture and two shared materials for crust and interiors. Target 18000 triangles total. Final cluster spans about 1.7 times the intact diameter. Keep the original center, orientation and scale reference. Generate one static terminal arrangement only, not animation frames. No fire, smoke, light sphere, base or scenery.
```

配套动画规格（给Blender制作，不粘进Tripo模型输入框）：

- 0.0–0.8秒：完整状态，所有碎块严密拼合，轮廓与完整模型一致。
- 0.8–1.6秒：几条主要裂缝逐渐张开，外壳仍能辨认原有地貌。
- 1.6–3.5秒：大块缓慢分离，中小碎块更快向外散开；每块刚性移动和转动，无软体拉伸。
- 3.5–5.5秒：减速进入预先设计的终态，保留宽阔可见缝隙与不对称分布。
- 5.5–6.0秒：保持终态，碎块不消失、不重新拼合。
- 输出动画的最后一帧必须与静态碎片终态完全一致。中心不漂移；不烘焙镜头、烟雾、火光和粒子到网格。

## 4. 火卫一破坏后的终态参考

```text
Static final debris state of one tidally shattered Phobos, matching the supplied intact Phobos in shape, crater features and warm-gray low-poly rock style. An asymmetric elongated cluster of solid fragments, stretched along one clear tidal direction. Aim for 8 large chunks, 12 medium chunks and 20 small shards. The largest pieces preserve curved outer surfaces and recognizable sections of the main crater; expose darker solid gray-brown rock inside. Each fragment is an independent disconnected watertight mesh with capped fracture faces and outward normals. No thin hollow shell, fused bridges or uniform wedge slices. Softly shaded broad facets, restrained detail, two shared materials for exterior and interior. Target 12000 triangles total. Final debris spans about twice the intact longest axis, with large readable gaps and smaller shards toward the stretched ends. Preserve original center and scale reference. One static final 3D arrangement, not a storyboard. No Mars, fire, smoke, rings, base or scenery.
```

配套动画规格（给Blender制作，不粘进Tripo模型输入框）：

- 0.0–1.0秒：完整不规则天体。
- 1.0–2.5秒：沿既有沟槽出现主裂缝，先产生少量清晰的大块。
- 2.5–5.5秒：大块沿一个主方向分离，小碎块分布在两端，表现潮汐拉裂后的短碎片链。
- 5.5–7.0秒：碎块缓慢转动、减速，进入设计终态。
- 7.0–8.0秒：保持终态；保留原大陨石坑的可辨认碎片。
- 不做成与火星完全相同的均匀放射爆炸。最终状态仍是若干大块和小块，尚未形成完整环。
- 最后一帧与静态碎片终态一致，无循环和消失。

通用负向提示词；仅在界面提供独立负向输入时使用：

```text
photorealistic, noisy microtexture, glossy metal, voxel, LEGO, spikes, buildings, labels, pedestal, background, painted shadows, hollow shells, fused fragments, fire, smoke
```

## Tripo3D 操作步骤

1. 打开 https://studio.tripo3d.ai/ 并登录，进入生成页，选择文字生成。先分别生成第1、2条完整模型。若界面只显示图片入口，切换文字入口，或先用站内文字生成图片，再转成3D。
2. 完整模型优先选面向游戏资产的Clean Topology / Smart Mesh / Smart Low Poly一类选项；不同版本名称可能变化。拓扑选三角面，预算按上表设置；若提供自动真实尺寸，先关闭，尺寸留到导出后统一校准。
3. 旋转模型检查全部方向：火星保持完整球体；火卫一保持不规则长形。轮廓满意后再贴图。材质Metallic设0，Roughness约0.85–0.9。颜色保持哑光、简洁；不需要8K照片纹理。
4. 第3、4条可用于生成终态参考。若界面支持参考图加文字，配上相应完整模型的截图，帮助保持颜色和地貌；它不能保证重新生成的网格与原模型完全一致。
5. 若需要Tripo输出分开的碎片，可尝试Build & Refine中的Generate in Parts。先完成分块，再减面和贴图。不要假设Smart Low Poly、自动贴图和Parts可以同时开启；官方API明确存在组合限制。
6. 在Segmentation或部件列表逐块选取，确认每次只选中一块。放大检查断面是否封闭、是否存在隐藏连接。自动分割/补全部件不等同于精确的几何破碎；不满足要求时在Blender从完整模型重新切块。
7. 导出优先保留GLB，并备份FBX及贴图。保存完整模型、终态参考和所有贴图。静态模型勾选动画导出并不会产生破坏动画。
8. 进入Blender后统一中心、朝向、单位和缩放。完整模型与其碎裂版本必须共用同一缩放系数；不要将散开后的碎片包络重新缩成600 m或2 m，否则碎块会比完整状态缩水。

## 制作真正的破坏动画

1. 导入完整GLB，保留原模型，复制一份用于切块，应用缩放。
2. 使用Cell Fracture或手工布尔切割。火星先切约12大块，部分大块再细分；火卫一先切约8大块，再细分出中小块。确保封闭内切面，保留外表面UV。
3. 为每块保存严密拼合的初始变换和最终散开的变换，再按上面的时间表制作刚性平移/转动关键帧。推荐先用可控关键帧制作固定终态；使用物理模拟时需烘焙后再校准终态。
4. 导出UE用动画时，可使用中心根骨骼加每碎块一根骨骼，每块顶点仅以100%权重跟随自己的骨骼，导出FBX骨骼网格与动画。普通静态FBX不会自动携带UE可直接播放的碎块动画。
5. 同时另存最后一帧为静态碎片模型，所有碎块位置与动画终帧一致。最终每个天体交付完整静态模型、破坏动画、终态静态模型和贴图；另附MP4预览用于验收，MP4不替代3D动画资产。

从MarII看火卫一只有约2 m的远景大小，破坏细节会很小；需要细看时应安排近景镜头。

## 官方参考

- Tripo入门：https://www.tripo3d.ai/help/getting-started/how-to-use-tripo
- Tripo Studio界面、分块与贴图：https://www.tripo3d.ai/blog/tripo-studio-tutorial-english
- Tripo参数与1024字符提示词上限：https://developers.tripo3d.ai/en/docs/generation-text-to-model/standard
- Tripo动画预设/重定向：https://developers.tripo3d.ai/en/docs/animations-retarget
- Blender Cell Fracture：https://extensions.blender.org/add-ons/cell-fracture/
- UE FBX骨骼动画流程：https://dev.epicgames.com/documentation/en-us/unreal-engine/fbx-skeletal-mesh-pipeline-in-unreal-engine

