# 飞船上下船、着陆与镜头修复（2026-09-17）

## 原因与修复

1. **SpaceWorld 驾驶员不能下船及登船后卡死**：`APawn::UnPossessed()` 会清空角色的 PlayerState。原逻辑在控制器已经改为控制飞船后，仍通过角色的 PlayerState 查找座位，因此找不到驾驶员。座位现在同时保存 PlayerState 与 Character，服务器从稳定的座位关系确认驾驶员/乘客身份。另一个卡死原因是登船时切换输入映射后又调用 `FlushPressedKeys()`，仍按住的同一个 F 会在同一帧被重新识别成下船输入，造成连续两次 Pawn 切换。现在移除该清键调用，飞船映射明确忽略切换时已按下的按键，并要求 F 先释放后才允许发起下船。下船成功后恢复原角色控制权、碰撞、可见性和移动；先找到可用出口再释放座位，失败时保留原状态。
2. **船底穿入地面**：实际 `BP_Spacecraft` 模型缩放后半尺寸为 `(525, 500, 200)` cm，原 FlightCollision 半尺寸仅为 `(375, 110, 135)` cm。旧着陆高度少算了约 65 cm。现在默认根据 Blueprint 模型边界及变换自动调整飞行碰撞盒，着陆高度同时覆盖可见模型和碰撞体，并保留配置的离地间隙。碰撞查询也使用碰撞盒真实偏移、旋转。找不到无重叠停放位置时不再强行标记着陆成功。
3. **飞船镜头被锁死**：上一版在 Tick 中持续把 SpringArm 与 Controller 旋转覆盖为船体朝向，导致登船后镜头像被焊在正前方。现在恢复由玩家 ControlRotation 驱动的第三人称 SpringArm；停船和飞行时鼠标都能改变视角，飞行时同一鼠标输入继续提交服务器权威转向，滚轮调整距离，Q/E 滚转。
4. **自动着陆收尾卡住**：重力探测方向与真实三角面法线不同，原 `HitPoint + Normal * Clearance` 在收尾时引入横向位移。现在只修正法线方向的高度误差，并检查最终姿态和碰撞净空。
5. **再次着陆后的地表流程**：着陆完成后恢复 SurfaceGameplayReady，允许后续上船及起飞。
6. **出舱放置与球面重力**：按球面径向重力设置人物朝向，搜索船体周围无阻挡的胶囊位置；地面出口不可用时尝试周围较高的空位并进入 Falling。GameplayPlanet 增加复制通知，让客户端绑定相同星球重力；重复呈现通知不会覆盖上船前的碰撞/可见性缓存。

## 本轮修改文件

- `Source/space/Core/JTSExpeditionTypes.h`：座位保存角色引用。
- `Source/space/Ships/JTSSpacecraftActor.cpp`、`.h`：座位解析、上下船、模型碰撞边界、着陆高度、镜头和着陆流程恢复。
- `Source/space/Components/JTSSpacecraftFlightMovementComponent.cpp`：俯仰方向、旋转碰撞检查、碰撞盒变换、着陆收尾。
- `Source/space/Player/JTSCharacter.cpp`、`.h`：可失败且保持原状态的出舱流程、出口碰撞检测、球面朝向、呈现恢复和星球关联复制。
- `Source/space/Tests/JTSSpacecraftRegressionTests.cpp`：三组引擎自动化回归。
- `Scripts/inspect_spacecraft_geometry.py`：实际飞船 Blueprint 的模型与碰撞尺寸只读检查，不保存资产。
- `SpacecraftFixValidation.md`：本报告。

开始工作时已有的飞行组件头文件、PlayerController 等未提交修改予以保留。没有修改模型、地图或 Blueprint 二进制资产；没有加入新的 Gameplay 系统。

## 编译与验证结果

- 使用项目现有 UE 5.8.2、`spaceEditor Win64 Development` 配置，通过常规 Build.bat 编译；最终结果 **Succeeded**。没有使用 Live Coding。
- `JTS.Spacecraft.HullClearanceAndCamera`：**Success**。使用实际 BP_Spacecraft，检查碰撞盒尺寸、球面顶部/赤道/背面/斜向停放、碰撞净空，以及飞船镜头由 ControlRotation 驱动且不会焊死到船体朝向。
- `JTS.Spacecraft.PossessionAndDisembark`：**Success**。检查四人登船、登船键仍按住时不会同帧下船、释放后下一次 F 恢复原角色、驾驶员原角色 PlayerState 被引擎清空后的座位解析、重复上下船、乘客下船、非乘员拒绝和出口阻挡时状态保留。
- `JTS.Spacecraft.TakeoffLandingCycle`：**Success**。检查起飞、飞行中拒绝下船、自动着陆完成、恢复地表就绪、下船、再次上船和第二次起飞。
- Blueprint 尺寸检查：**Success，0 errors / 0 warnings**；修正后碰撞盒半尺寸为 `(525, 500, 200)` cm。
- 实际 `L_SpaceWorld` 以 `-game -nullrhi` 启动验证：正常退出，日志记录真实地表命中、飞船 Grounded=TRUE、角色出生成功、星球重力启用。新的出生飞船位置为 `(-550.12, 2442.75, 15255.78)`，旧日志位置为 `(-547.54, 2433.44, 15191.50)`，沿地表法线提高约 65 cm。
- `git diff --check` 通过。

验证输出：`Saved/Automation/Spacecraft/index.json`、`Saved/Logs/SpacecraftRegression.log`、`Saved/Logs/SpacecraftGeometryVerified.log`、`Saved/Logs/SpacecraftRuntimeSmoke.log`。

这些检查包含服务端四个玩家控制器的回归；尚未执行真实多进程联机测试或人工画面/手感验收。NullRHI 验证不渲染画面。Python commandlet 中地图碰撞体初始化不完整，因此真实地表验证使用游戏运行模式和带物理场景的自动化世界。

## Editor 验收步骤

1. 用正常方式重新打开项目，避免旧 Live Coding 模块继续运行。无需重建飞船 Blueprint。
2. 打开 `L_SpaceWorld` 并 Play，长按 F 上船。确认完成登船后仍停留在飞船视角，不会因为同一个 F 自动下船；松开 F 后再次按 F，确认人物出现在船体外侧、沿当地重力落到地面，并可重复上下船。
3. 上船后先在停船状态移动鼠标确认可环视，再按 Space 起飞，用鼠标转向、W/S 推进、A/D 平移、Q/E 滚转、滚轮调距。确认镜头和飞行输入都正常响应。
4. 在合法着陆区域内减速，按 L 自动着陆，完成后按 F 下船，再上船起飞。
5. 多人验收使用 Listen Server、2–4 Players，分别由主机和客户端驾驶及下船；本次自动化不能替代实际网络延迟下的手感检查。
6. `BP_Spacecraft` 的 `Ship | Collision | Auto Size Flight Collision From Spacecraft Mesh` 默认开启。本模型无需手工修改碰撞盒；如未来手工配置飞行碰撞，可关闭此项，但着陆高度仍考虑可见模型底部。
