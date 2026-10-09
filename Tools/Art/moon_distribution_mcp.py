"""Opt-in MCP authoring and PIE validation for the seven Moon resource forms.

Register in a full Editor using `py D:/projects/space/Tools/Art/moon_distribution_mcp.py`.
The authoring tool preserves existing instance settings after the first migration.
"""
import json
import math
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration

DATA = '/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay'
SPAWNER = '/Game/Space/Blueprints/Planets/Moon/BP_MoonResourceSpawner'
ACTOR = '/Game/Space/Blueprints/Modes/Moon/BP_MoonResourceActor'
SOURCE = '/Game/ThirdParty/Moon/Resources/Rock(Kenney)'
MESH_ROOT = '/Game/Space/Meshes/Resources/Moon'
FIELDS = ['small_rock', 'medium_rock', 'large_rock', 'medium_metal_rock',
          'large_metal_rock', 'medium_crystal_rock', 'large_crystal_rock']
EA = unreal.EditorAssetLibrary


def save(asset):
    assert EA.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def editor_spawners():
    return [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if isinstance(a, unreal.JTSMoonResourceSpawner)]


def describe(spawner):
    ratios = spawner.get_editor_property('resource_ratios')
    kinds = spawner.get_editor_property('resource_gameplay_data').get_editor_property('moon_resource_spawn_settings').resource_kinds
    enabled = [name for name in FIELDS if kinds.get_editor_property(name)]
    return {'path': spawner.get_path_name(), 'location': [spawner.get_actor_location().x, spawner.get_actor_location().y, spawner.get_actor_location().z],
            'data': spawner.get_editor_property('resource_gameplay_data').get_path_name(),
            'radius_cm': spawner.get_editor_property('radius'),
            'area_m2': spawner.get_coverage_area_square_meters(),
            'density_per_100_m2': spawner.get_editor_property('resource_density'),
            'overlap_priority': spawner.get_editor_property('overlap_priority'),
            'target_count': spawner.get_target_resource_count(),
            'visible_ratios': {name: ratios.get_editor_property(name) for name in enabled},
            'total_percent': sum(ratios.get_editor_property(name) for name in enabled),
            'coverage_segments': spawner.get_coverage_preview_segment_count()}


@unreal.uclass()
class MoonDistributionTools(unreal.ToolsetDefinition):
    """Configure content, inspect real Details state, and check server gameplay through MCP."""

    @toolset_registry.tool_call
    @staticmethod
    def configure_assets(legacy_target_count: int = 10) -> str:
        """Migrate L_SpaceWorld spawner abundance and add authored crystal variants and their pickup definition."""
        assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(), 'Stop PIE first'
        data = unreal.load_asset(DATA)
        settings = data.get_editor_property('moon_resource_spawn_settings')
        if any(s.get_editor_property('coverage_planet') is None for s in editor_spawners()):
            settings.resource_kinds = unreal.JTSMoonResourceKinds()
        data.set_editor_property('moon_resource_spawn_settings', settings,
                                 notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        save(data)
        sm = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        crystal_meshes = []
        for name in ['rock_crystals', 'rock_crystalsLargeA', 'rock_crystalsLargeB']:
            path = MESH_ROOT + '/SM_Moon_' + name + '_Crystal'
            mesh = unreal.load_asset(path) if EA.does_asset_exist(path) else EA.duplicate_asset(SOURCE + '/' + name, path)
            assert mesh
            # Source slots retain the crystal material. Metal variants remain separate existing assets.
            sm.remove_collisions(mesh)
            assert sm.set_convex_decomposition_collisions(mesh, 1, 16, 100000)
            save(mesh)
            crystal_meshes.append(mesh)
        blueprint = unreal.load_asset(ACTOR)
        defaults = unreal.get_default_object(blueprint.generated_class())
        defaults.set_editor_property('medium_crystal_rock_meshes', crystal_meshes[:1])
        defaults.set_editor_property('large_crystal_rock_meshes', crystal_meshes[1:])
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
        # Both paths exist in this project; configure each resource Blueprint used by existing levels/tests.
        parent = unreal.load_asset('/Game/Space/Blueprints/Modes/BP_MoonResourceActor')
        parent_defaults = unreal.get_default_object(parent.generated_class())
        parent_defaults.set_editor_property('medium_crystal_rock_meshes', crystal_meshes[:1])
        parent_defaults.set_editor_property('large_crystal_rock_meshes', crystal_meshes[1:])
        unreal.BlueprintEditorLibrary.compile_blueprint(parent)
        save(parent)
        crystal_path = '/Game/Space/Data/Items/DA_Item_Crystal'
        crystal = unreal.load_asset(crystal_path) if EA.does_asset_exist(crystal_path) else EA.duplicate_asset('/Game/Space/Data/Items/DA_Item_Ore', crystal_path)
        crystal.set_editor_property('item_id', unreal.JTSItemId.CRYSTAL)
        crystal.set_editor_property('display_name', 'Crystal')
        crystal.set_editor_property('description', 'Crystal recovered from crystal-bearing Moon rocks.')
        pickup_path = MESH_ROOT + '/SM_Moon_CrystalPickup'
        pickup = unreal.load_asset(pickup_path) if EA.does_asset_exist(pickup_path) else EA.duplicate_asset('/Engine/BasicShapes/Cone', pickup_path)
        assert pickup
        pickup.set_material(0, unreal.load_asset(SOURCE + '/crystal'))
        save(pickup)
        crystal.set_editor_property('world_pickup_mesh', pickup)
        crystal.set_editor_property('accent_color', unreal.LinearColor(0.45, 0.25, 1, 1))
        crystal.set_editor_property('world_pickup_width', 24.0)
        save(crystal)
        spawner_bp = unreal.load_asset(SPAWNER)
        spawner_defaults = unreal.get_default_object(spawner_bp.generated_class())
        spawner_defaults.set_editor_property('resource_gameplay_data', data)
        spawner_defaults.set_editor_property('resource_actor_class', blueprint.generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(spawner_bp)
        save(spawner_bp)
        planets = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                   if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'Moon']
        assert len(planets) == 1, [a.get_path_name() for a in planets]
        result = []
        for spawner in editor_spawners():
            migrating = spawner.get_editor_property('coverage_planet') is None
            spawner.modify()
            spawner.set_editor_property('resource_gameplay_data', data)
            spawner.set_editor_property('coverage_planet', planets[0])
            spawner.refresh_resource_configuration()
            if migrating:
                # Preserve the former PDA's authored target count while moving abundance onto the instance.
                area = spawner.get_coverage_area_square_meters()
                spawner.set_editor_property('resource_density', legacy_target_count * 100.0 / area if area > 0 else 0)
                spawner.set_editor_property('resource_ratios', unreal.JTSMoonResourceDistribution())
                spawner.refresh_resource_configuration()
            result.append(describe(spawner))
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        Path(unreal.Paths.project_saved_dir(), 'moon_distribution_assets.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_configuration() -> str:
        """Return the circle, density and currently visible normalized percentages of placed spawners."""
        return json.dumps([describe(s) for s in editor_spawners()])

    @toolset_registry.tool_call
    @staticmethod
    def save_editor_previews() -> str:
        """Save migrated editor-only outline components while preserving every authored region setting."""
        assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        blueprint = unreal.load_asset(SPAWNER)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        save(blueprint)
        spawners = editor_spawners()
        for spawner in spawners:
            spawner.modify()
            spawner.refresh_resource_configuration()
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        result = [describe(s) for s in spawners]
        Path(unreal.Paths.project_saved_dir(), 'moon_union_assets.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        if spawners:
            locations = [s.get_actor_location() for s in spawners]
            centre = unreal.Vector(*(sum(getattr(p, axis) for p in locations) / len(locations) for axis in ('x', 'y', 'z')))
            planet = spawners[0].get_editor_property('coverage_planet')
            up = planet.get_radial_up_vector(centre) if planet else unreal.Vector(0, 0, 1)
            view = centre + up * 15000.0
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
                view, unreal.MathLibrary.find_look_at_rotation(view, centre))
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def validate_overlap_pie() -> str:
        """Check automatic multi-region initialization, exclusive overlap ownership and game-hidden previews."""
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert world
        regions = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonResourceSpawner)
        regions.sort(key=lambda s: s.get_path_name())
        assert len(regions) >= 2, 'Place at least two regions before running this PIE check'
        a, b = regions[:2]
        original = [{field: s.get_editor_property(field).copy() if field == 'resource_ratios' else s.get_editor_property(field)
                     for field in ('resource_ratios', 'resource_density', 'overlap_priority', 'override_coverage_radius', 'radius')}
                    for s in regions]
        locations = [s.get_actor_location() for s in regions]
        planet = a.get_editor_property('coverage_planet')
        report = {}

        def kind(s, name):
            ratios = unreal.JTSMoonResourceDistribution()
            for field in FIELDS: ratios.set_editor_property(field, 100.0 if field == name else 0.0)
            s.set_editor_property('resource_ratios', ratios)

        def nodes():
            return [n for n in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonResourceActor)
                    if n.get_owner() in (a, b)]

        try:
            for s in regions[2:]:
                s.set_editor_property('override_coverage_radius', True)
                s.set_editor_property('radius', 0.0)
            for s in (a, b):
                preview = s.get_components_by_class(unreal.JTSMoonResourcePreviewComponent)[0]
                assert preview.get_editor_property('is_editor_only')
                assert s.get_coverage_preview_segment_count() == 0
                assert not preview.get_editor_property('visible')
                assert preview.get_editor_property('hidden_in_game')
                s.set_editor_property('override_coverage_radius', True)
                s.set_editor_property('radius', 2000.0)
                s.set_editor_property('resource_density', 0.8)
            report['preview_editor_only_and_game_hidden'] = True
            kind(a, 'medium_rock')
            kind(b, 'large_crystal_rock')
            b.set_actor_location(a.get_actor_location(), False, True)
            a.set_editor_property('overlap_priority', 10)
            b.set_editor_property('overlap_priority', 0)
            a.generate_resources()
            target = b.get_target_resource_count()
            assert len(nodes()) == target and all(n.get_owner() == b for n in nodes())
            report['lower_number_wins'] = len(nodes())
            b.set_editor_property('overlap_priority', 20)
            a.generate_resources()
            assert len(nodes()) == target and all(n.get_owner() == a for n in nodes())
            report['old_winner_replaced'] = True
            kind(b, 'medium_rock')
            a.generate_resources()
            owner = nodes()[0].get_owner()
            a.set_editor_property('overlap_priority', 100)
            b.set_editor_property('overlap_priority', -100)
            a.generate_resources()
            assert len(nodes()) == target and all(n.get_owner() == owner for n in nodes())
            report['same_ratios_and_density_ignore_priority_without_stacking'] = True
            b.set_editor_property('resource_density', 1.6)
            a.set_editor_property('overlap_priority', -100)
            b.set_editor_property('overlap_priority', 100)
            a.generate_resources()
            assert len(nodes()) == target and all(n.get_owner() == a for n in nodes())
            lower_density_count = len(nodes())
            a.set_editor_property('overlap_priority', 100)
            b.set_editor_property('overlap_priority', -100)
            a.generate_resources()
            assert len(nodes()) == b.get_target_resource_count() and all(n.get_owner() == b for n in nodes())
            report['density_only_difference_respects_priority'] = {
                'lower_density_winner_count': lower_density_count, 'higher_density_winner_count': len(nodes())}
            a.set_editor_property('overlap_priority', 0)
            b.set_editor_property('overlap_priority', 0)
            a.generate_resources()
            owner = nodes()[0].get_owner()
            assert len(nodes()) == owner.get_target_resource_count() and all(n.get_owner() == owner for n in nodes())
            a.generate_resources()
            assert len(nodes()) == owner.get_target_resource_count() and all(n.get_owner() == owner for n in nodes())
            report['density_only_equal_priority_one_seeded_random_layer'] = True
            b.set_editor_property('resource_density', 0.0)
            b.set_editor_property('overlap_priority', -100)
            a.generate_resources()
            assert not nodes()
            report['same_ratios_zero_density_winner_masks_overlap'] = True
            b.set_editor_property('resource_density', 0.8)
            kind(b, 'large_crystal_rock')
            a.set_editor_property('overlap_priority', 0)
            b.set_editor_property('overlap_priority', 0)
            a.generate_resources()
            owner = nodes()[0].get_owner()
            assert len(nodes()) == target and all(n.get_owner() == owner for n in nodes())
            a.generate_resources()
            assert all(n.get_owner() == owner for n in nodes())
            report['equal_priority_one_seeded_random_layer'] = True
            a.set_editor_property('overlap_priority', 10)
            up = planet.get_radial_up_vector(a.get_actor_location())
            tangent = [up.y, -up.x, 0.0]
            length = math.sqrt(sum(v * v for v in tangent))
            if length < 0.001: tangent, length = [1.0, 0.0, 0.0], 1.0
            tangent = unreal.Vector(*(v / length for v in tangent))
            angle = 3000.0 / planet.get_approximate_radius()
            b.set_actor_location(planet.get_planet_center() +
                                 (up * math.cos(angle) + tangent * math.sin(angle)) * planet.get_approximate_radius(), False, True)
            a.generate_resources()
            current = nodes()
            assert target < len(current) < target * 2
            assert any(n.get_owner() == a for n in current) and any(n.get_owner() == b for n in current)
            for n in current:
                if n.get_owner() == a:
                    assert planet.approximate_surface_arc_distance(n.get_actor_location(), b.get_actor_location()) > 1998.0
            report['partial_overlap_union_count'] = len(current)
            report['overlap_contains_only_winner_nonoverlap_preserved'] = True
        finally:
            for s, settings, location in zip(regions, original, locations):
                for field, value in settings.items(): s.set_editor_property(field, value)
                s.set_actor_location(location, False, True)
            a.generate_resources()
        Path(unreal.Paths.project_saved_dir(), 'moon_union_pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        return json.dumps(report)

    @toolset_registry.tool_call
    @staticmethod
    def validate_pie() -> str:
        """Exercise every 100-percent category, deterministic generation and landed pilot-only lights in PIE."""
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert world
        spawner = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonResourceSpawner)[0]
        # Struct wrappers reference live UObject storage; retain an independent copy for restoration.
        original = spawner.get_editor_property('resource_ratios').copy()
        report = {'generation': {}}
        try:
            for name in FIELDS:
                ratios = unreal.JTSMoonResourceDistribution()
                for field in FIELDS: ratios.set_editor_property(field, 100.0 if field == name else 0.0)
                spawner.set_editor_property('resource_ratios', ratios)
                count = spawner.generate_resources()
                nodes = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSMoonResourceActor)
                         if a.get_owner() == spawner]
                pickups = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSWorldPickupActor)
                           if a.get_owner() == spawner]
                kinds = sorted(set(str(a.get_node_size()) for a in nodes))
                assert count == spawner.get_target_resource_count(), (name, count)
                assert len(nodes) == count if name != 'small_rock' else len(pickups) == count
                expected = getattr(unreal.JTSMoonResourceNodeSize, name.upper(), None)
                if nodes: assert all(a.get_node_size() == expected for a in nodes), (name, kinds)
                before = sorted([[a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z] for a in nodes + pickups])
                assert spawner.generate_resources() == count
                after_actors = [a for cls in (unreal.JTSMoonResourceActor, unreal.JTSWorldPickupActor)
                                for a in unreal.GameplayStatics.get_all_actors_of_class(world, cls) if a.get_owner() == spawner]
                after = sorted([[a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z] for a in after_actors])
                assert before == after, name + ' is not deterministic'
                report['generation'][name] = {'count': count, 'kinds': kinds, 'seed_reproducible': True}
        finally:
            spawner.set_editor_property('resource_ratios', original)
            spawner.generate_resources()
        ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
        player = unreal.GameplayStatics.get_player_pawn(world, 0)
        assert isinstance(player, unreal.JTSCharacter) and ship.is_landed()
        initial = ship.are_headlights_on()
        ship.call_method('ServerToggleHeadlights')
        assert ship.are_headlights_on() == initial
        player.set_actor_location(ship.get_boarding_interaction_center(), False, True)
        assert ship.try_board_player(player)
        ship.call_method('ServerToggleHeadlights')
        assert ship.are_headlights_on() != initial
        ship.call_method('ServerToggleHeadlights')
        assert ship.are_headlights_on() == initial
        ship.call_method('ServerToggleHeadlights')
        assert ship.try_disembark_player(player)
        after_exit = ship.are_headlights_on()
        ship.call_method('ServerToggleHeadlights')
        assert ship.are_headlights_on() == after_exit
        report['headlights'] = {'landed_toggle': True, 'on_foot_denied': True, 'disembark_preserves_lights': True}
        Path(unreal.Paths.project_saved_dir(), 'moon_distribution_pie.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        return json.dumps(report)


_registration = Registration([MoonDistributionTools])
_registration.register()
unreal.log('MOON_DISTRIBUTION_MCP_REGISTERED')
