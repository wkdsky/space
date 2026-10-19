# 多天体共同可见时的连续表现

## 原因与实现

旧表现分别缩放每颗天体，但保留其固定位置。飞船在卫星附近越过各自缩放区间时，会看到火星突然缩小，与真实移动距离不符。

`JTSCompressedSpaceComponent` 增加关卡配置 `Bodies[].PresentationGroup`。同组使用固定主天体和一个共同变换：网格尺寸与成员之间的相对位置一起缩放。主天体在捕获关卡配置时按原始半径选定，切换当前星球、近地状态或镜头不会改变它。

- 整组近地球壳及成员间航线被完整包围，并保留额外余量；区域内全部使用原始尺寸和位置，画面大小只随几何距离变化。
- 离开整个天体群后，才用五次缓动逐渐过渡到共同远景比例；各成员保持关卡原有尺寸比例与轨道位置比例，并持续渲染。
- 原始天文坐标、重力中心、独立近地边界和着陆逻辑保持各自职责。远景网格不参与碰撞，返回完整局部几何后恢复原有碰撞。
- 导航投影、遮挡和雷达方向采用实际表现中心；导航距离与剧情拦截使用原始物理参考。

配置保存在 `L_SpaceWorld`：`EarthFragments`、`Moon` 属于 `EarthMoon`；火星、火卫一、火卫二属于 `MarsSystem`。C++ 没有具体星球名称、资源路径或 Actor 引用。未配置组的独立天体继续使用原有表现。

## Editor

已保存关卡配置，打开 `L_SpaceWorld` 直接运行。以后添加同一星系的天体时，在 `SpaceWorldManager → CompressedSpace → Bodies` 中设置相同 `PresentationGroup`。

`GroupLocalPaddingRadii` 控制局部区域余量，`GroupTransitionRadii` 控制整组远景过渡宽度。具体天体仍单独配置着陆和剧情；火卫一的星图与屏幕标记开关继续关闭。

配置复现入口：停止 PIE 后，通过项目 Unreal MCP 调用 `SpaceDebugAuthoringTools.run(operation="celestial_coherence_configure")`。只更新已有五个条目的分组并保存当前关卡。

## 验证入口

`spaceEditor Win64 Development` 编译成功。`CoherentCelestialSystem`、`IndependentCompressedSpace`、`CelestialNavigation`、`ExternalFlightInterference`、`AdaptiveFlightEnvelope` 五项回归全部通过，零错误、零警告；报告为 `Saved/SpaceDebug/CelestialCoherenceAutomation/index.json`。

- C++ `JTS.Spacecraft.CoherentCelestialSystem`：三体局部转场、参考切换、三方向整组过渡、相对位置与比例、标记位置、真实中心与碰撞恢复。
- `celestial_coherence_matrix`：实际关卡的火星三体与地月双向航线，共 8 条、每条 301 个位置。
- `celestial_coherence_probe` / `celestial_coherence_status`：PIE 使用生产表现更新，检查上述航线及两组的整体离开/返回，共 12 条、每条 121 个位置。该项为位置扫描，不作为真实驾驶输入验证。
- `celestial_coherence_forward_probe` / `celestial_coherence_forward_status`：只设置初始位置，随后持续提交正常服务器前进输入，保持航向，检查前进约 1 km 时三个实际网格不发生独立缩放。
- `celestial_coherence_view_a` / `celestial_coherence_view_b`：同航向、相隔 1 km 的画面对比。
- `celestial_coherence_restore`：恢复验证期间临时调整的编辑器后台限速设置。

报告保存在 `Saved/SpaceDebug/celestial_coherence_*.json`。多人延迟及多客户端画面尚未实测；表现参考沿用已有共享飞船位置。

本次实际关卡结果：8 条局部航线、2408 个计算位置均保持原始几何；PIE 的 12 条路线、1452 个实际更新位置全部通过。正常服务器前进输入产生约 1.024 km 位移，34 个样本中航向不变，火星半径始终为 170000 cm，其几何视直径由 15.217° 自然增加至 16.491°；前后画面已检查。

火卫一重新检查 96 个球面位置，20 个位置能通过完整支撑检查，实际下降后四脚接地并着陆；离船、重新登船和起飞成功。导航快照确认其星图与屏幕标记仍关闭。

火星拦截剧情复查完成：426 个正常服务器输入样本覆盖制动、转向、外推与恢复，始终处于近地边界之外，结束朝外且速度归零。PIE 已停止，临时后台限速设置已恢复。

## 修改文件

- `Source/space/Components/JTSCompressedSpaceComponent.h`、`.cpp`：分组配置与共同变换。
- `Source/space/UI/JTSCelestialNavigationLibrary.cpp`：表现中心与原始导航距离分开查询。
- `Source/space/Components/JTSSpacecraftInterferenceComponent.cpp`：使用捕获的原始剧情中心。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：三体连续表现回归。
- `Content/Space/Maps/L_SpaceWorld.umap`：地月和火星系统分组配置。
- `Tools/Editor/validate_celestial_coherence.py`、`configure_space_debug.py`：定向配置与关卡验证入口。
- `Tools/Editor/configure_mars_access.py`：剧情验证也查询原始中心。
- `Docs/CelestialCoherence.md`：配置及验证说明。
