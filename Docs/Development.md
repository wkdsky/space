# 开发、Editor 与文件维护

现有规则见 [Architecture.md](Architecture.md)，游戏操作见 [Gameplay.md](Gameplay.md)，月球参数见 [Moon.md](Moon.md)。本次整理只调整文档、历史输出和资产工具位置，未修改 Runtime C++、关卡或 Blueprint 资产。

## 编译与检查

项目关联 Unreal Engine 5.8；现有目标为 `spaceEditor`、`space`、`spaceServer`。本机 Editor 编译命令：

```powershell
& 'D:\Software\UE_5.8\Engine\Build\BatchFiles\Build.bat' spaceEditor Win64 Development 'D:\projects\space\space.uproject' -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
```

修改 C++ 后先保存资产并关闭该项目的全部 Unreal Editor，再完整编译并重开，避免 Live Coding / 热重载留下旧模块。编译失败先处理第一个有效错误，不改无关文件绕过。

`Source/space/Tests` 保留可重跑的原生回归测试：资源分布 / 产量 / 并集、飞船包络 / 支撑 / 灯光、聚居地人口、蚂蚁 Mass / 状态、立方体群体 / 身体分离、星际能力及 UI 等。可在 Session Frontend → Automation 搜索 `JTS`，按本次改动选择相关用例。生成的 JSON、日志和截图放 Saved，不积累到 Docs。

过去的单机 / 服务器自动化结果不等于多客户端联机验收。四客户端延迟、语音及实机性能仍需独立验证。最近历史记录中有立方体身体单步修正断言与 900 cm/s 配置不一致、拥挤场出生个体退出距离断言失败；本次清理未重跑或修改这些测试，不将旧报告表述为当前全套通过。

## 常用资产入口

| 需要调整的内容 | Editor 入口 |
| --- | --- |
| 地图、启动流程与网络服务 | `Config/DefaultEngine.ini`、`DefaultGame.ini`；关卡 World Settings / GameMode Blueprint |
| 玩家 / 飞船选择、起始物品、地表控制器 | 实际使用的 `BP_EarthGameMode` / `BP_SpaceWorldGameMode` |
| 星球中心、真实表面、Streaming | PlanetAnchor / SpaceWorldManager 的 Blueprint 与关卡实例 |
| 首次停放与出生点 | 对应 `BP_JTSPlanetArrivalAnchor` / `SpacecraftArrivalPoint` |
| 飞船包络 / 起落 / 支架 | `BP_Twinfork_R1` 的 SurfaceEnvelope、FlightMovement、LandingSupport、Presentation 组件 |
| 普通物品、采矿、拾取与普通武器数值 | `/Game/Space/Data/Items/DA_Item_*` |
| 星际奖池 / 激活颜色 | `/Game/Space/Data/Shop/DA_StellarAllianceLoot` |
| 星际组合、基准能力、手持模型和 FX 类型 | `/Game/Space/Data/Weapons/DA_StellarWeaponCatalog` |
| 高举姿势 | `ABP_JTSPlayer_Casual_2 → Stellar / Pose` |
| 目标状态模型、材质与 sockets | `BP_StellarTargetStatusFX`；冰雹在 `BP_StellarFreezingFX` |
| 月球资源、蚂蚁、巢穴与地标 | [Moon.md](Moon.md) 的 PDA / Spawner / SurfaceController 入口 |

新飞船必须测量自己的真实足垫平面、展开接地点及伸缩段，不能直接沿用 Twinfork 尺寸。当前 Twinfork 足底为 56 × 40 cm、安全采样半尺寸 26 × 18 cm、局部底面 Z=-18 cm。更换网格后重新校准 LandingSupport 的 Feet。

## 保留的工具

`Scripts` 与 `Tools/Art` 是按需执行的开发工具，不是游戏运行依赖。现有项目资产已经配置；修改参数优先在 Editor 对应资产中保存，重新执行配置脚本会应用脚本中的基准值。

| 用途 | 文件 / 顺序 |
| --- | --- |
| 月球资源模型配置 | `Tools/Art/configure_moon_resources.py`；只更新资产，不保存关卡 |
| 月球范围 MCP 编辑 / 检查 | `Tools/Art/moon_distribution_mcp.py`，按需加载，不自动启动 |
| 星际手持模型 | 普通 Python 执行 `generate_stellar_held_models.py`，再在 Unreal Python 执行 `apply_stellar_scepters.py` |
| 星际战斗 / 能力美术 | 对应 `generate_stellar_combat_effects.py` / `generate_stellar_abilities.py`，再执行相应 configure 脚本 |
| 状态、黑洞与斥力美术 | `configure_stellar_status_effects.py`、`configure_black_hole_physics.py`、`configure_stellar_repulsion.py` |
| Twinfork 源网格 | 普通 Python 执行 `generate_twinfork_meshes.py`、`generate_twinfork_gear_plume.py`、`generate_twinfork_plume.py`；输出 `SourceArt/TwinforkR1` |
| 原生战斗音效 | 普通 Python 执行 `generate_combat_audio.py` / `generate_punch_audio.py`；输出 `SourceAssets/Audio` |
| 飞船足垫 / 包络基准 | Unreal Python 执行 `configure_landing_support.py` / `configure_surface_envelope.py` |
| 骷髅宇航员及尸体源模型 | `Tools/Art` 中现有 fetch、build、fit、import / configure 工具；源文件与授权在 `SourceArt/SkeletonAstronaut` / `MoonAntNests` |

以上未写全路径的 Python 文件均位于 `Tools/Art`。在 Editor Python 控制台中可执行：

```text
py D:/projects/space/Tools/Art/configure_surface_envelope.py
```

先停止 PIE，再执行资产配置。足垫脚本现在只更新 Blueprint 默认值，不删除关卡 Actor、不保存当前关卡；两个飞船配置脚本的输出统一写入 `Saved/AssetConfiguration`。Python 远程执行默认关闭，仅在实际需要时临时开启并在结束后恢复。

## 联机配置

当前 `DefaultPlatformService=NULL` 用于本机 / LAN。EOS 使用 [DefaultEngineEOS.example.ini](../Config/DefaultEngineEOS.example.ini) 按部署环境配置；不将凭据硬编码进 C++。所有菜单调用 OnlineSessionSubsystem，不新增裸 IP 加入界面。

语音使用 VoiceSubsystem 与 Provider。当前配置不启用 Provider positional voice 或无线电功能；实际可用能力需配好 Provider 后验证，不增加自定义音频传输。

## 目录维护与此次清理

Docs 只保留四份当前说明：架构、玩法、月球、开发。阶段实施记录、重复配置说明、旧截图链接及 Validation 报告已移除。

Saved 保留真实 SaveGames、Autosaves、Editor Config、当前编辑器日志及 UE 常规状态目录；历史一次性修改 / 探测脚本、QA 目录、测试输出、旧日志、崩溃与 Shader 调试数据可清理。可复用的生成 / 配置脚本归 Tools/Art，可重新编辑的源文件归 SourceArt / SourceAssets。

此次从 Saved 迁出的源文件为 `SourceArt/TwinforkR1`、`SourceAssets/UI/Navigation` 和 `SourceAssets/Audio` 中的普通枪械 WAV。已导入的 Unreal 资产可继续使用；以后需要重新导入这些来源时，在资产 Import Settings 的 Source File 中选择新位置，或使用 Reimport With New File。此次没有批量改写资产导入元数据。

`Docs/Validation` 和 `Tools/Validation` 已删除，SourceArt 中 14 份历史验证 / 测试 JSON 也已清理，原生测试及有复用价值的 Art 检查工具保留。工具按需产生的新报告属于临时输出；模型构建所需的几何、来源和授权文件保留。Python 缓存已清理并加入忽略规则，避免再次混入版本控制。

此次整理无需 Editor 手动操作；未运行 C++ 编译。检查范围为文档链接、Python 语法、删除路径边界及已有源码 / 配置 / 修改中资产的保留情况，不宣称重新验收了 Gameplay。
