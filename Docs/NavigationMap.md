# 三阶段动态导航图

目的：把现有近地高度导航扩展为地表定位、星域导航和星际导航，并让层级变化保留连续的空间关系。

职责和依赖：`FJTSNavigationMapModel` 计算只读地图快照和跟随坐标；`UJTSSurfaceMapCapture` 为本地 HUD 创建正交 SceneCapture 和 RenderTarget；`SJTSCruiseNavigation` 显示缩略图、标记和缩放动画；原 HUD 管理显示和隐藏。依赖现有 WorldManager、CompressedSpace、PlanetAnchor、共享飞船和本地角色，没有增加 Actor、Actor Tick、RPC 或新的飞行/重力规则。

- 近地飞行保留弧形地表、飞船图标和离地高度。离开既有近地飞行边界后，弧面收缩成对应星球，同星域成员和叠加引力色带逐渐显现。
- 飞船停靠时显示北向上的真实场景缩略图，登船后到起飞前仍保留。512×512 正交 SceneCapture 直接渲染场景网格、材质底色、建筑、岩石和资源，隐藏角色与飞船模型并叠加独立标记。相机每次 HUD 更新都围绕角色重建球面切平面，不再等待时间或距离阈值刷新；关闭雾、大气和日夜照明影响，保证地表可读。使用当前项目延迟渲染器的 BaseColor 捕捉，避免依赖仅开发构建可用的 Unlit 调试显示；缩略图纵向翻转以对齐球面北向和标记。
- 左下角显示当前星球名，右下角显示本地玩家相对星球旋转轴的纬度/经度，下方显示飞船地表距离。白色圆点为玩家，金色箭头为飞船，边缘箭头指向超出图幅的飞船。
- 地表导航从 GameState 的已复制 ActiveSpacecraft 读取共享飞船，不依赖当前 SurfaceController 是否已经绑定飞船；角色和飞船各自保留独立的位置。
- 星域成员复用 `CompressedSpace.Bodies.PresentationGroup`，同组天文半径最大的天体作为主星。地球星域标记地球、月球；火星星域仅标记火星，火卫一与火卫二保留在分组及引力计算中，但不显示轮廓、名称或高度标记。
- 星域固定飞船为中心，将天体与飞船的三维位置差变换到飞船完整姿态的局部空间，再投影到倾斜扫描平面。平移、偏航、俯仰、翻滚均改变相对位置，球面经线和高度连线表达立体关系。所有目标共用径向压缩比例；地表标记和缩略图则共用真实正交投影与米制比例尺。
- 离开星域后，主星和卫星连续收拢为一个主星标记。星际图只显示主星名，保留飞船航向和平面俯仰提示。返航动画沿相同过程反向播放，阶段边界采用迟滞，避免来回闪烁。

引力色带是根据各天体配置绘制的相对强度示意，不改变任何玩法重力。视觉背景可通过 `NavigationSurfaceGravity` 配置地图贡献，零表示不绘制该背景的引力场。

## Blueprint / Editor 配置

本次配置已经通过 Unreal MCP 保存到原工程的 `L_SpaceWorld`，无需再执行手工配置；可直接进入 PIE。下面各项供后续调整表现参数时使用。

1. 在 `L_SpaceWorld` 选中 SpaceWorldManager，展开 `Space World > Navigation`。可调整过渡时长、星域范围、迟滞和地表图幅，默认每级过渡 1.4 秒、地表半宽 60 米。`GroundMapResolution` 控制缩略图分辨率（128–1024，默认 512），`GroundCaptureHeight` 控制相对球面径向的拍摄高度。
2. 其 CompressedSpace 组件中的 `Bodies` 保留关卡配置的 Actor 引用和星域分组。新增卫星时设置相同 PresentationGroup 与天文坐标/半径，无需修改 C++。
3. PlanetAnchor 的 `Show On Star Chart` 控制地图上的天体轮廓/名称；`Show Screen Marker` 继续独立控制主画面标记。视觉背景还需在所属星球的 BackgroundNavigation 中配置名称和地图开关。
4. 已提供 `Tools/Editor/navigation_map_authoring.py` 的 `configure()` 与专用 `navigation_map_mcp.py`。MCP `NavigationMapTools.run` 可执行 `configure`、`inspect_editor`、`start_qa`、`qa_status` 和 `capture`。配置时必须停止 PIE。

## 修改文件

- `UI/JTSNavigationMapModel.h/.cpp`：阶段、分组、连续地表跟随与飞船局部三维投影。
- `UI/JTSSurfaceMapCapture.h/.cpp`：本地真实场景正交捕捉与资源生命周期。
- `UI/SJTSNavigationMap.h/.cpp`：地图绘制、引力色带、层级动画。
- `UI/JTSCruiseNavigationWidget.h/.cpp`、`UI/JTSPrototypeHUDWidget.cpp`：本地 HUD 接入与只读验证接口。
- `World/JTSCelestialNavigationTypes.h`、`World/JTSSpaceWorldManager.h`、`Components/JTSCompressedSpaceComponent.h`：Blueprint 可配置的显示参数。
- `Tests/JTSNavigationMapTests.cpp`：坐标、分组、阶段、反向动画、玩家居中、连续跟随、三维投影与生命周期回归。
- `Tools/Editor/navigation_map_*.py`、`configure_celestial_navigation.py`：关卡配置和 PIE 验证。
- `Content/Space/Maps/L_SpaceWorld.umap`：导航显示配置。

## 参考

- [GTA V 官方手册](https://dlassets-ssl.xboxlive.com/public/content/4f0a3089-ba2c-4f3d-9e38-102a41cbd885/GameManual/e1fc93ea-684e-4e2b-8511-5509682bff31/es-ES/index.html)：角色附近区域、地图跟随和朝向提示。
- [Elite Dangerous 官方手册](https://hosting.zaonce.net/elite/website/assets/ELITE-DANGEROUS-GAME-MANUAL.pdf)：飞船居中的三维扫描盘与上下高度连线。本项目据此设计位置投影，不复制游戏资产。
- [Epic 正交相机文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/orthographic-camera-in-unreal-engine)：真实场景的正交缩略图。

## 验证结果

2026-10-30 原工程已完成更新并验证。当前结果、截图和构建日志位于 `Saved/NavigationMap/MainProject/`。

- 原工程 UE 5.8 `spaceEditor Win64 Development` 完整编译、链接成功，耗时 40.08 秒；没有使用 Live Coding。
- `JTS.NavigationMap` 6 项测试与 `JTS.Spacecraft.CelestialNavigation` 共 7 项全部通过，无测试错误或警告。包含真实正交图像方向匹配、球面坐标、微小连续移动、独立玩家视图、三轴转动、平移、阶段切换和资源生命周期。
- 原工程 PIE 完成 14 个状态检查：火星地表、连续行走、登船未起飞、近地、星域、旋转、平移、星际、返航、月球地表与地月星域，全部通过。验证脚本通过现有服务器调试旅行接口选择火星起点，不依赖用户上次保存的调试目的地。
- 连续行走 20 帧，地图中心和角色位置误差均为 0 cm，玩家标记偏移均为 0。角色移动 700.016 cm，512 像素缩略图中的静态地形特征移动 30 像素，与 120 米图幅的理论值 29.867 像素一致，确认实际渲染纹理随角色连续移动。
- 火星星域接触列表只有火星，完整姿态旋转和平移后数值投影均变化；地月星域显示地球与月球。月球缩略图可见场景中实际生成的资源分布。
- 原关卡已保存：`MarI`、`MarII` 的 `Show On Star Chart` 为 false，地球、月球、火星为 true；星域分组和引力来源保留。停止 PIE 后检查关卡和内容的未保存修改列表，均为空。
- 此前独立副本还完成两轮各 14 个 PIE 检查，停止及重进 PIE 未出现捕捉组件生命周期错误。

本轮没有进行多人联网或 Shipping 打包性能测试。修改仅涉及各本地 HUD 的只读表现，没有增加网络状态或改变服务器玩法规则。
