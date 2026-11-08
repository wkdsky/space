"""Scoped Unreal editor operations called only through the Moon V3 MCP toolset."""
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT/'SourceArt/MoonSurfaceV3'
CREATURES = ROOT/'SourceArt/MoonCreaturesV3'
QA = ROOT/'Saved/MoonSurfaceV3'
MESHES = '/Game/Space/Environment/MoonV3/Meshes'
MATERIALS = '/Game/Space/Environment/MoonV3/Materials'
SPECIES = '/Game/Space/Creatures/MoonV3'
BPS = '/Game/Space/Blueprints/Planets/Moon/EcologyV3'
TAG = 'MoonEcologyV3'
EA = unreal.EditorAssetLibrary

def recipe():return json.loads((ART/'surface_recipe.json').read_text(encoding='utf-8'))
def vector(v):return unreal.Vector(*v)
def xyz(v):return [v.x,v.y,v.z]
def save(asset):
    assert asset and EA.save_loaded_asset(asset,only_if_is_dirty=False)
def editor():return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def actors():return {a.get_actor_label():a for a in editor().get_all_level_actors()}
def editor_world():
    ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    assert not ed.get_game_world(),'Stop PIE before authoring'
    world=ed.get_editor_world()
    assert world.get_path_name()=='/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld'
    return world

def material(name,color,roughness=.88):
    path=MATERIALS+'/'+name
    mat=unreal.load_asset(path)
    if mat:return mat
    mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,MATERIALS,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided',True)
    lib=unreal.MaterialEditingLibrary
    node=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,0,0)
    node.set_editor_property('constant',unreal.LinearColor(*color[:3],1))
    lib.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    value=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,0,150)
    value.set_editor_property('r',roughness)
    lib.connect_material_property(value,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat);save(mat)
    return mat

def surface_materials():
    result={}
    for name,hexc in recipe()['palette'].items():
        rgb=[int(hexc[i:i+2],16)/255 for i in (1,3,5)]
        linear=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in rgb]
        result['M_MS3_'+name]=material('M_MS3_'+name,linear)
    return result

def fbx_task(source,folder,name,skeletal=False,animation=False,skeleton=None):
    # The legacy FBX factory takes reimport settings from the asset, ahead of FbxImportUI.
    existing=unreal.load_asset(folder+'/'+name)
    if existing:
        data=existing.get_editor_property('asset_import_data')
        data.set_editor_property('convert_scene_unit',True)
        data.set_editor_property('import_uniform_scale',1)
        if skeletal and not animation:data.set_editor_property('update_skeleton_reference_pose',True)
    options=unreal.FbxImportUI()
    options.automated_import_should_detect_type=False
    options.import_as_skeletal=skeletal
    options.import_mesh=not animation
    options.import_animations=animation
    options.import_materials=False;options.import_textures=False
    options.create_physics_asset=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if animation else (unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
    if skeleton:options.skeleton=skeleton
    if not skeletal:
        options.static_mesh_import_data.convert_scene_unit=True
        options.static_mesh_import_data.combine_meshes=True
        options.static_mesh_import_data.auto_generate_collision=False
    else:
        options.skeletal_mesh_import_data.convert_scene_unit=True
        options.anim_sequence_import_data.convert_scene_unit=True
        options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
    task=unreal.AssetImportTask()
    task.filename=str(source);task.destination_path=folder;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=False;task.options=options
    return task

def import_tasks(tasks):
    world=editor_world()
    previous=unreal.SystemLibrary.get_console_variable_int_value('Interchange.FeatureFlags.Import.FBX')
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    try:unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    finally:unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX '+str(previous))
    for task in tasks:assert task.imported_object_paths,task.filename

def blueprint(name,parent):
    bp=unreal.load_asset(BPS+'/'+name)
    if not bp:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',parent)
        bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BPS,unreal.Blueprint,factory)
    assert bp
    return bp,unreal.get_default_object(bp.generated_class())

def import_species(species):
    assert species in ('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')
    editor_world();folder=SPECIES+'/'+species
    manifest=next(r for r in json.loads((CREATURES/'manifest.json').read_text())['creatures'] if r['species']==species)
    mats={m['name']:material(m['name'],m['color'],m['roughness']) for m in json.loads((CREATURES/'materials.json').read_text())[species]}
    name='SK_'+species
    import_tasks([fbx_task(CREATURES/species/(name+'.fbx'),folder,name,True)])
    mesh=unreal.load_asset(folder+'/'+name);assert mesh
    slots=list(mesh.materials)
    for slot in slots:
        key=str(slot.material_slot_name)
        assert key in mats,key
        slot.material_interface=mats[key]
    mesh.set_editor_property('materials',slots)
    skeleton=mesh.skeleton
    assert skeleton
    tasks=[fbx_task(CREATURES/species/(c['action']+'.fbx'),folder+'/Animations',c['action'],True,True,skeleton) for c in manifest['clips']]
    import_tasks(tasks)
    clips={c['name']:unreal.load_asset(folder+'/Animations/'+c['action']) for c in manifest['clips']}
    assert all(clips.values()),str(clips)
    for asset in [mesh,skeleton]+list(clips.values()):save(asset)
    bp,cdo=blueprint('BP_'+species,unreal.JTSPlanetSurfaceCreature)
    cdo.get_editor_property('mesh').set_skeletal_mesh_asset(mesh)
    # Both species have an authored root at the ground, +X forward.
    cdo.get_editor_property('mesh').set_relative_transform(unreal.Transform(),False,True)
    labels={'MirrorSkimmer':'镜虫','Riftlip':'裂唇','SpireWorker':'塔工蚁','SpireSoldier':'塔兵蚁'}
    cdo.set_editor_property('display_name',unreal.Text(labels[species]))
    cdo.set_editor_property('defends_nest',species.startswith('Spire'))
    cdo.set_editor_property('retaliates',species=='Riftlip')
    cdo.set_editor_property('repairs_refuge',species=='SpireWorker')
    cdo.set_editor_property('roam_radius',250 if species.startswith('Spire') else 650)
    cdo.set_editor_property('startle_distance',320 if species=='MirrorSkimmer' else 210)
    cdo.set_editor_property('home_arrival_distance',220 if species.startswith('Spire') else 120)
    cdo.set_editor_property('leash_radius',900 if species.startswith('Spire') else 160)
    cdo.set_editor_property('attack_damage',12 if species=='SpireSoldier' else (4 if species=='SpireWorker' else 6))
    cdo.set_editor_property('attack_reach',100 if species=='SpireSoldier' else 65)
    cdo.get_editor_property('health').set_editor_property('max_health',{'MirrorSkimmer':24,'Riftlip':40,'SpireWorker':32,'SpireSoldier':90}[species])
    cdo.get_editor_property('steering').set_editor_property('max_slope_degrees',65)
    cdo.get_editor_property('steering').set_editor_property('probe_distance',28)
    cdo.get_editor_property('steering').set_editor_property('body_radius',9)
    state=unreal.JTSForagerState
    clipmap={state.IDLE:'Idle',state.WALK:'Walk',state.FEED:'Feed',state.STARTLE:'FleeStart',state.FLEE:'Run',state.BURROW:'Burrow',state.HIDDEN:'UndergroundIdle',state.EMERGE:'Emerge',state.ATTACK:'Attack',state.HIT:'HitReact',state.DEAD:'Death'}
    if species=='SpireWorker':clipmap[state.FEED]='Repair'
    if species=='SpireSoldier' and 'Guard' in clips:clipmap[state.IDLE]='Guard'
    cdo.set_editor_property('animations',{key:clips[value] for key,value in clipmap.items()})
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    # Clip stride metadata describes the authored animation, not a playable movement speed.
    from moon_earthen_unreal import configure_species
    configure_species(species)
    return json.dumps(dict(species=species,mesh=mesh.get_path_name(),skeleton=skeleton.get_path_name(),clips=len(clips),blueprint=bp.get_path_name()))

def import_surface(batch):
    if batch==3:return import_refuges()
    assert batch in (0,1,2)
    editor_world();mats=surface_materials()
    rows=recipe()['assets'][batch*30:(batch+1)*30]
    import_tasks([fbx_task(ART/'FBX'/(r['name']+'.fbx'),MESHES,r['name']) for r in rows])
    for row in rows:
        mesh=unreal.load_asset(MESHES+'/'+row['name']);assert mesh
        slots=list(mesh.static_materials)
        for slot in slots:
            key=str(slot.material_slot_name);assert key in mats,key
            slot.material_interface=mats[key]
        mesh.set_editor_property('static_materials',slots)
        if 'Tower' in row['name']:
            mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        save(mesh)
    return json.dumps(dict(batch=batch,imported=len(rows)))


def import_refuges():
    """Reimport only separate refuge assets, with FBX metres converted to UE centimetres."""
    editor_world()
    names=[f'SM_MS3_RiftRefuge_{i}' for i in range(4)]+[f'SM_MS3_FixedNest_{i+1:02d}' for i in range(9)]
    mapping={n:n.replace('RiftRefuge','RiftFissure').replace('FixedNest','MirrorFixedNest') for n in names}
    import_tasks([fbx_task(ART/'FBX'/(old+'.fbx'),MESHES,new) for old,new in mapping.items()])
    subsystem=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for name in mapping.values():
        mesh=unreal.load_asset(MESHES+'/'+name)
        slots=list(mesh.static_materials)
        for slot in slots:
            key=str(slot.material_slot_name)
            if key=='M_MS3_Moon':key='M_MS3_PropStone'
            slot.material_interface=unreal.load_asset(MATERIALS+'/'+key)
            assert slot.material_interface,key
        mesh.set_editor_property('static_materials',slots)
        mesh.set_editor_property('allow_cpu_access',True)
        subsystem.remove_collisions(mesh);save(mesh)
    return json.dumps({old:dict(refPath=MESHES+'/'+new+'.'+new) for old,new in mapping.items()})

def rotation(forward,up):
    up=unreal.MathLibrary.normal(up)
    forward=unreal.MathLibrary.normal(forward-up*unreal.MathLibrary.dot_vector_vector(forward,up))
    right=unreal.MathLibrary.cross_vector_vector(up,forward)
    return unreal.MathLibrary.make_rotation_from_axes(forward,right,up)

def author_level():
    world=editor_world();plan=recipe();state=actors();sub=editor()
    moon=state['MoonPlanet'];planet=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    controller=next(a for a in state.values() if isinstance(a,unreal.JTSMoonSurfaceController) and str(a.get_planet_id())=='Moon')
    sun=state['Sun_Main'];tr=moon.get_actor_transform();q=tr.rotation
    assert xyz(tr.scale3d)==[82,82,82]
    def point(p):return tr.transform_location(vector([v/0.82 for v in p]))
    def direction(d):return q.rotate_vector(vector(d))
    def pose(row):return point(row['point']),rotation(direction(row['forward']),direction(row.get('up',row.get('direction'))))
    # All required imports must exist before removing any old actors.
    for row in plan['assets']:assert unreal.load_asset(MESHES+'/'+row['name']),row['name']
    classes={k:unreal.load_asset(BPS+'/BP_'+k).generated_class() for k in ('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')}
    for species,cls in classes.items():
        if species.startswith('Spire'):
            bp=unreal.load_asset(BPS+'/BP_'+species)
            unreal.get_default_object(cls).set_editor_property('home_arrival_distance',220)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    habitat_bp,hcdo=blueprint('BP_MoonHabitat',unreal.JTSPlanetEcologyHabitat)
    unreal.BlueprintEditorLibrary.compile_blueprint(habitat_bp);save(habitat_bp)
    colony_bp,ncdo=blueprint('BP_MoonClosedColony',unreal.JTSPlanetColonyNest)
    ncdo.get_editor_property('health').set_editor_property('max_health',160)
    unreal.BlueprintEditorLibrary.compile_blueprint(colony_bp);save(colony_bp)
    baseline=json.loads((QA/'baseline_transforms.json').read_text())['actors']
    preserved_corpse=state['Moon_Corpse_SkeletonAstronaut']
    corpse_before=preserved_corpse.get_actor_transform()
    removed=[]
    for a in list(state.values()):
        old=isinstance(a,(unreal.JTSMoonResourceSpawner,unreal.JTSMoonCubeCluster,unreal.JTSMoonAntNestActor))
        if old or a.actor_has_tag(TAG):
            removed.append(a.get_actor_label());assert sub.destroy_actor(a)
    def spawn(cls,label,loc,rot,folder):
        a=sub.spawn_actor_from_class(cls,loc,rot);assert a
        a.set_actor_label(label);a.set_folder_path('Planets/Moon/EcologyV3/'+folder)
        a.set_editor_property('tags',[unreal.Name(TAG),unreal.Name('PlanetSurfaceContent')])
        a.set_owner(planet)
        return a
    def static(label,row,baked=False):
        loc,rot=pose(row)
        a=spawn(unreal.StaticMeshActor,label,loc,tr.rotation.rotator() if baked else rot,'Surface')
        a.static_mesh_component.set_static_mesh(unreal.load_asset(MESHES+'/'+row['mesh']))
        a.static_mesh_component.set_collision_profile_name('NoCollision')
        a.static_mesh_component.set_cast_shadow(not baked and 'Refuge' not in row['mesh'])
        a.static_mesh_component.set_cull_distance(18000 if baked else 14000)
        if row.get('scale'):a.set_actor_scale3d(unreal.Vector(row['scale'],row['scale'],row['scale']))
        return a
    def habitat(label,row,species,count,core,activity,fringe=False,chance=1,light='ANY'):
        loc,rot=pose(row)
        a=spawn(habitat_bp.generated_class(),label,loc,rot,'Habitats')
        for key,value in dict(enemy_class=classes[species],enemy_count=count,core_radius=core*100,spawn_radius=activity*100,
             minimum_spacing=80,minor_axis_ratio=row.get('minor',1),boundary_phase=row.get('phase',0),
             fringe_only=fringe,expedition_chance=chance,maintain_population=False,sun_light=sun,
             light_preference=getattr(unreal.JTSHabitatLight,light),terminator_margin=.08,
             require_refuge=species in ('MirrorSkimmer','Riftlip')).items():a.set_editor_property(key,value)
        return a
    active=[];fixed=[];refuges={};foodactors={};crackactors={}
    for row in plan['cracks']:crackactors[row['id']]=static('MoonV3_'+row['id'],row,True)
    for row in plan['food_patches']:foodactors[row['id']]=static('MoonV3_'+row['id'],row,True)
    for row in plan['decorations']:static('MoonV3_'+row['id'],row)
    # The nine exact original actor origins and rotations are retained in the new recipes.
    for row in plan['fixed_nests']:
        b=baseline['BP_MoonAntNestEntrance_C_'+str(row['index']-1)]
        loc=unreal.Vector(**b['location']);rot=unreal.Rotator(**b['rotation'])
        a=spawn(habitat_bp.generated_class(),f'Moon_Nest_{row["index"]:02d}',loc,rot,'FixedCraterNests')
        for key,value in dict(enemy_class=classes[row['species']],enemy_count=1,core_radius=420,spawn_radius=420,
            minimum_spacing=100,minor_axis_ratio=1,sun_light=sun,destructible_entrance=True,require_refuge=True,
            light_preference=unreal.JTSHabitatLight.ANY).items():a.set_editor_property(key,value)
        a.get_editor_property('refuge_mesh').set_static_mesh(unreal.load_asset(MESHES+'/'+row['mesh']))
        a.set_editor_property('refuges',[a]);active.append(a);fixed.append(a)
    for index,row in enumerate(plan['regions']):
        regioncracks=[crackactors[k] for k in row['crack_ids']]
        regionfoods=[foodactors[k] for k in row['food_ids']]
        if regioncracks:
            # One open rock fold per zone; the remaining cracks stay sparse flat seams.
            original=next(c for c in plan['cracks'] if c['id']==row['crack_ids'][0])
            r=dict(original,mesh='SM_MS3_RiftRefuge_'+str(index%4))
            mouth=static('MoonV3_Refuge_'+row['id'],r)
            regioncracks=[mouth]+regioncracks[1:]
        refuges[row['id']]=regioncracks
        core=habitat('MoonV3_'+row['id']+'_Rift_Core',row,'Riftlip',1+index%2,row['core_radius_m'],row['activity_radius_m'])
        core.set_editor_property('refuges',regioncracks)
        core.set_editor_property('food_points',regionfoods);active.append(core)
        halo=habitat('MoonV3_'+row['id']+'_Rift_Fringe',row,'Riftlip',0,row['core_radius_m'],row['activity_radius_m'],True,0)
        # Fringe animals use their own starting location as their home instead of crossing a whole region.
        halo.set_editor_property('refuges',regioncracks);active.append(halo)
        if row['direction'][1]>.18:
            mirror=habitat('MoonV3_'+row['id']+'_Mirror_Fringe',row,'MirrorSkimmer',1,row['core_radius_m'],row['activity_radius_m'],True,.08,'SUNLIT')
            mirror.set_editor_property('refuges',fixed);active.append(mirror)
        else:mirror=None
        region_shells=[]
        for j,food in enumerate(regionfoods):
            fr=next(f for f in plan['food_patches'] if food.get_actor_label()=='MoonV3_'+f['id'])
            feeding=habitat('MoonV3_'+fr['id']+'_Mirror',fr,'MirrorSkimmer',2,2.5,8,False,1,'SUNLIT')
            # A visible low shell alongside this patch is also its escape mouth.
            local=dict(fr,mesh='SM_MS3_MirrorRefuge_'+str((index+j)%4))
            loc,rot=pose(local);localpoint=loc+direction(fr['forward'])*320
            grounded=planet.project_point_to_surface(localpoint)
            assert grounded
            localpoint=grounded.impact_point
            refuge=spawn(unreal.StaticMeshActor,'MoonV3_'+fr['id']+'_Shell',localpoint,rot,'Refuges')
            refuge.static_mesh_component.set_static_mesh(unreal.load_asset(MESHES+'/'+local['mesh']))
            refuge.static_mesh_component.set_collision_profile_name('NoCollision')
            refuge.static_mesh_component.set_cast_shadow(False)
            region_shells.append(refuge)
            # Project the mouth origin back onto the real mesh after the offset.
            # The shared settlement frame keeps the animal itself on the true mesh at runtime.
            feeding.set_editor_property('refuges',[refuge]);feeding.set_editor_property('food_points',[food]);active.append(feeding)
        if mirror:mirror.set_editor_property('refuges',region_shells+fixed)
    colonies=[]
    for row in plan['towers']:
        loc,rot=pose(row)
        nest=spawn(colony_bp.generated_class(),'MoonV3_'+row['id'],loc,rot,'ShadowColonies')
        for stage,key in ((0,'closed_mesh'),(1,'damaged_mesh'),(2,'broken_mesh')):
            nest.set_editor_property(key,unreal.load_asset(MESHES+f'/SM_MS3_Tower_{row["variant"]}_{stage}'))
        nest.get_editor_property('nest_mesh').set_static_mesh(nest.get_editor_property('closed_mesh'))
        nest.set_editor_property('planet',planet);nest.set_editor_property('surface_controller',controller)
        workers=habitat('MoonV3_'+row['id']+'_Workers',row,'SpireWorker',3,2.6,3,False,1,'SHADOW')
        guards=habitat('MoonV3_'+row['id']+'_Guards',row,'SpireSoldier',1,2.6,3,False,1,'SHADOW')
        for h in (workers,guards):
            h.set_editor_property('refuges',[nest]);h.set_editor_property('maintenance_target',nest)
        nest.set_editor_property('workers',workers);nest.set_editor_property('guards',guards)
        colonies.append(nest)
    controller.modify()
    controller.set_editor_property('placed_moon_ant_nests',[])
    controller.set_editor_property('placed_ecology_habitats',active)
    controller.set_editor_property('moon_cube_cluster',None)
    controller.set_editor_property('moon_resource_spawner',None)
    controller.set_editor_property('corpse_surface_anchor',None)
    controller.set_editor_property('placed_corpse_landmark',preserved_corpse)
    data=unreal.load_asset('/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay')
    data.modify();data.set_editor_property('moon_ant_nest_count',0);save(data)
    # The Moon actor/mesh is a protected source. Only independent attachments are authored.
    assert preserved_corpse.get_actor_transform()==corpse_before
    import moon_surface_randomize
    moon_surface_randomize.configure()
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    result=dict(removed=removed,fixed_nests=len(fixed),active_habitats=len(active),dormant_colonies=len(colonies),coverage=plan['coverage'])
    (QA/'authoring_result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)

def inspect_surface():
    state=actors();plan=recipe()
    fixed=[a for a in state.values() if a.get_actor_label().startswith('Moon_Nest_')]
    baseline=json.loads((QA/'baseline_transforms.json').read_text())['actors']
    errors=[]
    def check_transform(actor,reference):
        if (actor.get_actor_location()-unreal.Vector(**reference['location'])).length()>.01:errors.append(actor.get_actor_label()+' moved')
        if (actor.get_actor_scale3d()-unreal.Vector(**reference['scale'])).length()>1e-5:errors.append(actor.get_actor_label()+' rescaled')
        rotation=actor.get_actor_rotation()
        if any(abs((getattr(rotation,key)-value+180)%360-180)>1e-4 for key,value in reference['rotation'].items()):errors.append(actor.get_actor_label()+' rotated')
    for a in fixed:
        i=int(a.get_actor_label().rsplit('_',1)[1])-1;b=baseline['BP_MoonAntNestEntrance_C_'+str(i)]
        check_transform(a,b)
        if not a.get_editor_property('destructible_entrance'):errors.append(a.get_actor_label()+' entrance is not destructible')
    symbolic=[a for a in state.values() if a.actor_has_tag(TAG) and isinstance(a,unreal.StaticMeshActor)]
    for a in symbolic:
        c=a.static_mesh_component
        if c.get_collision_profile_name() != 'NoCollision' or c.get_collision_enabled()!=unreal.CollisionEnabled.NO_COLLISION:
            errors.append(a.get_actor_label()+' must retain the NoCollision profile')
    for a in state.values():
        if isinstance(a,unreal.JTSPlanetEcologyHabitat) and a not in fixed and a.get_editor_property('destructible_entrance'):
            errors.append(a.get_actor_label()+' must not be a destructible entrance')
    corpse=state['Moon_Corpse_SkeletonAstronaut'];b=baseline['BP_MoonSkeletonAstronautCorpse_C_0']
    check_transform(corpse,b)
    check_transform(state['MoonPlanet'],baseline['StaticMeshActor_1'])
    anchor=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    check_transform(anchor,baseline['JTSPlanetAnchor_1'])
    old=[a.get_actor_label() for a in state.values() if isinstance(a,(unreal.JTSMoonResourceSpawner,unreal.JTSMoonCubeCluster,unreal.JTSMoonAntNestActor))]
    import_data=unreal.load_asset(MESHES+'/SM_MS3_FixedNest_01').get_editor_property('asset_import_data')
    result=dict(fixed_nests=len(fixed),symbolic_nonblocking_modules=len(symbolic),old_interaction_generators=old,ecology_actors=sum(a.actor_has_tag(TAG) for a in state.values()),coverage=plan['coverage'],errors=errors,
        mesh=state['MoonPlanet'].static_mesh_component.static_mesh.get_path_name(),
        layouts=dict(cracks=0,food_patches=sum(len(r['food_ids']) for r in plan['regions']),closed_colonies=len(plan['towers']),scattered_rocks=2*len(plan['regions'])),
        nearby_unmanaged=[dict(label=a.get_actor_label(),type=a.get_class().get_name()) for a in state.values() if not a.actor_has_tag(TAG) and (a.get_actor_location()-anchor.get_planet_center()).length()<22000],
        scene_captures=[dict(name=a.get_name(),label=a.get_actor_label(),position=xyz(a.get_actor_location()),target=str(a.capture_component2d.get_editor_property('texture_target'))) for a in state.values() if isinstance(a,unreal.SceneCapture2D)],
        import_settings={k:str(import_data.get_editor_property(k)) for k in ('convert_scene_unit','import_uniform_scale','convert_scene')})
    (QA/'editor_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)

def capture_scene(view):
    assert view in ('overview','crater','resources','crystals','carbon','mirror','mirror_inside','rift','tower','feeding','creature','player','player_view','MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')
    game_world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    world=game_world or editor_world()
    state={a.get_actor_label():a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)} if game_world else actors()
    moon=state['MoonPlanet'];center=next(a.get_planet_center() for a in state.values() if isinstance(a,unreal.JTSPlanetAnchor) and str(a.get_planet_id())=='Moon')
    if view=='overview':
        offset=moon.get_actor_transform().rotation.rotate_vector(unreal.Vector(-12000,51000,18000))
        location=center+offset;target=center;up=unreal.Vector(0,0,1);fov=46
    elif view in ('resources','crystals','carbon'):
        if view=='carbon':
            candidates=[a for a in state.values() if isinstance(a,unreal.JTSPlanetSurfaceAttachment)
                and a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name().startswith('SM_MR_Carbon_')]
        else:
            candidates=[a for a in state.values() if isinstance(a,unreal.JTSMoonResourceActor)
                and a.get_node_size()==(unreal.JTSMoonResourceNodeSize.LARGE_CRYSTAL_ROCK if view=='crystals' else unreal.JTSMoonResourceNodeSize.LARGE_METAL_ROCK)]
        assert candidates,'Enter Moon gameplay to generate resource models'
        sunlight=-state['Sun_Main'].get_actor_forward_vector()
        landmark=max(candidates,key=lambda a:unreal.MathLibrary.dot_vector_vector((a.get_actor_location()-center).normal(),sunlight))
        up=(landmark.get_actor_location()-center).normal()
        tangent=(sunlight-up*unreal.MathLibrary.dot_vector_vector(sunlight,up)).normal()
        target=landmark.get_actor_location()+up*(8 if view=='carbon' else 60)
        location=target+up*(160 if view=='carbon' else 350)+tangent*(220 if view=='carbon' else 650);fov=52
    elif view=='crater':
        landmark=state['Moon_Corpse_SkeletonAstronaut'];target=landmark.get_actor_location();up=(target-center).normal()
        location=target+up*11000-landmark.get_actor_forward_vector()*3500;fov=60
    elif view=='player_view':
        manager=unreal.GameplayStatics.get_player_controller(world,0).player_camera_manager
        location=manager.get_camera_location();rotation=manager.get_camera_rotation();up=unreal.MathLibrary.get_up_vector(rotation)
        target=location+unreal.MathLibrary.get_forward_vector(rotation)*200;fov=manager.get_fov_angle()
    elif view=='player':
        landmark=unreal.GameplayStatics.get_player_pawn(world,0)
        up=landmark.get_actor_up_vector();target=landmark.get_actor_location()+up*15
        location=target+up*70+landmark.get_actor_forward_vector()*250+landmark.get_actor_right_vector()*200;fov=44
    elif view in ('creature','MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier'):
        landmark=getattr(unreal,'_moon_surface_probe',None) if view=='creature' else None
        if not landmark or not landmark.get_editor_property('mesh').is_visible():
            prefix='BP_Mirror' if view=='creature' else 'BP_'+view
            landmark=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature) if a.get_editor_property('mesh').is_visible() and a.get_class().get_name().startswith(prefix))
        up=landmark.get_actor_up_vector();target=landmark.get_actor_location()+up*17
        location=target+up*65+landmark.get_actor_forward_vector()*120+landmark.get_actor_right_vector()*105;fov=43
    else:
        if view=='feeding':
            landmark=next(a for a in state.values() if isinstance(a,unreal.JTSPlanetSurfaceAttachment)
                and 'PortableFeed' in a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name())
        elif view=='rift':
            fissures=[a for a in state.values() if isinstance(a,unreal.JTSPlanetSurfaceAttachment)
                and a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh')
                and any(k in a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name() for k in ('RiftFissure','RiftRim'))]
            assert fissures,'Enter Moon gameplay to generate the independent fissures'
            sun=state['Sun_Main']
            landmark=max(fissures,key=lambda a:unreal.MathLibrary.dot_vector_vector((a.get_actor_location()-center).normal(),-sun.get_actor_forward_vector()))
        else:
            label={'mirror':'Moon_Nest_01','mirror_inside':'Moon_Nest_01','tower':'MoonV3_Tower_00'}[view]
            landmark=state[label]
            if view=='tower' and game_world and hasattr(unreal,'_moon_tower_reference'):
                landmark=unreal._moon_tower_reference
        up=landmark.get_actor_up_vector() if view!='feeding' else (landmark.get_actor_location()-center).normal()
        target=landmark.get_actor_location()+up*50
        location=target+up*(230 if view!='tower' else 330)+landmark.get_actor_forward_vector()*650+landmark.get_actor_right_vector()*220;fov=52
        if view=='rift':
            target=landmark.get_actor_location()-up*30
            side=1 if unreal.MathLibrary.dot_vector_vector(-state['Sun_Main'].get_actor_forward_vector(),landmark.get_actor_right_vector())>=0 else -1
            location=target+up*200+landmark.get_actor_forward_vector()*100+landmark.get_actor_right_vector()*(500*side);fov=70
        if view in ('mirror','mirror_inside'):
            target=landmark.get_actor_location()-up*15
            location=landmark.get_actor_location()+up*170+landmark.get_actor_forward_vector()*320+landmark.get_actor_right_vector()*150;fov=68
        if view=='feeding':
            tangent=landmark.get_actor_forward_vector()
            location=target+up*260+tangent*600+unreal.MathLibrary.cross_vector_vector(up,tangent)*180
    forward=(target-location).normal()
    view_up=(up-forward*unreal.MathLibrary.dot_vector_vector(up,forward)).normal()
    right=unreal.MathLibrary.cross_vector_vector(view_up,forward)
    rot=unreal.MathLibrary.make_rotation_from_axes(forward,right,view_up)
    def temporary(cls,loc,rotation):
        if not game_world:return editor().spawn_actor_from_class(cls,loc,rotation,True)
        pose=unreal.Transform(location=loc,rotation=rotation)
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        actor=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        return lib.call_method('FinishSpawningActor',(actor,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    a=temporary(unreal.SceneCapture2D,location,rot)
    lamp=None
    if view in ('tower','mirror_inside','Riftlip','SpireWorker','SpireSoldier'):
        # Inspection lamp, equivalent to looking at this night-side colony with a suit light.
        lamp=temporary(unreal.PointLight,location+up*60,unreal.Rotator())
        if view=='mirror_inside':lamp.set_actor_location(landmark.get_actor_location()-up*20,False,True)
        lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        lamp.point_light_component.set_intensity(1000000 if view=='tower' else 20000)
        lamp.point_light_component.set_attenuation_radius(1800)
    component=a.capture_component2d
    if game_world and view in ('resources','crystals','carbon','rift','mirror','mirror_inside','creature','MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier'):component.set_editor_property('hidden_actors',[unreal.GameplayStatics.get_player_pawn(world,0)])
    tex=unreal.RenderingLibrary.create_render_target2d(world,1600,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    component.set_editor_property('texture_target',tex)
    component.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property('fov_angle',fov)
    component.set_editor_property('always_persist_rendering_state',True)
    # Fixed exposure also makes the geometrical shadow side inspectable without repainting it black.
    pp=component.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_min_brightness',True)
    pp.set_editor_property('override_auto_exposure_max_brightness',True)
    pp.set_editor_property('auto_exposure_min_brightness',1)
    pp.set_editor_property('auto_exposure_max_brightness',1)
    component.set_editor_property('post_process_settings',pp)
    component.set_editor_property('post_process_blend_weight',1)
    component.capture_scene()
    output=QA/(view+'.png')
    class Pending:
        def __init__(self):self.frames=0;self.handle=unreal.register_slate_post_tick_callback(self.tick)
        def tick(self,delta):
            self.frames+=1
            if self.frames<8:return
            try:unreal.RenderingLibrary.export_render_target(world,tex,str(QA),view+'.png')
            finally:
                a.destroy_actor()
                if lamp:lamp.destroy_actor()
                unreal.unregister_slate_post_tick_callback(self.handle)
    unreal._moon_surface_capture=Pending()
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(location,rot)
    return json.dumps(dict(view=view,file=str(output)))

def validate_play(stage):
    if stage.startswith('earthen_'):
        import importlib, moon_earthen_unreal
        return importlib.reload(moon_earthen_unreal).run(stage.removeprefix('earthen_'))
    if stage.startswith('blockade_'):
        import importlib, moon_blockade
        return importlib.reload(moon_blockade).run(stage.removeprefix('blockade_'))
    if stage.startswith('mining_'):
        import importlib, mining_feedback
        return importlib.reload(mining_feedback).run(stage.removeprefix('mining_'))
    if stage.startswith('polish_'):
        import importlib, moon_scene_polish
        return importlib.reload(moon_scene_polish).run(stage.removeprefix('polish_'))
    if stage.startswith('refinement_'):
        import importlib, moon_surface_refinement
        return importlib.reload(moon_surface_refinement).run(stage.removeprefix('refinement_'))
    if stage.startswith('repair_'):
        import importlib,moon_surface_cut_unreal
        try:return importlib.reload(moon_surface_cut_unreal).run(stage.removeprefix('repair_'))
        except Exception:
            import traceback
            return traceback.format_exc()
    if stage.startswith('weapon_'):
        import importlib
        import weapon_pose_validation
        try:
            return importlib.reload(weapon_pose_validation).run(stage)
        except Exception:
            import traceback
            return traceback.format_exc()
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world,'Start server PIE first'
    creatures=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature)
    nests=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetColonyNest)
    controller=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonSurfaceController) if str(a.get_planet_id())=='Moon')
    planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
    if stage=='refuges':
        rows=[];counts={}
        for animal in creatures:
            species=animal.get_class().get_name()
            if not species.startswith(('BP_MirrorSkimmer','BP_Riftlip')):continue
            refuge=animal.get_editor_property('refuge_actor');assert refuge,animal.get_path_name()+' has no refuge'
            mesh=refuge.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh')
            assert mesh
            is_rift=species.startswith('BP_Riftlip')
            assert any(k in mesh.get_name() for k in ('RiftFissure','RiftRim')) if is_rift else ('Mirror' in mesh.get_name() or mesh.get_name().startswith('SM_MS3_FixedNest_')),(species,mesh.get_name())
            distance=planet.approximate_surface_arc_distance(animal.get_actor_location(),refuge.get_actor_location())
            rows.append(dict(species=species,refuge=refuge.get_path_name(),mesh=mesh.get_name(),distance_cm=distance,state=str(animal.get_editor_property('state'))))
            if is_rift:counts[refuge.get_path_name()]=counts.get(refuge.get_path_name(),0)+1
        assert rows,'No resident creatures initialized'
        assert counts and max(counts.values())<=2,counts
        mouths=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label().startswith('Moon_Nest_')]
        assert len(mouths)==9
        assert all(a.get_editor_property('enemy_class').get_name().startswith('BP_MirrorSkimmer') for a in mouths)
        report=dict(creatures=rows,rift_residents_per_fissure=list(counts.values()),fixed_mirror_mouths=len(mouths))
        (QA/'refuge_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        return json.dumps(report)
    if stage=='refuge_destroy':
        animal=next(a for a in creatures if a.get_class().get_name().startswith('BP_Riftlip'))
        refuge=animal.get_editor_property('refuge_actor');assert refuge
        before=planet.get_gameplay_surface_actor().get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh')
        affected=[a for a in creatures if a.get_editor_property('refuge_actor')==refuge]
        unreal._moon_destroyed_refuge_residents=affected
        unreal.GameplayStatics.apply_damage(refuge,1000,None,None,None)
        after=planet.get_gameplay_surface_actor().get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh')
        assert before==after
        return json.dumps(dict(affected=len(affected),original_moon_mesh_unchanged=before==after))
    if stage=='refuge_destroy_check':
        result=[dict(state=str(a.get_editor_property('state')),refuge=str(a.get_editor_property('refuge_actor'))) for a in unreal._moon_destroyed_refuge_residents]
        assert all(a.get_editor_property('refuge_actor') is None and a.get_editor_property('state') not in (unreal.JTSForagerState.HIDDEN,unreal.JTSForagerState.BURROW) for a in unreal._moon_destroyed_refuge_residents)
        return json.dumps(result)
    if stage=='travel':
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        pc.call_method('ServerRequestDebugPlanet',(unreal.Name('Moon'),False))
    if stage=='inspect_planet':
        return json.dumps(dict(center=xyz(planet.get_planet_center()),surface=planet.get_gameplay_surface_actor().get_path_name(),
            surface_transform=str(planet.get_gameplay_surface_actor().get_actor_transform()),marker=str(controller.get_editor_property('placed_ecology_habitats')[0].get_actor_location()),
            has_surface=planet.has_gameplay_surface(),sample=str(planet.project_point_to_surface(controller.get_editor_property('placed_ecology_habitats')[0].get_actor_location()))))
    if stage=='visibility':
        scenery=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.actor_has_tag(TAG)]
        return json.dumps(dict(total=len(scenery),hidden=sum(bool(a.get_editor_property('hidden')) for a in scenery),owner_bound=sum(a.get_owner()==planet for a in scenery)))
    if stage=='species':
        result=[]
        for prefix in ('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier'):
            cohort=[a for a in creatures if a.get_class().get_name().startswith('BP_'+prefix)]
            if not cohort:continue
            a=cohort[0];mesh=a.get_editor_property('mesh')
            result.append(dict(species=prefix,count=len(cohort),states=[str(c.get_editor_property('state')) for c in cohort],
                root_to_body_cm=(mesh.get_socket_location('body')-mesh.get_socket_location('root')).length(),
                refuge_distance_cm=planet.approximate_surface_arc_distance(a.get_actor_location(),a.get_editor_property('refuge'))))
        return json.dumps(result)
    if stage=='retaliate':
        animal=next(a for a in creatures if a.get_class().get_name().startswith('BP_Riftlip'))
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        unreal._moon_surface_test_origin=pawn.get_actor_transform();unreal._moon_surface_probe=animal
        up=(animal.get_actor_location()-planet.get_planet_center()).normal()
        position=animal.get_actor_location()+animal.get_actor_forward_vector()*25+up*95
        pawn.get_movement_component().stop_movement_immediately()
        pawn.get_movement_component().disable_movement()
        pawn.get_component_by_class(unreal.JTSHealthComponent).set_max_health(100,True)
        pawn.set_actor_location(position,False,True)
        unreal.GameplayStatics.apply_damage(animal,1,pawn.get_controller(),pawn,None)
    if stage=='combat_probe':
        pawn=unreal.GameplayStatics.get_player_pawn(world,0);animal=unreal._moon_surface_probe
        return json.dumps(dict(animal=animal.get_name(),state=str(animal.get_editor_property('state')),
            player_health=pawn.get_component_by_class(unreal.JTSHealthComponent).get_health(),
            distance_cm=planet.approximate_surface_arc_distance(pawn.get_actor_location(),animal.get_actor_location())))
    if stage=='watch':
        class Watch:
            def __init__(self):
                self.rows=[];self.last=None;self.started=unreal.GameplayStatics.get_time_seconds(world)
                self.handle=unreal.register_slate_post_tick_callback(self.tick)
            def tick(self,delta):
                animal=unreal._moon_surface_probe
                elapsed=unreal.GameplayStatics.get_time_seconds(world)-self.started
                state=str(animal.get_editor_property('state'))
                if state!=self.last:
                    self.last=state
                    self.rows.append(dict(seconds=elapsed,state=state,visible=animal.get_editor_property('mesh').is_visible(),location=xyz(animal.get_actor_location())))
                    (QA/'behavior_cycle.json').write_text(json.dumps(self.rows,indent=2),encoding='utf-8')
                if elapsed>45:unreal.unregister_slate_post_tick_callback(self.handle)
        unreal._moon_surface_behavior_watch=Watch()
        return json.dumps(dict(watching=unreal._moon_surface_probe.get_name(),seconds=45))
    if stage=='combat_watch':
        pawn=unreal.GameplayStatics.get_player_pawn(world,0);animal=unreal._moon_surface_probe
        class CombatWatch:
            def __init__(self):
                self.rows=[];self.last=None;self.started=unreal.GameplayStatics.get_time_seconds(world)
                self.handle=unreal.register_slate_post_tick_callback(self.tick)
            def tick(self,delta):
                elapsed=unreal.GameplayStatics.get_time_seconds(world)-self.started
                state=str(animal.get_editor_property('state'));health=pawn.get_component_by_class(unreal.JTSHealthComponent).get_health()
                if (state,health)!=self.last:
                    self.last=(state,health)
                    self.rows.append(dict(seconds=elapsed,state=state,player_health=health))
                    (QA/'combat_cycle.json').write_text(json.dumps(self.rows,indent=2),encoding='utf-8')
                if elapsed>12:unreal.unregister_slate_post_tick_callback(self.handle)
        unreal._moon_surface_combat_watch=CombatWatch()
        return json.dumps(dict(watching=animal.get_name(),seconds=12))
    if stage=='startle':
        mirrors=[a for a in creatures if a.get_class().get_name().startswith('BP_Mirror')]
        mouths=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if a.get_actor_label().endswith('_Shell') or a.get_actor_label() in ('Moon_Nest_01','Moon_Nest_02','Moon_Nest_03','Moon_Nest_09')]
        animal=min(mirrors,key=lambda a:min((a.get_actor_location()-b.get_actor_location()).length() for b in mouths))
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        unreal._moon_surface_test_origin=pawn.get_actor_transform()
        unreal._moon_surface_probe=animal
        up=(animal.get_actor_location()-planet.get_planet_center()).normal()
        pawn.set_actor_location(animal.get_actor_location()-animal.get_actor_forward_vector()*95+up*95,False,True)
    if stage=='retreat_player':
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        pawn.set_actor_transform(unreal._moon_surface_test_origin,False,True)
    if stage in ('probe','startle','retreat_player') and hasattr(unreal,'_moon_surface_probe'):
        a=unreal._moon_surface_probe
        hit=planet.project_point_to_surface(a.get_actor_location())
        result=dict(probe=a.get_name(),state=str(a.get_editor_property('state')),
            mesh_visible=a.get_editor_property('mesh').is_visible(),
            ground_error_cm=(hit.impact_point-a.get_actor_location()).length() if hit else None,
            location=xyz(a.get_actor_location()),velocity=xyz(a.get_velocity()),
            refuge=xyz(a.get_editor_property('refuge')),
            refuge_distance_cm=planet.approximate_surface_arc_distance(a.get_actor_location(),a.get_editor_property('refuge')),
            habitat=a.get_editor_property('habitat').get_actor_label() if a.get_editor_property('habitat') else None,
            bone_positions={str(a.get_editor_property('mesh').get_bone_name(i)):xyz(a.get_editor_property('mesh').get_socket_location(a.get_editor_property('mesh').get_bone_name(i))) for i in range(min(4,a.get_editor_property('mesh').get_num_bones()))},
            mesh_relative_location=str(a.get_editor_property('mesh').get_editor_property('relative_location')),
            mesh_relative_scale=str(a.get_editor_property('mesh').get_editor_property('relative_scale3d')),
            nearest_mouth_cm=min((a.get_actor_location()-b.get_actor_location()).length() for b in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if b.get_actor_label().endswith('_Shell') or b.get_actor_label().startswith('Moon_Nest_')),
            bone_count=a.get_editor_property('mesh').get_num_bones())
        with (QA/'behavior_probe.jsonl').open('a',encoding='utf-8') as file:file.write(json.dumps(result)+'\n')
        return json.dumps(result)
    if stage=='initialize':
        for habitat in controller.get_editor_property('placed_ecology_habitats'):habitat.activate_ecology(planet,controller)
    elif stage in ('workers','guards'):
        nest=nests[0]
        unreal.GameplayStatics.apply_damage(nest,15 if stage=='workers' else 100,None,None,None)
    creatures=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceCreature)
    rows={}
    for a in creatures:
        name=a.get_class().get_name();rows[name]=rows.get(name,0)+1
    result=dict(stage=stage,population=rows,colonies=[dict(stage=n.get_editor_property('stage'),workers=n.get_editor_property('workers').get_spawned_enemy_count(),guards=n.get_editor_property('guards').get_spawned_enemy_count()) for n in nests],
        creatures_replicated=all(a.get_editor_property('replicates') for a in creatures),states={str(a.get_editor_property('state')):sum(b.get_editor_property('state')==a.get_editor_property('state') for b in creatures) for a in creatures})
    (QA/('pie_'+stage+'.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
    return json.dumps(result)
