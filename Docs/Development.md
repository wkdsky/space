# 开发、Editor 与验证

## 工程入口

工程使用 Unreal Engine 5.8，项目为 `space.uproject`。默认启动地图 `/Game/Space/FrontEnd/Maps/L_Entry`；正常流程从前端进入大厅、地球发射，再进入 SpaceWorld。仅测试宇宙和月球时可在 Content Browser 打开 `/Game/Space/Maps/L_SpaceWorld`，但直接 PIE 不等同于验证完整旅行和存档流程。

PowerShell 启动：

```powershell
& 'D:\Software\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' 'D:\projects\space\space.uproject'
```

## 编译与检查

使用项目现有 target 与配置。C++ 验证命令：

```powershell
& 'D:\Software\UE_5.8\Engine\Build\BatchFiles\Build.bat' spaceEditor Win64 Development 'D:\projects\space\space.uproject' -WaitMutex -NoHotReloadFromIDE -NoLiveCoding
```

项目另有 `space` 和 `spaceServer` target。必要时先关闭编辑器，避免 Live Coding / 热重载干扰；编译失败先解决第一个有效错误，不改无关文件绕过。

测试代码位于 `Source/space/Tests`。根据变更选择相应自动化测试和 PIE 场景；文档整理或预览路径移动无需为此编译整个工程。历史报告中的“通过”不能替代当前测试结果。

## Editor 核对步骤

1. 打开目标地图，停止 PIE 后检查关卡实例与引用。
2. 在 Blueprint Class Defaults 查看通用默认值，再检查关卡实例覆盖及 Data Asset；不要只读 C++ 默认参数。
3. 月球查看 `PDA_MoonSurfaceGameplay`、SurfaceController 的 PlacedEcologyHabitats，以及 NestBossEncounter 的七个 Nests 引用。旧 `MoonAntNestCount = 0` 保持关闭旧生成器。
4. 宇宙查看 SpaceWorldManager 的五个 PlanetAnchor、压缩空间组、导航显示开关及干扰开关。
5. 飞船查看 BP_Spacecraft / 当前船型、船坞 Definition、扫描区域、武器硬点与舱体组件。
6. 修改复制玩法时使用至少两名玩家验证服务器结果，再验证跨图 / 读档恢复；只看本地 UI 不足以证明正确。

月球验证需保护原始 Mesh、碰撞、主坑和当前七个固定洞口位置。具体规则见 [Moon](Moon.md)。

## 工具使用

优先 Unreal MCP / Blender MCP。现有 `Tools/Art`、`Tools/Editor`、`Scripts` 混有配置工具、验证工具和历史迁移脚本；文件存在不意味着可直接批量执行。先查看入口、适用资产、前置地图、输入来源与覆盖行为。

- `mirror_boss_editor.py` 包含当前七巢穴配置与旧数据迁移。
- 月球 surface、cut、refinement 等历史脚本可能操作旧布局或地形。禁止用切割 / 替换原始月球 Mesh 的路径恢复生态。
- 飞船 `SourceArt/ShipRefit`、`ShipDocks` 等目录的 Blender 生成脚本与源模型放在一起；UE 资产配置脚本位于 `Scripts`。
- 火星电影配置工具可能结束编辑器进程，应在独立会话运行，详见 [美术资产](ArtAssets.md)。

## 目录维护

`Docs` 只保留当前使用的说明，入口见 [索引](README.md)。根目录整体设计描述目标，不应把未实现流程写成当前能力。

`Saved` 中保存本机档案、远征存档、调试偏好、编辑器用户设置和源控状态，不能整目录删除。通常可清理关闭会话的备份日志、崩溃残留、ShaderDebugInfo、SemanticSearch 缓存及一次性截图 / QA 输出；正在使用的日志保留。

`SourceArt` 保留可重建输入、授权与精选预览；自动备份和临时报告按 [美术资产规则](ArtAssets.md) 清理。`Content/Movies` 中的正式影片供运行时使用，不能当普通预览删除。移动任何导入源之前先检查资产导入路径，避免破坏 Reimport。

本次整理仅核对当前代码、文件引用和编辑器配置；没有把旧报告中的完整玩法测试结果继承为本次验证。结束后保持 Unreal Editor 可见，便于继续自主调试。
