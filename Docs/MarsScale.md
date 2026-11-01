# 火星尺寸校准（2026-10-31）

已通过项目 Unreal MCP 的原生 Actor、Object、Scene 和 Asset 工具缩小并保存 `L_SpaceWorld` 中的火星。没有新增运行时系统。

## 比例依据

从 LOD0 真实网格顶点测得，月球可见最大跨度约 307.63 m；原火星约 2058.60 m，是月球的 6.69 倍。这里的跨度是世界 XYZ 三轴中的最大值，不是旋转后包围盒的直径。

[NASA 火星数据](https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html)和[月球数据](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html)给出的平均半径分别为 3389.5、1737.4 km，因此火星/月球线性比例为 1.95090。保留现有月球时，火星目标可见直径为 **600.15 m**。

地球碎块实测整体跨度约 2228.84 m。既有 `configure_earth_fragments.py` 使用 `EARTH_SIZE_MULTIPLIER = 2.0` 放大完整残骸范围；将此设计倍率除去，再按现实火星/地球半径比换算，得到约 592.89 m，与 600 m 目标接近。残骸没有完整球体，不能把其外轮廓当作原始地球直径。

## 已保存配置

| 项目 | 修改前 | 修改后 |
| --- | ---: | ---: |
| 火星可见最大直径 | 2058.60 m | 600.15 m |
| 火星/月球直径比 | 6.69182 | 1.95090 |
| 火星 Surface Actor 均匀 Scale | 2060.543685 | 600.721586 |
| PlanetAnchor ApproximateRadius | 170000 cm | 49561.03125 cm |
| GravityInfluenceRange | 190000 cm | 69561.03125 cm |
| SpaceExitRange | 192000 cm | 71561.03125 cm |

线性缩放系数约为 **0.2915355**。球心与地表朝向保持原有配置，Surface Actor 的偏心轴心位置围绕球心同步缩放。原日照侧真实地表上的 `Mars_LandingAnchor` 随地表移动；`Mars_ArrivalAnchor` 保留相对落点的原有人员/飞船空间偏移，避免把船体尺度、人物净空和离地查询距离一起缩小。

`ApproximateRadius` 沿用既有旋转后包围盒口径，因此大于实际可见球体半径。不要把它改成 30000 cm 来表示约 600 m 的可见直径。

`configure_independent_space.py` 中的原始配置半径同步改为 49561.031064 cm，避免复现旧配置时恢复成大火星。地月、火卫一、火卫二的实体配置和中心未调整。火星导航半径仍为 3390 km，既有组表现、近地高度和保留的减速区域宽度沿用当前设置。远景天文缩放与近地实体尺寸具有不同作用，极远处圆盘不会简单按本次线性系数缩小。

## Editor 操作

已保存，无需手动缩放或重新绑定。打开 `/Game/Space/Maps/L_SpaceWorld` 后 Play 即可。验证后已停止 PIE，编辑器视口停在缩小后的火星。

## 验证

- 原生 MCP 碰撞射线确认移动后的 LandingAnchor 仍位于真实表面：从地表上方 3000 cm 开始查询，命中距离为 3000.00073 cm。
- MCP 自动化回归 **5/5 通过，零错误、零警告**：`MarsLandingMap`、`CoherentCelestialSystem`、`IndependentCompressedSpace`、`CelestialNavigation`、`AdaptiveFlightEnvelope`。
- 实际 PIE 使用配置的 `BP_Twinfork_R1`：开局为 `Landed`，`LastLandingFailure=None`，四脚接触，人物当前星球为 Mars。地图回归同时验证真实网格降落、登船、下船和胶囊净空。
- 重新加载关卡，MCP 读回 Scale 和半径与保存值一致，关卡无未保存修改。通过 MCP 截取并检查火星视口。
- 本次未修改 C++，无需重新编译。

本次修改文件：`Content/Space/Maps/L_SpaceWorld.umap`、`Tools/Editor/configure_independent_space.py`、`Docs/MarsScale.md`。

报告：`Saved/PlanetScaleAudit/mcp_scale_before.json`、`mcp_scale_after.json`、`mcp_automation_summary.json`、`mcp_pie_validation.json`、`mcp_disk_readback.json`。视口截图为同目录 `Mars_After.png`。
