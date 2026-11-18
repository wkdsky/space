"""Idempotent level/Blueprint authoring; no planet mesh, collision or scale edits."""
import unreal
import math
import runpy
from pathlib import Path

def author():
    return runpy.run_path(str(Path(__file__).with_name('author_mars_chapter.py')))['author']()

def inspect():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    planets = []
    for a in actors:
        if isinstance(a, unreal.JTSPlanetAnchor):
            p = {k: str(a.get_editor_property(k)) for k in ['planet_id', 'approximate_radius', 'gravity_strength', 'gameplay_surface_actor']}
            p['location'] = str(a.get_actor_location())
            planets.append(p)
    mars=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
    anchor=mars.get_editor_property('landing_anchor_actor')
    hit=mars.project_point_to_surface(anchor.get_actor_location())
    gm=unreal.get_default_object(unreal.load_class(None,'/Game/Space/Blueprints/Modes/BP_SpaceWorldGameMode.BP_SpaceWorldGameMode_C'))
    ship=unreal.get_default_object(unreal.load_class(None,'/Game/Space/Blueprints/Ships/BP_Spacecraft.BP_Spacecraft_C'))
    return {'surface_controllers':str(gm.get_editor_property('surface_gameplay_controllers')),'docks':str(ship.get_component_by_class(unreal.JTSShipDockComponent).get_editor_property('initial_installed_ids')),
            'planets': planets, 'actors': len(actors),'landing':str(anchor.get_actor_transform()),'hit':str(hit),
            'lights':[(a.get_actor_label(),str(a.get_actor_rotation())) for a in actors if isinstance(a,unreal.DirectionalLight)]}

def spike():
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors=sub.get_all_level_actors()
    mars=next(a for a in actors if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
    origin=mars.get_editor_property('landing_anchor_actor').get_actor_location()
    frame=mars.get_surface_frame_at(origin,unreal.Vector(1,0,0))
    up=frame.up; forward=frame.forward; right=frame.right
    def create(name,pos,scale=None):
        a=next((a for a in actors if a.get_actor_label()==name),None)
        if not a:a=sub.spawn_actor_from_class(unreal.StaticMeshActor if scale else unreal.TargetPoint,pos,frame.transform.rotation.rotator())
        a.set_actor_label(name)
        a.set_folder_path('MarsChapter/Spike')
        if scale:
            a.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
            a.set_actor_scale3d(unreal.Vector(*scale))
        return a
    create('Mars_Site_Station',frame.location)
    # The second marker is only a candidate until the canyon is inspected.
    angle=math.radians(105)
    direction=up*math.cos(angle)+right*math.sin(angle)
    h=mars.project_point_to_surface(mars.get_planet_center()+direction*mars.get_approximate_radius())
    create('Mars_Site_Noctis',h.impact_point)
    floor=frame.location-up*2500
    create('Mars_Spike_Floor',floor,(20,20,.3))
    create('Mars_Spike_Ceiling',floor+up*600,(20,20,.3))
    for i,(offset,scale) in enumerate([(forward*1000,(.3,20,6)),(-forward*1000,(.3,20,6)),(right*1000,(20,.3,6)),(-right*1000,(20,.3,6))]):
        create('Mars_Spike_Wall_'+str(i),floor+offset+up*300,scale)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return {'station':str(frame.location),'noctis':str(h.impact_point),'gravity_deviation_degrees':math.degrees(math.atan(1000/(mars.get_approximate_radius()-2500))),'original_mesh_modified':False}


def validate():
    sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    aa=sub.get_all_level_actors()
    ours=[a for a in aa if a.get_actor_label().startswith('MC_')]
    errors=[]
    zones=[a for a in ours if isinstance(a,unreal.JTSInteriorZone)]
    transits=[a for a in ours if isinstance(a,unreal.JTSInteriorTransit)]
    cameras=[a for a in ours if a.get_actor_label().startswith('MC_Camera_')]
    for a in transits:
        if not a.get_editor_property('destination'):errors.append(a.get_actor_label()+':missing destination')
    for a in zones:
        if not a.get_editor_property('floor_actors'):errors.append(a.get_actor_label()+':missing floor')
    for a in ours:
        scale=a.get_actor_scale3d()
        if min(abs(scale.x),abs(scale.y),abs(scale.z))<.00001:errors.append(a.get_actor_label()+':collapsed scale')
    for a in cameras:
        if not a.get_editor_property('refresh_on_entry'):errors.append(a.get_actor_label()+':no refresh zone')
        if a.get_editor_property('shutdown_seconds')<=0 or not a.get_editor_property('shutdown_sound'):errors.append(a.get_actor_label()+':missing shutdown presentation')
    ambience=[a for a in ours if isinstance(a,unreal.AmbientSound)]
    if len(ambience)!=7:errors.append('Expected seven local ambient sound emitters')
    for a in ambience:
        sound=a.audio_component.get_editor_property('sound')
        if not sound or not sound.get_editor_property('looping') or not sound.get_editor_property('attenuation_settings'):errors.append(a.get_actor_label()+':missing localized loop')
    for a in ours:
        if isinstance(a,unreal.JTSFacilityHazard) and a.get_actor_label().startswith(('MC_B3_IceLoad','MC_IceBranch_Falling_')) and not a.get_editor_property('trigger_sound'):errors.append(a.get_actor_label()+':missing ice sound')
    if len(cameras)!=5:errors.append('Expected exactly five cameras')
    core=unreal.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_FabricatorCore')
    if not core:errors.append('Core asset missing')
    nests=[a for a in ours if a.get_actor_label().startswith('MC_Nest_') and isinstance(a,unreal.JTSPlanetEcologyHabitat)]
    if len(nests)!=3:errors.append('Expected three mechanical nests')
    meshes=[]
    for planet in aa:
        if not isinstance(planet,unreal.JTSPlanetAnchor) or str(planet.get_planet_id()) not in ('Mars','MarI','MarII'):continue
        surface=planet.get_editor_property('gameplay_surface_actor')
        component=surface.get_component_by_class(unreal.StaticMeshComponent)
        meshes.append({'planet':str(planet.get_planet_id()),'actor':surface.get_name(),'mesh':component.static_mesh.get_path_name(),'transform':str(surface.get_actor_transform()),'collision':str(component.get_collision_enabled()),'profile':str(component.get_collision_profile_name())})
    return {'errors':errors,'actor_count':len(ours),'zones':len(zones),'transits':len(transits),'cameras':len(cameras),'ambience':len(ambience),'nests':len(nests),'logs':len([a for a in ours if isinstance(a,unreal.JTSReadableDocument)]),'planet_mesh_actors':meshes}

def runtime_probe():
    return runpy.run_path(str(Path(__file__).with_name('probe_mars_chapter.py')))['run']()
