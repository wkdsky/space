# 环形山蚂蚁巢穴入口

关卡配置目标为 `L_SpaceWorld` 中原 `Moon_CorpseAnchor_A` 标记的环形山。每个 `Moon_AntNest_01`～`Moon_AntNest_09` 都是一个独立巢穴 Actor，只有入口外观和既有蚂蚁活动逻辑。

## 地形识别与外观

从当前 MoonPlanet 的真实 LOD0 顶点与三角形提取与标记相邻的盆地连通区域，利用相邻三角形找到盆地与内壁的交界闭环。山脚包含 18 段边，拟合半径约 47.16 米；两侧坡面夹角约 41～53 度。另行提取外圈山脊边缘，区分山脊与内侧山脚。

九处位置按交界闭环长度分布，避开折面顶点。原入口网格各自匹配所在边的盆地平面与坡壁平面，保留为可关闭形变时使用的外观。当前九个实例使用下面的参数化随机轮廓，外沿薄岩石边框仍与月面使用同一材质。仅制作洞口，未制作巢穴内部。

## 入口参数随机形变

`UJTSNestEntranceMeshComponent` 只负责洞口和薄岩石边沿的几何、材质及 Visibility 查询碰撞；巢穴仍沿用原有活动、生成和销毁逻辑。组件依赖现有 `ProceduralMeshComponent` 模块，无 Tick、无新增插件。开启形变时，隐藏原 `NestMesh` 并关闭其碰撞；攻击锚点和地表边界使用新的可见几何。

在 `L_SpaceWorld` 选中 `Moon_AntNest_`，选择 **EntranceShape** 组件，展开 **Nest Entrance → Shape**：

| 参数 | 作用 |
| --- | --- |
| Width / Height | 洞口宽度和高度，单位 cm |
| Profile Power | 两端的收尖程度 |
| Floor Roundness | 下缘弧度；0 为贴地开口，接近 1 为橄榄状轮廓 |
| Lean | 在坡壁平面内向左右偏斜 |
| Irregularity | 岩石轮廓起伏强度 |
| Seed | 复现边缘起伏的固定种子 |
| Rim Width | 岩石边沿宽度 |

**Profile** 记录随机预设类别；具体外形由上面的独立参数决定。可直接调参数即时预览，也可按 **Randomize Entrance** 给当前实例重新随机一组参数，然后保存关卡。参数只在明确调用随机方法时改变，重建、重开关卡及 PIE 都不重新随机。运行时随机仅允许服务器执行，形状参数通过组件复制同步给客户端。

`Wall Slope` 是原真实坡壁在入口局部坐标系中的拟合结果；宽度、高度、偏斜等变形都保持在这个平面中，不使用固定 World-Z。洞口正面相对坡壁保留 0.25 cm 偏移以避免重叠闪烁。橄榄形和细缝的底部直接接触地面，不附加矩形门槛。

九个实例已经按固定分配种子 `20261009` 随机并保存：

| 样式 | 实例 |
| --- | --- |
| 细缝 | 01、08 |
| 偏斜 | 02、03（分别向左右偏斜） |
| 橄榄形 | 04、09 |
| 不规则拱形 | 05、06、07 |

每个实例的尺寸、偏斜和起伏种子都不同。项目实例配置由 `Tools/Art/configure_moon_ant_entrance_shapes.py` 完成，参数记录在 `SourceArt/MoonAntNests/moon_ant_entrance_shapes.json`。使用 `validate_moon_ant_entrance_shapes.py` 检查参数持久性、确定性重建、贴面距离及攻击射线；`capture_moon_ant_entrance_shapes.py` 保存实际 Unreal 视口图片。

本次形变验证：`spaceEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 编译、链接成功；入口 Blueprint 通过 warnings-as-errors 编译。重启编辑器和标准 PIE 中九个实例均保留已保存参数，四类轮廓覆盖完整；确定性重建、Visibility 射线命中当前入口组件、蚂蚁活动均通过。洞口相对原坡壁/地面最大偏移约 0.250002 cm。双人 PIE 直接从 SpaceWorld 启动时，客户端被既有禁止中途加入规则拒绝，因此本次未验证客户端外观同步；测试偏好已恢复，没有改动加入规则。

实际 Unreal 视口：[细缝](../SourceArt/MoonAntNests/Moon_AntNest_Shape_Slit.png)、[橄榄形](../SourceArt/MoonAntNests/Moon_AntNest_Shape_Olive.png)、[偏斜](../SourceArt/MoonAntNests/Moon_AntNest_Shape_Leaning.png)、[不规则拱形](../SourceArt/MoonAntNests/Moon_AntNest_Shape_Irregular.png)。验证结果在 `moon_ant_entrance_shapes_validation.json` 与 `moon_ant_entrance_shapes_pie_validation.json`。

## 逻辑职责

- `BP_MoonAntNestEntrance` 复用 `AJTSMoonAntNestActor`；每个关卡实例选择其对应的拟合网格。
- `AJTSMoonSurfaceController::PlacedMoonAntNests` 由关卡配置九个引用。控制器激活这些巢穴，保留已拟合的坐标与旋转；该配置优先于旧的尸体周围随机巢穴生成路径。
- 原尸体锚点被替换为第一个独立巢穴，控制器清除 `CorpseSurfaceAnchor`。骷髅宇航员尸体继续作为单独的关卡地标，不再决定这些入口的位置。
- 巢穴、蚂蚁生成与销毁仍由服务器处理，沿用现有生成计时器、活动上限与蚂蚁游走状态。关卡入口等待地表配置完成后激活，停止地表玩法时清理蚂蚁并保留入口实例。

## 修改文件与资产

`Source/space/World/JTSMoonAntNestActor.h/.cpp`、`JTSMoonSurfaceController.h/.cpp` 提供关卡巢穴引用、激活和清理，以及 Blueprint 可配置的巢穴缩放、颜色。

入口 Blueprint 为 `/Game/Space/Blueprints/Planets/Moon/BP_MoonAntNestEntrance`，九个网格位于 `/Game/Space/Meshes/Creatures/MoonAnt`，黑色材质为 `/Game/Space/Materials/Moon/M_MoonAntNestVoid`。关卡和 `PDA_MoonSurfaceGameplay` 配置这些实例和九巢穴数量。

源模型、识别数据与验证结果存于 `SourceArt/MoonAntNests`。工具为 `Tools/Art/inspect_moon_ant_crater.py`、`analyze_moon_ant_crater.py`、`build_moon_ant_entrances.py`、`moon_ant_nests_mcp.py`、`validate_moon_ant_entrances.py` 与 `save_moon_ant_nest_viewport.py`；它们仅用于编辑器创作，不随游戏运行。

## Editor 查看

打开 `L_SpaceWorld`，在 World Outliner 搜索 `Moon_AntNest_`，可找到九个独立对象。选中单个对象按 **F** 查看入口；在 `BP_MoonSurfaceController` 的 `Placed Moon Ant Nests` 中查看九个引用。PIE 时检查各入口附近的蚂蚁活动。

## 验证状态

- `spaceEditor Win64 Development -WaitMutex -NoHotReloadFromIDE` 完整编译与链接成功。
- `BP_MoonAntNest`、`BP_MoonAntNestEntrance`、`BP_MoonSurfaceController` 均通过 warnings-as-errors 编译。
- 原 `Moon_CorpseAnchor_A` 已移除；九个入口各自具有 `SceneRoot`、保留的 `NestMesh` 与可选的 `EntranceShape`，控制器引用与关卡对象一致。原导入网格的黑色洞口距所在墙面或地面最大约 0.250003 cm，用于避免重叠闪烁；根部位于真实山脚交界边上。
- 两次标准 PIE 初始化均保持九个关卡巢穴的位置与单位缩放，没有生成旧随机巢穴。最终采样共有 23 只活动蚂蚁，九个巢穴各有 2～3 只；所有蚂蚁的 Owner 都属于这九个巢穴。
- 短间隔追踪中仍存活的七只蚂蚁均移动，位移约 1.72～8.01 米，沿用原有游走、钻出和钻回状态。原骷髅宇航员尸体仍只有一具。未执行多人客户端联机测试。
- 已停止 PIE 并保存资产与关卡，最终脏资产和脏关卡列表为空。

验证数据：`moon_ant_nests_geometry_validation.json`、`moon_ant_nests_pie_validation.json`、`moon_ant_motion_validation.json`。最终原生视口截图为 [入口近景](../SourceArt/MoonAntNests/Moon_AntNest_Close.png) 和 [九巢穴分布](../SourceArt/MoonAntNests/Moon_AntNests_CraterLayout.png)。
