# Jump to Space - Codex Instructions


# Design Principles

- 保持系统职责单一，避免大型类承担多个职责。
- C++负责通用规则和系统能力。
- Blueprint负责具体项目配置、资产选择和关卡实例设置。
- 不要将关卡配置、资源配置、具体Actor引用硬编码到C++。
- 优先使用 Composition、Component、Subsystem、Interface。


# C++ and Blueprint Rules

## C++

负责：

- Gameplay规则
- 系统逻辑
- 数据结构
- 通用接口
- Runtime行为


## Blueprint

负责：

- GameMode实例配置
- Level配置
- Player Blueprint选择
- Spacecraft Blueprint选择
- Planet具体参数配置
- 美术和表现调整


Blueprint用于配置C++提供的能力，不替代核心系统。


# Gameplay Architecture


## Flow Layer

### GameMode

负责：

- 游戏阶段流程
- 玩家出生
- 飞船创建
- 当前玩法初始化
- 使用Blueprint配置


例如：

Earth GameMode：
- 收集阶段
- 发射流程
- 保存旅行数据


Space Entry GameMode：
- 第一次进入SpaceWorld时初始化玩家、飞船、Planet状态


Planet Gameplay GameMode：
- 管理具体星球玩法配置



## World Layer


### SpaceWorldManager

负责：

- SpaceWorld状态管理
- Planet注册
- 当前Planet查询
- Planet Streaming
- Travel状态


### PlanetAnchor

负责：

- 真实球体定义
- Planet中心
- Surface查询
- Planet重力来源
- Landing相关数据



## Vehicle Layer


### SpacecraftActor

负责：

- 飞行
- 推进
- 转向
- 起飞
- 着陆
- 飞船状态


飞船生成和出生位置由GameMode决定。


## Gameplay Layer


### SurfaceController

负责：

具体Planet地表玩法。

例如Moon：

- 资源生成
- 矿脉
- 生物
- 巢穴
- POI
- 地表事件


不同Planet可以拥有不同SurfaceController。


## Entity Layer


### Character

负责：

- 玩家控制
- 移动
- 交互
- 装备


### Item / Resource

负责：

- 物品自身数据
- 交互行为


# Planet Rules

所有真实Planet使用球面规则。

重力方向：

Planet Center → Actor位置

不要使用固定World-Z重力。

Surface逻辑基于真实Mesh Surface。


# World Structure

SpaceWorld是长期存在的通用宇宙玩法空间。

不要设计：

Earth地图 → Moon地图 → Mars地图


正确：

Earth

↓

SpaceWorld

↓

Current Planet

↓

Planet Gameplay


# Project Structure

推荐：

Characters/

Components/

Systems/

World/

Planets/

Ships/

Items/

UI/


保持目录与职责对应。


# Unreal C++ Rules

遵循UE5 C++规范。

使用：

- UCLASS
- USTRUCT
- UENUM
- UINTERFACE
- UPROPERTY
- UFUNCTION


命名：

A = Actor

U = UObject

F = Struct

E = Enum


避免：

- 巨型类
- 大量Tick
- 无必要抽象
- 无关重构


# Code Changes

新增系统前说明：

1. 目的
2. 职责
3. 依赖


保持修改范围最小。

不要修改无关代码。


# Unreal Assets

不要直接修改：

- uasset
- umap


需要编辑器操作时：

说明具体Editor步骤。


# Build Rules

允许编译项目验证C++修改。

编译：

- 使用项目现有Build配置。
- 必要时关闭Unreal Editor。
- 避免Live Coding导致的问题。


编译失败：

- 优先解决第一个有效错误。
- 不通过修改无关文件绕过问题。


# Validation Report

完成后报告：

1. 修改文件
2. 实现内容
3. Editor需要操作的步骤
4. 编译结果