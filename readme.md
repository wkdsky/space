# Jump to Space

## 本地开发命令

```powershell
D:\Software\UE_5.8\Engine\Build\BatchFiles\Build.bat spaceEditor Win64 Development D:\projects\space\space.uproject -WaitMutex -NoHotReloadFromID
```

UE 热重载快捷键：`Ctrl + Alt + F11`。

## 已有系统边界

| 系统 | 负责内容 |
| --- | --- |
| `GameMode` | 是否创建飞船、使用哪个飞船、玩家出生位置，以及玩法阶段规则。 |
| `SpacecraftActor` | 飞行、推进、转向、起飞、着陆和飞船状态。 |
| `SpaceWorldManager` | 宇宙世界状态、行星、Streaming 和行星查询。 |
| `SurfaceController` | 某个具体行星的地表玩法。 |

## 本机备份辅助命令

以下命令使用个人机器路径，不属于项目构建或运行流程：

```powershell
Get-ChildItem "C:\Users\coner\.codex" -Recurse -File |
Where-Object { $_.Name -like "*01a09009-4e5a-7a33-9dd4-c425159a23f5*" } |
Copy-Item -Destination "C:\Users\coner\codexbackup"
```

---

## 战斗、物品效果与目标交互架构

本节基于当前 C++ 代码审查记录现状，并定义后续完整战斗系统的演进方向。它区分“当前已经实现”和“建议新增”；不能把建议中的类或数据字段误认为已经存在。

### 结论

是，后续应采用**数据驱动的物品效果 + 组件化的运行时职责 + 接口化的目标响应**。但“组件化”不等于把一个 `UActorComponent` 挂到每个 `UJTSItemDefinition` 或 `FJTSItemInstance` 上：

- `UJTSItemDefinition` 是共享、只读的 `PrimaryDataAsset`；它应描述物品能产生哪些效果、有哪些传递方式和表现配置，不能保存玩家专属状态或执行运行时逻辑。
- `FJTSItemInstance` 是库存中的轻量实例；它保存数量、耐久度和实例 ID，当前不是 Actor，不能直接承载 `UActorComponent`。
- 组件应放在有生命周期和网络所有权的 Actor 上：玩家/武器实体负责发起与命中，目标 Actor 上的组件负责消费效果并改变自身状态。
- 每个物品可以同时配置“战斗伤害”和“采矿工作量”等多个效果。命中后由**目标所拥有的响应组件**决定消费哪一种效果，因此同一把镐、刀或枪对敌人、矿脉、门锁可以产生不同结果。
- 只有真正的设计例外才使用“指定物品”限制。限制应该是目标的可配置规则，优先匹配玩法标签；确实必须唯一时再匹配精确 `ItemId`，而不是把 `Knife`、`Axe` 等判断散落在攻击代码中。

简言之：物品声明“我能做什么”，传递组件确认“我如何命中”，目标组件决定“我接受什么以及如何变化”。这比“每种物品继承一个武器类”或“攻击组件逐个 `Cast` 目标类型”更符合项目的 Composition / Component / Interface 原则。

### 当前代码已经具备的基础

| 层级 | 当前类 | 现有职责和实际行为 |
| --- | --- | --- |
| 物品实例 | `FJTSItemInstance`、`UJTSInventoryComponent` | 库存复制物品实例；前四个物理槽位是快捷栏，`GetActiveItem()` / `GetActiveItemId()` 决定当前手持物。 |
| 物品定义 | `UJTSItemDefinition` | 数据资产提供 `CapabilityMask`、`CombatDamage`、`MiningWork`、`RangedDamage`、攻击间隔、射程和自动射击等配置。`JTSItemDefinitionLibrary` 会优先加载 `Content/Space/Data/Items/DA_Item_*`，资产缺失时才使用 C++ fallback。 |
| 直接交互 | `UInteractionComponent`、`IInteractable` | 用视野、距离和遮挡寻找目标；客户端只提交目标意图，服务器重新校验距离、视线和目标状态后才调用 `Interact`。 |
| 近战传递 | `UJTSMeleeComponent`、`JTSAnimNotify_AttackHit` | 攻击输入开始动作，动画 Notify 在命中帧调用 `PerformHitCheck()`；服务器重新寻找目标并在单次挥击内去重。 |
| 远程传递 | `UJTSRangedWeaponComponent` | 当前是服务器权威的 `ECC_Visibility` 命中扫描（hitscan），不是投射物；服务器命中后通过 `MulticastShotTrace` 只同步弹道表现。 |
| 生命 | `UJTSHealthComponent` | 监听 UE 标准 `OnTakeAnyDamage`，在服务器修改并复制生命/死亡状态，向 UI 和死亡流程广播事件。 |
| 旧式目标接口 | `IJTSMeleeTarget` | 为 Moon 原型的无生命目标提供近战可用性、提示和命中回调；目前是近战的兼容后备路径。 |
| 月球矿脉 | `AJTSMoonResourceActor` | 保存并复制采矿工作量；直接交互、近战和远程命中最终都会重新验证当前手持物的 `MiningWork`，再扣除矿脉工作量并生成掉落。 |
| 月球生物/巢穴 | `AJTSMoonAntActor`、`AJTSMoonAntNestActor` | 蚂蚁拥有 `UJTSHealthComponent`；巢穴仍通过 `IJTSMeleeTarget` 直接判断 `Knife` / `Axe`，属于需要迁出的原型特例。 |

当前的物品定义已经正确地把战斗伤害与采矿工作量分开：`CombatDamage` 不是 `MiningWork`，`RangedDamage` 也不是把采矿值换个名字。这是后续扩展的正确起点。

### 当前真实命中流程

```text
LMB 输入
  └─ AJTSCharacter
       ├─ 当前物品带 RangedWeapon 能力：UJTSRangedWeaponComponent::StartFire()
       │    └─ ServerStartFire -> 服务器 LineTrace
       │         ├─ 命中 AJTSMoonResourceActor：直接调用 ApplyMiningWork(..., MiningWork)
       │         └─ 命中带 UJTSHealthComponent 的 Actor：ApplyDamage(..., RangedDamage)
       │
       └─ 其他手持物：UJTSMeleeComponent::AttackPressed()
            └─ JTSAnimNotify_AttackHit -> ServerPerformHitCheck
                 ├─ 目标带 UJTSHealthComponent：ApplyDamage(..., CombatDamage)
                 └─ 否则目标实现 IJTSMeleeTarget：ReceiveMeleeHit(...)
                      └─ 矿脉从当前手持物重新读取 MiningWork

E 交互
  └─ UInteractionComponent::ServerTryInteract
       └─ IInteractable::Interact
            └─ 矿脉从当前手持物重新读取 MiningWork
```

所以，“同一物品因目标不同发生不同作用”的雏形已经存在：例如带 `MiningWork` 的手持物命中矿脉时推进采矿进度，命中带生命组件的敌人时使用战斗伤害。枪械也可以同时配置 `RangedDamage` 和 `MiningWork`。不过，这个分发仍由攻击组件和月球目标的专用代码共同完成，尚未成为可扩展的通用系统。

### 当前实现的边界与缺口

下面这些结论是代码现状，不是对已有功能的否定；它们说明为什么完整战斗系统不能只继续往现有 `if` / `Cast` 中加分支。

1. `EJTSItemCapability::RangedWeapon` 已用于选择远程攻击；但 `Mining` 和 `MeleeOverride` 当前不是通用行为准入条件。采矿实际看 `MiningWork > 0`，`MeleeOverride` 没有运行时消费点。能力位目前更多是目录/展示信息，而不是完整效果协议。
2. `UJTSMeleeComponent::GetCurrentAttackType()` 仍对 `Knife`、`Axe`、`Pickaxe` 使用 `ItemId` 分支；`AJTSMoonAntNestActor` 也硬编码了 Knife/Axe 特例。新增一种切割工具、钻头或震荡器时会继续扩大枚举判断。
3. `UJTSRangedWeaponComponent::FireOnce()` 明确 `Cast<AJTSMoonResourceActor>`。每新增一种可受远程工具作用的目标，都要修改武器组件，而不是把规则放到目标自身。
4. `IJTSMeleeTarget` 只携带攻击者和粗粒度 `EJTSMeleeAttackType`；它没有命中点、来源物品实例、效果类型、基础数值或命中方式，无法可靠表达投射物、范围伤害、弱点和特殊工具。
5. 当前远程是 hitscan，没有 `AJTSProjectileActor`；弹药、装填、后坐力、投射物飞行和命中后的统一效果分发尚未实现。`FJTSItemInstance::Durability` 已有字段，但没有消耗逻辑。
6. 半自动枪械的 `RangedFireInterval` 只用于自动开火定时器；普通重复 `StartFire` 请求尚无服务器射速节流。近战的标准动画命中路径也应在服务器维护攻击序号/有效窗口，而不能仅依赖客户端动画 Notify。
7. `UJTSHealthComponent` 目前只处理数值生命和死亡；没有伤害类型、护甲/抗性、部位倍率、护盾、状态效果或阵营/友军伤害策略。所有现有伤害都传入 `UDamageType::StaticClass()`，来源物品语义不会保留到生命组件。
8. 本次代码范围中没有 AI 对玩家的独立攻击路径；目前 `AJTSMoonAntActor` 的 `ApplyDamage` 调用是旧接口下“玩家攻击蚂蚁”的兼容处理，不是蚂蚁攻击玩家。
9. 近战可用性仍含 Moon 地表玩法的原型门槛，完整系统需要改为当前 Expedition/Planet 玩法状态的通用检查。

### 目标结构：效果、传递与响应分离

建议的职责拆分如下。名称是建议命名，实施时可根据现有目录命名调整，但职责边界不应混合。

```text
物品 Data Asset（静态配置）
  UJTSItemDefinition
    └─ FJTSItemActionSpec[]
         ├─ 传递配置：近战 / 直接交互 / Hitscan / Projectile / 范围
         ├─ 命中配置：射程、冷却、弹体类、命中策略
         └─ FJTSItemEffectSpec[]：伤害、采矿、切割、破解等效果

玩家或武器 Actor（运行时发起）
  UJTSMeleeComponent / UJTSRangedWeaponComponent / UInteractionComponent
    └─ 服务器验证命中后，构造 FJTSInteractionContext
         └─ UJTSItemEffectResolverComponent 提交给命中目标

目标 Actor（运行时响应）
  UJTSInteractionReceiverComponent（路由）
    ├─ UJTSHealthComponent：消费 Effect.Damage.*
    ├─ UJTSMiningComponent：消费 Effect.Mining.Work
    ├─ UJTSResistanceComponent：按目标材质/部位调整倍率
    └─ UJTSRequirementComponent：验证特定物品或标签要求
```

#### 1. 物品定义只描述能力和数值

建议新增两个数据结构，而不是继续无限增加 `CombatDamage`、`RangedDamage`、`MiningWork` 这类平行字段：

| 建议结构 | 关键字段 | 职责 |
| --- | --- | --- |
| `FJTSItemActionSpec` | `DeliveryMode`、冷却、射程、命中形状、可选 `ProjectileClass`、`Effects` | 描述某次使用如何传递效果；一件物品可以有近战和远程两个 Action。 |
| `FJTSItemEffectSpec` | `EffectTag`、`BaseMagnitude`、可选伤害标签/元素标签 | 描述“命中有效目标后可施加什么”，例如物理伤害、采矿工作、切割或破解。 |
| `FJTSInteractionContext` | 攻击者、控制器、来源 `ItemId` 与 `InstanceId`、Action、服务器命中 `FHitResult`、效果快照 | 一次服务器端交互的不可变上下文；不由客户端直接提供，也不需要整体复制。 |
| `FJTSInteractionResult` | 是否接受、已应用效果、是否消费、反馈标签 | 让命中表现、日志和 UI 从同一权威结果取信息。 |

建议引入 `GameplayTags` 模块，使用受约束的 `FGameplayTag` / `FGameplayTagContainer`，例如：

```text
Effect.Damage.Kinetic
Effect.Damage.Thermal
Effect.Mining.Work
Effect.Cutting
Effect.Hacking
Tool.Mining.Pick
Tool.Cutting
Item.Keycard.LunarLab
Target.Material.Ore
```

当前 `AffinityTags` 的注释明确其用途是商店展示、推荐和排序；不要把它改成权威玩法限制。应单独新增 Gameplay Tag 字段，避免 UI 改名或展示排序意外改变战斗结果。

物品效果的语义必须保持清楚：`Effect.Damage.Kinetic` 才能扣生命，`Effect.Mining.Work` 才能推进矿脉；不要把一个“Damage”数值交给矿脉后再猜测它是不是采矿。这样一件物品同时可作为武器和工具，而每个效果仍可独立平衡。

例如，镐的近战 Action 可以同时携带 `Effect.Damage.Kinetic` 与 `Effect.Mining.Work`；蚂蚁的生命组件只消费前者，矿脉组件只消费后者。手枪的 hitscan Action 也可携带两种效果，但矿脉会按照自己的材料规则、效率和要求消费采矿工作，生物则按护甲/弱点消费伤害。

#### 2. 传递组件只负责“是否命中”

保留并收敛现有组件的单一职责：

- `UJTSMeleeComponent`：服务器验证攻击状态、近战窗口、距离、视线和命中 Sweep；不认识矿脉、蚂蚁、门锁等具体 Actor 类。
- `UJTSRangedWeaponComponent`：服务器验证当前物品、弹药/耐久、射速和瞄准；hitscan 或投射物命中后都进入同一个效果提交入口。
- `UInteractionComponent`：继续负责 E 键目标发现、提示和服务器距离/视线校验；当交互本质是“对目标使用当前工具”时，提交 `DeliveryMode = DirectUse` 的同一上下文，而不是由每个目标自行读取背包。
- 未来 `AJTSProjectileActor`：由服务器生成并复制，保存发射瞬间的来源物品和效果快照；命中时调用同一效果提交入口。玩家随后切换或丢弃武器不能改变已经发射的子弹效果。

这里不应在客户端把“伤害值、完整效果数组、命中 Actor”当成可信参数上传。客户端最多表达输入/瞄准意图；服务器从权威库存、控制器视角和碰撞结果重新构造上下文。

#### 3. 目标组件负责“接受什么、如何变化”

目标 Actor 可拥有一个路由组件和多个小型响应组件。路由可以通过 `IJTSItemEffectReceiver` 接口遍历这些组件并按优先级得到结果；实际状态修改必须发生在对应的小组件中。

| 组件 | 只负责什么 | 当前对象的迁移方式 |
| --- | --- | --- |
| `UJTSHealthComponent` | 消费伤害效果、生命复制、死亡事件。后续可扩展为接收带上下文的伤害，而旧的 UE `ApplyDamage` 继续作为环境伤害/兼容入口。 | 蚂蚁和玩家继续复用。 |
| `UJTSMiningComponent` | 消费 `Effect.Mining.Work`，保存剩余工作量、掉落和资源生成事务。 | 从 `AJTSMoonResourceActor` 拆出，Actor 保留网格、球面放置和表现。 |
| `UJTSResistanceComponent` | 按效果标签、材质、部位或状态给出倍率/免疫。 | 先可选地挂到生物和矿脉，避免把抗性写进武器。 |
| `UJTSRequirementComponent` | 在响应前验证物品标签、工具等级、任务状态或精确 `ItemId`。 | 将 Moon Ant Nest 的 Knife/Axe 判断迁入此组件或其 Data Asset 配置。 |
| 专用响应组件 | 只消费自身语义，例如切割、破解、激活、治疗或回收。 | 新星球内容由 Blueprint 配置组件和规则，不改通用攻击组件。 |

“必须使用某个特殊物品”应按以下优先级设计：

1. 大多数情况匹配 Gameplay Tag 或 Tag Query，例如任何带 `Tool.Cutting` 的工具都能切割。
2. 需要等级时匹配标签加数值门槛，例如 `Tool.Mining` 且等级至少为 2。
3. 叙事钥匙、唯一任务物或一次性祭品才匹配精确 `EJTSItemId` / `InstanceId`。

规则属于目标的组件/Data Asset，Blueprint 可以为某个具体门、遗迹或星球资源配置它；通用 C++ 不应保存关卡 Actor 引用，也不应写“某星球只有某把刀有效”。

### 多人权威与全局规则

这是 1–4 人共享 Expedition，战斗不能有单机捷径。建议完整链路遵循以下顺序：

1. 客户端通过所属 Pawn 组件发送“开始攻击/停止攻击/使用工具”意图。
2. 服务器验证当前阶段、玩家是否存活/可操作、当前快捷栏物品、冷却、弹药/耐久、阵营规则、距离、视线、碰撞和目标状态。
3. 服务器产生 `FJTSInteractionContext`，并让目标响应组件消费效果；客户端上传的伤害值和目标结论一律不可信。
4. 生命、采矿进度、掉落、死亡和状态效果由目标组件复制；命中特效、枪线、音效和 UI 提示只在权威结果后用 Multicast 或客户端表现触发。
5. 投射物在服务器命中时走同一入口；不要让投射物 `OnHit` 直接绕过效果路由调用 `ApplyDamage`。

友军伤害、PVP、无敌阶段、安全区和当前玩法阶段是全局规则，应由 `AJTSGameplayGameModeBase`/派生 GameMode 在服务器裁决，并通过 `AJTSGameState` 的阶段状态供客户端表现。它们不属于某个物品或某个矿脉组件。

### 与现有代码的渐进迁移

不要为这项工作重写背包、拾取、生命复制或地表系统。建议按以下小步推进，每一步都能独立回归：

1. **定义通用类型。** 在 `Core/` 或 `Items/` 新建效果、Action、上下文和结果类型；为 Gameplay Tags 增加模块依赖和项目标签配置。保留旧字段并提供到新 Action Spec 的兼容映射，先不批量重做已有 Data Asset。
2. **先落地目标响应。** 为矿脉提取 `UJTSMiningComponent`，让蚂蚁/玩家继续使用 `UJTSHealthComponent`，为巢穴增加可配置的要求组件。此阶段可以保留 `IJTSMeleeTarget` 适配器，避免破坏原型关卡。
3. **接入统一提交入口。** 修改 `UJTSMeleeComponent`、`UJTSRangedWeaponComponent` 和直接交互路径：命中后不再 `Cast<AJTSMoonResourceActor>`，也不再按 `ItemId` 决定目标效果，而是提交上下文给目标接收器。
4. **补齐服务器战斗校验。** 将近战攻击序号/有效命中窗口和远程 `NextFireTime` 放在服务器；接入弹药、装填、耐久消耗、死亡后禁止操作、阵营/友军伤害和伤害结果日志。
5. **增加投射物和复杂响应。** 先确保 hitscan 与近战共用完整效果路径，再增加服务器生成的投射物、范围效果、护甲/弱点和状态效果；不要另造一条投射物专用伤害链。
6. **最后处理表现和内容。** 动画、枪口火焰、音效、命中反馈和具体资产放到 Blueprint；每个 Planet 的资源类型、抗性和特殊门槛由 SurfaceController / Data Asset 配置。

当前不应仅为本轮改动强行引入 GAS。现有 `UJTSHealthComponent`、库存和组件网络模型足以支持上述第一阶段；当产品明确需要多属性、可叠加 Buff/Debuff、大规模技能、复杂冷却和客户端预测时，再单独评估 GAS 迁移。

### 验收矩阵

| 场景 | 服务器预期结果 |
| --- | --- |
| 镐近战命中蚂蚁 | 只消费伤害效果，生命正确复制，单个攻击窗口不重复扣血。 |
| 同一把镐命中矿脉或按 E 使用 | 只消费采矿工作，矿脉进度/掉落正确复制。 |
| 手枪或机枪命中矿脉 | 命中扫描/投射物走统一入口，目标仅按配置消费采矿效果。 |
| 手枪或机枪命中生物 | 目标仅按配置消费远程伤害，护甲/弱点规则可生效。 |
| 特殊门锁/遗迹 | 普通物品被拒绝；满足标签、等级或精确物品要求后才产生对应效果。 |
| 子弹发射后切换物品 | 已发射子弹使用发射瞬间保存的效果快照，不受当前快捷栏变化影响。 |
| 客户端伪造目标、伤害或射速 | 服务器重新验证后拒绝；客户端不能直接改生命、矿脉、掉落或耐久。 |
| 多人同时攻击同一目标 | 每次权威命中只结算一次，状态与表现对所有客户端一致。 |

### 后续 Editor 配置步骤

本次只更新文档，不需要在 Unreal Editor 中修改资产。开始实施上述系统时，应按以下步骤配置：

1. 在 Project Settings 注册效果、工具和目标材质的 Gameplay Tags，并为 `GameplayTags` 添加 C++ 模块依赖。
2. 在每个 `DA_Item_*` 中配置 Action 与 Effect；不要填入关卡 Actor 引用。武器、工具和混合物品都使用同一套数据结构。
3. 在目标 Blueprint 上添加相应的生命、采矿、抗性、要求或专用响应组件；特殊要求在组件/Data Asset 中配置标签或精确物品。
4. 为近战、交互、投射物和地表探测建立职责明确的 Collision Profile/Channel。当前多条路径使用 `ECC_Visibility`，完整战斗系统不应让攻击、交互和地表 Probe 共享含义模糊的通道。
5. 确认攻击蒙太奇每个有效攻击窗口只有一个 `JTSAnimNotify_AttackHit`，并在两客户端 Listen Server 场景验证上表的每一项。

