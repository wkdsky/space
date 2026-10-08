# 月球 Kenney 石头与金属资源

复用现有 `AJTSMoonResourceSpawner` 球面范围生成和 `AJTSMoonResourceActor` 服务器采矿规则；模型选择保存在 Blueprint，小石头外观保存在物品 Data Asset。第三方模型复制到 `/Game/Space/Meshes/Resources/Moon`，以便配置碰撞和金属材质。

| 类型 | Kenney 来源 | 完整采集掉落 | 采矿工作量 |
| --- | --- | --- | --- |
| 小石头 | `rocks_smallB` | 直接拾取 1 石头 | 无需开凿 |
| 中石头 | `rock1` | 2 石头 | 5 |
| 大石头 | `rock_largeA/B` | 6 石头 | 24 |
| 中石头 + 金属 | `rock_crystals` | 2 石头 + 2 金属 | 10 |
| 大石头 + 金属 | `rock_crystalsLargeA/B` | 6 石头 + 6 金属 | 30 |

混合模型的 `crystal` 材质槽使用第三方 `metal` 材质。金属沿用现有 `EJTSResourceType::Ore` / `EJTSItemId::Ore`，物品显示名改为 `Metal`。混合矿采完一次性生成两类掉落；任一掉落生成失败时回滚本次全部掉落，保留矿点供后续开采。普通采矿和工程拆解共用掉落路径。

导入网格有偏移原点，运行时按包围盒居中并统一基础尺寸；中型约 100 cm、大型约 200 cm、小石头约 32 cm，矿点保留原有随机缩放。继续使用真实 Planet Mesh Surface 放置和径向朝向；模型变体编号及矿点掉落状态复制到客户端。

## 修改文件

- `Source/space/World/JTSMoonResourceActor.h/.cpp`：混合矿类型、双掉落、Blueprint 网格变体、原点校正和复制。
- `Source/space/World/JTSMoonResourceSpawner.h/.cpp`：五类资源权重、中型混合矿产量、复用原矿石权重生成大型混合矿。
- `Source/space/Items/JTSItemDefinition.h`、`JTSWorldPickupActor.cpp`：Data Asset 配置拾取模型和基础尺寸。
- `Source/space/Tests/JTSMoonResourceTests.cpp`：采矿掉落与实际 Blueprint 模型、朝向、碰撞回归。
- `Tools/Art/configure_moon_resources.py`：可重复执行的资产配置脚本。
- `Content/Space/Meshes/Resources/Moon/`：7 个模型副本，矿点添加凸包碰撞。
- `BP_MoonResourceActor`、`BP_MoonResourceSpawner`、`PDA_MoonSurfaceGameplay`、`DA_Item_Rock`、`DA_Item_Ore`：模型、生成类、分布和显示配置。

## Editor 操作

资源交互标记统一为单行 `[E] Rocks`（包括小石头和石头 + 金属矿）；散落金属显示 `[E] Metal`。不再显示矿点尺寸、混合类型、工具状态或工作量。HUD 按文字实际尺寸布局并关闭自动换行，标记位于模型顶部中心。顶部中心按当地球面法线和物理包围盒计算，保留模型相对偏移，不使用包围盒角点或渲染用 BoundsScale 膨胀。

此标记修正涉及 `JTSPrototypeHUDWidget.cpp`、`JTSMoonResourceActor.h/.cpp`、`JTSWorldPickupActor.cpp` 和现有 `JTSSurfacePlacementBounds.h/.cpp`。`JTSMoonResourceTests.cpp` 增加任意径向方向、模型偏移和 BoundsScale 下的顶部中心回归检查。

资产已经保存，无需重新设置。在 `L_SpaceWorld` 启动 PIE，前往月球资源区即可验证。小石头直接拾取；其余四类使用当前可用的采矿武器，例如动力战锤，混合矿同时产出石头与金属。

调节入口：

1. `BP_MoonResourceActor` → Class Defaults → Moon / Presentation：四组模型变体。
2. `PDA_MoonSurfaceGameplay` → Moon Resource Spawn Settings：五类权重为 40 / 20 / 15 / 15 / 10；大型混合矿继续使用 `OreWeight`。保留原配置的总数 10、区域半径 2000、排除距离及种子。
3. 同一 Data Asset 的 `LargeRockTotalYieldUnits` 和 `OreDepositTotalYieldUnits` 分别配置大型混合矿的石头与金属产量；`MediumMetalYieldUnits` 配置中型混合矿金属量。
4. `DA_Item_Rock` → Presentation：小石头模型和 32 cm 基础尺寸。

重新应用资产配置可在 Unreal Python 执行 `Tools/Art/configure_moon_resources.py`。不保存关卡。

## 验证

- `spaceEditor Win64 Development`：编译成功，关闭 Editor 编译，禁用 Live Coding。
- `JTS.Moon.Resources.MixedMiningYield`：四类矿点真实采矿、拒绝错误手持物、同时产生两类资源、禁止重复掉落。
- `JTS.Moon.Resources.AuthoredMeshAndCollision`：实际 Blueprint 的各模型变体、原点居中、尺寸一致、多材质保留和任意径向朝向下的开采射线碰撞。
- `JTS.Stellar.Abilities.EngineeringPreservesResourceYield`：现有工程拆解产量回归。
- Unreal MCP 实际 `L_SpaceWorld` PIE 检查：生成 10 个资源，包含小石头、中石头、大石头、中型混合矿和大型混合矿，全部使用配置后的模型。

3 项回归测试全部通过，0 失败。首次加载动力战锤时记录 1 条已有资产缺失警告，系统按现有规则使用 C++ 默认物品定义，采矿测试通过。报告保存在 `Saved/Automation/MoonResources/index.json`。

实际双客户端联网会话尚未测试；相关矿点类型、变体编号和产量字段使用现有 Actor 复制机制。
