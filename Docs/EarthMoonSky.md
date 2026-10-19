# 地月远景尺寸校准

> 远景参数保持有效；当前宽范围自适应飞行见 [AdaptiveFlight.md](AdaptiveFlight.md)。

关卡使用独立的远景表现倍率，保留月球和地球碎块的近地模型、碰撞、中心位置及 384,400 km 地月导航距离。没有新增 Runtime 系统或星球特判。

实测完整 Mesh 顶点的可见横向、纵向角尺寸，而不是把 Actor 包围盒当作星球圆盘。原月球用于缩放的包围半径为 20,893.34 cm，实际 Mesh 最远顶点仅 15,402.40 cm；这使可见月球比预期小。地球使用真实碎块轮廓，包含碎块之间的空隙。

| 从对方星球看目标 | 原横向 / 纵向角直径 | VisualRadiusMultiplier | 调整后横向 / 纵向角直径 |
| --- | --- | ---: | --- |
| 地球 → 月球 | 0.347° / 0.378° | 1.43 | 0.496° / 0.541° |
| 月球 → 地球碎块 | 1.563° / 1.724° | 1.25 | 1.953° / 2.154° |

参考 [NASA 地月比例资料](https://www.jpl.nasa.gov/edu/resources/lesson-plan/modeling-the-earth-moon-system/)，地月平均距离 384,400 km、月球直径约 3,475 km、地球直径约 12,742 km，对应角直径约 0.52° 和 1.90°。月球按真实角尺寸校准；碎块横纵平均范围约大 8%，最大方向约大 13%，避免把爆炸范围夸张放大。角尺寸是几何轮廓范围，照明相位会影响亮部面积；90° 宽视角下的真实月球仍是小圆盘。

倍率只参与既有远景缩放。进入近地前缩放上限恢复为 1，不会让月球地表、资源、重力或飞行范围变大。飞行参数沿用 `CruiseApproach.md` 中的自适应设置。

## 修改与 Editor 配置

- `Content/Space/Maps/L_SpaceWorld.umap`：已保存两个表现倍率。
- `Tools/Editor/configure_independent_space.py`：同步配置来源，重新生成配置时保留本次校准。
- `Tools/Editor/validate_earth_moon_sky.py`：限定范围的配置、几何测量、截图和原生飞行验证。
- `Tools/Editor/configure_space_debug.py`：增加 `earth_moon_*` 调试入口。

直接运行当前 `L_SpaceWorld` 即可，无需手动修改资产。需要继续调整时，在关卡的 SpaceWorldManager → CompressedSpace → Bodies 中修改对应条目的 `VisualRadiusMultiplier`。月球为 1.43，EarthFragments 为 1.25。不要修改月球 Surface Actor Scale 或导航距离来补偿远景。

调试桥入口：停止 PIE 后执行 `earth_moon_configure` / `earth_moon_validate` / `earth_moon_geometry`；启动 PIE 后执行 `earth_moon_view_EarthFragments` / `earth_moon_view_Moon`，再执行对应 `earth_moon_capture_*` 截图。`earth_moon_flight` / `earth_moon_flight_status` 复用实际驾驶员 RPC，测试地月双向普通 / Shift 推进，保持原生 Tick、驾驶镜头及正常游戏时间。

## 验证

配置入口核对两个 Surface Transform、导航中心和物理半径没有变化。每条接近路径采样 1,001 点，角尺寸持续增大，无反向收缩，缩放始终在 (0, 1] 内，并在近地恢复 1。Mesh 几何测量和正常 90° 视角截图记录在 `Saved/SpaceDebug/earth_moon_geometry.json`、`earth_moon_sky_validation.json`、`EarthMoon_EarthFragments_After.png`、`EarthMoon_Moon_After.png`；原生飞行记录在 `earth_moon_sky_native.json`。

地月双向普通 / Shift 四段原生实飞通过，6,341 帧采样、零错误。月球→地球普通 / Shift 为 35.66 / 19.80 秒，地球→月球为 45.23 / 25.13 秒，接近衰减、进入近地速度和松键停止均正确，最大镜头支点误差约 0.00000105 cm。

重新执行 11 项相关 C++ 自动化全部通过，零错误、零警告，记录 `earth_moon_sky_automation.json`。本次只改关卡数据和 Editor Python，未改 C++；此前 `spaceEditor Win64 Development` 编译通过。

最后关闭并重新打开 Editor，磁盘关卡读回月球倍率 1.43、地球倍率 1.25，月球原始 Scale 仍为 82，地球碎块原始 Scale 仍为 220126.216724；记录 `earth_moon_sky_inspect.json`。当前已打开 `L_SpaceWorld`，PIE 停止。
