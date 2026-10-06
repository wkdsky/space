# 星际权杖与喷射聚焦

2026-10-06：十二种星际武器统一为上方圆球核心、下方短杖的手持模型。对应核心决定发光颜色，配件使用不同的金属托座。模型总长 59 厘米，握柄原点在掌心，核心中心在模型 `(0, 0, 36)`，发射点在 `(8, 0, 36)`。

持握时权杖竖直；施放时右臂举高并伸直，结束施放后平滑返回持握姿势。竖直方向使用角色的地表向上方向，适用于真实球面星球。持续施放状态由服务器复制，黑洞的单次释放使用短暂的举杖动作，独立黑洞继续存在不会让人物一直举手。

喷射的基础左键为 **120° 全角、10 米**，按住右键可独立施放 **30° 全角、22 米** 聚焦喷射，同时相机进入肩后瞄准，基础 FOV 从 90° 过渡至 60°。射程指圆锥沿中心线的长度；两侧按全角判定，避免 120° 特效已经覆盖侧方敌人但命中范围被球形查询提前截断。同时按住左右键时采用聚焦模式，松开右键而继续按左键时恢复广角喷射。距离与范围升级沿用已有配件点数，伤害、遮挡、能量消耗仍由服务器验证。

持续施放的心跳超时排除当前服务器慢帧的时长，避免首次创建特效时，攻击定时器先于同帧本地心跳执行而误停。没有客户端心跳的攻击仍会在正常服务器帧上超时；慢帧伤害/能量积分仍最多累计 0.2 秒。

## 修改文件

- `Source/space/Items/JTSStellarWeaponCatalog.h`：聚焦距离/角度和竖直权杖配置。
- `Source/space/Components/JTSStellarWeaponComponent.h/.cpp`：右键独立喷射、施放状态及单次举杖事件。
- `Source/space/Components/JTSWeaponVisualComponent.h/.cpp`：握柄保持在掌心，权杖始终沿地表向上。
- `Source/space/Animation/JTSAnimInstance.h/.cpp`：持握/举杖姿势、平滑过渡和蓝图可配置角度。
- `Source/space/Player/JTSCharacter.cpp`：星际武器聚焦进入现有瞄准相机。
- `Source/space/Tests/JTSStellarWeaponTests.cpp`：全角、距离、右键独立输入、释放、能量、模型和 200 目标覆盖回归。
- `Tools/Art/generate_stellar_held_models.py`、`import_stellar_held_models.py`、`configure_stellar_held_models.py`、`apply_stellar_scepters.py`、`configure_stellar_combat_effects.py`。
- `SourceArt/StellarWeapons/` 和 `Content/Space/Meshes/Weapons/Stellar/` 的十二个模型。
- `Content/Space/Data/Weapons/DA_StellarWeaponCatalog.uasset`。
- `Content/Space/Blueprints/Player/Animations/ABP_JTSPlayer_Casual_2.uasset`。

## Unreal MCP 与 Editor 操作

已通过项目本地 Unreal MCP 端点执行模型导入/目录配置，并通过 `ObjectTools` 查询及设置动画蓝图默认值，再编译、保存蓝图。`Stellar|Pose` 中持握上臂/前臂为 `-55° / 0°`，施放上臂/前臂为 `85° / 85°`，过渡速度为 `12`。配置留在动画蓝图，C++ 不绑定项目资源路径。

无需手动连接动画图或替换模型。打开项目后，在圆形和其正下方菱形装备匹配核心/配件，按 Tab 选中；按住左键广角喷射，按住右键聚焦喷射，松开恢复。重新生成模型时依次运行生成脚本，再在重编译的 Editor Python 控制台执行 `Tools/Art/apply_stellar_scepters.py`。

模型改造涵盖全部十二种组合。原有九种 `PresentationOnly` 组合仍保留其未实现的攻击状态；本次没有替其发明攻击规则。聚焦射线和黑洞玩法继续使用各自现有技能。

## 验证

- `spaceEditor Win64 Development` 编译成功，日志：`Saved/Logs/StellarScepterFinalBuild.log`。
- `JTS.Stellar` 全部 22 项测试通过（20 项无警告，2 项含已有普通物品资源警告），0 项失败。报告：`Saved/StellarScepterFinalTests/index.json`。
- Unreal MCP 启动实际 PIE，装备火核心与喷射配件，分别按住左键、独立按住右键及松开：持续施放消耗能量，相机 FOV 为 `90° → 60° → 90°`，松开后恢复持握姿势。
- 施放时手腕比持握时高约 57 厘米，上臂和前臂方向点积约 `1.0`，验证为伸直姿势。运行记录：`Saved/StellarScepterRuntime.json`；截图：`Saved/StellarScepterWideCast.png`、`Saved/StellarScepterFocusedCast.png`。
- 范围回归覆盖 120°/30° 边界、10 米/22 米前向射程、宽角侧面命中、200 目标覆盖、右键独立施放，以及首次特效慢帧后持续输入仍然有效。
