"""Scoped Earth/Moon distant-presentation calibration; never resizes gameplay geometry."""
import json
import math
import runpy
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'
TUNING = {'Moon':1.43, 'EarthFragments':1.25}


def xyz(v):
    return [v.x, v.y, v.z]


def context():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    planets = {str(a.get_planet_id()): a for a in actors if isinstance(a, unreal.JTSPlanetAnchor)}
    manager = next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager))
    return world, actors, planets, manager.get_editor_property('compressed_space')


def run(operation):
    world, actors, planets, c = context()
    if not world:
        c.capture_authored_bodies(False)
    if operation == 'configure':
        assert not world, 'Stop PIE before saving the level'
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.')
        entries = c.get_editor_property('bodies')
        before = {}
        c.modify()
        for index, e in enumerate(entries):
            if not isinstance(e.actor, unreal.JTSPlanetAnchor):
                continue
            key = str(e.actor.get_planet_id())
            if key in TUNING:
                before[key] = dict(multiplier=e.visual_radius_multiplier,
                    surface_transform=e.actor.get_gameplay_surface_actor().get_actor_transform().export_text(),
                    navigation_center=xyz(e.navigation_position_kilometers), radius_km=e.radius_kilometers)
                e.visual_radius_multiplier = TUNING[key]
                entries[index] = e
        c.set_editor_property('bodies', entries)
        c.capture_authored_bodies(False)
        for e in c.get_editor_property('bodies'):
            key = str(e.actor.get_planet_id()) if isinstance(e.actor, unreal.JTSPlanetAnchor) else ''
            if key in before:
                assert e.visual_radius_multiplier == TUNING[key]
                assert e.actor.get_gameplay_surface_actor().get_actor_transform().export_text() == before[key]['surface_transform']
                assert xyz(e.navigation_position_kilometers) == before[key]['navigation_center']
                assert e.radius_kilometers == before[key]['radius_km']
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        (OUT/'earth_moon_sky_configuration.json').write_text(json.dumps(dict(before=before, multipliers=TUNING), indent=2), encoding='utf8')
        return json.dumps(dict(saved=True, multipliers=TUNING, gameplay_transforms_unchanged=True))
    if operation == 'geometry':
        assert not world
        rows = []
        for key, other_key in [('Moon', 'EarthFragments'), ('EarthFragments', 'Moon')]:
            p = planets[key]
            observer = planets[other_key].get_planet_center()
            forward = (p.get_planet_center()-observer).normal()
            right = unreal.MathLibrary.cross_vector_vector(unreal.Vector(0, 0, 1), forward).normal()
            up = unreal.MathLibrary.cross_vector_vector(forward, right)
            points = []
            for m in p.get_gameplay_surface_actor().get_components_by_class(unreal.StaticMeshComponent):
                if not m.static_mesh:
                    continue
                for section in range(m.static_mesh.get_num_sections(0)):
                    verts = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(m.static_mesh, 0, section)[0]
                    transform = m.get_world_transform()
                    points.extend(unreal.MathLibrary.transform_location(transform, v) for v in verts)
            ratio = c.visual_scale(observer, p)
            projected = []
            for v in points:
                offset = p.get_planet_center()+(v-p.get_planet_center())*ratio-observer
                depth = unreal.MathLibrary.dot_vector_vector(offset, forward)
                projected.append([math.degrees(math.atan2(unreal.MathLibrary.dot_vector_vector(offset, axis), depth)) for axis in [right, up]])
            spans = [max(v[i] for v in projected)-min(v[i] for v in projected) for i in range(2)]
            rows.append(dict(planet=key, vertices=len(points), angle_xy_degrees=spans,
                geometry_radius_cm=max((v-p.get_planet_center()).length() for v in points)))
        (OUT/'earth_moon_geometry.json').write_text(json.dumps(rows, indent=2), encoding='utf8')
        return json.dumps(rows)
    if operation == 'inspect':
        rows = []
        for e in c.get_editor_property('bodies'):
            if e.actor not in [planets['Moon'], planets['EarthFragments']]:
                continue
            p = e.actor
            surface = p.get_gameplay_surface_actor()
            component = p.get_gameplay_surface_component()
            mesh = component.get_editor_property('static_mesh') if component else None
            bounds = mesh.get_bounding_box() if mesh else None
            other = planets['EarthFragments' if str(p.get_planet_id()) == 'Moon' else 'Moon']
            pos = other.get_planet_center()
            distance = (pos-p.get_planet_center()).length()
            ratio = c.visual_scale(pos, p)
            rows.append(dict(planet=str(p.get_planet_id()), radius_km=e.radius_kilometers,
                multiplier=e.visual_radius_multiplier, display_radius_km=c.display_radius_kilometers(p),
                authored_radius_cm=c.authored_radius(p), scale=xyz(surface.get_actor_scale3d()) if surface else None,
                mesh=mesh.get_path_name() if mesh else None, mesh_min=xyz(bounds.min) if bounds else None, mesh_max=xyz(bounds.max) if bounds else None,
                mesh_rotation=str(component.get_world_rotation()) if component else None,
                ratio=ratio, enclosing_angle_degrees=math.degrees(2*math.asin(c.authored_radius(p)*ratio/distance)),
                components=[dict(name=m.get_name(), mesh=m.static_mesh.get_path_name() if m.static_mesh else None,
                    scale=xyz(m.get_world_scale()), location=xyz(m.get_world_location()),
                    bounds=str(m.static_mesh.get_bounding_box()) if m.static_mesh else None)
                    for m in (surface.get_components_by_class(unreal.StaticMeshComponent) if surface else [])]))
        result = dict(rows=rows)
        (OUT/'earth_moon_sky_inspect.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    if operation == 'validate':
        assert not world, 'Validate against full authored geometry with PIE stopped'
        rows = []
        for key, other_key in [('Moon', 'EarthFragments'), ('EarthFragments', 'Moon')]:
            p, source = planets[key], planets[other_key]
            forward = (p.get_planet_center()-source.get_planet_center()).normal()
            start = source.get_planet_center()+forward*(c.authored_radius(source)+2000)
            end = p.get_planet_center()-forward*c.bubble_radius(p)
            scales = [c.visual_scale(start+(end-start)*(i/1000), p) for i in range(1001)]
            assert all(0 < v <= 1 for v in scales)
            angles = [2*math.asin(min(.999999, c.authored_radius(p)*v/(start+(end-start)*(i/1000)-p.get_planet_center()).length())) for i, v in enumerate(scales)]
            assert all(b >= a-1e-9 for a, b in zip(angles, angles[1:]))
            assert abs(scales[-1]-1) < 1e-9
            assert c.visual_scale(p.get_planet_center(), p) == 1
            assert c.visual_scale(p.get_planet_center()-forward*(c.bubble_radius(p)-1), p) == 1
            rows.append(dict(target=key, samples=len(scales), departure_scale=scales[0],
                near_scale=scales[-1], max_step=max(b-a for a, b in zip(scales, scales[1:]))))
        result = dict(passed=True, routes=rows)
        (OUT/'earth_moon_sky_validation.json').write_text(json.dumps(result, indent=2), encoding='utf8')
        return json.dumps(result)
    if operation.startswith('capture_'):
        assert world
        source = operation[len('capture_'):]
        return runpy.run_path(str(Path(__file__).with_name('capture_space_sky.py')))['run'](source, 'EarthMoon_'+source+'_After.png')
    if operation == 'flight':
        assert world
        base = runpy.run_path(str(Path(__file__).with_name('validate_planet_flight_balance.py')))['BalanceProbe']
        class Probe(base):
            def save(self):
                (OUT/'earth_moon_sky_native.json').write_text(json.dumps(self.result, indent=2), encoding='utf8')
        probe = Probe(world, actors, planets, next(a for a in actors if isinstance(a, unreal.JTSSpaceWorldManager)))
        probe.stages = probe.stages[:4]
        unreal._earth_moon_sky_probe = probe
        return 'Native ordinary/Shift flight started in both Earth/Moon directions'
    if operation == 'flight_status':
        p = unreal._earth_moon_sky_probe
        p.save()
        return json.dumps(dict(status=p.result['status'], stage=p.stage, legs=p.result['legs'], errors=p.result['errors']))
    if operation.startswith('view_'):
        assert world, 'Start PIE first'
        source = operation[len('view_'):]
        target = 'Moon' if source == 'EarthFragments' else 'EarthFragments'
        setup = runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run']
        setup('setup_'+target+'_from_'+source+'_depart', world, actors)
        ship = next(a for a in actors if isinstance(a, unreal.JTSSpacecraftActor))
        direction = (planets[target].get_planet_center()-planets[source].get_planet_center()).normal()
        ship.set_actor_location(planets[source].get_planet_center()+direction*(c.authored_radius(planets[source])+2000), False, True)
        ship.activate_flight_camera_third_person()
        return json.dumps(dict(source=source, target=target, position=xyz(ship.get_actor_location()),
            fov=ship.get_flight_camera().field_of_view))
    raise ValueError(operation)
