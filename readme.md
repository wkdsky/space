D:\Software\UE_5.8\Engine\Build\BatchFiles\Build.bat spaceEditor Win64 Development D:\projects\space\space.uproject -WaitMutex -NoHotReloadFromID


游戏系统遵顼下面的边界：
GameMode
负责：
    是否创建飞船
    使用哪个飞船
    玩家出生在哪里

SpacecraftActor
负责：
    飞行
    推进
    转向
    起飞
    着陆

SpaceWorldManager
负责：
    世界
    行星
    Streaming
    行星查询

SurfaceController
负责：
    地表玩法

ue热重载：ctrl + alt + f11



Get-ChildItem "C:\Users\coner\.codex" -Recurse -File |
Where-Object { $_.Name -like "*01a08935-03ad-7043-ae0f-42629ad41b81*" } |
Copy-Item -Destination "C:\Users\coner\codexbackup"