"""Create the editor-owned assets for the Jump to Space front-end refresh.

This intentionally creates empty Blueprint children of the native presentation
widgets.  The C++ classes supply a safe runtime fallback and replicated state;
the Blueprint assets are the project-facing UMG extension points for visual
composition, animations and art replacement in the editor.
"""

import unreal


MAP_PATH = "/Game/Space/Maps/L_PreLaunchLobby"
WIDGETS = (
    ("WBP_FrontEndRoot", "/Script/space.JTSFrontEndRootWidget"),
    ("WBP_MainMenu", "/Script/space.JTSMainMenuWidget"),
    ("WBP_ExpeditionSelect", "/Script/space.JTSExpeditionSelectWidget"),
    ("WBP_NewExpedition", "/Script/space.JTSNewExpeditionWidget"),
    ("WBP_JoinExpedition", "/Script/space.JTSJoinExpeditionWidget"),
    ("WBP_PreLaunchLobby", "/Script/space.JTSPreLaunchLobbyWidget"),
    ("WBP_PlayerCard", "/Script/space.JTSPlayerCardWidget"),
    ("WBP_PlayerCustomize", "/Script/space.JTSPlayerCustomizeWidget"),
    ("WBP_Settings", "/Script/space.JTSSettingsWidget"),
)


def create_prelaunch_map():
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError("Could not create {}".format(MAP_PATH))
    else:
        unreal.EditorLevelLibrary.load_level(MAP_PATH)

    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings()
    game_mode_class = unreal.load_class(None, "/Script/space.JTSPreLaunchLobbyGameMode")
    if game_mode_class is None:
        raise RuntimeError("JTSPreLaunchLobbyGameMode was not loaded")
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("Created/updated {} with JTSPreLaunchLobbyGameMode".format(MAP_PATH))


def create_widget_blueprints():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for asset_name, parent_path in WIDGETS:
        asset_path = "/Game/Space/UI/{}".format(asset_name)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            unreal.log("Keeping existing {}".format(asset_path))
            continue

        parent_class = unreal.load_class(None, parent_path)
        if parent_class is None:
            raise RuntimeError("Could not resolve {} for {}".format(parent_path, asset_name))
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        created = asset_tools.create_asset(asset_name, "/Game/Space/UI", unreal.WidgetBlueprint, factory)
        if created is None:
            raise RuntimeError("Could not create {}".format(asset_path))
        unreal.EditorAssetLibrary.save_loaded_asset(created)
        unreal.log("Created {}".format(asset_path))


create_prelaunch_map()
create_widget_blueprints()
unreal.log("Jump to Space front-end/lobby UMG asset generation completed")
