"""Creates the authored v1 Item DataAssets, shop widget wrapper, and SpaceWorld terminal.

Run with UnrealEditor-Cmd -run=pythonscript. The script is idempotent so production values can
be regenerated after adding a clean Content directory without hand-entering any fields in UE.
"""

import unreal


ITEM_PATH = "/Game/Space/Data/Items"
UI_PATH = "/Game/Space/UI"
SPACE_WORLD_PATH = "/Game/Space/Maps/L_SpaceWorld"

HOLDABLE = 1 << 0
WEARABLE = 1 << 1
MINING = 1 << 2
MELEE_OVERRIDE = 1 << 3
RANGED = 1 << 4
STACKABLE = 1 << 5
SHOP = 1 << 6


def item_cost(resource_type, amount):
    cost = unreal.JTSItemCost()
    cost.set_editor_property("resource_type", resource_type)
    cost.set_editor_property("amount", amount)
    return cost


def text(value):
    # Unreal's Python property bridge converts Python strings to FText.  This
    # works in both editor and commandlet contexts, unlike the unavailable
    # Text.from_string helper on the 5.8 Python bindings.
    return value


def asset_path(name):
    return ITEM_PATH + "/" + name


def get_or_create_item_asset(name):
    path = asset_path(name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        loaded = unreal.EditorAssetLibrary.load_asset(path)
        if loaded:
            return loaded

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.JTSItemDefinition)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, ITEM_PATH, unreal.JTSItemDefinition, factory)
    if not created:
        raise RuntimeError("Could not create item asset " + path)
    return created


def apply_definition(name, data):
    asset = get_or_create_item_asset(name)
    asset.modify()
    asset.set_editor_properties({
        "item_id": data["item_id"],
        "display_name": text(data["display_name"]),
        "description": text(data["description"]),
        "primary_category": data["category"],
        "shop_categories": data["shop_categories"],
        "affinity_tags": [unreal.Name(tag) for tag in data["tags"]],
        "accent_color": unreal.LinearColor(*data["color"]),
        "capability_mask": data["capabilities"],
        "max_stack_size": data.get("max_stack", 1),
        "default_durability": data.get("durability", -1.0),
        "wearable_slot": data.get("wearable_slot", unreal.JTSWearableSlot.NONE),
        "inventory_capacity_bonus": data.get("capacity_bonus", 0),
        "combat_damage": data.get("combat_damage", 1.0),
        "melee_attack_interval": data.get("melee_interval", 0.45),
        "mining_work": data.get("mining_work", 0.0),
        "ranged_damage": data.get("ranged_damage", 0.0),
        "ranged_fire_interval": data.get("ranged_interval", 0.35),
        "ranged_range": data.get("ranged_range", 8000.0),
        "automatic_fire": data.get("automatic", False),
        "shop_costs": data.get("costs", []),
    })
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save item asset " + asset.get_path_name())
    unreal.log("JTS_SUPPLY_ASSETS: saved " + asset.get_path_name())


def create_shop_widget_wrapper():
    path = UI_PATH + "/WBP_ShopRoot"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        blueprint = unreal.EditorAssetLibrary.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.JTSShopWidget)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "WBP_ShopRoot", UI_PATH, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError("Could not create WBP_ShopRoot")
    # UE 5.8 exposes this helper as BlueprintEditorLibrary; older project
    # installs used KismetEditorLibrary.  A generated class-only widget has no
    # graph dependency, but compile when the editor binding is available.
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    elif hasattr(unreal, "KismetEditorLibrary"):
        unreal.KismetEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False):
        raise RuntimeError("Could not save WBP_ShopRoot")
    unreal.log("JTS_SUPPLY_ASSETS: saved " + blueprint.get_path_name() + " type=" + blueprint.get_class().get_name())
    return blueprint


def add_spaceworld_terminal():
    if not hasattr(unreal, "EditorLevelLibrary"):
        raise RuntimeError("EditorScriptingUtilities is not loaded; cannot author L_SpaceWorld terminal")
    if not unreal.EditorLevelLibrary.load_level(SPACE_WORLD_PATH):
        raise RuntimeError("Could not load " + SPACE_WORLD_PATH)

    # Actor Python wrappers in UE 5.8 do not expose UObject::IsA.  The label
    # is owned by this idempotent authoring script and is sufficient to locate
    # the native terminal it previously placed.
    existing = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
                if actor and actor.get_actor_label() == "JTS Supply Terminal"]
    if existing:
        terminal = existing[0]
    else:
        terminal = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.JTSShopTerminalActor,
            unreal.Vector(0.0, 0.0, 180.0),
            unreal.Rotator(0.0, 0.0, 0.0),
            False)
        if not terminal:
            raise RuntimeError("Could not place JTSShopTerminalActor into L_SpaceWorld")
        terminal.set_actor_label("JTS Supply Terminal")

    # The GameMode relocates this authored terminal next to the live shared spacecraft on arrival.
    terminal.set_folder_path("JumpToSpace/Gameplay/Shop")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save L_SpaceWorld")
    unreal.log("JTS_SUPPLY_ASSETS: authored terminal " + terminal.get_path_name())


rock = unreal.JTSResourceType.ROCK
ore = unreal.JTSResourceType.ORE
item = unreal.JTSItemId
category = unreal.JTSItemCategory
shop_category = unreal.JTSShopCategory

definitions = {
    "DA_Item_Pickaxe": {
        "item_id": item.PICKAXE, "display_name": "Pickaxe",
        "description": "A dependable lunar tool. Strong mining work and steady melee impact.",
        "category": category.MINING, "shop_categories": [shop_category.MINING, shop_category.UTILITY],
        "tags": ["Mining", "Melee", "Utility"], "color": (1.0, 0.70, 0.18, 1.0),
        "capabilities": HOLDABLE | MINING | MELEE_OVERRIDE | SHOP,
        "combat_damage": 2.0, "melee_interval": 0.62, "mining_work": 4.0,
        "costs": [item_cost(rock, 4)],
    },
    "DA_Item_Knife": {
        "item_id": item.KNIFE, "display_name": "Field Knife",
        "description": "A fast close-quarters blade. It can chip rock, but it is not a mining tool.",
        "category": category.WEAPONS, "shop_categories": [shop_category.WEAPONS, shop_category.UTILITY],
        "tags": ["Weapon", "Melee", "Lightweight"], "color": (0.82, 0.89, 1.0, 1.0),
        "capabilities": HOLDABLE | MINING | MELEE_OVERRIDE | SHOP,
        "combat_damage": 3.0, "melee_interval": 0.28, "mining_work": 0.75,
        "costs": [item_cost(rock, 3), item_cost(ore, 1)],
    },
    "DA_Item_Pistol": {
        "item_id": item.PISTOL, "display_name": "Service Pistol",
        "description": "Accurate sidearm prototype. Infinite test-cell ammunition; low mining work per shot.",
        "category": category.WEAPONS, "shop_categories": [shop_category.WEAPONS],
        "tags": ["Weapon", "Ranged", "Sidearm"], "color": (0.42, 0.72, 1.0, 1.0),
        "capabilities": HOLDABLE | MINING | RANGED | SHOP,
        "combat_damage": 1.0, "mining_work": 0.35, "ranged_damage": 2.5,
        "ranged_interval": 0.42, "ranged_range": 9000.0,
        "costs": [item_cost(rock, 2), item_cost(ore, 4)],
    },
    "DA_Item_MachineGun": {
        "item_id": item.MACHINE_GUN, "display_name": "Machine Gun",
        "description": "Sustained-fire prototype. Infinite test-cell ammunition; weak mining work accumulates through rate of fire.",
        "category": category.WEAPONS, "shop_categories": [shop_category.WEAPONS],
        "tags": ["Weapon", "Ranged", "Sustained Fire"], "color": (1.0, 0.34, 0.18, 1.0),
        "capabilities": HOLDABLE | MINING | RANGED | SHOP,
        "combat_damage": 1.0, "mining_work": 0.25, "ranged_damage": 0.85,
        "ranged_interval": 0.12, "ranged_range": 8500.0, "automatic": True,
        "costs": [item_cost(rock, 6), item_cost(ore, 10)],
    },
    "DA_Item_Backpack": {
        "item_id": item.BACKPACK, "display_name": "Expedition Backpack",
        "description": "Wearable expedition pack. Adds eight inventory slots; only one can occupy the backpack slot.",
        "category": category.WEARABLES, "shop_categories": [shop_category.WEARABLES, shop_category.UTILITY],
        "tags": ["Wearable", "Capacity", "Utility"], "color": (0.22, 0.94, 0.55, 1.0),
        "capabilities": WEARABLE | SHOP, "wearable_slot": unreal.JTSWearableSlot.BACKPACK,
        "capacity_bonus": 8, "combat_damage": 0.0,
        "costs": [item_cost(rock, 5), item_cost(ore, 2)],
    },
    "DA_Item_Rock": {
        "item_id": item.ROCK, "display_name": "Rock",
        "description": "Single-unit expedition material. It can be held as an improvised melee item.",
        "category": category.RESOURCES, "shop_categories": [shop_category.RESOURCES],
        "tags": ["Material", "Improvised"], "color": (0.48, 0.50, 0.56, 1.0),
        "capabilities": HOLDABLE, "max_stack": 1, "combat_damage": 1.0,
    },
    "DA_Item_Ore": {
        "item_id": item.ORE, "display_name": "Ore",
        "description": "Single-unit conductive expedition material. It can be held as an improvised melee item.",
        "category": category.RESOURCES, "shop_categories": [shop_category.RESOURCES],
        "tags": ["Material", "Conductive", "Improvised"], "color": (0.10, 0.72, 0.95, 1.0),
        "capabilities": HOLDABLE, "max_stack": 1, "combat_damage": 1.0,
    },
}

for asset_name, definition in definitions.items():
    apply_definition(asset_name, definition)

create_shop_widget_wrapper()
add_spaceworld_terminal()
unreal.log("JTS_SUPPLY_ASSETS: complete")
