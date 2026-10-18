"""Measure actual projected Phobos size and placement in the existing PIE sky."""
import json
import math
from pathlib import Path
import unreal


def run():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world
    ship = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSSpacecraftActor)[0]
    bodies = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
    mars = next(a for a in bodies if a.actor_has_tag('Mars'))
    phobos = next(a for a in bodies if a.actor_has_tag('MarI'))
    deimos = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.JTSPlanetAnchor) if str(a.get_planet_id()) == 'MarII')
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = pc.player_camera_manager
    location = camera.get_camera_location()
    forward = unreal.MathLibrary.get_forward_vector(camera.get_camera_rotation())
    center = phobos.get_actor_bounds(False)[0]
    mars_center = mars.get_actor_bounds(False)[0]
    bounds = phobos.static_mesh_component.static_mesh.get_bounding_box()
    extent = (bounds.max-bounds.min)*0.5
    radius = (extent.x*extent.y*extent.z)**(1/3)*phobos.get_actor_scale3d().x
    lights = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DirectionalLight)
    sources = [('player_camera', location), ('ship', ship.get_actor_location())]
    result = {'phobos_hidden': phobos.get_editor_property('hidden'), 'scale': phobos.get_actor_scale3d().x,
        'light_forward': str(lights[0].get_actor_forward_vector()) if lights else None,
        'positions': {'phobos': str(center), 'mars': str(mars_center), 'deimos': str(deimos.get_planet_center()), 'player': str(location), 'ship': str(ship.get_actor_location())},
        'phobos_radius_cm': radius, 'mars_center_to_phobos_cm': (center-mars_center).length(),
        'mars_center_to_deimos_cm': (mars_center-deimos.get_planet_center()).length(), 'views': []}
    for name, view in sources:
        def direction(start, end):
            delta = [getattr(end, a)-getattr(start, a) for a in ('x','y','z')]
            length = math.sqrt(sum(x*x for x in delta))
            return [x/length for x in delta]
        def dot(a,b): return sum(x*y for x,y in zip(a,b))
        to_phobos = direction(view,center)
        to_mars = direction(view,mars_center)
        distance = (center-view).length()
        angular = math.degrees(2*math.asin(min(radius/distance, 1)))
        separation = math.degrees(math.acos(max(-1,min(1,dot(to_phobos,to_mars)))))
        sun = lights[0].get_actor_forward_vector() if lights else unreal.Vector()
        solar_cos = dot(to_phobos,[sun.x,sun.y,sun.z])
        up = direction(deimos.get_planet_center(),view)
        elevation = math.degrees(math.asin(max(-1,min(1,dot(to_phobos,up)))))
        result['views'].append(dict(source=name, distance_cm=distance, angular_diameter_deg=angular,
            diameter_pixels_1920_fov90=1920*radius/distance,
            separation_from_mars_deg=separation, elevation_deg=elevation,
            approximate_illuminated_fraction=(1+solar_cos)*0.5))
    output = Path(unreal.Paths.project_saved_dir())/'MarsSystem'/'phobos_visibility.json'
    output.write_text(json.dumps(result,indent=2),encoding='utf8')
    return json.dumps(result)
