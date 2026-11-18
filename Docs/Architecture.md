# 架构与联机

## 职责边界

C++ 提供规则、数据结构、通用接口和 Runtime 能力；Blueprint / Data Asset 配置资源、类、参数和关卡实例。GameMode 决定玩家与飞船的生成位置，不把具体 Actor 或关卡资产引用写死在通用 C++ 中。

| 层级 | 当前职责 |
| --- | --- |
| `AJTSGameplayGameModeBase` 及派生类 | 服务器玩法初始化、阶段推进、出生、旅行 |
| `AJTSGameState` | 复制全局阶段、期限、当前飞船和星球状态；承载团队发现记录 |
| `AJTSPlayerState` | 玩家准备、房主、外观色、登船与远征状态 |
| `AJTSSpaceWorldManager` | 天体注册、当前星球、流送、旅行与宇宙状态 |
| `AJTSPlanetAnchor` | 天体中心、真实表面查询、球面重力、着陆依据 |
| `AJTSSpacecraftActor` 与组件 | 飞行、起落、乘员、仓储和模块能力 |
| Planet SurfaceController / 生态组件 | 对应星球的资源、聚居地、生物与事件 |
| Character / 交互组件 | 玩家输入、移动、交互、装备与战斗 |

优先沿用 Component、Subsystem、Interface。聚居地生物和范围资源分别复用已有模板，避免把多个系统继续堆进单个 Actor。

## 启动与旅行

默认入口为 `/Game/Space/FrontEnd/Maps/L_Entry`，前端使用 MainMenu GameMode。常规流程为 Entry → PreLaunchLobby → EarthLaunchPrototype → SpaceWorld。进入共享远征后，地球收集、发射和跨关卡推进由服务器掌握；Earth → SpaceWorld 使用 seamless travel。

SpaceWorld 是持续使用的通用宇宙关卡。星球玩法由各自配置控制，不通过不断切换独立单人地图实现。

## 1–4 人共享远征

单人也运行同一套服务器权威规则。Listen Server 与未来 Dedicated Server 共用玩法路径；远征开始后关闭普通加入，不支持房主迁移。

客户端通过自己拥有的 Controller / Pawn RPC 提交意图。服务器校验交互距离与目标、背包实例、装备修改、伤害、资源生成、AI、船仓、登船和飞行输入，再修改复制状态。UI 展示结果，不能以客户端本地库存或 Widget 状态作为交易依据。

飞船最多四名乘员、一个驾驶员。只有驾驶员可提交不可靠飞行意图，服务器模拟与复制移动。飞船武器还校验驾驶权限、起落状态和输入时效。

`UJTSOnlineSessionSubsystem` 是面向 UI 的 OnlineSubsystem 包装层。项目默认 provider 为 NULL；EOS 配置示例不代表已配置线上凭据。语音由 `UJTSVoiceSubsystem` 对接 provider，不维护自定义音频传输或裸 IP 加入界面。

## 跨关卡与保存

`UJTSExpeditionSubsystem` 管理服务器远征快照与房主存档。`UJTSGameInstance` 的客户端本地状态不能代替共享远征数据。当前快照包含：

- 地图、星球、阶段、检查点、飞船类，共享资源和材料加工图。
- 已安装船坞 ID、团队已分析类型、扫描样本与扫描货物。
- 星际兑换机的待发奖状态及收件人身份。
- 各玩家背包、30 格个人船柜、星际装配、能量、固件、核心进度、生命与防御成长。
- 已封堵入口、重要物品与已击败遭遇的标识。

这是按系统恢复的快照，并非所有世界 Actor 的完整序列化。新增可持久化功能时，需要明确写入、跨图恢复、旧存档兼容以及服务器归属。

本机档案、调试偏好和远征槽分别保存于 `Saved/SaveGames`。目录整理时必须保留这些文件，不能把整个 Saved 当成可丢弃缓存。

## 世界与表现

球面移动、贴地、重力和着陆依赖 PlanetAnchor 与真实 Mesh 表面，不能退回固定 World-Z。导航距离、压缩显示与近地实际尺度是不同的数据用途，详见 [SpaceWorld](SpaceWorld.md)。美术演出是否接入玩法、是否联机同步，需要分别说明，不能仅凭预览资产存在就视为完成。
