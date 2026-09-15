"""Load-level/asset validation for the front-end and pre-launch lobby."""

import unreal


MAP_PATH = "/Game/Space/Maps/L_PreLaunchLobby"
EXPECTED_GAME_MODE = "/Script/space.JTSPreLaunchLobbyGameMode"
WIDGET_PATHS = (
    "/Game/Space/UI/WBP_FrontEndRoot",
    "/Game/Space/UI/WBP_MainMenu",
    "/Game/Space/UI/WBP_ExpeditionSelect",
    "/Game/Space/UI/WBP_NewExpedition",
    "/Game/Space/UI/WBP_JoinExpedition",
    "/Game/Space/UI/WBP_PreLaunchLobby",
    "/Game/Space/UI/WBP_PlayerCard",
    "/Game/Space/UI/WBP_PlayerCustomize",
    "/Game/Space/UI/WBP_Settings",
)


assert unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH), "Pre-launch map is missing"
unreal.EditorLevelLibrary.load_level(MAP_PATH)
world = unreal.EditorLevelLibrary.get_editor_world()
game_mode = world.get_world_settings().get_editor_property("default_game_mode")
assert game_mode is not None, "Pre-launch map has no GameMode override"
assert game_mode.get_path_name() == EXPECTED_GAME_MODE, "Unexpected GameMode {}".format(game_mode.get_path_name())

for path in WIDGET_PATHS:
    asset = unreal.EditorAssetLibrary.load_asset(path)
    assert asset is not None, "Could not load {}".format(path)
    asset_name = path.rsplit("/", 1)[-1]
    generated_class = unreal.load_class(None, "{}.{}_C".format(path, asset_name))
    assert generated_class is not None, "{} has no generated widget class".format(path)
    unreal.log("Verified {} -> {}".format(path, generated_class.get_path_name()))

unreal.log("Front-end and pre-launch lobby assets loaded successfully")
