"""UE editor: apply the generated per-fragment flight wraps without moving/scaling the model.

Geometry source: Saved/EarthFragments/envelope_hulls.json, produced by
export_fragment_envelope.py followed by build_fragment_envelope.py in Blender.
"""
import json
from pathlib import Path
import unreal


def configure():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not editor.get_game_world(), 'Stop PIE first.'
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    earth = next(a for a in actors if isinstance(a, unreal.JTSPlanetAnchor) and str(a.get_planet_id()) == 'EarthFragments')
    surface = earth.get_gameplay_surface_actor()
    geometry = earth.get_fragment_envelope()
    root = Path(unreal.Paths.project_saved_dir()) / 'EarthFragments'
    source = root / 'envelope_hulls.json'
    path = '/Game/Space/Environment/EarthFragments/DA_EarthFragmentsEnvelope'
    data = unreal.load_asset(path)
    if source.exists():
        report = json.loads(source.read_text(encoding='utf-8'))
        export = json.loads((root / 'envelope_source.json').read_text(encoding='utf-8'))
        assert abs(surface.get_actor_scale3d().x - export['scale']) < 0.01, 'Re-export geometry after resizing.'
        if not data:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', unreal.JTSFragmentEnvelopeData)
            data = unreal.AssetToolsHelpers.get_asset_tools().create_asset('DA_EarthFragmentsEnvelope', '/Game/Space/Environment/EarthFragments', unreal.JTSFragmentEnvelopeData, factory)
        assert data
        data.modify()
        data.set_editor_property('region_radii', unreal.Vector(*report['region_radii']))
        hulls = []
        for h in report['fragments']:
            hull = unreal.JTSFragmentHull()
            hull.set_editor_property('vertices', [unreal.Vector(*p) for p in h['vertices']])
            hull.set_editor_property('triangles', h['triangles'])
            hull.set_editor_property('bounds_min', unreal.Vector(*h['bounds_min']))
            hull.set_editor_property('bounds_max', unreal.Vector(*h['bounds_max']))
            hulls.append(hull)
        data.set_editor_property('fragments', hulls)
        assert unreal.EditorAssetLibrary.save_asset(path)
    assert data, 'Generate envelope geometry first.'
    assert len(data.get_editor_property('fragments')) == 67
    earth.modify()
    geometry.modify()
    geometry.set_editor_property('data', data)
    geometry.set_editor_property('geometry_rotation', surface.get_actor_rotation())
    earth.set_editor_property('flight_collision_clearance', 0.0)
    earth.set_editor_property('fragmented_gameplay_surface', True)
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    print('FRAGMENT_ENVELOPE_CONFIGURED', data.get_path_name(), data.get_editor_property('region_radii'), 'scale', surface.get_actor_scale3d())


if __name__ == '__main__':
    configure()
