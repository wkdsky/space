"""Scoped material and surface attachment finishing through Unreal MCP."""
import json
import unreal
import moon_surface_unreal as s

TEXTURES='/Game/Space/Environment/MoonV3/Textures'
KINDS=('MirrorSkimmer','Riftlip','SpireWorker','SpireSoldier')

def enum(cls,name):
    return next(getattr(cls,k) for k in dir(cls) if k.replace('_','').lower()==name.replace('_','').lower())

def linear(h):
    values=[int(h[i:i+2],16)/255 for i in (1,3,5)]
    return tuple(v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in values)

def texture(name):return unreal.load_asset(TEXTURES+'/'+name)

def sample(mat,tex,x,y,kind='Color'):
    node=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSample,x,y)
    node.set_editor_property('texture',tex)
    node.set_editor_property('sampler_type',enum(unreal.MaterialSamplerType,'SAMPLERTYPE_'+kind))
    return node

def material(name):
    return unreal.load_asset(s.MATERIALS+'/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,s.MATERIALS,unreal.Material,unreal.MaterialFactoryNew())

def import_textures():
    tasks=[]
    for source in sorted((s.ART/'Textures').glob('*.png')):
        task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=TEXTURES;task.destination_name=source.stem
        task.automated=True;task.replace_existing=True;task.save=False;tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for task in tasks:
        tex=texture(task.destination_name);assert tex
        if task.destination_name.endswith('_Normal'):
            tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('srgb',False);tex.set_editor_property('flip_green_channel',True)
        elif task.destination_name.endswith(('_Roughness','_Detail')):
            tex.set_editor_property('srgb',False);tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        else:tex.set_editor_property('srgb',True)
        s.save(tex)
    return len(tasks)

def materials():
    palette=json.loads((s.ART/'finish_palette.json').read_text(encoding='utf-8'))
    lib=unreal.MaterialEditingLibrary;count=0
    for kind in KINDS:
        for role in palette['species'][kind]:
            mat=material('M_MC_'+kind+'_'+role);lib.delete_all_material_expressions(mat)
            mat.set_editor_property('two_sided',False)
            color=sample(mat,texture('T_'+kind+'_BaseColor'),-500,0)
            rough=sample(mat,texture('T_'+kind+'_Roughness'),-500,250,'Masks')
            lib.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
            lib.connect_material_property(rough,'R',unreal.MaterialProperty.MP_ROUGHNESS)
            lib.recompile_material(mat);s.save(mat);count+=1
    for role,h in palette['environment'].items():
        if role=='FeedScrape':continue
        name='M_MS3_PropStone' if role=='Moon' else 'M_MS3_'+role
        mat=material(name);lib.delete_all_material_expressions(mat);mat.set_editor_property('two_sided',True)
        detail=sample(mat,texture('T_MoonMineral_Detail'),-700,0,'Masks')
        normal=sample(mat,texture('T_MoonMineral_Normal'),-700,400,'Normal')
        blend=lib.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,-100,0)
        for pin,factor,y in (('A',.70,0),('B',1.16,180)):
            c=lib.create_material_expression(mat,unreal.MaterialExpressionConstant3Vector,-450,y)
            c.set_editor_property('constant',unreal.LinearColor(*(min(1,v*factor) for v in linear(h)),1))
            lib.connect_material_expressions(c,'',blend,pin)
        lib.connect_material_expressions(detail,'G' if role.startswith('Tower') else 'R',blend,'Alpha')
        lib.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
        lib.connect_material_property(detail,'B',unreal.MaterialProperty.MP_ROUGHNESS)
        lib.connect_material_property(normal,'RGB',unreal.MaterialProperty.MP_NORMAL)
        lib.recompile_material(mat);s.save(mat);count+=1
    return count

def import_environment():
    names=json.loads((s.ART/'finish_assets.json').read_text())
    s.import_tasks([s.fbx_task(s.ART/'FBX'/(n+'.fbx'),s.MESHES,n) for n in names])
    for name in names:
        mesh=unreal.load_asset(s.MESHES+'/'+name);slots=list(mesh.static_materials)
        for slot in slots:
            key=str(slot.material_slot_name)
            slot.material_interface=material('M_MS3_PropStone' if key in ('M_MS3_Moon','M_MS3_PropStone') else key)
        mesh.set_editor_property('static_materials',slots)
        if 'PortableFeed' in name:mesh.set_editor_property('allow_cpu_access',True)
        s.save(mesh)
    return len(names)

def import_creature(kind):
    assert kind in KINDS
    path=s.SPECIES+'/'+kind;name='SK_'+kind
    mesh=unreal.load_asset(path+'/'+name);skeleton=mesh.skeleton
    anims=s.EA.list_assets(path+'/Animations',recursive=True,include_folder=False)
    s.import_tasks([s.fbx_task(s.CREATURES/kind/(name+'.fbx'),path,name,True,False,skeleton)])
    mesh=unreal.load_asset(path+'/'+name);assert mesh.skeleton==skeleton
    slots=list(mesh.materials)
    for slot in slots:slot.material_interface=material(str(slot.material_slot_name))
    mesh.set_editor_property('materials',slots);s.save(mesh)
    return dict(species=kind,skeleton_preserved=True,material_slots=len(slots),existing_animations=len(anims))

def run(stage):
    if stage=='runtime':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world();assert world
        props=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetSurfaceAttachment)
        names=[a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name() for a in props]
        assert not any('Crack' in n for n in names)
        gaps=[]
        planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat):
            if not a.get_class().get_name().startswith('BP_MoonMirrorFeedingArea'):continue
            gaps += [planet.approximate_surface_arc_distance(f.get_actor_location(),r.get_actor_location()) for f in a.get_editor_property('food_points') for r in a.get_editor_property('refuges')]
        return json.dumps(dict(attachments=len(props),free_standing_cracks=0,feeding_patches=sum('PortableFeed' in n for n in names),
            feeding_refuges=len(gaps),minimum_patch_refuge_distance_cm=min(gaps,default=0)))
    if stage=='materials':
        s.editor_world();return json.dumps(dict(materials=materials()))
    if stage=='textures':
        s.editor_world();return json.dumps(dict(textures=import_textures(),materials=materials()))
    if stage=='environment':
        s.editor_world();return json.dumps(dict(meshes=import_environment()))
    if stage in KINDS:
        s.editor_world();return json.dumps(import_creature(stage))
    if stage=='verify':
        s.editor_world();result=[];lib=unreal.MaterialEditingLibrary
        for kind in KINDS:
            mesh=unreal.load_asset(s.SPECIES+'/'+kind+'/SK_'+kind)
            clips=[unreal.load_asset(p) for p in s.EA.list_assets(s.SPECIES+'/'+kind+'/Animations',recursive=True,include_folder=False)]
            clips=[a for a in clips if isinstance(a,unreal.AnimSequence)]
            bp=unreal.get_default_object(unreal.load_asset(s.BPS+'/BP_'+kind).generated_class())
            result.append(dict(species=kind,slots=len(mesh.materials),all_textured=all(len(lib.get_used_textures(slot.material_interface))>=2 for slot in mesh.materials),
                animations=len(clips),animation_skeletons_valid=all(a.get_editor_property('skeleton')==mesh.skeleton for a in clips),
                bound_behavior_clips=len(bp.get_editor_property('animations'))))
        configured=[]
        for a in s.actors().values():
            if isinstance(a,unreal.JTSPlanetEcologyHabitat):
                for e in a.get_editor_property('surface_scatter').get_editor_property('entries'):
                    configured += [m.get_name() for m in e.get_editor_property('meshes') if m]
        assert not any('Crack' in name for name in configured)
        return json.dumps(dict(creatures=result,standalone_cracks=0,texture_assets=len(s.EA.list_assets(TEXTURES)),
            moon_materials=[str(m) for m in s.actors()['MoonPlanet'].static_mesh_component.get_materials()]))
    if stage == 'inspect':
        s.editor_world()
        lib=unreal.MaterialEditingLibrary
        result=[]
        for name in ('M_MC_MirrorSkimmer_Shell','M_MC_Riftlip_Shell','M_MS3_Feed','M_MS3_Void','M_MS3_Tower'):
            mat=unreal.load_asset(s.MATERIALS+'/'+name)
            result.append(dict(name=name,base=str(lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR)),
                textures=[t.get_path_name() for t in lib.get_used_textures(mat)]))
        mesh=unreal.load_asset(s.SPECIES+'/MirrorSkimmer/SK_MirrorSkimmer')
        slots=[dict(slot=str(slot.material_slot_name),material=str(slot.material_interface),textures=[str(t) for t in lib.get_used_textures(slot.material_interface)]) for slot in mesh.materials]
        return json.dumps(dict(materials=result,slots=slots,moon_materials=[str(m) for m in s.actors()['MoonPlanet'].static_mesh_component.get_materials()]))
    raise ValueError(stage)
