# 独立近地泡泡与压缩星际航行

2026-10-18 的当前规则。此配置替代此前地月共享天空、火星卫星共享走廊和按出发星球整组显隐的规则。

飞船的近地/巡航速度跃迁与当前航程见 [反重力飞行配置](AntigravityFlight.md)。以下坐标已同步到该配置；文末保留上一轮空间映射验证记录。

每个玩法星球单独拥有一个近地球形范围。飞船进入哪个范围，就只绑定哪个星球；范围外是自由航行，CurrentPlanet 和 FlightPlanet 清空。月球、地球碎片区、火卫二均如此。火星和火卫一仍是纯背景，拥有各自独立的距离与显示过渡，不增加碰撞、起降点、玩法、星盘目标或画面锁定框。

## 坐标和距离

`SpaceWorldManager.CompressedSpace` 是无 Tick 的通用组件，关卡里的 Bodies 数组配置 Actor、导航坐标（公里）、物理半径和美术倍率。组件不包含星球名称、资源路径或具体 Actor 引用的硬编码。依赖既有 PlanetAnchor、表面 Actor 和共享飞船位置；不产生新的玩法状态或网络输入。

一套连续映射 `N = f(WorldPosition)` 同时提供所有目标的距离和飞船速度。映射在各自近地范围内保持相同的局部比例，外侧连接压缩星际区域。各轴独立、连续且单调，天体所在的局部坐标带保持统一比例；同一轴向位置必须对应同一导航坐标。坐标带和近地球形范围均不重叠，因此斜向接近也不会发生目标距离回折。导航速度是此映射对实际速度的方向导数，显示速度随模拟航程的进展变化。服务器仍是唯一的移动和近地状态权威，客户端用同一份关卡数据处理表现。

星盘方位使用屏幕中天体所在的实际方向，距离使用压缩空间坐标。短途显示公里，较远显示 AU；星盘采用固定对数距离刻度。天体中心不会随进入、离开泡泡而挪动。远景大小按自身天文距离计算，进入自身近地范围之前连续恢复完整模型和碰撞。背景始终无碰撞。

以现有地球中心为关卡原点，实际世界仅占约 218 km，仍远小于天文尺度：

| 天体 | 关卡偏移（cm） | 导航坐标（km） |
| --- | --- | --- |
| 地球碎片 | (0, 0, 0) | (0, 0, 0) |
| 月球 | (2500000, 0, 0) | (384400, 0, 0) |
| 火星 | (12000000, 12000000, 12000000) | (45229620.09, 45229620.09, 45229620.09) |
| 火卫一 | (12340000, 12225000, 12231607.88) | 火星 + (6800, 4500, 4632.16) |
| 火卫二 | (12425000, 13050000, 12304444.91) | 火星 + (8500, 21000, 6088.90) |

这是为游戏选择的静态轨道相位，不是实时星历。地月中心距采用约 384400 km；火卫一和火卫二距火星中心分别采用约 9378 / 23459 km。参考 [NASA 月球资料](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html)、[NASA 火星资料](https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html)。火卫一保留 4 倍显示半径参数以保证可读性。火星完整模型半径约 1700 m，使火卫二位置看到的火星约 16°。

月球、火卫二的落点、地表实例和 PlayerStart 已随星球一起平移。地球碎片原有模型及地表玩法位置保留。近地边界仍对应原来导航的 14 km 高度，世界高度约 176.9 m；近地内部使用真实表面查询并在边界前连续接到球形范围。

火卫二默认落点另外选在通过现有起落架支撑检查的真实模型表面，火星在当地地平线上方约 19°。飞船查询点沿球心径向位于该点上方，避免倾斜三角面法线让初始化的重力探测偏离已验证地形。人物出生仍使用既有飞船外部安全位置查询；没有放宽支撑或碰撞检查。

## 标记

月球和火卫二以共享飞船到天体中心的固定模拟距离判定资格，与导航使用同一坐标换算；镜头/FOV/乘客位置不会改变资格。画面投影仅决定框的位置和尺寸。按 90° 水平 FOV、16:9 的约 5% 画面直径一次性校准，运行时不判断画面占比。进入各自近地范围后取消标记，并在地表导航曲线下显示星球名字。

当前固定中心距离：月球约 61774 km，火卫二约 512.3 km。它们是模拟距离，不是关卡中摆放模型的厘米数。

显示半径以物理半径和关卡美术倍率为基础，最低满足近地模型与统一局部坐标比例的衔接。这个下限只影响火卫二的微小远景，显示半径约为其物理半径的 2.3 倍；近地模型保持原尺寸。缩放只由固定显示半径与模拟距离计算，不使用按观察方向变化的膨胀过渡。固定标记距离保存在 `NavigationMarkerRangeKilometers`；旧的世界距离属性仅供未配置压缩空间的关卡兼容使用。

修复了 `GetVisualRadius` 将小于 1 cm 的有效远景模型错误替换成默认大半径的问题，避免出现虚假的大框和遮挡球。

## Editor 与验证

通过 Unreal MCP 的 `SpaceDebugAuthoringTools.run` 调用 `independent_configure`，在停止 PIE 后配置并保存 `/Game/Space/Maps/L_SpaceWorld`。配置已经写入关卡，无需手动重配。不要用历史的火星共享天空脚本覆盖此配置。

相关文件：

- `Source/space/Components/JTSCompressedSpaceComponent.h/.cpp`
- `Source/space/World/JTSSpaceWorldManager.h/.cpp`、`JTSPlanetAnchor.h/.cpp`、`JTSCelestialNavigationTypes.h`
- `Source/space/UI/JTSCelestialNavigationLibrary.h/.cpp`、`JTSCelestialMarkerWidget.h/.cpp`、`JTSCruiseNavigationWidget.cpp`
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`
- `Tools/Editor/configure_independent_space.py`、`configure_celestial_navigation.py`、`configure_space_debug.py`
- `Content/Space/Maps/L_SpaceWorld.umap`

此前空间映射验证记录（速度跃迁配置之前，`Saved/SpaceDebug/independent_*`；当前验证见反重力飞行文档）：

- `spaceEditor Win64 Development` 编译成功，74.17 秒；未使用 Live Coding。记录：`independent_build.log`。
- `CelestialNavigation`、`IndependentCompressedSpace`、`InterplanetaryCruise`、`MarIILandingMap`、`TakeoffLandingCycle` 五项自动回归通过，零错误、零警告。覆盖单调距离、连续远景、斜向进出、速度积分、碰撞恢复和固定距离标记。
- 生产关卡中地月及月球/火卫二双向空间扫描共 964 次采样，零错误。这个扫描用于覆盖路径，与下项原生飞行验证分开记录。
- 原生移动与 owner `ServerSetFlightInput` RPC 驱动火卫二→月球→火卫二，3937 次采样，零错误。两段分别约 533.59 / 511.95 游戏秒，到达时均进入对应天体的近地范围。验证期间仅用 8 倍时间倍率缩短等待，结束后恢复 1 倍。
- 双本地玩家分别使用 55° / 150° FOV，月球和火卫二画面直径约 10.7% / 1.5% 时同时显示标记；乘客转头离开及转回、固定范围外的近距离镜头、进入各自近地范围均通过。截取图片已检查。
- 保存后重新启动 PIE，默认仍是火卫二，`surface_ready=true`，人物正常出生，飞船为 landed。火星、火卫一无碰撞、无 Tick、无导航和锁定资格。

验证用相机、飞船位置、分屏和时间倍率均仅发生在 PIE，不写入关卡。完成后停止 PIE，恢复验证期间临时关闭的编辑器后台限速设置。
