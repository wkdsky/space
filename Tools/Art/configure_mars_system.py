"""Author non-interactive Mars/Phobos backdrops around the existing Deimos.

Invoked by the opt-in MarsAuthoringTools MCP toolset, loaded in the editor.
Transforms and local-sky membership stay in level instances; SpaceWorldManager owns presentation.
These actors have no gameplay registration, gravity, NavMesh, landing or collision.
Read-only star-chart metadata is authored separately by configure_celestial_navigation.py.
"""
import json
import math
from pathlib import Path
import unreal

EA = unreal.EditorAssetLibrary
REPORT = Path(unreal.Paths.project_saved_dir()).resolve() / 'MarsSystem'
REPORT.mkdir(parents=True, exist_ok=True)


def vec(v):
    return [v.x, v.y, v.z]


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def inspect_scene():
    result = {'world':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(), 'actors':[]}
    for actor in actors():
        if not isinstance(actor, (unreal.JTSPlanetAnchor, unreal.StaticMeshActor, unreal.JTSPlanetSurfaceAnchor, unreal.JTSPlanetArrivalAnchor, unreal.JTSSpaceWorldManager, unreal.DirectionalLight)):
            continue
        row=dict(label=actor.get_actor_label(),path=actor.get_path_name(),type=actor.get_class().get_name(),location=vec(actor.get_actor_location()),scale=vec(actor.get_actor_scale3d()),rotation=str(actor.get_actor_rotation()))
        if isinstance(actor,unreal.JTSPlanetAnchor):
            row['planet_id']=str(actor.get_planet_id())
            row['center']=vec(actor.get_planet_center())
            row['visual_radius']=actor.get_visual_radius()
            row['properties']={}
            for name in ['approximate_radius','gameplay_surface_actor','gravity_strength','gravity_influence_range','space_exit_range','heliocentric_distance_kilometers','parent_planet_id','celestial_companion','companion_apparent_separation_centimeters','companion_distant_size_multiplier','takeoff_transition_altitude','space_flight_altitude','default_approach_entry_distance','approach_transition_altitude','landing_assist_altitude','landing_anchor_actor','approach_entry_anchor_actor']:
                try:
                    value=actor.get_editor_property(name)
                    row['properties'][name]=value.get_path_name() if isinstance(value,unreal.Object) else str(value)
                except Exception as e: row['properties'][name]=str(e)
        if isinstance(actor,unreal.StaticMeshActor):
            sm=actor.static_mesh_component.static_mesh
            if sm:
                row.update(mesh=sm.get_path_name(),source_bounds=str(sm.get_bounding_box()),triangles=sm.get_num_triangles(0),materials=[str(m.material_interface) for m in sm.static_materials])
        result['actors'].append(row)
    (REPORT/'before.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)


ASSETS = {
    'Mars': ('mars planet 3d model/mars+planet+3d+model.fbx', 'mars planet 3d model/mars+planet+3d+model.fbm/mars+planet+3d+model_basecolor.jpg'),
    'MarI': ('marI 3d model/marI+3d+model.fbx', 'marI 3d model/marI+3d+model.fbm/marI+3d+model_basecolor.jpg'),
}
FOLDER = '/Game/Space/Environment/MarsSystem'
# Keep orbital distances physical; compensate only the tiny moon's raster footprint.
# At the existing Deimos spawn, 1x is ~1 pixel at 1920px/90 FOV, 4x is ~6 pixels.
PHOBOS_DISPLAY_RADIUS_MULTIPLIER = 4.0
PHOBOS_ORBITAL_PHASE_DEGREES = -20.0


def save(asset):
    assert EA.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def import_assets():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    root=Path(unreal.Paths.project_dir()).resolve()
    at=unreal.AssetToolsHelpers.get_asset_tools()
    sm_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    result={}
    for body,(fbx,jpg) in ASSETS.items():
        path=FOLDER+'/'+body
        mesh=unreal.load_asset(path+'/SM_'+body) if EA.does_asset_exist(path+'/SM_'+body) else None
        if not mesh:
            options=unreal.FbxImportUI()
            options.automated_import_should_detect_type=False
            options.import_as_skeletal=False
            options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            options.import_mesh=True
            options.import_materials=options.import_textures=False
            options.static_mesh_import_data.combine_meshes=True
            options.static_mesh_import_data.auto_generate_collision=False
            task=unreal.AssetImportTask()
            task.filename=str(root/fbx)
            task.destination_path=path
            task.destination_name='SM_'+body
            task.automated=True
            task.options=options
            at.import_asset_tasks([task])
            mesh=unreal.load_asset(path+'/SM_'+body)
        assert mesh, body+' FBX import failed'
        texture=unreal.load_asset(path+'/T_'+body+'_BaseColor') if EA.does_asset_exist(path+'/T_'+body+'_BaseColor') else None
        if not texture:
            task=unreal.AssetImportTask()
            task.filename=str(root/jpg)
            task.destination_path=path
            task.destination_name='T_'+body+'_BaseColor'
            task.automated=True
            at.import_asset_tasks([task])
            texture=unreal.load_asset(path+'/T_'+body+'_BaseColor')
        assert texture
        texture.set_editor_property('max_texture_size',2048)
        texture.set_editor_property('never_stream',False)
        matpath=path+'/M_'+body+'_Surface'
        mat=unreal.load_asset(matpath) if EA.does_asset_exist(matpath) else at.create_asset('M_'+body+'_Surface',path,unreal.Material,unreal.MaterialFactoryNew())
        mel=unreal.MaterialEditingLibrary
        if not mel.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR):
            tex=mel.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,0)
            tex.texture=texture
            mel.connect_material_property(tex,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
            rough=mel.create_material_expression(mat,unreal.MaterialExpressionConstant,-250,200)
            rough.r=0.95
            mel.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
            fill=mel.create_material_expression(mat,unreal.MaterialExpressionMultiply,-200,350)
            fill.set_editor_property('const_b',0.025)
            mel.connect_material_expressions(tex,'RGB',fill,'A')
            mel.connect_material_property(fill,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.recompile_material(mat)
        if not mel.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR):
            tex=mel.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)
            fill=mel.create_material_expression(mat,unreal.MaterialExpressionMultiply,-200,350)
            fill.set_editor_property('const_b',0.025)
            mel.connect_material_expressions(tex,'RGB',fill,'A')
            mel.connect_material_property(fill,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            mel.recompile_material(mat)
        assert len(mesh.static_materials)==1, 'Expected one material section for '+body
        mesh.set_material(0,mat)
        sm_editor.remove_collisions(mesh)
        setup=mesh.get_editor_property('body_setup')
        setup.set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        setup.set_editor_property('double_sided_geometry',False)
        sm_editor.set_lod_build_settings(mesh,0,sm_editor.get_lod_build_settings(mesh,0))
        for asset in (texture,mat,mesh): save(asset)
        result[body]=dict(mesh=mesh.get_path_name(),triangles=mesh.get_num_triangles(0),bounds=str(mesh.get_bounding_box()),materials=len(mesh.static_materials))
    (REPORT/'assets.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)


def normalized(v):
    return v/v.length()


def background_material(material):
    """Remove the old independent WPO shrink; SpaceWorldManager owns local-sky presentation."""
    mel = unreal.MaterialEditingLibrary
    old_offset = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if old_offset:
        mel.delete_material_expression(material, old_offset)
        mel.recompile_material(material)
        save(material)


def configure():
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not editor.get_game_world(), 'Stop PIE before authoring'
    assert editor.get_editor_world().get_path_name()=='/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld'
    subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    placed=actors()
    deimos=next(a for a in placed if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
    center=deimos.get_planet_center()
    # A shared compressed spatial scale preserves Mars/moon radius and orbit ratios.
    # Existing Deimos gameplay geometry, anchors and its manager remain untouched.
    mars_radius=60000.0
    units_per_km=mars_radius/3389.5
    direction=normalized(unreal.Vector(-0.62,0.43,0.65))
    mars_center=center+direction*(23459.0*units_per_km)
    radial=direction*-1.0
    orbit_normal=normalized(unreal.Vector(0,0,1)-radial*radial.z)
    tangent=normalized(unreal.MathLibrary.cross_vector_vector(orbit_normal,radial))
    phase=math.radians(PHOBOS_ORBITAL_PHASE_DEGREES)
    phobos_radial=radial*math.cos(phase)+tangent*math.sin(phase)
    phobos_center=mars_center+phobos_radial*(9378.0*units_per_km)
    # Axial tilt and fixed orbital phase are presentation settings, not simulated orbits.
    pole=normalized(orbit_normal*math.cos(math.radians(25.19))+radial*math.sin(math.radians(25.19)))
    axis_x=normalized(unreal.MathLibrary.cross_vector_vector(tangent,pole))
    axis_y=unreal.MathLibrary.cross_vector_vector(pole,axis_x)
    mars_rotation=unreal.MathLibrary.make_rotation_from_axes(axis_x,axis_y,pole)
    phobos_forward=phobos_radial*-1.0
    phobos_rotation=unreal.MathLibrary.make_rotation_from_axes(phobos_forward,normalized(unreal.MathLibrary.cross_vector_vector(orbit_normal,phobos_forward)),orbit_normal)
    result={'units_cm_per_real_km':units_per_km,'deimos_reference_center_cm':vec(center),
        'orbital_phase_degrees':PHOBOS_ORBITAL_PHASE_DEGREES,
        'phobos_display_radius_multiplier':PHOBOS_DISPLAY_RADIUS_MULTIPLIER,'actors':[]}
    backgrounds = []
    with unreal.ScopedEditorTransaction('Configure Mars local-sky backgrounds'):
        for name,body_center,rotation,radius in [('Mars',mars_center,mars_rotation,mars_radius),('MarI',phobos_center,phobos_rotation,11.1*units_per_km*PHOBOS_DISPLAY_RADIUS_MULTIPLIER)]:
            mesh=unreal.load_asset(FOLDER+'/'+name+'/SM_'+name)
            assert mesh
            bounds=mesh.get_bounding_box()
            extent=(bounds.max-bounds.min)*0.5
            source_center=(bounds.min+bounds.max)*0.5
            source_radius=max(extent.x,extent.y,extent.z) if name=='Mars' else (extent.x*extent.y*extent.z)**(1.0/3.0)
            scale=radius/source_radius
            location=body_center-rotation.quaternion().rotate_vector(source_center*scale)
            label=name+'_Background'
            actor=next((a for a in placed if a.get_actor_label()==label),None)
            if not actor: actor=subsystem.spawn_actor_from_class(unreal.StaticMeshActor,location,rotation)
            actor.modify()
            actor.set_actor_label(label)
            actor.set_folder_path('Backgrounds/MarsSystem')
            actor.tags=['CelestialBackground',name,'NonInteractive']
            component=actor.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_collision_profile_name('NoCollision')
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property('generate_overlap_events',False)
            component.set_editor_property('can_ever_affect_navigation',False)
            component.set_editor_property('affect_distance_field_lighting',False)
            component.set_editor_property('affect_dynamic_indirect_lighting',False)
            component.set_editor_property('visible_in_ray_tracing',False)
            component.set_cast_shadow(False)
            component.set_editor_property('min_draw_distance',0.0)
            component.set_editor_property('ld_max_draw_distance',0.0)
            component.set_editor_property('never_distance_cull',True)
            actor.set_actor_enable_collision(False)
            actor.set_actor_location(location,False,False)
            actor.set_actor_rotation(rotation,False)
            actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            material=unreal.load_asset(FOLDER+'/'+name+'/M_'+name+'_Surface')
            background_material(material)
            backgrounds.append(actor)
            result['actors'].append(dict(label=label,path=actor.get_path_name(),center_cm=vec(body_center),location_cm=vec(location),radius_cm=radius,uniform_scale=scale,triangles=mesh.get_num_triangles(0),material_slots=len(mesh.static_materials),collision='NoCollision',shadow=False,tick=False))
    deimos.modify()
    deimos.set_editor_property('parent_planet_id', 'Mars')
    deimos.set_editor_property('celestial_background_actors', backgrounds)
    deimos.set_editor_property('celestial_background_companion', backgrounds[0])
    # Deimos keeps its enlarged playable mesh nearby, and reaches its real disk
    # size at Mars using the existing Earth/Moon reversible local-pair curve.
    separation=(mars_center-center).length()
    deimos_visual_radius=deimos.get_visual_radius()
    distant_ratio=(6.2*units_per_km)/deimos_visual_radius
    deimos.set_editor_property('companion_apparent_separation_centimeters', mars_radius+(separation-mars_radius)/distant_ratio)
    deimos.set_editor_property('companion_distant_size_multiplier', 1.0)
    deimos.get_gameplay_surface_actor().modify()
    deimos.get_gameplay_surface_actor().static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    landing=next(a for a in placed if a.get_actor_label()=='MarII_LandingAnchor')
    mars_angle=math.degrees(2*math.asin(mars_radius/(mars_center-landing.get_actor_location()).length()))
    phobos_distance=(phobos_center-landing.get_actor_location()).length()
    result['phobos_orbit_radius_cm']=(phobos_center-mars_center).length()
    result['phobos_orbit_over_mars_radius']=result['phobos_orbit_radius_cm']/mars_radius
    result['phobos_angular_diameter_from_deimos_landing_degrees']=math.degrees(2*math.asin(11.1*units_per_km*PHOBOS_DISPLAY_RADIUS_MULTIPLIER/phobos_distance))
    result['phobos_diameter_pixels_1920_fov90']=1920*11.1*units_per_km*PHOBOS_DISPLAY_RADIUS_MULTIPLIER/phobos_distance
    result.update(mars_angular_diameter_from_deimos_landing_degrees=mars_angle,presentation_owner=deimos.get_path_name(),presentation_rule='SpaceWorldManager local sky',source_reference='https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html',gameplay_planet_ids=[str(a.get_planet_id()) for a in placed if isinstance(a,unreal.JTSPlanetAnchor)])
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    (REPORT/'placement.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)


def prepare_view():
    placed=actors()
    deimos=next(a for a in placed if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
    landing=next(a for a in placed if a.get_actor_label()=='MarII_LandingAnchor')
    report=json.loads((REPORT/'placement.json').read_text(encoding='utf-8'))
    mars_center=unreal.Vector(*report['actors'][0]['center_cm'])
    phobos_center=unreal.Vector(*report['actors'][1]['center_cm'])
    up=normalized(landing.get_actor_location()-deimos.get_planet_center())
    location=landing.get_actor_location()+up*220.0
    forward=normalized(mars_center*0.85+phobos_center*0.15-location)
    right=normalized(unreal.MathLibrary.cross_vector_vector(up,forward))
    view_up=unreal.MathLibrary.cross_vector_vector(forward,right)
    tilt=math.radians(17.0)
    forward=forward*math.cos(tilt)-view_up*math.sin(tilt)
    view_up=unreal.MathLibrary.cross_vector_vector(forward,right)
    rotation=unreal.MathLibrary.make_rotation_from_axes(forward,right,view_up)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(location,rotation)
    subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    camera=subsystem.spawn_actor_from_class(unreal.CameraActor,location,rotation,transient=True)
    camera.set_actor_label('TEMP_MarsBackground_QA')
    camera.tags=['MarsBackgroundQACamera']
    camera.camera_component.set_editor_property('field_of_view',90.0)
    camera.camera_component.set_editor_property('aspect_ratio',4.0/3.0)
    camera.camera_component.set_editor_property('constrain_aspect_ratio',True)
    unreal._mars_background_screenshot_task=unreal.AutomationLibrary.take_high_res_screenshot(1280,960,str(REPORT/'Deimos_Background_Final.png'),camera)
    return json.dumps(dict(location=vec(location),rotation=dict(pitch=rotation.pitch,yaw=rotation.yaw,roll=rotation.roll)))


def validate_editor():
    result={'backgrounds':[]}
    placed=actors()
    for actor in placed:
        if actor.actor_has_tag('MarsBackgroundQACamera'):
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).eject_pilot_level_actor()
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
    ids=sorted(str(a.get_planet_id()) for a in placed if isinstance(a,unreal.JTSPlanetAnchor))
    assert ids==['EarthFragments','MarII','Moon'], ids
    deimos=next(a for a in placed if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='MarII')
    assert sorted(a.get_actor_label() for a in deimos.get_editor_property('celestial_background_actors'))==['MarI_Background','Mars_Background']
    assert deimos.get_editor_property('celestial_background_companion').get_actor_label()=='Mars_Background'
    assert deimos.get_editor_property('companion_apparent_separation_centimeters')>1000000
    assert deimos.get_gameplay_surface_actor().static_mesh_component.get_editor_property('mobility')==unreal.ComponentMobility.MOVABLE
    for name in ASSETS:
        matching=[a for a in placed if a.get_actor_label()==name+'_Background']
        assert len(matching)==1,matching
        actor=matching[0]
        comp=actor.static_mesh_component
        assert comp.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not comp.get_editor_property('generate_overlap_events')
        assert not comp.get_editor_property('can_ever_affect_navigation')
        assert not comp.get_editor_property('cast_shadow')
        assert not actor.is_actor_tick_enabled()
        mesh=comp.static_mesh
        assert unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_simple_collision_count(mesh)==0
        mat=mesh.get_material(0)
        assert not unreal.MaterialEditingLibrary.get_material_property_input_node(mat,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        result['backgrounds'].append(dict(label=actor.get_actor_label(),collision=str(comp.get_collision_enabled()),tick=actor.is_actor_tick_enabled(),triangles=mesh.get_num_triangles(0),slots=len(mesh.static_materials)))
    before=json.loads((REPORT/'before.json').read_text(encoding='utf-8'))
    for old in before['actors']:
        if old['label'] not in ['MarII__2_','MarII_PlanetAnchor','MarII_LandingAnchor','MarII_ArrivalAnchor','MoonPlanet','EarthFragments_NearMoon']: continue
        current=next(a for a in placed if a.get_path_name()==old['path'])
        assert (current.get_actor_location()-unreal.Vector(*old['location'])).length()<0.01,old['label']
        assert (current.get_actor_scale3d()-unreal.Vector(*old['scale'])).length()<0.01,old['label']
    result.update(gameplay_planet_ids=ids,existing_transforms_preserved=True)
    mars=next(a for a in placed if a.get_actor_label()=='Mars_Background')
    phobos=next(a for a in placed if a.get_actor_label()=='MarI_Background')
    mars_center=mars.get_actor_bounds(False)[0]
    phobos_center=phobos.get_actor_bounds(False)[0]
    result['phobos_orbit_radius_cm']=(phobos_center-mars_center).length()
    # This check catches accidental changes to orbital distance while tuning visibility.
    assert abs(result['phobos_orbit_radius_cm']-9378*60000/3389.5)<0.1
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    (REPORT/'validation_editor.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)


def inspect_pie():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world,'Start PIE first'
    result={'gameplay_planet_ids':[],'backgrounds':[]}
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor):
        result['gameplay_planet_ids'].append(str(a.get_planet_id()))
    assert sorted(result['gameplay_planet_ids'])==['EarthFragments','MarII','Moon']
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.StaticMeshActor):
        if not a.actor_has_tag('CelestialBackground'): continue
        comp=a.static_mesh_component
        assert comp.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION
        assert not a.is_actor_tick_enabled()
        result['backgrounds'].append(dict(name=a.get_name(),collision=str(comp.get_collision_enabled()),tick=False))
    assert len(result['backgrounds'])==2
    result['world']=world.get_path_name()
    (REPORT/'validation_pie.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)
