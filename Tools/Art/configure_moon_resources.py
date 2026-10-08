"""Apply Kenney Moon rock models after building spaceEditor; only saves resource assets.

Run in the full Unreal Editor with `py <this file>` (StaticMeshEditorSubsystem is required).
Imported models have offset pivots: the runtime centres their physical bounds.
"""
import unreal

SOURCE = '/Game/ThirdParty/Moon/Resources/Rock(Kenney)'
MESH_ROOT = '/Game/Space/Meshes/Resources/Moon'
EA = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
SM = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
if SM is None:
    raise RuntimeError('Run this script in the full Unreal Editor; StaticMeshEditorSubsystem is not initialized by the Python commandlet.')


def save(asset):
    if not EA.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + asset.get_path_name())


def mesh(name):
    path = MESH_ROOT + '/SM_Moon_' + name
    asset = unreal.load_asset(path) if EA.does_asset_exist(path) else EA.duplicate_asset(SOURCE + '/' + name, path)
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError('Expected StaticMesh: ' + path)
    # One convex hull makes every rock hittable and blocking without complex collision.
    SM.remove_collisions(asset)
    if not SM.set_convex_decomposition_collisions(asset, 1, 16, 100000):
        raise RuntimeError('Could not generate collision for ' + path)
    if 'crystals' in name:
        metal = unreal.load_asset(SOURCE + '/metal')
        for index, slot in enumerate(asset.static_materials):
            if slot.material_interface and slot.material_interface.get_name() == 'crystal':
                asset.set_material(index, metal)
    save(asset)
    return asset


def configure():
    EA.make_directory(MESH_ROOT)
    meshes = {name: mesh(name) for name in ['rocks_smallB', 'rock1', 'rock_largeA', 'rock_largeB',
        'rock_crystals', 'rock_crystalsLargeA', 'rock_crystalsLargeB']}
    blueprint = unreal.load_asset('/Game/Space/Blueprints/Modes/BP_MoonResourceActor')
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property('medium_rock_meshes', [meshes['rock1']])
    defaults.set_editor_property('large_rock_meshes', [meshes['rock_largeA'], meshes['rock_largeB']])
    defaults.set_editor_property('medium_metal_rock_meshes', [meshes['rock_crystals']])
    defaults.set_editor_property('large_metal_rock_meshes', [meshes['rock_crystalsLargeA'], meshes['rock_crystalsLargeB']])
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    save(blueprint)

    spawner = unreal.load_asset('/Game/Space/Blueprints/Planets/Moon/BP_MoonResourceSpawner')
    unreal.get_default_object(spawner.generated_class()).set_editor_property('resource_actor_class', blueprint.generated_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(spawner)
    save(spawner)

    data = unreal.load_asset('/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay')
    settings = data.get_editor_property('moon_resource_spawn_settings')
    # Keep authored region size, count, exclusions and seed. Distribute five resource forms.
    settings.small_rock_weight = 40
    settings.medium_rock_weight = 20
    settings.large_rock_weight = 15
    settings.medium_metal_rock_weight = 15
    settings.ore_weight = 10
    settings.medium_metal_yield_units = 2
    data.set_editor_property('moon_resource_spawn_settings', settings)
    save(data)

    rock = unreal.load_asset('/Game/Space/Data/Items/DA_Item_Rock')
    rock.set_editor_property('world_pickup_mesh', meshes['rocks_smallB'])
    rock.set_editor_property('world_pickup_width', 32.0)
    save(rock)
    metal = unreal.load_asset('/Game/Space/Data/Items/DA_Item_Ore')
    metal.set_editor_property('display_name', 'Metal')
    metal.set_editor_property('description', 'Metal recovered from metal-bearing Moon rocks.')
    save(metal)
    unreal.log('MOON_RESOURCE_CONFIGURATION_COMPLETE')


configure()
