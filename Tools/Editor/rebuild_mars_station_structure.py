"""Level-owned M2.1/M2.2 recipes. Centimetres, radial frames, no planet edits."""
import json
import math
from pathlib import Path
import unreal as u

ROOT = '/Game/Space/Environment/MarsSystem/Chapter/Refined'
SAVED = Path(u.Paths.project_saved_dir())

def vec(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]

def direction(t,v):
    return t.rotation.get_axis_x()*v.x+t.rotation.get_axis_y()*v.y+t.rotation.get_axis_z()*v.z

def setp(o, **values):
    for key, value in values.items():
        o.set_editor_property(key, value)
    return o

def context():
    sub = u.get_editor_subsystem(u.EditorActorSubsystem)
    aa = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
    p = next(a for a in aa.values() if isinstance(a, u.JTSPlanetAnchor)
             and str(a.get_planet_id()) == 'Mars')
    assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_name() == 'L_SpaceWorld'
    assert not u.EditorLevelLibrary.get_pie_worlds(True), 'Stop PIE first'
    return sub, aa, p

def planet_signature(p):
    surface = p.get_gameplay_surface_actor()
    c = surface.get_component_by_class(u.StaticMeshComponent)
    return dict(actor=surface.get_path_name(), mesh=c.static_mesh.get_path_name(),
                location=vec(surface.get_actor_location()), rotation=vec(surface.get_actor_transform().rotation.euler()),
                mesh_scale=vec(surface.get_actor_scale3d()),collision=str(c.get_collision_enabled()),
                profile=str(c.get_collision_profile_name()), scale=vec(p.get_actor_scale3d()),
                radius=p.get_approximate_radius())

def inspect():
    _, aa, p = context()
    basis = aa['MC_R_Lift_Housing'].get_actor_transform()
    rows = []
    for label, a in aa.items():
        if not label.startswith(('MC_', 'Mars_Site')):
            continue
        q = basis.inverse_transform_location(a.get_actor_location())
        row = dict(label=label, path=a.get_path_name(), cls=a.get_class().get_name(),
                   world=vec(a.get_actor_location()), local=vec(q), rotation=str(a.get_actor_rotation()),
                   scale=vec(a.get_actor_scale3d()), parent=a.get_attach_parent_actor().get_actor_label()
                   if a.get_attach_parent_actor() else None,
                   bounds=str(a.get_actor_bounds(False)),
                   meshes=[dict(component=c.get_name(), mesh=c.static_mesh.get_path_name() if c.static_mesh else None,
                                location=vec(c.get_world_location()), rotation=str(c.get_world_rotation()),
                                scale=vec(c.get_world_scale()), collision=str(c.get_collision_enabled()))
                           for c in a.get_components_by_class(u.StaticMeshComponent)])
        if isinstance(a, u.JTSInteriorZone):
            row.update(zone=str(a.zone_id), extent=vec(a.bounds.get_unscaled_box_extent()),
                       floors=[f.get_actor_label() for f in a.get_editor_property('floor_actors')])
        if isinstance(a, u.JTSInteriorTransit):
            dest=a.get_editor_property('destination')
            row.update(destination=dest.get_actor_label() if dest else None)
        c=a.get_component_by_class(u.JTSFacilityConditionComponent)
        if c:
            row.update(conditions=c.export_text() if hasattr(c,'export_text') else
                       [r.export_text() for r in c.get_editor_property('requirements')])
        rows.append(row)
    result=dict(planet=planet_signature(p), center=vec(p.get_planet_center()),
                basis=str(basis), actors=rows)
    (SAVED/'MarsStationStructureBefore.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    return dict(snapshot='Saved/MarsStationStructureBefore.json', actors=len(rows),
                rooms=[r for r in rows if 'zone' in r and r['zone'] in ('Hub','B1','B2','B3','B4','SeedVault')],
                transits=[dict(label=r['label'],local=r['local'],destination=r['destination']) for r in rows if 'destination' in r],
                dome=[r for r in rows if r['label'] in ('MC_R_Dome_Ribs','MC_R_Lift_Housing')])

def radial_frame(p, point, y_axis):
    up=p.get_radial_up_vector(point)
    y_axis=(y_axis-up*y_axis.dot(up)).normal()
    return u.Transform(location=point, rotation=u.MathLibrary.make_rot_from_zx(
        up,y_axis.cross(up)), scale=u.Vector(1,1,1))

def ground(p, point):
    hit=p.project_point_to_surface(point)
    assert hit.blocking_hit, 'No real Mars mesh hit'
    return hit.impact_point

def roof_cover(p,t,w,d,h):
    cover=[]
    for i in range(10):
        for j in range(12):
            point=t.transform_location(u.Vector(w*(i/9-.5),d*(j/11-.5),h+70))
            surface=ground(p,point)
            cover.append((surface-point).dot(p.get_radial_up_vector(point)))
    return min(cover)

def layout(aa,p,heading_degrees=5):
    origin=aa['MC_R_Lift_Housing'].get_actor_location()
    up=p.get_radial_up_vector(origin)
    noctis=aa['MC_Noctis_Waypoint'].get_actor_location()-origin
    south=(noctis-up*noctis.dot(up)).normal()
    north=-south*math.cos(math.radians(heading_degrees))+up.cross(-south)*math.sin(math.radians(heading_degrees))
    hub=radial_frame(p,origin-up*2800,north)
    rooms={}
    for name,xy,height in [('B1',(-1,0),800),('B2',(1,0),1200),
                           ('B3',(0,1),1000),('B4',(0,-1),900),
                           ('SeedVault',(2**-.5,2**-.5),1000)]:
        direction=hub.rotation.get_axis_x()*xy[0]+hub.rotation.get_axis_y()*xy[1]
        distance=6220 if name!='SeedVault' else 7400
        surface=ground(p,origin+direction*distance)
        depth=2800
        t=radial_frame(p,surface-p.get_radial_up_vector(surface)*depth,direction)
        # Keep every roof below the real mesh, including low crater faces.
        w,d= (2400,2400) if name=='SeedVault' else (3600,4800)
        cover=roof_cover(p,t,w,d,height)
        for _ in range(8):
            if cover>=800:break
            depth+=max(10,810-cover)
            t.translation=surface-p.get_radial_up_vector(surface)*depth
            cover=roof_cover(p,t,w,d,height)
        rooms[name]=dict(t=t,w=w,d=d,h=height,depth=depth,
                         cover=roof_cover(p,t,w,d,height))
    return hub,rooms

def plan():
    _,aa,p=context();hub,rooms=layout(aa,p)
    out=dict(hub_world_cm=vec(hub.translation),hub_diameter_m=20,hub_height_m=10,
             hub_cover_m=roof_cover(p,hub,2000,2000,1000)/100,
             rooms=[dict(name=k,world_cm=vec(r['t'].translation),net_m=[r['w']/100,r['d']/100,r['h']/100],
                         floor_depth_m=round(r['depth']/100,3),roof_cover_m=round(r['cover']/100,3),
                         radial_delta_deg=round(math.degrees(math.acos(max(-1,min(1,hub.rotation.get_axis_z().dot(r['t'].rotation.get_axis_z()))))),3)) for k,r in rooms.items()])
    out['heading_candidates']=[]
    for degrees in (-20,-15,-10,-5,5,10,15,20):
        hh,rr=layout(aa,p,degrees)
        out['heading_candidates'].append(dict(degrees=degrees,depths_m={k:round(v['depth']/100,2) for k,v in rr.items()}))
    out['solar_candidates']=[]
    for degrees in (-30,-20,-10,0,10,20,30):
        base,centers=solar_layout(aa,p,degrees);score=[]
        landing=base.inverse_transform_location(aa['MC_Station_Landing'].get_actor_location())
        for cx,cy in centers:
            for strip in range(6):
                for segment in range(4):
                    center=ground(p,base.transform_location(u.Vector(cx+1146+strip*400,cy-1750+segment*600,0)))
                    up=p.get_radial_up_vector(center);normal=p.project_point_to_surface(center).impact_normal
                    score.append(math.degrees(math.acos(max(-1,min(1,up.dot(normal))))))
        out['solar_candidates'].append(dict(degrees=degrees,max_slope=max(score),mean_slope=sum(score)/len(score),landing_gap=min(math.hypot(max(0,abs(landing.x-x)-3300),max(0,abs(landing.y-y)-2100))/100-15 for x,y in centers)))
    (SAVED/'MarsStationStructurePlan.json').write_text(json.dumps(out,indent=2),encoding='utf8')
    return out

def requirements(o,ids):
    items=[setp(u.JTSFacilityRequirement(),source=u.JTSFacilityFact.FLAG,id=i) for i in ids]
    setp(o.get_component_by_class(u.JTSFacilityConditionComponent),requirements=items,mode=u.JTSConditionMode.ALL)

def attach(a,t,p):
    a.detach_from_actor(u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD)
    a.set_actor_transform(t,False,False)
    a.attach_to_actor(p,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
    a.set_owner(p)
    if 'PlanetSurfaceContent' not in [str(tag) for tag in a.tags]:a.tags=list(a.tags)+['PlanetSurfaceContent']

def spawn(sub,aa,p,label,cls,t,folder='Structure'):
    a=aa.get(label)
    if not a:a=sub.spawn_actor_from_class(cls,t.translation,t.rotation.rotator());a.set_actor_label(label);aa[label]=a
    if isinstance(a,u.StaticMeshActor):a.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
    attach(a,t,p);a.set_folder_path('MarsChapter/Station'+folder)
    return a

def mesh_actor(sub,aa,p,label,mesh,t,material=None,collision=True,folder='Structure'):
    a=spawn(sub,aa,p,label,u.StaticMeshActor,t,folder)
    c=a.static_mesh_component;c.set_mobility(u.ComponentMobility.MOVABLE)
    c.set_static_mesh(mesh);setp(c,override_materials=[])
    if material:c.set_material(0,material)
    c.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    c.set_collision_object_type(u.CollisionChannel.ECC_WORLD_STATIC)
    return a

def block(sub,aa,p,label,t,offset,size,mat='Wall',collision=True):
    world=u.Transform(location=t.transform_location(u.Vector(*offset)),rotation=t.rotation.rotator(),
                      scale=u.Vector(*(v/100 for v in size)))
    return mesh_actor(sub,aa,p,label,u.load_asset('/Engine/BasicShapes/Cube'),world,
        u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_'+mat),collision)

def instance_materials():
    # The shared tracker and cable instances also use the existing frame/metal materials.
    for name in ('Orange','White','Alloy','Dark'):
        material=u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_'+name)
        # Loading an ISM can auto-enable this flag without marking the asset dirty.
        setp(material,used_with_instanced_static_meshes=True)
        u.MaterialEditingLibrary.recompile_material(material)
        u.EditorAssetLibrary.save_loaded_asset(material,False)

def power_inspect():
    _,aa,p=context()
    guides={k:str(a.get_editor_property('read_flag')) for k,a in aa.items() if k in ['MC_R_Guide_Sector'+s for s in 'ABC']}
    refs={}
    for label,flag in guides.items():
        if flag in ('','None'):continue
        refs[flag]=[k for k,a in aa.items() for c in a.get_components_by_class(u.JTSFacilityConditionComponent)
                    if any(str(r.id)==flag for r in c.get_editor_property('requirements'))]
    paths=[ROOT+'/SM_MP_Solar_RollOut',ROOT+'/SM_MP_Solar_RollBox']+list(u.EditorAssetLibrary.list_assets(ROOT+'/ProjectedFilm',True,False))
    asset_refs={path:list(u.EditorAssetLibrary.find_package_referencers_for_asset(path,False)) for path in paths if u.EditorAssetLibrary.does_asset_exist(path)}
    result=dict(guides=guides,flag_references=refs,asset_references=asset_refs,planet=planet_signature(p),
                protected={k:dict(transform=str(aa[k].get_actor_transform()),mesh=[c.static_mesh.get_path_name() for c in aa[k].get_components_by_class(u.StaticMeshComponent) if c.static_mesh]) for k in ('MC_R_Dome_Ribs','MC_R_Dome_Glazing','MC_R_EcologyEntranceDoor','MC_R_Guide_DomeOperations','MC_R_Lift_Housing')},
                rack_old=vec(aa['MC_R_Recovery_ServiceParts'].get_actor_location()) if 'MC_R_Recovery_ServiceParts' in aa else None)
    (SAVED/'MarsPowerBefore.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    return result


def power_material(name,color,metallic=0,emission=0,blink=False):
    at=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
    mat=u.load_asset(ROOT+'/M_MP_Power'+name)
    if not mat:mat=at.create_asset('M_MP_Power'+name,ROOT,u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    c=setp(mel.create_material_expression(mat,u.MaterialExpressionConstant3Vector,-450,0),constant=u.LinearColor(*color,1))
    mel.connect_material_property(c,'',u.MaterialProperty.MP_BASE_COLOR)
    rough=setp(mel.create_material_expression(mat,u.MaterialExpressionConstant,-450,100),r=.58)
    mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    metal=setp(mel.create_material_expression(mat,u.MaterialExpressionConstant,-450,200),r=metallic)
    mel.connect_material_property(metal,'',u.MaterialProperty.MP_METALLIC)
    if emission:
        mult=mel.create_material_expression(mat,u.MaterialExpressionMultiply,-100,0)
        mel.connect_material_expressions(c,'',mult,'A')
        intensity=setp(mel.create_material_expression(mat,u.MaterialExpressionConstant,-450,300),r=emission)
        if blink:
            time=mel.create_material_expression(mat,u.MaterialExpressionTime,-800,400)
            sine=setp(mel.create_material_expression(mat,u.MaterialExpressionSine,-600,400),period=4.)
            assert mel.connect_material_expressions(time,'',sine,'')
            absolute=mel.create_material_expression(mat,u.MaterialExpressionAbs,-400,400)
            assert mel.connect_material_expressions(sine,'',absolute,'')
            pulse=mel.create_material_expression(mat,u.MaterialExpressionMultiply,-200,300)
            mel.connect_material_expressions(absolute,'',pulse,'A');mel.connect_material_expressions(intensity,'',pulse,'B')
            minimum=setp(mel.create_material_expression(mat,u.MaterialExpressionAdd,0,300),const_b=.35)
            mel.connect_material_expressions(pulse,'',minimum,'A');intensity=minimum
        mel.connect_material_expressions(intensity,'',mult,'B');mel.connect_material_property(mult,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    setp(mat,used_with_instanced_static_meshes=True);mel.recompile_material(mat);u.EditorAssetLibrary.save_loaded_asset(mat,False)
    return mat


def assets():
    context();instance_materials()
    dust=power_material('Cell',(.008,.022,.052),.25)
    red=power_material('RedDust',(.095,.029,.016))
    at=u.AssetToolsHelpers.get_asset_tools();source=Path(u.Paths.project_dir())/'SourceArt/MarsStation/Refined'
    specs=json.loads((source/'MarsStationSolar25.json').read_text(encoding='utf8'))
    result=[]
    u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'Interchange.FeatureFlags.Import.FBX 0')
    for name in ('SM_MP_Solar_Tracker','SM_MP_Solar_Canopy'):
        mesh=import_power_mesh(source,name)
        for i,slot in enumerate(mesh.get_editor_property('static_materials')):
            key=str(slot.material_slot_name).replace('MP_','')
            mat=dust if key=='SolarDust' else red if key=='RedDust' else u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_'+{'Frame':'White','Metal':'Alloy','Graphite':'Dark','Orange':'Orange'}.get(key,'Dark'))
            mesh.set_material(i,mat)
        u.EditorAssetLibrary.set_metadata_tag(mesh,'MarsSolar25',json.dumps(specs[name]));u.EditorAssetLibrary.save_loaded_asset(mesh,False)
        result.append(dict(name=name,triangles=mesh.get_num_triangles(0),spec=specs[name]))
    return dict(solar=result,power=power_assets())


def import_power_mesh(source,name):
    at=u.AssetToolsHelpers.get_asset_tools()
    existing=u.load_asset(ROOT+'/'+name)
    if existing:u.get_editor_subsystem(u.StaticMeshEditorSubsystem).remove_collisions(existing)
    task=setp(u.AssetImportTask(),filename=str(source/(name+'.fbx')),destination_path=ROOT,destination_name=name,automated=True,save=True,replace_existing=True)
    options=setp(u.FbxImportUI(),import_mesh=True,import_materials=False,import_textures=False,import_as_skeletal=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH)
    setp(options.static_mesh_import_data,combine_meshes=True,auto_generate_collision=False,one_convex_hull_per_ucx=True)
    setp(task,options=options);at.import_asset_tasks([task]);mesh=u.load_asset(ROOT+'/'+name);assert mesh,name
    if name=='SM_MP_Power_CoreModule':
        # Legacy reimports retain the barcode material slot name by index.
        slots=mesh.get_editor_property('static_materials')
        for i,slot in enumerate(slots):
            if str(slot.material_slot_name)=='Power_Black':slots[i]=setp(slot,material_slot_name='Power_CoreLabel')
        mesh.set_editor_property('static_materials',slots)
    if name.startswith('SM_MP_Power_'):
        u.get_editor_subsystem(u.StaticMeshEditorSubsystem).add_simple_collisions(mesh,u.ScriptingCollisionShapeType.BOX)
    setp(mesh.get_editor_property('body_setup'),collision_trace_flag=u.CollisionTraceFlag.CTF_USE_DEFAULT)
    return mesh


def power_assets():
    from editor_toolset.toolsets.actor import ActorTools
    from editor_toolset.toolsets.blueprint import BlueprintTools
    source=Path(u.Paths.project_dir())/'SourceArt/MarsStation/Power'
    colors={'White':(.72,.76,.77),'Orange':(.9,.19,.026),'Metal':(.25,.3,.32),'Dark':(.012,.016,.019),'Concrete':(.26,.26,.25),'Gold':(.62,.37,.055),'Green':(.014,.8,.065),'Yellow':(.95,.57,.018),'Black':(.003,.004,.005)}
    materials={k:power_material(k,v,.7 if k in ('Metal','Gold') else 0,2 if k=='Green' else 0 if k=='Black' else .2) for k,v in colors.items()}
    materials['Red']=power_material('Red',(1,.008,.003),0,3,True)
    materials['CoreLabel']=power_core_label()
    meshes={}
    for name in json.loads((source/'manifest.json').read_text(encoding='utf8')):
        mesh=import_power_mesh(source,name)
        for i,slot in enumerate(mesh.get_editor_property('static_materials')):mesh.set_material(i,materials[str(slot.material_slot_name).replace('Power_','')])
        u.EditorAssetLibrary.save_loaded_asset(mesh,False);meshes[name.replace('SM_MP_Power_','')]=mesh
    item_path='/Game/Space/Data/Items';at=u.AssetToolsHelpers.get_asset_tools()
    item=u.load_asset(item_path+'/DA_Item_CombinerCoreModule')
    if not item:item=at.create_asset('DA_Item_CombinerCoreModule',item_path,u.JTSItemDefinition,setp(u.DataAssetFactory(),data_asset_class=u.JTSItemDefinition))
    setp(item,drop_on_death=True)
    held=setp(item.get_editor_property('held_presentation'),override_prototype_profile=True,mesh=meshes['CoreModule'],mesh_scale=1.,grip_transform=u.Transform(location=u.Vector(0,0,28.3),rotation=u.Rotator(pitch=90)),body_scale=u.Vector(.1,.1,.1),barrel_scale=u.Vector(0,0,0),sight_scale=u.Vector(0,0,0))
    setp(item,item_id=u.JTSItemId.COMBINER_CORE_MODULE,display_name='汇流核心组件',description='手持组件，对准待检修汇流器按 E 更换。备件位于生态球大门内的备件架。',primary_category=u.JTSItemCategory.UTILITY,capability_mask=33,max_stack_size=3,combat_damage=0,world_pickup_mesh=meshes['CoreModule'],world_pickup_width=45,held_presentation=held,accent_color=u.LinearColor(.9,.19,.026,1))
    u.EditorAssetLibrary.save_loaded_asset(item,False)
    bp_root='/Game/Space/Blueprints/Planets/Mars/Chapter'
    def bp(name,parent):
        a=u.load_asset(bp_root+'/'+name)
        if not a:a=at.create_asset(name,bp_root,u.Blueprint,setp(u.BlueprintFactory(),parent_class=parent))
        return a
    def component(b,name,mesh,loc=(0,0,0),rot=None,visible=True):
        cdo=u.get_default_object(b.generated_class());cs=ActorTools.get_components(cdo)
        c=next((c for c in cs if c.get_name().replace('_GEN_VARIABLE','')==name),None)
        if not c:c=ActorTools.add_component(b,u.StaticMeshComponent.static_class(),name)
        c.set_static_mesh(mesh);c.set_mobility(u.ComponentMobility.MOVABLE);c.set_collision_profile_name('NoCollision')
        setp(c,relative_location=u.Vector(*loc),relative_rotation=rot or u.Rotator(),visible=visible,hidden_in_game=False)
        return c
    comb=bp('BP_Mars_SolarCombiner',u.JTSFacilityController)
    cdo=u.get_default_object(comb.generated_class());cdo.mesh.set_static_mesh(meshes['CombinerBody'])
    cdo.status_light.set_static_mesh(meshes['StatusLens']);cdo.status_light.set_relative_location(u.Vector(0,0,191),False,False)
    cdo.status_light.set_relative_scale3d(u.Vector(1,1,1))
    setp(cdo,always_relevant=True,required_held_item=u.JTSItemId.COMBINER_CORE_MODULE,required_held_item_count=1,repair_costs={},repair_seconds=0,interaction_distance=200,status_activation_delay=1.5,locked_prompt='待检修',completed_prompt='已修复',missing_held_item_prompt='需要汇流核心组件（生态球大门内的备件架）',held_item_action_prompt='[E] 更换核心组件',inactive_material=materials['Red'],active_material=materials['Green'])
    cdo.status_light.set_material(0,materials['Red']);requirements(cdo,['Mars_EcologyGate_Opened','Mars_PowerDiagnosis_Read'])
    component(comb,'CabinetBase',meshes['CombinerBase'])
    component(comb,'CabinetDoor',meshes['CombinerDoor'],(44,57,15),u.Rotator(yaw=55))
    component(comb,'CoreBurntInSlot',meshes['CoreBurnt'],(44,-45,95),u.Rotator(roll=90))
    component(comb,'CoreInstalled',meshes['CoreModule'],(37,-45,95),u.Rotator(roll=90),False)
    component(comb,'CoreDiscarded',meshes['CoreBurnt'],(78,35,24),u.Rotator(pitch=90,yaw=25),False)
    component(comb,'SectorGlyph',meshes['SectorA'])
    u.BlueprintEditorLibrary.compile_blueprint(comb);u.EditorAssetLibrary.save_loaded_asset(comb,False)
    rack=bp('BP_Mars_CoreSupplyRack',u.JTSItemSupplyRack);cdo=u.get_default_object(rack.generated_class());cdo.mesh.set_static_mesh(meshes['SupplyRack'])
    setp(cdo,always_relevant=True,item_id=u.JTSItemId.COMBINER_CORE_MODULE,slot_count=4,initial_count=4,refill_delay=10,stop_flag='Mars_Power',completion_flags=['Mars_Sector_'+s+'_Repaired' for s in 'ABC'],reserve_count=1,slot_locations=[u.Vector(10,-60+i*40,66) for i in range(4)])
    setp(cdo.refill_conditions,requirements=[setp(u.JTSFacilityRequirement(),id='Mars_Power',negate=True)],mode=u.JTSConditionMode.ALL)
    for i in range(4):
        component(rack,'Supply'+str(i),meshes['CoreModule'],(0,-60+i*40,66),u.Rotator(yaw=90))
        c=component(rack,'SlotLamp'+str(i),meshes['StatusLens'],(33,-60+i*40,60),u.Rotator(pitch=90));c.set_relative_scale3d(u.Vector(.25,.25,.25));c.set_material(0,materials['Green'])
    u.BlueprintEditorLibrary.compile_blueprint(rack);u.EditorAssetLibrary.save_loaded_asset(rack,False)
    # Discover before authoring graph nodes; never guess localized node identifiers.
    graph=u.BlueprintEditorLibrary.find_event_graph(comb)
    nodes={key:list(BlueprintTools.find_node_types(graph,key)) for key in ('OnStateChanged','OnRuntimeActivated','SetRelativeRotation','SetRelativeLocation','SetVisibility','SetHiddenInGame','SetMaterial','Delay','GetActive','CabinetDoor','ReceiveBeginPlay','BindEvent')}
    (SAVED/'MarsPowerGraphNodes.json').write_text(json.dumps(nodes,ensure_ascii=False,indent=2),encoding='utf8')
    return dict(meshes=list(meshes),item=item.get_path_name(),blueprints=[comb.get_path_name(),rack.get_path_name()],graph_nodes=nodes,presentation=power_presentation())


def power_core_label():
    """Printed module identification; keep lettering out of the handheld mesh budget."""
    at=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
    task=setp(u.AssetImportTask(),filename=str(Path(u.Paths.project_dir())/'SourceArt/MarsStation/Power/T_MP_CoreLabel.png'),destination_path=ROOT,automated=True,save=True,replace_existing=True)
    at.import_asset_tasks([task]);texture=u.load_asset(ROOT+'/T_MP_CoreLabel')
    assert texture
    mat=u.load_asset(ROOT+'/M_MP_PowerCoreLabel')
    if not mat:mat=at.create_asset('M_MP_PowerCoreLabel',ROOT,u.Material,u.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    sample=setp(mel.create_material_expression(mat,u.MaterialExpressionTextureSample,-400,0),texture=texture)
    assert mel.connect_material_property(sample,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    rough=setp(mel.create_material_expression(mat,u.MaterialExpressionConstant,-400,100),r=.65)
    mel.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
    setp(mat,used_with_instanced_static_meshes=True);mel.recompile_material(mat);u.EditorAssetLibrary.save_loaded_asset(mat,False)
    return mat


def solar_layout(aa,p,degrees=20):
    origin=aa['MC_R_Lift_Housing'].get_actor_location();up=p.get_radial_up_vector(origin)
    route=aa['MC_Noctis_Waypoint'].get_actor_location()-origin;route=(route-up*route.dot(up)).normal()
    landing=aa['MC_Station_Landing'].get_actor_location()-origin;landing=(landing-up*landing.dot(up)).normal()
    heading=-(route+landing).normal()
    heading=heading*math.cos(math.radians(degrees))+up.cross(heading)*math.sin(math.radians(degrees))
    return radial_frame(p,origin,heading),[(-7800,2000),(0,7900),(7800,2000)]


def supported(p,base,x,y,hx,hy):
    point=ground(p,base.transform_location(u.Vector(x,y,0)));t=radial_frame(p,point,base.rotation.get_axis_y());heights=[]
    for xx in (-hx,0,hx):
        for yy in (-hy,0,hy):heights.append((ground(p,t.transform_location(u.Vector(xx,yy,0)))-point).dot(t.rotation.get_axis_z()))
    t.translation=point+t.rotation.get_axis_z()*(max(heights)+3)
    return t,max(heights)-min(heights)


def surface():
    from editor_toolset.toolsets.actor import ActorTools
    sub,aa,p=context();signature=planet_signature(p);instance_materials()
    before=json.loads((SAVED/'MarsPowerBefore.json').read_text(encoding='utf8'))
    assert not any(before['flag_references'].values()),'Merge referenced guide flags before removing guides'
    for path,refs in before['asset_references'].items():
        assert all(r=='/Game/Space/Maps/L_SpaceWorld' or '/ProjectedFilm/' in r for r in refs),('External film reference',path,refs)
    retired=[]
    for label,a in list(aa.items()):
        if label.startswith(('MC_S25_Film_','MC_S25_RollBox_','MC_S25_CableRuns','MC_R_Guide_Sector','MC_R_GuideStand_Sector','MC_R_Sign_Sector','MC_S25_Foot_','MC_S25_CanopyFoot_','MC_S25_Power','MC_R_PV_Trench_','MC_R_Cable_','MC_DustSweep')):
            sub.destroy_actor(a);aa.pop(label);retired.append(label)
    base,centers=solar_layout(aa,p);rows=[];footprints=[];sector_transforms={};piers=[]
    container=spawn(sub,aa,p,'MC_S25_TrackerInstances',u.StaticMeshActor,base,'Solar');container.static_mesh_component.set_static_mesh(None)
    instances=container.get_component_by_class(u.InstancedStaticMeshComponent)
    if not instances:instances=ActorTools.add_component(container,u.InstancedStaticMeshComponent.static_class(),'FixedTrackers25')
    instances.set_mobility(u.ComponentMobility.MOVABLE);instances.set_static_mesh(u.load_asset(ROOT+'/SM_MP_Solar_Tracker'));instances.set_collision_profile_name('BlockAll');instances.clear_instances()
    def footprint(t,hx,hy,sector,kind):
        corners=[t.transform_location(u.Vector(x,y,0)) for x,y in ((-hx,-hy),(hx,-hy),(hx,hy),(-hx,hy))]
        points=[base.inverse_transform_location(ground(p,q)) for q in corners]
        footprints.append(dict(sector=sector,kind=kind,polygon=[[q.x,q.y] for q in points]))
    def pier(label,t,x,y,spread):
        if spread<=30:return
        q=t.transform_location(u.Vector(x,y,0));g=ground(p,q);depth=max(0,(q-g).dot(t.rotation.get_axis_z()))
        if depth>3:piers.append(block(sub,aa,p,label,t,(x,y,-depth/2),(95,95,depth),'Wall').get_actor_label())
    for index,(cx,cy) in enumerate(centers):
        letter='ABC'[index];transforms=[]
        positions=[(-2828+col*1094,-1500+row*1500) for row in range(3) for col in range(4)]
        positions += [(1498+col*1094,-1500+row*1500) for row in range(2) for col in range(2)]
        for j,(x,y) in enumerate(positions):
            t,spread=supported(p,base,cx+x,cy+y,272,655);instances.add_instance(t,world_space=True);transforms.append(t)
            footprint(t,275,655,letter,'Tracker')
            old=aa.get('MC_Solar_'+str(index)+'_'+str(j))
            if old:attach(old,t,p);old.static_mesh_component.set_static_mesh(None)
            for k,yy in enumerate((-360,360)):pier('MC_S25_Foot_'+letter+'_'+str(j)+'_'+str(k),t,0,yy,spread)
        canopy,spread=supported(p,base,cx+2138,cy+1450,913,700)
        mesh_actor(sub,aa,p,'MC_S25_Canopy_'+letter,u.load_asset(ROOT+'/SM_MP_Solar_Canopy'),canopy,folder='Solar');footprint(canopy,913,700,letter,'Canopy')
        for ix,x in enumerate((-810,0,810)):
            for iy,y in enumerate((-550,550)):pier('MC_S25_CanopyFoot_'+letter+str(ix)+str(iy),canopy,x,y,spread)
        nest=aa['MC_Solar_Woodlice_'+str(index)];attach(nest,radial_frame(p,ground(p,canopy.translation),base.rotation.get_axis_y()),p);setp(nest,spawn_radius=600)
        distance=math.hypot(cx,cy);dx,dy=cx/distance,cy/distance
        edge=min(3100/abs(dx) if dx else 1e10,2200/abs(dy) if dy else 1e10)
        x,y=cx-dx*(edge+680),cy-dy*(edge+680)
        t,spread=supported(p,base,x,y,100,80);t.rotation=u.MathLibrary.make_rot_from_zx(p.get_radial_up_vector(t.translation),base.translation-t.translation).quaternion()
        old=aa['MC_Sector_'+letter];completed=str(old.get_editor_property('completed_flag'))
        if old.get_class()!=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_SolarCombiner').generated_class():sub.destroy_actor(old);aa.pop('MC_Sector_'+letter)
        box=spawn(sub,aa,p,'MC_Sector_'+letter,u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_SolarCombiner').generated_class(),t,'Solar')
        setp(box,completed_flag=completed,followup_flag='',display_name=letter+' 区汇流器',repair_costs={},required_held_item=u.JTSItemId.COMBINER_CORE_MODULE,repair_seconds=0,status_activation_delay=1.5)
        requirements(box,['Mars_EcologyGate_Opened','Mars_PowerDiagnosis_Read'])
        # Explicit authored fault pose also makes re-running after preview captures deterministic.
        for c in box.get_components_by_class(u.StaticMeshComponent):
            name=c.get_name()
            if name.startswith('CabinetDoor'):
                c.set_relative_location(u.Vector(44,57,15),False,False);c.set_relative_rotation(u.Rotator(yaw=55),False,False)
            if name.startswith('CoreBurntInSlot'):
                c.set_relative_location(u.Vector(44,-45,95),False,False);c.set_relative_rotation(u.Rotator(roll=90),False,False);c.set_visibility(True)
            if name.startswith('CoreInstalled'):
                c.set_relative_location(u.Vector(37,-45,95),False,False);c.set_visibility(False)
            if name.startswith('CoreDiscarded'):
                c.set_relative_location(u.Vector(78,35,24),False,False);c.set_visibility(False)
        box.status_light.set_material(0,u.load_asset(ROOT+'/M_MP_PowerRed'))
        box.status_light.set_relative_scale3d(u.Vector(1,1,1))
        glyph=next(c for c in box.get_components_by_class(u.StaticMeshComponent) if c.get_name().startswith('SectorGlyph'));glyph.set_static_mesh(u.load_asset(ROOT+'/SM_MP_Power_Sector'+letter))
        sector_transforms[letter]=dict(center=[cx,cy],combiner=vec(base.inverse_transform_location(t.translation)),instances=[i+index*16 for i in range(16)])
        rows.append(dict(sector=letter,center_distance_m=(ground(p,base.transform_location(u.Vector(cx,cy,0)))-base.translation).length()/100,trackers=16,panel_area_m2=16*72+280))
    # Round cables use a 1 m engine cylinder (radius 50 cm), with no collision.
    cable_actor=spawn(sub,aa,p,'MC_S25_PowerCables',u.StaticMeshActor,base,'Solar');cable_actor.static_mesh_component.set_static_mesh(None)
    cables=ActorTools.add_component(cable_actor,u.InstancedStaticMeshComponent.static_class(),'RoundGroundCables')
    cables.set_mobility(u.ComponentMobility.MOVABLE);cables.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cylinder'));cables.set_material(0,u.load_asset(ROOT+'/M_MP_PowerBlack'));cables.set_collision_profile_name('NoCollision')
    # Thin ground leads do not need thousands of Lumen surface-cache cards.
    setp(cables,affect_distance_field_lighting=False,affect_dynamic_indirect_lighting=False)
    def cable_segment(a,b,radius):
        delta=b-a
        if delta.length()>.01:cables.add_instance(u.Transform(location=(a+b)/2,rotation=u.MathLibrary.make_rot_from_zx(delta,p.get_radial_up_vector((a+b)/2)),scale=u.Vector(radius/50,radius/50,delta.length()/100+.015)),world_space=True)
    def cable(points,radius=4,offset=0,markers=False,label=''):
        # Catmull-Rom interpolation rounds every turn; each short segment is individually projected to the real mesh.
        vectors=[u.Vector(*q) for q in points];controls=[vectors[0]]+vectors+[vectors[-1]];samples=[]
        for i in range(1,len(controls)-2):
            a,b,c,d=controls[i-1:i+3];n=max(2,math.ceil((c-b).length()/50))
            for j in range(n):
                v=j/n;sample=(b*2+(c-a)*v+(a*2-b*5+c*4-d)*v*v+(a*-1+b*3-c*3+d)*v*v*v)*.5;samples.append(sample)
        samples.append(vectors[-1]);length=0;next_marker=1000
        for i,(a,b) in enumerate(zip(samples,samples[1:])):
            side=u.Vector(-(b.y-a.y),b.x-a.x,0).normal()*offset
            wa=ground(p,base.transform_location(a+side));wb=ground(p,base.transform_location(b+side));up=p.get_radial_up_vector((wa+wb)/2)
            wa=wa+p.get_radial_up_vector(wa)*radius;wb=wb+p.get_radial_up_vector(wb)*radius;delta=wb-wa
            if delta.length()<.01:continue
            cables.add_instance(u.Transform(location=(wa+wb)/2,rotation=u.MathLibrary.make_rot_from_zx(delta,up),scale=u.Vector(radius/50,radius/50,delta.length()/100+.015)),world_space=True)
            length+=delta.length()
            if markers and length>=next_marker:
                q=ground(p,base.transform_location(b+side.normal()*45));mt=radial_frame(p,q,base.rotation.get_axis_y())
                mesh_actor(sub,aa,p,'MC_S25_PowerMarker_'+label+'_'+str(int(next_marker/1000)),u.load_asset(ROOT+'/SM_MP_Power_CableMarker'),mt,collision=False,folder='Solar');next_marker+=1000
        return length/100
    lengths={}
    for letter,(cx,cy) in zip('ABC',centers):
        box=aa['MC_Sector_'+letter];q=base.inverse_transform_location(box.get_actor_location());direction2=u.Vector(q.x,q.y,0).normal();end=direction2*1900
        start=u.Vector(q.x,q.y,0);mid=start*.54+end*.46+u.Vector(-direction2.y,direction2.x,0)*140
        controls=[(start.x,start.y,0),(mid.x,mid.y,0),(end.x,end.y,0)]
        lengths[letter]=[cable(controls,4,offset,offset==-6,letter) for offset in (-6,6)]
        wallpoint=base.transform_location(end);wt=radial_frame(p,wallpoint,base.rotation.get_axis_y());wt.rotation=u.MathLibrary.make_rot_from_zx(p.get_radial_up_vector(wallpoint),wallpoint-base.translation).quaternion()
        mesh_actor(sub,aa,p,'MC_S25_PowerWallBox_'+letter,u.load_asset(ROOT+'/SM_MP_Power_WallJunction'),wt,folder='Solar')
        for side in (-6,6):
            endpoint=ground(p,wallpoint+wt.rotation.get_axis_y()*side);cable_segment(endpoint+p.get_radial_up_vector(endpoint)*4,wt.transform_location(u.Vector(0,side,12)),4)
        for col in range(6):
            local=(-2828+col*1094,-1500) if col<4 else (1498+(col-4)*1094,-1500)
            target=ground(p,base.transform_location(u.Vector(cx+local[0],cy+local[1]-360,0)));jt=radial_frame(p,target,base.rotation.get_axis_y());jt.translation=target+jt.rotation.get_axis_z()*110+jt.rotation.get_axis_x()*16
            mesh_actor(sub,aa,p,'MC_S25_PowerColumnBox_'+letter+str(col),u.load_asset(ROOT+'/SM_MP_Power_ColumnJunction'),jt,collision=False,folder='Solar')
            port=jt.transform_location(u.Vector(0,0,4));cable_segment(target+p.get_radial_up_vector(target)*4,port,3)
            j=base.inverse_transform_location(target);a=u.Vector(j.x,j.y,0);b=u.Vector(q.x,q.y,0);bend=a*.5+b*.5+u.Vector(0,-200,0)
            cable([vec(a),vec(bend),vec(b)],3)
    # Rack sits beside the existing diagnosis desk; the desk and folder stay untouched.
    label='MC_R_Recovery_ServiceParts'
    if label in aa:sub.destroy_actor(aa.pop(label))
    desk=aa['MC_R_Flow_DiagnosisDesk'];dt=desk.get_actor_transform();rt=u.Transform(location=dt.transform_location(u.Vector(170,0,0)),rotation=dt.rotation.rotator(),scale=u.Vector(1,1,1))
    rack=spawn(sub,aa,p,'MC_CoreSupplyRack',u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_CoreSupplyRack').generated_class(),rt,'Solar')
    data=aa['MC_R_Guide_DomeOperations'].get_editor_property('document')
    setp(data,body='赤原生态站 · 主电源故障维修交接\n\n三个太阳能扇区（A、B、C）的汇流器核心组件均已烧坏。电池板和储能柜本体正常，请勿拆卸。\n\n备件就在这张工作台旁边的四格备件架上。拿起一个汇流核心组件，手提到汇流器正面，按 E 更换。每区换一个，共三台；无需其他材料。\n\n三个扇区全部修好后，主电源自动恢复，剩余备件统一回收。电梯通电后，在电梯门前操作开门下行。\n\n门禁使用独立蓄电池，刷开生态球大门不代表主电源已恢复。棚架阵下有穴虱，检修时注意。')
    u.EditorAssetLibrary.save_loaded_asset(data,False);requirements(aa['MC_Power'],['Mars_Sector_'+s+'_Repaired' for s in 'ABC'])
    assert planet_signature(p)==signature,'Planet changed'
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    deleted=[]
    for path in before['asset_references']:
        if u.EditorAssetLibrary.does_asset_exist(path):
            refs=u.EditorAssetLibrary.find_package_referencers_for_asset(path,True)
            assert not [r for r in refs if r!='/Game/Space/Maps/L_SpaceWorld' and '/ProjectedFilm/' not in r],(path,refs)
            if u.EditorAssetLibrary.delete_asset(path):deleted.append(path)
    result=dict(sectors=rows,sector_transforms=sector_transforms,footprints=footprints,piers=piers,retired=retired,deleted_assets=deleted,main_cable_length_m=lengths,total_panel_area_m2=4296)
    (SAVED/'MarsPowerAuthoring.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    u.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    return result


def room_zone(sub,aa,p,name,t,w,d,h,floors):
    mid=u.Transform(location=t.transform_location(u.Vector(0,0,h/2)),rotation=t.rotation.rotator(),scale=u.Vector(1,1,1))
    zone=spawn(sub,aa,p,'MC_'+name+'_Zone',u.JTSInteriorZone,mid)
    setp(zone,zone_id=name,planet=p,display_name=name,floor_actors=floors)
    zone.bounds.set_box_extent(u.Vector(w/2+25,d/2+25,h/2+50))
    return zone

def shell(sub,aa,p,name,t,w,d,h,doors=None,floor=True,z0=0):
    doors=doors or {};prefix='MC_S22_'+name
    floors=[]
    if floor:floors.append(block(sub,aa,p,prefix+'_Floor',t,(0,0,z0-20),(w+80,d+80,40),'Dark'))
    block(sub,aa,p,prefix+'_Roof',t,(0,0,h+20),(w+80,d+80,40),'Dark')
    # A second independent opaque skin surrounds the six faces, sharing only the intended openings.
    block(sub,aa,p,prefix+'_OuterRoof',t,(0,0,h+55),(w+140,d+140,30),'Dark')
    block(sub,aa,p,prefix+'_OuterFloor',t,(0,0,z0-55),(w+140,d+140,30),'Dark')
    for edge in ('N','S','E','W'):
        along_x=edge in ('N','S');span=w if along_x else d
        fixed=(d/2 if along_x else w/2)*(1 if edge in ('N','E') else -1)
        openings=sorted(doors.get(edge,[]))
        for layer,thick,shift in (('Inner',40,20),('Outer',30,55)):
            fixed2=fixed+(1 if fixed>0 else -1)*shift
            def wall(key,lo,hi,bottom,top):
                if hi-lo<.1 or top-bottom<.1:return
                offset=((lo+hi)/2,fixed2,(bottom+top)/2) if along_x else (fixed2,(lo+hi)/2,(bottom+top)/2)
                size=(hi-lo,thick,top-bottom) if along_x else (thick,hi-lo,top-bottom)
                block(sub,aa,p,prefix+'_'+edge+'_'+layer+'_'+key,t,offset,size,'Wall' if layer=='Inner' else 'Dark')
            cursor=-span/2-40
            for i,(center,width,bottom,top) in enumerate(openings):
                lo,hi=center-width/2,center+width/2
                wall(str(i)+'Side',cursor,lo,z0,h)
                wall(str(i)+'Sill',lo,hi,z0,bottom)
                wall(str(i)+'Header',lo,hi,top,h)
                cursor=hi
            wall('End',cursor,span/2+40,z0,h)
    room_zone(sub,aa,p,name,t,w,d,h-z0,floors)
    return floors

def migrate(sub,aa,p,hub,rooms):
    # Keep a level-persisted local pose per actor, so this pass can be run again safely.
    old={}
    for name in ('Hub','B1','B2','B3','B4','SeedVault'):
        floor=aa['MC_'+name+'_Floor'] if 'MC_'+name+'_Floor' in aa else None
        zone=aa['MC_'+name+'_Zone']
        pos=(floor.get_actor_location()+floor.get_actor_up_vector()*20) if floor else zone.get_actor_location()-zone.get_actor_up_vector()*300
        if name=='B3':pos=zone.get_actor_location()-zone.get_actor_up_vector()*300
        old[name]=u.Transform(location=pos,rotation=zone.get_actor_rotation(),scale=u.Vector(1,1,1))
    updates=[];warnings=[]
    for label,a in list(aa.items()):
        if label.startswith(('MC_S22_','MC_S25_')) or isinstance(a,u.JTSInteriorZone):continue
        tags=[str(v) for v in a.tags];saved=next((v[8:] for v in tags if v.startswith('S22Pose:')),None)
        name=None
        if saved:
            data=json.loads(saved);name=data['room'];q=u.Vector(*data['local']);rel=u.Quat(*data['rotation'])
        else:
            if label.startswith(('MC_Hub_','MC_R_Hub_','MC_R_Sign_Hub','MC_Audio_Hub','MC_Camera_Hub','MC_R_Guide_Hub')):name='Hub'
            for n in ('B1','B2','B3','B4'):
                if label.startswith(('MC_'+n+'_','MC_R_'+n+'_','MC_Resource_'+n,'MC_Log_'+n,'MC_R_LogStand_MC_Log_'+n,'MC_R_Guide_'+n,'MC_R_Sign_'+n,'MC_Badge_'+n,'MC_Audio_'+n)):name=n
                i=n[-1]
                if label in ('MC_SupervisorRemains_'+i,'MC_R_Supervisor_Desk'+i):name=n
            if label.startswith(('MC_SeedVault_','MC_Tuantuan_','MC_Encounter_Tuantuan','MC_Camera_SeedVault','MC_R_Vault_')):name='SeedVault'
            if not name:continue
            q=old[name].inverse_transform_location(a.get_actor_location())
            rel=old[name].rotation.inversed()*a.get_actor_transform().rotation
            data=dict(room=name,local=vec(q),rotation=[rel.x,rel.y,rel.z,rel.w])
            a.tags=list(a.tags)+['S22Pose:'+json.dumps(data,separators=(',',':'))]
        new=hub if name=='Hub' else rooms[name]['t']
        t=u.Transform(location=new.transform_location(q),rotation=(new.rotation*rel).rotator(),scale=a.get_actor_scale3d())
        if label=='MC_R_Hub_Status':t.translation=new.transform_location(u.Vector(-650,650,q.z))
        updates.append((a,t))
        if name=='B3' and abs(q.y)<1800 and abs(q.x)<1800 and isinstance(a,(u.JTSReadableDocument,u.JTSExpeditionKeyPickup)):
            warnings.append(label+': legacy interaction remains in the B3 pool footprint; relocated to the entry platform')
            t.translation=new.transform_location(u.Vector(max(-1400,min(1400,q.x)),-2100,max(30,q.z)))
        if name=='B3' and label in ('MC_R_LogStand_MC_Log_B3','MC_R_B3_Desk','MC_R_Supervisor_Desk3','MC_SupervisorRemains_3'):
            t.translation=new.transform_location(u.Vector(max(-1400,min(1400,q.x)),-2100,q.z))
    for a,t in updates:attach(a,t,p)
    return [a.get_actor_label() for a,t in updates],warnings

def corridor(sub,aa,p,name,start,end,start_forward,end_forward,width=600,height=500):
    length=(end-start).length();count=max(12,math.ceil(length/100));floors=[];points=[];tangents=[]
    for i in range(count+1):
        s=i/count
        point=start*(2*s**3-3*s*s+1)+start_forward*(length*(s**3-2*s*s+s))+end*(-2*s**3+3*s*s)+end_forward*(length*(s**3-s*s))
        tangent=start*(6*s*s-6*s)+start_forward*(length*(3*s*s-4*s+1))+end*(-6*s*s+6*s)+end_forward*(length*(3*s*s-2*s))
        points.append(point);tangents.append(tangent.normal())
    for i in range(count):
        a,b=points[i],points[i+1];forward=(b-a).normal();center=(a+b)/2
        up=p.get_radial_up_vector(center);side=forward.cross(up).normal()
        normal=side.cross(forward).normal()
        t=u.Transform(location=center,rotation=u.MathLibrary.make_rot_from_xy(side,forward),scale=u.Vector(1,1,1))
        segment=(b-a).length();prefix='MC_S22_'+name+'_'+str(i)
        floor=block(sub,aa,p,prefix+'_Floor',t,(0,0,-20),(width+80,segment+8,40),'Dark');floors.append(floor)
        block(sub,aa,p,prefix+'_Roof',t,(0,0,height+20),(width+140,segment+12,40),'Dark')
        block(sub,aa,p,prefix+'_OuterRoof',t,(0,0,height+55),(width+180,segment+12,30),'Dark')
        block(sub,aa,p,prefix+'_OuterFloor',t,(0,0,-55),(width+180,segment+12,30),'Dark')
        for sign in (-1,1):
            block(sub,aa,p,prefix+'_Wall'+str(sign),t,(sign*(width/2+20),0,height/2),(40,segment+12,height),'Wall')
            block(sub,aa,p,prefix+'_OuterWall'+str(sign),t,(sign*(width/2+55),0,height/2),(30,segment+12,height+80),'Dark')
        room_zone(sub,aa,p,name+'_'+str(i),t,width,segment+25,height,[floor])
    return dict(name=name,start=vec(start),end=vec(end),width_m=width/100,height_m=height/100,
                length_m=sum((points[i+1]-points[i]).length() for i in range(count))/100,
                points=[vec(v) for v in points],floors=floors)

def interior():
    sub,aa,p=context();signature=planet_signature(p);hub,rooms=layout(aa,p)
    # Snapshot before any old shell is removed, then retain every existing behavior actor.
    moved,warnings=migrate(sub,aa,p,hub,rooms)
    removed=[]
    for label,a in list(aa.items()):
        generated_zone=isinstance(a,u.JTSInteriorZone) and (str(a.zone_id).startswith(('Hall_B','Hall_SeedVault')) or any(s in str(a.zone_id) for s in ('_Airlock','_Control')))
        if label.startswith('MC_S22_') or generated_zone:
            sub.destroy_actor(a);aa.pop(label)
    for label,a in list(aa.items()):
        old_shell=any(label.startswith('MC_'+n+'_') and any(label[len('MC_'+n+'_'):].startswith(part) for part in ('Floor','Roof','Wall_','Door_','DoorHeader_'))
            for n in ('Hub','B1','B2','B3','B4','SeedVault','Hall_West','Hall_East','Hall_North','Hall_South','Hall_Vault'))
        obsolete=label.startswith(('MC_B3_PoolRim_','MC_B3_PoolStep_','MC_Tuantuan_Pillar_')) or any(label.startswith('MC_R_'+n+'_'+part) for n in ('B1','B2','B3','B4') for part in ('Deck_','WallX_','WallY_'))
        obsolete=obsolete or any(label.startswith('MC_R_Rib_'+n) for n in ('West','East','North','South'))
        if old_shell or obsolete:
            sub.destroy_actor(a);aa.pop(label);removed.append(label)
    # Centre shaft: 64-sided circular envelope with five actual corridor mouths.
    cylinder=u.load_asset('/Engine/BasicShapes/Cylinder')
    floor=mesh_actor(sub,aa,p,'MC_S22_Hub_Floor',cylinder,u.Transform(location=hub.transform_location(u.Vector(0,0,-20)),rotation=hub.rotation.rotator(),scale=u.Vector(20,20,.4)),u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_Dark'))
    mesh_actor(sub,aa,p,'MC_S22_Hub_Roof',cylinder,u.Transform(location=hub.transform_location(u.Vector(0,0,1020)),rotation=hub.rotation.rotator(),scale=u.Vector(21.4,21.4,.4)),u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_Dark'))
    block(sub,aa,p,'MC_S22_Hub_OuterRoof',hub,(0,0,1055),(2150,2150,30),'Dark')
    block(sub,aa,p,'MC_S22_Hub_OuterFloor',hub,(0,0,-55),(2150,2150,30),'Dark')
    for i in range(64):
        angle=(i+.5)*math.tau/64;x,y=math.cos(angle),math.sin(angle)
        direction=hub.rotation.get_axis_x()*x+hub.rotation.get_axis_y()*y
        t=u.Transform(location=hub.transform_location(u.Vector(1000*x,1000*y,0)),rotation=u.MathLibrary.make_rot_from_zx(hub.rotation.get_axis_z(),direction),scale=u.Vector(1,1,1))
        delta=min(abs(math.atan2(math.sin(angle-a),math.cos(angle-a))) for a in (0,math.pi/2,math.pi,3*math.pi/2,math.pi/4))
        door=delta<math.asin(.30)
        block(sub,aa,p,'MC_S22_Hub_Wall_'+str(i),t,(20,0,750 if door else 500),(40,105,500 if door else 1000),'Wall')
        block(sub,aa,p,'MC_S22_Hub_OuterWall_'+str(i),t,(55,0,750 if door else 500),(30,110,500 if door else 1080),'Dark')
    room_zone(sub,aa,p,'Hub',hub,2000,2000,1000,[floor])
    corridors=[]
    for name,r in rooms.items():
        t,w,d,h=r['t'],r['w'],r['d'],r['h']
        if name=='SeedVault':
            shell(sub,aa,p,name,t,w,d,h,{'S':[(0,400,0,400)]})
            end=t.transform_location(u.Vector(0,-1200,0))
        else:
            holes={'S':[(0,600,0,500)],'N':[(250,700,400,650),(-400,300,300,650)]}
            if name=='B4':holes['E']=[(1800,600,0,500),(0,400,0,400)]
            floors=shell(sub,aa,p,name,t,w,d,h,holes,floor=name!='B3',z0=-200 if name=='B3' else 0)
            if name=='B3':
                floors=[block(sub,aa,p,'MC_S22_B3_EntryPlatform',t,(0,-2100,-20),(3600,600,40),'Dark'),
                        block(sub,aa,p,'MC_S22_B3_EndPlatform',t,(0,2100,-20),(3600,600,40),'Dark'),
                        block(sub,aa,p,'MC_S22_B3_PoolBottom',t,(0,0,-220),(3600,3600,40),'Wall')]
                for x in (-1200,-400,400,1200):
                    for y in (-1200,-400,400,1200):
                        floors.append(block(sub,aa,p,'MC_S22_B3_Pier_'+str(x)+'_'+str(y),t,(x,y,-100),(400,400,200),'Wall'))
                        block(sub,aa,p,'MC_S22_B3_Column_'+str(x)+'_'+str(y),t,(x,y,h/2),(60,60,h),'Wall')
                # Keep old ice and mechanics intact; only its structural surroundings are rebuilt.
                poolframe=u.Transform(location=t.transform_location(u.Vector(0,0,-200)),rotation=t.rotation.rotator(),scale=u.Vector(1,1,1))
                room_zone(sub,aa,p,name,poolframe,w,d,h+200,floors)
            else:
                spacing=1200 if name=='B1' else 1000 if name=='B4' else 1200
                for x in (-1200,0,1200):
                    for y in range(-1800,1801,spacing):
                        if name=='B1' and abs(x)<900 and abs(y)<900:continue
                        block(sub,aa,p,'MC_S22_'+name+'_Column_'+str(x)+'_'+str(y),t,(x,y,h/2),(60,60,h),'Wall')
            # Airlock and control room retain the wing's local radial frame.
            air=u.Transform(location=t.transform_location(u.Vector(0,-2940,0)),rotation=t.rotation.rotator(),scale=u.Vector(1,1,1))
            shell(sub,aa,p,name+'_Airlock',air,800,1000,500,{'N':[(0,600,0,500)],'S':[(0,600,0,500)]})
            control=u.Transform(location=t.transform_location(u.Vector(0,2840,300)),rotation=t.rotation.rotator(),scale=u.Vector(1,1,1))
            shell(sub,aa,p,name+'_Control',control,1200,800,400,{'S':[(250,700,100,350),(-400,300,0,350)]})
            # Raised control landing and a continuous six-metre ramp.
            rampstart=t.transform_location(u.Vector(-400,1800,0));rampend=t.transform_location(u.Vector(-400,2400,300))
            ramp=corridor(sub,aa,p,name+'_ControlRamp',rampstart,rampend,t.rotation.get_axis_y(),t.rotation.get_axis_y(),300,400)
            # Ramp walls/roof are not appropriate inside the open hall.
            for label,a in list(aa.items()):
                if label.startswith('MC_S22_'+name+'_ControlRamp_') and not label.endswith('_Floor'):
                    sub.destroy_actor(a);aa.pop(label)
            block(sub,aa,p,'MC_S22_'+name+'_ControlApproach',t,(-400,2420,280),(300,160,40),'Dark')
            zone=aa['MC_'+name+'_Zone'];setp(zone,floor_actors=list(zone.get_editor_property('floor_actors'))+ramp['floors']+[aa['MC_S22_'+name+'_ControlApproach']])
            for sx in (-750,-50):block(sub,aa,p,'MC_S22_'+name+'_RampPillar_'+str(sx),t,(sx,1850,h/2),(60,60,h),'Wall')
            end=air.transform_location(u.Vector(0,-500,0))
        direction=(end-hub.translation);direction=(direction-hub.rotation.get_axis_z()*direction.dot(hub.rotation.get_axis_z())).normal()
        # Begin 0.6m inside the circular wall; finite-width mouths then meet without a gap.
        start=hub.translation+direction*940
        corridors.append(corridor(sub,aa,p,'Hall_'+name,start,end,direction,t.rotation.get_axis_y(),600,500))
        # Move the existing vault access actor into the new hub-to-vault doorway.
        if name=='SeedVault':
            door=aa['MC_SeedVault_Door'];attach(door,u.Transform(location=t.transform_location(u.Vector(0,-1200,0)),rotation=t.rotation.rotator(),scale=door.get_actor_scale3d()),p)
            if 'MC_R_Vault_Frame' in aa:attach(aa['MC_R_Vault_Frame'],u.Transform(location=t.transform_location(u.Vector(0,-1200,0)),rotation=t.rotation.rotator(),scale=aa['MC_R_Vault_Frame'].get_actor_scale3d()),p)
            # The old cube blocker has its thin dimension on X, unlike the authored vault mesh.
            arena=aa['MC_Tuantuan_ArenaDoor']
            attach(arena,u.Transform(location=t.transform_location(u.Vector(0,-1200,200)),rotation=(t.rotation*u.Rotator(0,90,0).quaternion()).rotator(),scale=arena.get_actor_scale3d()),p)
    # Close the deferred tunnel and worm openings with independent removable structural caps.
    b4=rooms['B4']['t']
    block(sub,aa,p,'MC_S22_B4_TunnelCap',b4,(1875,1800,250),(30,600,500),'Dark')
    block(sub,aa,p,'MC_S22_B4_WormCap',b4,(1875,0,200),(30,400,400),'Wall')
    shortcut=aa['MC_B4_Shortcut'];attach(shortcut,u.Transform(location=b4.transform_location(u.Vector(1650,0,100)),rotation=b4.rotation.rotator(),scale=shortcut.get_actor_scale3d()),p)
    pitpos=ground(p,b4.transform_location(u.Vector(2400,0,0)))
    pitt=radial_frame(p,pitpos,b4.rotation.get_axis_y())
    mesh_actor(sub,aa,p,'MC_S22_SurfaceCollapsePit',u.load_asset(ROOT+'/SM_MP_Station_CollapsePit'),pitt,folder='Solar')
    attach(aa['MC_Soil_Exit'],u.Transform(location=pitt.transform_location(u.Vector(0,-350,110)),rotation=pitt.rotation.rotator(),scale=u.Vector(1,1,1)),p)
    # Paired elevator destinations remain the existing actors, with unchanged conditions/IDs.
    attach(aa['MC_Hub_Arrival'],u.Transform(location=hub.transform_location(u.Vector(0,0,110)),rotation=hub.rotation.rotator(),scale=u.Vector(1,1,1)),p)
    attach(aa['MC_Hub_Elevator'],u.Transform(location=hub.transform_location(u.Vector(-300,-300,0)),rotation=hub.rotation.rotator(),scale=aa['MC_Hub_Elevator'].get_actor_scale3d()),p)
    for name,r in rooms.items():
        for label in ('MC_'+name+'_Light',):
            if label in aa:aa[label].set_actor_location(r['t'].transform_location(u.Vector(0,0,r['h']-100)),False,False)
    for oldname,newname in [('West','B1'),('East','B2'),('North','B3'),('South','B4'),('Vault','SeedVault')]:
        label='MC_Hall_'+oldname+'_Zone'
        if label in aa:sub.destroy_actor(aa.pop(label))
        c=next(c for c in corridors if c['name']=='Hall_'+newname)
        for suffix in ('Light','Sign'):
            a=aa.get('MC_Hall_'+oldname+'_'+suffix)
            if a:
                point=(u.Vector(*c['start'])+u.Vector(*c['end']))/2
                a.set_actor_location(point+p.get_radial_up_vector(point)*(400 if suffix=='Light' else 200),False,False)
    assert planet_signature(p)==signature,'Planet changed'
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    result=dict(moved=moved,warnings=warnings,removed_shell_parts=len(removed),corridors=[{k:v for k,v in r.items() if k!='floors'} for r in corridors],planet=signature)
    (SAVED/'MarsStationInteriorAuthoring.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    return dict(moved=len(moved),removed_shell_parts=len(removed),warnings=warnings,
                corridors=[{k:v for k,v in r.items() if k not in ('floors','points')} for r in corridors])

def trace(world,a,b,ignore=(),radius=0):
    if radius:
        hit=u.SystemLibrary.sphere_trace_single(world,a,b,radius,u.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False,list(ignore),u.DrawDebugTrace.NONE,True)
    else:
        hit=u.SystemLibrary.line_trace_single(world,a,b,u.TraceTypeQuery.TRACE_TYPE_QUERY1,
            False,list(ignore),u.DrawDebugTrace.NONE,True)
    return hit.to_tuple() if hit else None

def panel_geometry(mesh):
    description=mesh.get_static_mesh_description(0);normals=[];area=0
    for i in range(description.get_triangle_count()):
        # UE 5.8's wrapper preallocates three entries, then Algo::Copy appends the actual IDs.
        ids=list(description.get_triangle_vertices(u.TriangleID(i)))[-3:]
        a,b,c=[description.get_vertex_position(v) for v in ids]
        cross=(b-a).cross(c-a);triangle_area=cross.length()/2
        # Only the broad plate faces exceed 10m²; cells, frames and supports are smaller.
        if triangle_area<=100000:continue
        normal=cross.normal()
        if normal.z<0:normal=u.Vector(-normal.x,-normal.y,-normal.z)
        normals.append(normal);area+=triangle_area
    return normals,area/20000

def audit():
    _,aa,p=context();hub,rooms=layout(aa,p)
    world=p.get_world();rows=[];errors=[]
    measured_rooms=dict(Hub=dict(t=hub,w=2000,d=2000,h=1000,depth=2800),**rooms)
    for name,r in rooms.items():
        if name=='SeedVault':continue
        for suffix,offset,w,d,h in [('Airlock',(0,-2940,0),800,1000,500),('Control',(0,2840,300),1200,800,400)]:
            measured_rooms[name+'_'+suffix]=dict(t=u.Transform(location=r['t'].transform_location(u.Vector(*offset)),rotation=r['t'].rotation.rotator(),scale=u.Vector(1,1,1)),w=w,d=d,h=h)
    for name,r in measured_rooms.items():
        t=r['t'];point=t.transform_location(u.Vector(1550 if name!='Hub' and name!='SeedVault' else 400,-2150 if name not in ('Hub','SeedVault') else -400,200))
        if name.endswith('Airlock'):point=t.transform_location(u.Vector(350,-400,200))
        if name.endswith('Control'):point=t.transform_location(u.Vector(-580,300,200))
        ignore=[a for label,a in aa.items() if not label.startswith('MC_S22_'+name+'_')]
        hits=[];coords=[]
        for v in (u.Vector(1,0,0),u.Vector(-1,0,0),u.Vector(0,1,0),u.Vector(0,-1,0),u.Vector(0,0,1),u.Vector(0,0,-1)):
            result=trace(world,point,point+direction(t,v)*20000,ignore)
            hits.append(result[9].get_actor_label() if result and result[9] else None)
            coords.append(t.inverse_transform_location(result[4]) if result else None)
        if not all(hits):errors.append(name+': six-direction enclosure ray failed')
        size=[(coords[0].x-coords[1].x)/100,(coords[2].y-coords[3].y)/100,(coords[4].z-coords[5].z)/100] if all(coords) else []
        if name=='Hub':
            diameters=[];center=t.transform_location(u.Vector(0,0,200))
            for angle in (math.pi/8,5*math.pi/8):
                axis=direction(t,u.Vector(math.cos(angle),math.sin(angle),0))
                ends=[trace(world,center,center+axis*sign*2000,ignore)[4] for sign in (-1,1)]
                diameters.append((ends[1]-ends[0]).length()/100)
            size=diameters+[size[2]]
        expected=[r['w']/100,r['d']/100,r['h']/100]
        if size and any(abs(a-b)>.5 for a,b in zip(size,expected)):errors.append(name+': net dimensions differ '+str(size))
        cover=roof_cover(p,t,r['w'],r['d'],r['h'])/100
        rows.append(dict(room=name,world_floor_center_cm=vec(t.translation),radial_up=vec(t.rotation.get_axis_z()),expected_net_m=expected,measured_net_m=[round(v,3) for v in size],
            floor_depth_m=round((ground(p,t.translation)-t.translation).length()/100,3),
            roof_cover_m=round(cover,3),radial_alignment_deg=round(math.degrees(math.acos(max(-1,min(1,t.rotation.get_axis_z().dot(p.get_radial_up_vector(t.translation)))))),4),
            six_direction_hits=hits))
    # Sample opaque roof panels themselves, including the raised control rooms and curved corridors.
    exposed=[];roof_min=1e9;geometry_cover=1e9;geometry_samples=0
    for label,a in aa.items():
        if not label.startswith('MC_S22_') or label=='MC_S22_SurfaceCollapsePit':continue
        component=a.get_component_by_class(u.StaticMeshComponent)
        if not component or not component.static_mesh:continue
        bounds=component.static_mesh.get_bounding_box();t=a.get_actor_transform()
        for x in (bounds.min.x,bounds.max.x):
            for y in (bounds.min.y,bounds.max.y):
                for z in (bounds.min.z,bounds.max.z):
                    q=t.transform_location(u.Vector(x,y,z));surface=ground(p,q)
                    cover=(surface-q).dot(p.get_radial_up_vector(q))/100
                    geometry_cover=min(geometry_cover,cover);geometry_samples+=1
                    if cover<0:exposed.append(dict(label=label,cover_m=cover))
    for label,a in aa.items():
        if not label.startswith('MC_S22_') or not label.endswith('OuterRoof'):continue
        component=a.get_component_by_class(u.StaticMeshComponent)
        if not component or not component.static_mesh:continue
        bounds=component.static_mesh.get_bounding_box();t=a.get_actor_transform()
        for x in (bounds.min.x,0,bounds.max.x):
            for y in (bounds.min.y,0,bounds.max.y):
                q=t.transform_location(u.Vector(x,y,bounds.max.z));surface=ground(p,q)
                cover=(surface-q).dot(p.get_radial_up_vector(q))/100;roof_min=min(roof_min,cover)
                if cover<0:exposed.append(dict(label=label,cover_m=cover))
    if exposed:errors.append('Underground roof exposed at '+str(len(exposed))+' samples')
    if roof_min<8:errors.append('Roof cover below 8m')
    # Separating-axis lower bound on rock thickness between independent room envelopes.
    envelopes={}
    for name,r in measured_rooms.items():
        t=r['t'];ext=u.Vector(r['w']/2+70,r['d']/2+70,r['h']/2+70)
        center=t.transform_location(u.Vector(0,0,r['h']/2))
        envelopes[name]=(center,[t.rotation.get_axis_x(),t.rotation.get_axis_y(),t.rotation.get_axis_z()],[ext.x,ext.y,ext.z])
    clearances=[]
    names=list(envelopes)
    for i,name in enumerate(names):
        ca,axes_a,ea=envelopes[name]
        for other in names[i+1:]:
            if name.split('_')[0]==other.split('_')[0]:continue # connected airlock/main/control assembly
            cb,axes_b,eb=envelopes[other];axes=axes_a+axes_b
            axes += [a.cross(b).normal() for a in axes_a for b in axes_b if a.cross(b).length()>.001]
            gap=max(abs((cb-ca).dot(axis))-sum(abs(axis.dot(a))*e for a,e in zip(axes_a,ea))-sum(abs(axis.dot(a))*e for a,e in zip(axes_b,eb)) for axis in axes)
            clearances.append(dict(a=name,b=other,conservative_rock_m=round(gap/100,3)))
            if gap<400:errors.append(name+'/'+other+': rock clearance below 4m')
    solarbase,centers=solar_layout(aa,p)
    landing=solarbase.inverse_transform_location(aa['MC_Station_Landing'].get_actor_location())
    sector_checks=[]
    for i,(x,y) in enumerate(centers):
        # Entire rectangular footprint against the landing disk and the reserved route line.
        landing_gap=math.hypot(max(0,abs(landing.x-x)-3300),max(0,abs(landing.y-y)-2100))/100-15
        sector_checks.append(dict(sector=chr(65+i),landing_clearance_m=round(landing_gap,3)))
        if landing_gap<0:errors.append(chr(65+i)+': landing pad overlaps sector')
    boundaries=[]
    for cx,cy in centers:
        edge=[]
        for i in range(41):
            for x,y in ((-3300,2100*(i/20-1)),(3300,2100*(i/20-1)),(3300*(i/20-1),-2100),(3300*(i/20-1),2100)):
                edge.append(ground(p,solarbase.transform_location(u.Vector(cx+x,cy+y,0))))
        boundaries.append(edge)
    sector_gaps=[]
    for i in range(3):
        for j in range(i+1,3):
            gap=min((a-b).length() for a in boundaries[i] for b in boundaries[j])/100
            sector_gaps.append(dict(a=chr(65+i),b=chr(65+j),clearance_m=round(gap,3)))
            if gap<12:errors.append('Solar sector separation below 12m')
    route=aa['MC_Noctis_Waypoint'].get_actor_location()-solarbase.translation
    up=p.get_radial_up_vector(solarbase.translation);route=(route-up*route.dot(up)).normal()
    route_points=[solarbase.inverse_transform_location(ground(p,solarbase.translation+route*i*100)) for i in range(1,201)]
    pit=solarbase.inverse_transform_location(aa['MC_S22_SurfaceCollapsePit'].get_actor_location())
    for row,(cx,cy) in zip(sector_checks,centers):
        row['noctis_route_clearance_m']=round(min(math.hypot(max(0,abs(q.x-cx)-3300),max(0,abs(q.y-cy)-2100)) for q in route_points)/100,3)
        row['collapse_pit_clearance_m']=round(math.hypot(max(0,abs(pit.x-cx)-3300),max(0,abs(pit.y-cy)-2100))/100-2.5,3)
        if row['noctis_route_clearance_m']<=0 or row['collapse_pit_clearance_m']<=0:errors.append(row['sector']+': surface reserved area overlaps')
    # Measure the four cardinal corridor centre cross-sections against actual collision.
    corridor_rows=[]
    for name in rooms:
        floors=sorted([a for label,a in aa.items() if label.startswith('MC_S22_Hall_'+name+'_') and label.endswith('_Floor')],key=lambda a:int(a.get_actor_label().split('_')[-2]))
        widths=[];heights=[];depths=[]
        ignore=[a for label,a in aa.items() if not label.startswith('MC_S22_Hall_'+name+'_')]
        for a in floors:
            t=a.get_actor_transform();center=t.translation+t.rotation.get_axis_z()*220
            floor=t.translation+t.rotation.get_axis_z()*20
            depths.append((ground(p,floor)-floor).length()/100)
            hits=[trace(world,center,center+direction(t,v)*1000,ignore) for v in (u.Vector(1,0,0),u.Vector(-1,0,0),u.Vector(0,0,1),u.Vector(0,0,-1))]
            if all(hits):
                widths.append((hits[0][4]-hits[1][4]).length()/100)
                heights.append((hits[2][4]-hits[3][4]).length()/100)
        if not widths or max(abs(v-6) for v in widths)>.5 or max(abs(v-5) for v in heights)>.5:errors.append(name+': corridor cross-section')
        corridor_rows.append(dict(room=name,width_range_m=[min(widths),max(widths)] if widths else [],height_range_m=[min(heights),max(heights)] if heights else [],floor_depth_range_m=[min(depths),max(depths)] if depths else []))
    door_rows=[]
    for name,r in rooms.items():
        t=r['t'];tests=[('Entry',(0,-1220,200),400,400,'X')] if name=='SeedVault' else [
            ('HallEntry',(0,-2420,250),600,500,'X'),('AirlockOuter',(0,-3460,250),600,500,'X'),
            ('ControlDoor',(-400,2420,475),300,350,'X'),('ControlWindow',(250,2420,525),700,250,'X')]
        if name=='B4':tests += [('TunnelReserve',(1820,1800,250),600,500,'Y'),('WormReserve',(1820,0,200),400,400,'Y')]
        ignore=[a for label,a in aa.items() if not label.startswith('MC_S22_'+name+'_')]
        for label,local,width,height,axis in tests:
            point=t.transform_location(u.Vector(*local));side=t.rotation.get_axis_x() if axis=='X' else t.rotation.get_axis_y();up=t.rotation.get_axis_z()
            # Explicit copies: Unreal's Python unary Vector negation mutates the struct wrapper.
            opposite_side=u.Vector(-side.x,-side.y,-side.z);opposite_up=u.Vector(-up.x,-up.y,-up.z)
            hits=[trace(world,point,point+v*2000,ignore) for v in (side,opposite_side,up,opposite_up)]
            size=[(hits[0][4]-hits[1][4]).length()/100,(hits[2][4]-hits[3][4]).length()/100] if all(hits) else []
            expected=[width/100,height/100]
            if not size or any(abs(a-b)>.5 for a,b in zip(size,expected)):errors.append(name+'/'+label+': opening dimensions '+str(size))
            threshold=t.transform_location(u.Vector(local[0],local[1],local[2]-height/2))
            door_rows.append(dict(room=name,opening=label,expected_m=expected,measured_m=[round(v,3) for v in size],floor_depth_m=round((ground(p,threshold)-threshold).length()/100,3),world_cm=vec(point),hits=[h[9].get_actor_label() if h and h[9] else None for h in hits]))
    films=[]
    for label,a in aa.items():
        if not label.startswith('MC_S25_Film_'):continue
        t=a.get_actor_transform();gaps=[]
        ignore=[v for v in aa.values() if v!=a]
        for x in (-90,-30,30,90):
            for y in range(-275,276,50):
                q=t.transform_location(u.Vector(x,y,0));g=ground(p,q);up=p.get_radial_up_vector(g)
                hit=trace(world,g+up*100,g-up*50,ignore)
                if hit:gaps.append((hit[4]-g).dot(up)/100)
        if len(gaps)!=48 or (gaps and (min(gaps)<-.1 or max(gaps)>.3)):errors.append(label+': film does not follow surface')
        films.append(dict(label=label,surface_hits=len(gaps),min_m=min(gaps) if gaps else None,max_m=max(gaps) if gaps else None))
    instance=aa['MC_S25_TrackerInstances'].get_component_by_class(u.InstancedStaticMeshComponent)
    if instance.get_instance_count()!=36:errors.append('Tracker instance count')
    tracker_normals,tracker_area=panel_geometry(instance.static_mesh)
    canopy_normals,canopy_area=panel_geometry(aa['MC_S25_Canopy_A'].static_mesh_component.static_mesh)
    if len(tracker_normals)!=4 or len(canopy_normals)!=32:errors.append('Unexpected broad plate geometry')
    slopes=[]
    for i in range(instance.get_instance_count()):
        t=instance.get_instance_transform(i,world_space=True)
        up=p.get_radial_up_vector(t.translation)
        for local_normal in tracker_normals:
            normal=direction(t,local_normal)
            slopes.append(math.degrees(math.acos(max(-1,min(1,normal.dot(up))))))
    canopy_slopes=[];headings=[]
    heading=solarbase.rotation.get_axis_y()
    transforms=[instance.get_instance_transform(i,world_space=True) for i in range(instance.get_instance_count())]
    for sector in 'ABC':
        t=aa['MC_S25_Canopy_'+sector].get_actor_transform();transforms.append(t)
        for local_normal in canopy_normals:
            canopy_slopes.append(math.degrees(math.acos(max(-1,min(1,direction(t,local_normal).dot(p.get_radial_up_vector(t.translation)))))))
    if any(abs(v-25)>.1 for v in slopes+canopy_slopes):errors.append('Authored plate tilt differs from 25 degrees')
    for t in transforms:
        up=p.get_radial_up_vector(t.translation);projected=(heading-up*heading.dot(up)).normal()
        headings.append(math.degrees(math.acos(max(-1,min(1,t.rotation.get_axis_y().dot(projected))))))
    shared_materials={}
    for label in ['MC_S25_TrackerInstances','MC_S25_Canopy_A','MC_S25_Film_A_0_0']:
        a=aa[label];component=a.get_component_by_class(u.InstancedStaticMeshComponent) if label.endswith('Instances') else a.static_mesh_component
        shared_materials[label]={str(slot.material_slot_name):slot.material_interface.get_path_name() for slot in component.static_mesh.get_editor_property('static_materials') if 'SolarDust' in str(slot.material_slot_name)}
    if any(not slots or any(v!=ROOT+'/M_MP_SolarDust.M_MP_SolarDust' for v in slots.values()) for slots in shared_materials.values()):errors.append('Panel dust material mismatch')
    instance_flags={name:bool(u.load_asset('/Game/Space/Environment/MarsSystem/Chapter/Materials/M_Mars_'+name).get_editor_property('used_with_instanced_static_meshes')) for name in ('Orange','White','Alloy','Dark')}
    if not all(instance_flags.values()):errors.append('Frame/cable material missing instancing usage')
    before=json.loads((SAVED/'MarsStationStructureBefore.json').read_text(encoding='utf8'))
    expected_transits={r['label']:r['destination'] for r in before['actors'] if 'destination' in r}
    actual_transits={label:a.get_editor_property('destination').get_actor_label() if a.get_editor_property('destination') else None for label,a in aa.items() if isinstance(a,u.JTSInteriorTransit)}
    if any(actual_transits.get(k)!=v for k,v in expected_transits.items()):errors.append('Transit pairing changed')
    retained=[r for r in before['actors'] if r['cls'].startswith(('JTSReadable','JTSInteriorTransit','JTSFacility','BP_Key_','JTSSecurity','JTSBrittle','JTSRecovery')) and not r['label'].startswith('MC_DustSweep')]
    missing=[r['label'] for r in retained if r['label'] not in aa]
    if missing:errors.append('Missing legacy actors '+str(missing))
    budgets=[]
    mesh_paths=[ROOT+'/SM_MP_Solar_'+name for name in ('Tracker','Canopy','RollOut','RollBox')]
    mesh_paths += [ROOT+'/ProjectedFilm/SM_MP_Film_'+s+'_'+str(i)+'_'+str(j) for s in 'ABC' for i in range(6) for j in range(4)]
    for path in mesh_paths:
        mesh=u.load_asset(path);triangles=mesh.get_num_triangles(0);limit=3000 if path.endswith('Tracker') else 6000 if path.endswith('Canopy') else 600 if path.endswith('RollBox') else 200
        hulls=u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_convex_collision_count(mesh)
        budgets.append(dict(asset=path,triangles=triangles,limit=limit,collision_hulls=hulls))
        if triangles>limit or hulls<1:errors.append(path+': mesh budget or simple collision')
    radial_up=hub.rotation.get_axis_z();noctis=aa['MC_Noctis_Waypoint'].get_actor_location()-hub.translation
    noctis=(noctis-radial_up*noctis.dot(radial_up)).normal();b4_direction=rooms['B4']['t'].translation-hub.translation
    b4_direction=(b4_direction-radial_up*b4_direction.dot(radial_up)).normal()
    result=dict(errors=errors,rooms=rows,openings=door_rows,corridor_cross_sections=corridor_rows,film_surface_gaps=films,rock_clearances=clearances,minimum_roof_cover_m=round(roof_min,3),minimum_geometry_cover_m=round(geometry_cover,3),geometry_surface_samples=geometry_samples,asset_budgets=budgets,b4_noctis_heading_delta_deg=math.degrees(math.acos(max(-1,min(1,noctis.dot(b4_direction))))),
        measured_module_panel_area_m2=dict(tracker=tracker_area,canopy=canopy_area),
        exposed_roofs=exposed,sector_exclusion_checks=sector_checks,sector_clearances=sector_gaps,tracker_instances=instance.get_instance_count(),
        measured_canopy_tilt_range_deg=[min(canopy_slopes),max(canopy_slopes)],maximum_shared_heading_error_deg=max(headings),panel_materials=shared_materials,instance_material_flags=instance_flags,
        transit_pairs=actual_transits,missing_legacy_interactions=missing,
        measured_tracker_tilt_range_deg=[min(slopes),max(slopes)],
        power_requirements=[str(v.id) for v in aa['MC_Power'].conditions.get_editor_property('requirements')],
        retired_dust_remaining=[k for k in aa if k.startswith('MC_DustSweep')],
        combiner_followups=[str(aa['MC_Sector_'+s].get_editor_property('followup_flag')) for s in 'ABC'],planet=planet_signature(p),
        editor_state=dict(save_prefix=str(u.get_default_object(u.JTSExpeditionSubsystem).get_editor_property('save_slot_prefix')),
            dirty_maps=[v.get_name() for v in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
            dirty_content=[v.get_name() for v in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]))
    (SAVED/'MarsStationStructureAudit.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    return result

def view():
    _,aa,p=context();key=(SAVED/'MarsStructureView.txt').read_text().strip() if (SAVED/'MarsStructureView.txt').exists() else 'surface'
    hub,rooms=layout(aa,p)
    if key=='surface':
        t=aa['MC_R_Lift_Housing'].get_actor_transform();pos=t.transform_location(u.Vector(13000,-11000,12500));aim=t.translation
    elif key=='SolarA':
        t=aa['MC_S25_Film_A_3_2'].get_actor_transform();pos=t.transform_location(u.Vector(-1400,-2000,1000));aim=t.translation
    else:
        t=hub if key=='Hub' else rooms[key]['t'];pos=t.transform_location(u.Vector(1200,-2100,250));aim=t.transform_location(u.Vector(0,2000,400))
    u.EditorLevelLibrary.set_level_viewport_camera_info(pos,u.MathLibrary.make_rot_from_xz(aim-pos,p.get_radial_up_vector(pos)))
    u.EditorLevelLibrary.editor_set_game_view(True)
    return dict(view=key,position=vec(pos))

def prepare():
    defaults=u.get_default_object(u.JTSExpeditionSubsystem)
    settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))

    previous=dict(prefix=str(defaults.get_editor_property('save_slot_prefix')),slot=str(defaults.get_editor_property('save_slot_name')),clients=settings.get_editor_property('PlayNumberOfClients'),one_process=settings.get_editor_property('RunUnderOneProcess'))
    if not (SAVED/'MarsStructurePIEPrevious.json').exists():(SAVED/'MarsStructurePIEPrevious.json').write_text(json.dumps(previous),encoding='utf8')
    setp(defaults,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation')
    setp(settings,PlayNumberOfClients=2,RunUnderOneProcess=True)
    return dict(clients=2,isolated_save_prefix='MarsAccessValidation')

def probe():
    import runpy
    action=(SAVED/'StationFlowAction.txt').read_text(encoding='utf8').strip()
    if action.startswith('structure_'):
        result=runpy.run_path(str(Path(__file__).with_name('probe_mars_station_structure.py')))['run'](action[10:])
        (SAVED/('MarsStructurePIE_'+action+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
        return result
    result=runpy.run_path(str(Path(__file__).with_name('rebuild_mars_station_flow.py')))['probe']()
    action=result.get('action','status')
    (SAVED/('MarsStructurePIE_'+action+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    return {k:v for k,v in result.items() if k in ('action','playing','players','flags','gates','worlds','new_slot','can_scan','can_swipe','can_read','can_claim','can_repair','can_open','can_transit','saved_loadable','travel_requested','requested','interaction','item')}

def close():
    sub,aa,p=context();hub,rooms=layout(aa,p)
    vault=rooms['SeedVault']['t'];arena=aa['MC_Tuantuan_ArenaDoor']
    attach(arena,u.Transform(location=vault.transform_location(u.Vector(0,-1200,200)),rotation=(vault.rotation*u.Rotator(0,90,0).quaternion()).rotator(),scale=arena.get_actor_scale3d()),p)
    status=aa['MC_R_Hub_Status'];t=status.get_actor_transform();q=hub.inverse_transform_location(t.translation)
    t.translation=hub.transform_location(u.Vector(-650,650,q.z));attach(status,t,p)
    b3=rooms['B3']['t'];zone=aa['MC_B3_Zone']
    zone.set_actor_location(b3.transform_location(u.Vector(0,0,400)),False,False);zone.bounds.set_box_extent(u.Vector(1825,2425,650))
    for label in ('MC_R_LogStand_MC_Log_B3','MC_R_B3_Desk','MC_R_Supervisor_Desk3','MC_SupervisorRemains_3'):
        if label not in aa:continue
        actor=aa[label];t=actor.get_actor_transform();q=b3.inverse_transform_location(t.translation)
        t.translation=b3.transform_location(u.Vector(max(-1400,min(1400,q.x)),-2100,q.z));attach(actor,t,p)
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    previous_path=SAVED/'MarsStructurePIEPrevious.json'
    if previous_path.exists():
        previous=json.loads(previous_path.read_text(encoding='utf8'))
        defaults=u.get_default_object(u.JTSExpeditionSubsystem)
        setp(defaults,save_slot_prefix=previous['prefix'],save_slot_name=previous['slot'])
        settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        setp(settings,PlayNumberOfClients=previous['clients'],RunUnderOneProcess=previous['one_process'])
        previous_path.unlink()
    # Explicit one-shot restart request, also usable with an already registered older MCP entry point.
    restart=SAVED/'MarsStationRestart.request'
    if restart.exists():
        restart.unlink()
        return shutdown()
    return dict(restored=True)

def shutdown():
    _,_,planet=context()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Unsaved map'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Unsaved assets'
    def quit_after_response(delta):
        u.unregister_slate_post_tick_callback(handle)
        u.SystemLibrary.execute_console_command(planet.get_world(),'QUIT_EDITOR')
    handle=u.register_slate_post_tick_callback(quit_after_response)
    return dict(saved=True,restart_required=True)



def power_probe():
    from editor_toolset.toolsets.blueprint import BlueprintTools as B
    action=json.loads((SAVED/'MarsPowerAction.json').read_text(encoding='utf8'))
    if action['action'] in ('runtime','runtime_prepare','runtime_restore'):return power_runtime(action)
    if action['action']=='instance_materials':
        instance_materials();return dict(saved=True)
    if action['action']=='capture_messages':
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        u.SystemLibrary.execute_console_command(world,'EnableAllScreenMessages' if action.get('enabled',True) else 'DisableAllScreenMessages')
        return dict(screen_messages=action.get('enabled',True))
    if action['action']=='refresh_meshes':
        source=Path(u.Paths.project_dir())/'SourceArt/MarsStation/Power'
        names=action['names']
        u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'Interchange.FeatureFlags.Import.FBX 0')
        if 'SM_MP_Power_CoreModule' in names:power_core_label()
        for name in names:
            mesh=import_power_mesh(source,name)
            for i,slot in enumerate(mesh.get_editor_property('static_materials')):mesh.set_material(i,u.load_asset(ROOT+'/M_MP_Power'+str(slot.material_slot_name).replace('Power_','')))
            u.EditorAssetLibrary.save_loaded_asset(mesh,False)
        return dict(imported=names)
    if action['action']=='finish_assets':
        from editor_toolset.toolsets.actor import ActorTools
        item=u.load_asset('/Game/Space/Data/Items/DA_Item_CombinerCoreModule')
        held=setp(item.get_editor_property('held_presentation'),grip_transform=u.Transform(location=u.Vector(0,0,28.3),rotation=u.Rotator(pitch=90)))
        setp(item,drop_on_death=True,held_presentation=held);u.EditorAssetLibrary.save_loaded_asset(item,False)
        bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_SolarCombiner');cdo=u.get_default_object(bp.generated_class());cdo.status_light.set_relative_scale3d(u.Vector(1,1,1));setp(cdo,status_activation_delay=1.5)
        for component in ActorTools.get_components(cdo):
            if component.get_name().startswith('CoreDiscarded'):component.set_relative_location(u.Vector(78,35,24),False,False)
        u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,False)
        power_material('Red',(1,.008,.003),0,3,True)
        return dict(drop_on_death=True,stack=3)
    if action['action']=='presentation':return power_presentation()
    if action['action']=='visual_inspect':
        _,aa,p=context()
        return {k:[dict(name=c.get_name(),loc=vec(c.get_editor_property('relative_location')),scale=vec(c.get_editor_property('relative_scale3d')),world_local=vec(aa[k].get_actor_transform().inverse_transform_location(c.get_world_location())),materials=[m.get_path_name() if m else None for m in c.get_materials()],rot=str(c.get_editor_property('relative_rotation')),visible=c.is_visible(),hidden=c.get_editor_property('hidden_in_game'),bounds=str(c.static_mesh.get_bounding_box()) if c.static_mesh else None) for c in aa[k].get_components_by_class(u.StaticMeshComponent)] for k in ('MC_Sector_A','MC_CoreSupplyRack')}
    if action['action']=='module_materials':
        mesh=u.load_asset(ROOT+'/SM_MP_Power_CoreModule')
        return dict(triangles=mesh.get_num_triangles(0),uv_channels=u.get_editor_subsystem(u.StaticMeshEditorSubsystem).get_num_uv_channels(mesh,0),materials=[dict(slot=str(s.material_slot_name),material=s.material_interface.get_path_name() if s.material_interface else None) for s in mesh.get_editor_property('static_materials')])
    if action['action']=='materials':
        for key,color,metal in [('White',(.72,.76,.77),0),('Orange',(.9,.19,.026),0),('Metal',(.25,.3,.32),.7),('Concrete',(.26,.26,.25),0),('Gold',(.62,.37,.055),.7),('Dark',(.024,.032,.038),0),('Yellow',(.95,.57,.018),0)]:power_material(key,color,metal,.2)
        return dict(updated=True)
    if action['action']=='graph_state':
        out={}
        for name in ('SolarCombiner','CoreSupplyRack'):
            bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_'+name)
            out[name]=dict(status=str(bp.get_editor_property('status')),nodes=[])
            for g in B.list_graphs(bp):
                for n in B.find_nodes(g):
                    info=B.get_node_infos([n])[0]
                    if any(x in info.type_id for x in ('Equal','Get','Bind','Runtime','State')):
                        out[name]['nodes'].append(dict(graph=g.get_name(),type=info.type_id,pins={d:[dict(name=p.name,type=p.type_id,value=p.value,links=len(p.connected_pins)) for p in getattr(info,d)] for d in ('input_pins','output_pins')}))
        return out
    if action['action']=='nodes':
        result={}
        for name in ('SolarCombiner','CoreSupplyRack'):
            bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_'+name);g=u.BlueprintEditorLibrary.find_event_graph(bp)
            result[name]={key:list(B.find_node_types(g,key)) for key in ('OnStateChanged','OnRuntimeActivated','OnSlotsChanged','SetRelativeRotation','SetRelativeLocation','SetVisibility','SetMaterial','Delay','IsActive','CabinetDoor','BeginPlay','Tick','Bind','Slots','Supply0','SetTimer','MakeRotator','Lerp','Clamp','MoveComponentTo','Get(aCopy)','Active')}
        (SAVED/'MarsPowerGraphNodes.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
        return result
    if action['action']=='pins':
        bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_'+action.get('bp','SolarCombiner'));g=u.BlueprintEditorLibrary.find_event_graph(bp)
        out={}
        for t in action['types']:
            info=B.get_node_type_pins(g,t)
            out[t]={d:[dict(name=p.name,type=p.type_id,value=p.value) for p in getattr(info,d)] for d in ('input_pins','output_pins')}
        (SAVED/'MarsPowerGraphPins.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf8');return out

class PowerGraph:
    """Small editor-side graph writer; gameplay remains in the native actors."""
    def __init__(self,graph):
        from editor_toolset.toolsets.blueprint import BlueprintTools
        self.b=BlueprintTools;self.g=graph;self.e=u.BlueprintGraphEditor.get_graph_editor(graph);self.x=0;self.y=0
    def info(self,n):return self.b.get_node_infos([n])[0]
    def pin(self,n,name,output=False):
        return next(p.pin_id for p in (self.info(n).output_pins if output else self.info(n).input_pins) if p.name==name)
    def link(self,a,an,b,bn):self.b.connect_pins(self.pin(a,an,True),self.pin(b,bn))
    def value(self,n,k,v):self.b.set_pin_value(self.pin(n,k),str(v).lower() if isinstance(v,bool) else str(v))
    def place(self,n):
        self.x+=300;n.set_node_pos(u.IntPoint(self.x,self.y));return n
    def native(self,path,values=None):
        n=self.place(self.e.add_call_function_node(path))
        for k,v in (values or {}).items():self.value(n,k,v)
        return n
    def function(self,name,values=None):return self.native('/Script/Engine.'+name,values)
    def get(self,name):return self.place(self.e.add_get_member_variable_node(name))
    def set(self,name,value):
        n=self.place(self.e.add_set_member_variable_node(name));self.value(n,name,value);return n
    def out(self,n):return next(p.name for p in self.info(n).output_pins if p.name!='then' and 'delegate' not in p.name.lower())
    def exec(self,previous,n,input='execute',output='then'):
        self.link(previous,output,n,input);return n
    def branch(self,previous,condition):
        n=self.place(self.e.add_branch_node());self.exec(previous,n);self.link(condition,self.out(condition),n,'Condition');return n
    def comp(self,n,pin,name):
        get=self.get(name);self.link(get,name,n,pin);return n
    def show(self,previous,name,visible):
        n=self.function('SceneComponent.SetVisibility',{'bNewVisibility':visible});self.comp(n,'self',name);return self.exec(previous,n)
    def loc(self,previous,name,value):
        n=self.function('SceneComponent.K2_SetRelativeLocation',{'NewLocation':value});self.comp(n,'self',name);return self.exec(previous,n)
    def rot(self,previous,name,value):
        n=self.function('SceneComponent.K2_SetRelativeRotation',{'NewRotation':value});self.comp(n,'self',name);return self.exec(previous,n)
    def material(self,previous,name,value):
        n=self.function('PrimitiveComponent.SetMaterial',{'ElementIndex':0,'Material':ROOT+'/M_MP_Power'+value+'.M_MP_Power'+value});self.comp(n,'self',name);return self.exec(previous,n)
    def call(self,previous,name):
        types=list(self.b.find_node_types(self.g,name));t=next(t for t in types if t.endswith('|'+name))
        n=self.place(self.b.create_node(self.g,t,u.IntPoint()));return self.exec(previous,n)
    def bind(self,previous,dispatcher):
        event=self.place(self.e.add_dispatcher_event_node(dispatcher))
        t=next(t for t in self.b.find_node_types(self.g,dispatcher) if t.endswith('|BindEventto'+dispatcher))
        bind=self.place(self.b.create_node(self.g,t,u.IntPoint()));self.link(event,'OutputDelegate',bind,'Delegate');self.exec(previous,bind)
        return bind,event
    def move(self,previous,name,loc,rot,seconds):
        t=next(t for t in self.b.find_node_types(self.g,'MoveComponentTo') if t.endswith('|MoveComponentTo'))
        n=self.place(self.b.create_node(self.g,t,u.IntPoint()));self.comp(n,'Component',name)
        for k,v in dict(TargetRelativeLocation=loc,TargetRelativeRotation=rot,OverTime=seconds,bEaseIn=True,bEaseOut=True,bForceShortestRotationPath=True).items():self.value(n,k,v)
        return self.exec(previous,n,'Move')


def power_presentation():
    from editor_toolset.toolsets.actor import ActorTools
    from editor_toolset.toolsets.blueprint import BlueprintTools as B
    root='/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_'
    results={}
    for kind in ('SolarCombiner','CoreSupplyRack'):
        bp=u.load_asset(root+kind);g=u.BlueprintEditorLibrary.find_event_graph(bp)
        # These few shared devices must not inherit relevancy from the distant planet centre.
        setp(u.get_default_object(bp.generated_class()),always_relevant=True)
        for node in B.find_nodes(g):B.delete_node(node)
        if kind=='SolarCombiner' and 'RepairAnimating' not in list(B.list_variables(bp)):B.add_variable(bp,'RepairAnimating','bool')
        # Visibility alone drives the non-colliding presentation components.
        for c in ActorTools.get_components(u.get_default_object(bp.generated_class())):
            if isinstance(c,u.StaticMeshComponent):setp(c,hidden_in_game=False)
        fg=B.add_function_graph(bp,'ApplyStoredState' if kind=='SolarCombiner' else 'RefreshRackPresentation')
        for n in B.find_nodes(fg):
            if not isinstance(n,u.K2Node_FunctionEntry):B.delete_node(n)
        q=PowerGraph(fg);entry=next(n for n in B.find_nodes(fg) if isinstance(n,u.K2Node_FunctionEntry))
        if kind=='SolarCombiner':
            anim=q.get('RepairAnimating');branch=q.branch(entry,anim)
            state=q.get('bActive');statebranch=q.place(q.e.add_branch_node());q.link(branch,'else',statebranch,'execute');q.link(state,'bActive',statebranch,'Condition')
            for active,out in ((True,'then'),(False,'else')):
                q.y+=800;stub=q.function('SceneComponent.K2_SetRelativeRotation',{'NewRotation':'0,0,0' if active else '0,55,0'});q.comp(stub,'self','CabinetDoor');q.link(statebranch,out,stub,'execute');n=stub
                n=q.show(n,'CoreBurntInSlot',not active);n=q.show(n,'CoreInstalled',active);n=q.show(n,'CoreDiscarded',active)
                n=q.loc(n,'CoreBurntInSlot','44,-45,95');n=q.rot(n,'CoreBurntInSlot','0,0,90');n=q.loc(n,'CoreInstalled','37,-45,95')
                q.material(n,'StatusLight','Green' if active else 'Red')
        else:
            n=entry
            for i in range(4):
                for comp in ('Supply'+str(i),'SlotLamp'+str(i)):n=q.show(n,comp,False)
            slots=q.get('Slots');length=q.function('KismetArrayLibrary.Array_Length');q.link(slots,'Slots',length,'TargetArray')
            ready=q.function('KismetMathLibrary.GreaterEqual_IntInt');q.link(length,'ReturnValue',ready,'A');q.value(ready,'B',4)
            n=q.branch(n,ready)
            for i in range(4):
                slots=q.get('Slots');get=q.function('KismetArrayLibrary.Array_Get')
                q.link(slots,'Slots',get,'TargetArray');q.value(get,'Index',i)
                convert=q.function('KismetMathLibrary.Conv_ByteToInt');q.link(get,'Item',convert,'InByte')
                equal=q.function('KismetMathLibrary.Greater_IntInt');q.link(convert,'ReturnValue',equal,'A');q.value(equal,'B',0)
                for comp in ('Supply'+str(i),'SlotLamp'+str(i)):
                    show=q.function('SceneComponent.SetVisibility');q.comp(show,'self',comp);q.link(equal,'ReturnValue',show,'bNewVisibility');n=q.exec(n,show)
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        q=PowerGraph(g);begin=q.place(B.add_event(bp,'ReceiveBeginPlay'))
        n,event=q.bind(begin,'OnStateChanged' if kind=='SolarCombiner' else 'OnSlotsChanged')
        if kind=='SolarCombiner':
            n,runtime=q.bind(n,'OnRuntimeActivated');q.call(event,'ApplyStoredState')
            anim=q.set('RepairAnimating',True);q.exec(runtime,anim);n=anim
            n=q.rot(n,'CabinetDoor','0,55,0');n=q.rot(n,'CoreBurntInSlot','0,0,90');n=q.loc(n,'CoreBurntInSlot','44,-45,95')
            n=q.show(n,'CoreBurntInSlot',True);n=q.show(n,'CoreInstalled',False);n=q.show(n,'CoreDiscarded',False);n=q.material(n,'StatusLight','Red')
            n=q.move(n,'CabinetDoor','44,57,15','0,105,0',.25)
            n=q.move(n,'CoreBurntInSlot','78,35,24','90,25,0',.30)
            n=q.show(n,'CoreBurntInSlot',False);n=q.show(n,'CoreDiscarded',True)
            n=q.loc(n,'CoreInstalled','85,-45,95');n=q.show(n,'CoreInstalled',True)
            n=q.move(n,'CoreInstalled','37,-45,95','0,0,90',.40)
            n=q.move(n,'CabinetDoor','44,57,15','0,0,0',.55)
            last=q.set('RepairAnimating',False);n=q.exec(n,last);q.material(n,'StatusLight','Green')
            # Bind initialization only, independent of the live replacement chain.
            q.call(next(n for n in B.find_nodes(g) if 'BindEventtoOnRuntimeActivated' in B.get_node_infos([n])[0].type_id),'ApplyStoredState')
        else:
            q.call(event,'RefreshRackPresentation');q.call(n,'RefreshRackPresentation')
        u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,False)
        results[kind]=dict(nodes=len(B.find_nodes(g)),functions=len(B.find_nodes(fg)),status=str(bp.get_editor_property('status')))
    return results

def power_audit():
    _,aa,p=context();base,centers=solar_layout(aa,p);before=json.loads((SAVED/'MarsPowerBefore.json').read_text(encoding='utf8'))
    ism=aa['MC_S25_TrackerInstances'].get_component_by_class(u.InstancedStaticMeshComponent)
    def hull(points):
        points=sorted(set((round(a,3),round(b,3)) for a,b in points))
        def cross(o,a,b):return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
        lo=[];hi=[]
        for a in points:
            while len(lo)>=2 and cross(lo[-2],lo[-1],a)<=0:lo.pop()
            lo.append(a)
        for a in reversed(points):
            while len(hi)>=2 and cross(hi[-2],hi[-1],a)<=0:hi.pop()
            hi.append(a)
        return lo[:-1]+hi[:-1]
    def polygon(t,bounds):
        points=[]
        for x in (bounds.min.x,bounds.max.x):
            for y in (bounds.min.y,bounds.max.y):
                for z in (bounds.min.z,bounds.max.z):
                    v=base.inverse_transform_location(ground(p,t.transform_location(u.Vector(x,y,z))));points.append((v.x,v.y))
        return hull(points)
    def point_segment(p,a,b):
        dx,dy=b[0]-a[0],b[1]-a[1];f=max(0,min(1,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/(dx*dx+dy*dy or 1)))
        return math.hypot(p[0]-a[0]-dx*f,p[1]-a[1]-dy*f)
    def inside(p,poly):
        return all((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0])>=0 for a,b in zip(poly,poly[1:]+poly[:1]))
    def distance(a,b):
        if any(inside(v,b) for v in a) or any(inside(v,a) for v in b):return 0
        return min([point_segment(p,c,d) for p in a for c,d in zip(b,b[1:]+b[:1])]+[point_segment(p,c,d) for p in b for c,d in zip(a,a[1:]+a[:1])])
    bounds=ism.static_mesh.get_bounding_box();polys=[];tilts=[];azimuth=[]
    normals,tracker_area=panel_geometry(ism.static_mesh)
    assert normals,'Missing measurable panel face'
    normal=normals[0]
    for i in range(ism.get_instance_count()):
        t=ism.get_instance_transform(i,world_space=True);poly=polygon(t,bounds)
        polys.append(dict(sector='ABC'[i//16],kind='Tracker',polygon=poly))
        tilts.append(math.degrees(math.acos(max(-1,min(1,normal.z)))))
        up=p.get_radial_up_vector(t.translation);expected=(base.rotation.get_axis_y()-up*base.rotation.get_axis_y().dot(up)).normal();azimuth.append(math.degrees(math.acos(max(-1,min(1,expected.dot(t.rotation.get_axis_y()))))))
    canopy_area=0;canopy_normals=[]
    for letter in 'ABC':
        a=aa['MC_S25_Canopy_'+letter];mesh=a.static_mesh_component.static_mesh;n,area=panel_geometry(mesh);canopy_normals+=n;canopy_area+=area
        polys.append(dict(sector=letter,kind='Canopy',polygon=polygon(a.get_actor_transform(),mesh.get_bounding_box())))
    rows=[];errors=[]
    for letter,(cx,cy) in zip('ABC',centers):
        box=aa['MC_Sector_'+letter];q=base.inverse_transform_location(box.get_actor_location());dx,dy=-cx,-cy;length=math.hypot(dx,dy)
        line=abs((q.x-cx)*dy-(q.y-cy)*dx)/length/100
        toward=base.translation-box.get_actor_location();up=box.get_actor_up_vector();toward=(toward-up*toward.dot(up)).normal();dot=box.get_actor_forward_vector().dot(toward)
        cabinet_bounds=u.Box(min=u.Vector(-80,-100,0),max=u.Vector(80,100,195));cabinet=polygon(box.get_actor_transform(),cabinet_bounds)
        nearest=min(distance(cabinet,poly['polygon']) for poly in polys)/100
        sectorpolys=[v['polygon'] for v in polys if v['sector']==letter]
        corner=max(math.hypot(x,y) for poly in sectorpolys for x,y in poly)/100
        texts=[a.get_actor_label() for a in aa.values() if (a.get_actor_location()-box.get_actor_location()).length()<600 and a.get_component_by_class(u.TextRenderComponent)]
        rows.append(dict(sector=letter,nearest_panel_or_column_projection_m=round(nearest,3),line_offset_m=round(line,4),door_front_dot=round(dot,6),trackers=len(sectorpolys)-1,layout_center_distance_m=round(math.hypot(cx,cy)/100,3),center_distance_m=round((ground(p,base.transform_location(u.Vector(cx,cy,0)))-base.translation).length()/100,3),farthest_corner_m=round(corner,3),text_render_near_combiner=texts))
        if nearest<5 or line>1 or dot<.95 or corner>120 or len(sectorpolys)!=17 or texts:errors.append('Sector '+letter+' geometry')
    gaps={a+b:round(min(distance(x['polygon'],y['polygon']) for x in polys if x['sector']==a for y in polys if y['sector']==b)/100,3) for a,b in (('A','B'),('B','C'),('A','C'))}
    if min(gaps.values())<12:errors.append('Sector clearance')
    retired=[k for k in aa if k.startswith(('MC_S25_Film_','MC_S25_RollBox_','MC_S25_CableRuns','MC_R_Guide_Sector','MC_R_GuideStand_Sector','MC_R_Sign_Sector'))]
    unchanged=planet_signature(p)==before['planet']
    protected={}
    for label,old in before['protected'].items():
        # Struct repr includes a transient address; compare only its serialized fields.
        serialized=lambda v:v[v.index('{'):]
        protected[label]=serialized(str(aa[label].get_actor_transform()))==serialized(old['transform']) and [c.static_mesh.get_path_name() for c in aa[label].get_components_by_class(u.StaticMeshComponent) if c.static_mesh]==old['mesh']
    if retired or not unchanged or not all(protected.values()):errors.append('Protected/retired content')
    angle_canopy=[math.degrees(math.acos(max(-1,min(1,n.z)))) for n in canopy_normals]
    total=tracker_area*ism.get_instance_count()+canopy_area
    if ism.get_instance_count()!=48 or abs(total-4296)>1 or max(abs(a-25) for a in tilts+angle_canopy)>.1 or max(azimuth)>.1:errors.append('Panel count/area/tilt/heading')
    cables=aa['MC_S25_PowerCables'].get_component_by_class(u.InstancedStaticMeshComponent)
    result=dict(passed=not errors,errors=errors,sectors=rows,sector_gaps_m=gaps,tracker_count=ism.get_instance_count(),tracker_area_m2=round(tracker_area,3),total_panel_area_m2=round(total,3),tilt_range_degrees=[round(min(tilts+angle_canopy),4),round(max(tilts+angle_canopy),4)],parallel_heading_error_degrees=round(max(azimuth),4),retired_actors=retired,retired_assets_remaining=[x for x in before['asset_references'] if u.EditorAssetLibrary.does_asset_exist(x)],planet_unchanged=unchanged,protected_unchanged=protected,cable_instances=cables.get_instance_count(),cable_collision=str(cables.get_collision_enabled()),power_requirements=[str(r.id) for r in aa['MC_Power'].conditions.get_editor_property('requirements')])
    (SAVED/'MarsPowerAudit.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    for row in rows:u.log('MARS_POWER_SECTOR '+json.dumps(row,ensure_ascii=False))
    return result


def power_view():
    _,aa,p=context();key=(SAVED/'MarsStructureView.txt').read_text().strip();base,centers=solar_layout(aa,p)
    if key.startswith('sector'):
        letter=key[-1];cx,cy=centers['ABC'.index(letter)];d=u.Vector(cx,cy,0).normal();pos=ground(p,base.transform_location(d*2100));pos+=p.get_radial_up_vector(pos)*180
        aim=ground(p,base.transform_location(u.Vector(cx,cy,0)))+p.get_radial_up_vector(pos)*350
    elif key=='rack':
        t=aa['MC_CoreSupplyRack'].get_actor_transform();pos=t.transform_location(u.Vector(350,0,105));aim=t.transform_location(u.Vector(0,0,95))
    elif key=='core':
        t=aa['MC_CoreSupplyRack'].get_actor_transform();pos=t.transform_location(u.Vector(95,175,104));aim=t.transform_location(u.Vector(0,60,81))
    else:
        letter=key[-1];t=aa['MC_Sector_'+letter].get_actor_transform();fixed=key.startswith('fixed')
        for c in aa['MC_Sector_'+letter].get_components_by_class(u.StaticMeshComponent):
            name=c.get_name()
            if name.startswith('CabinetDoor'):c.set_relative_rotation(u.Rotator(yaw=0 if fixed else 55),False,False)
            if name.startswith(('CoreBurntInSlot','CoreInstalled','CoreDiscarded')):c.set_visibility((not fixed) if name.startswith('CoreBurntInSlot') else fixed)
        aa['MC_Sector_'+letter].status_light.set_material(0,u.load_asset(ROOT+'/M_MP_Power'+('Green' if fixed else 'Red')))
        pos=t.transform_location(u.Vector(444,0,145));aim=t.transform_location(u.Vector(10,0,95))
    u.EditorLevelLibrary.set_level_viewport_camera_info(pos,u.MathLibrary.make_rot_from_xz(aim-pos,p.get_radial_up_vector(pos)));u.EditorLevelLibrary.editor_set_game_view(True)
    result=dict(view=key,camera=vec(pos),rotation=vec(u.MathLibrary.make_rot_from_xz(aim-pos,p.get_radial_up_vector(pos)).quaternion().euler()))
    (SAVED/'MarsPowerView.json').write_text(json.dumps(result),encoding='utf8');return result

# Existing public recipe entry points now use the replacement solar implementation.
def audit():return power_audit()
def view():return power_view()


def power_runtime(action):
    """Isolated two-player editor fixture; press always enters the owning InteractionComponent."""
    phase=action.get('phase','status')
    settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
    performance=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
    if action['action']=='runtime_prepare':
        defaults=u.get_default_object(u.JTSExpeditionSubsystem)
        previous=dict(prefix=str(defaults.get_editor_property('save_slot_prefix')),slot=str(defaults.get_editor_property('save_slot_name')),clients=settings.get_editor_property('PlayNumberOfClients'),one_process=settings.get_editor_property('RunUnderOneProcess'))
        path=SAVED/'MarsPowerPIEPrevious.json'
        if not path.exists():path.write_text(json.dumps(previous),encoding='utf8')
        previous=json.loads(path.read_text(encoding='utf8'))
        if 'throttle' not in previous:previous['throttle']=performance.get_editor_property('bThrottleCPUWhenNotForeground');path.write_text(json.dumps(previous),encoding='utf8')
        setp(performance,bThrottleCPUWhenNotForeground=False)
        setp(defaults,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation')
        setp(settings,PlayNumberOfClients=2,RunUnderOneProcess=True)
        return dict(prepared=True,mode='Existing project PIE_ListenServer; verify connected authority at runtime')
    if action['action']=='runtime_restore':
        previous=json.loads((SAVED/'MarsPowerPIEPrevious.json').read_text(encoding='utf8'))
        defaults=u.get_default_object(u.JTSExpeditionSubsystem);setp(defaults,save_slot_prefix=previous['prefix'],save_slot_name=previous['slot'])
        setp(settings,PlayNumberOfClients=previous['clients'],RunUnderOneProcess=previous['one_process'])
        if 'throttle' in previous:setp(performance,bThrottleCPUWhenNotForeground=previous['throttle'])
        (SAVED/'MarsPowerPIEPrevious.json').unlink();return dict(restored=True)
    worlds=[w for w in u.EditorLevelLibrary.get_pie_worlds(True) if isinstance(u.GameplayStatics.get_game_state(w),u.JTSGameState)]
    if not worlds:return dict(playing=False)
    w=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority());aa=u.GameplayStatics.get_all_actors_of_class(w,u.Actor);by={a.get_actor_label():a for a in aa};gs=u.GameplayStatics.get_game_state(w)
    players=[a for a in aa if isinstance(a,u.JTSCharacter) and a.get_controller()]
    players.sort(key=lambda a:not a.get_controller().is_local_controller())
    exp=next(s for s in u.ObjectIterator(u.JTSExpeditionSubsystem) if s.get_outer()==u.GameplayStatics.get_game_instance(w))
    planet=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
    player=players[action.get('player',0)] if players else None
    def pid(a):
        ps=a.get_editor_property('player_state');return ps.get_editor_property('player_id') if ps else -1
    def owner_pawn(server):
        if server.get_controller().is_local_controller():return server
        return next(p for v in worlds if v!=w for p in u.GameplayStatics.get_all_actors_of_class(v,u.JTSCharacter) if p.is_locally_controlled() and pid(p)==pid(server))
    def aim(target,local,focus):
        t=target.get_actor_transform();pos=t.transform_location(u.Vector(*local));focus=t.transform_location(u.Vector(*focus));up=planet.get_radial_up_vector(pos)
        rotation=u.MathLibrary.make_rot_from_xz(focus-(pos+up*64),up)
        for pawn in [player]+([owner_pawn(player)] if not player.get_controller().is_local_controller() else []):
            pv=next(p for p in u.GameplayStatics.get_all_actors_of_class(pawn.get_world(),u.JTSPlanetAnchor) if str(p.get_planet_id())=='Mars')
            pawn.set_gameplay_planet(pv);pawn.set_actor_location(pos,False,False);pawn.initialize_planet_frame();pawn.get_controller().set_control_rotation(rotation);pawn.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
            if pawn.is_locally_controlled():
                cam=next((c for c in u.GameplayStatics.get_all_actors_of_class(pawn.get_world(),u.CameraActor) if c.get_actor_label()=='SolarFixtureCamera'),None)
                transform=u.Transform(location=pos+up*64,rotation=rotation,scale=u.Vector(1,1,1));api=u.get_default_object(u.GameplayStatics)
                if not cam:
                    cam=api.call_method('BeginDeferredActorSpawnFromClass',(pawn.get_world(),u.CameraActor.static_class(),transform,u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));api.call_method('FinishSpawningActor',(cam,transform,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));cam.set_actor_label('SolarFixtureCamera')
                cam.set_actor_location_and_rotation(transform.translation,rotation,False,False);pawn.get_controller().set_view_target_with_blend(cam,0)
    result=dict(action=phase,game_time=u.GameplayStatics.get_time_seconds(w))
    if phase=='network':
        result['network']=[]
        for v in worlds:
            actors=u.GameplayStatics.get_all_actors_of_class(v,u.Actor)
            rack=next(a for a in actors if a.get_actor_label()=='MC_CoreSupplyRack')
            result['network'].append(dict(authority=rack.has_authority(),properties={k:str(rack.get_editor_property(k)) for k in ('replicates','always_relevant','net_cull_distance_squared','net_dormancy','net_load_on_client')},location=vec(rack.get_actor_location()),owner=str(rack.get_owner()),players=[dict(id=pid(a),local=a.is_locally_controlled(),location=vec(a.get_actor_location()),camera=vec(a.get_controller().player_camera_manager.get_camera_location()) if a.get_controller() else None,distance=(a.get_actor_location()-rack.get_actor_location()).length()) for a in actors if isinstance(a,u.JTSCharacter)]))
    elif phase=='isolate':
        setp(exp,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation');result['new_slot']=exp.begin_new_expedition_in_slot(4,'Solar service multiplayer verification')
        for p in players:p.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
    elif phase=='connect':u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.ConnectClients')
    elif phase=='resume':
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.Resume')
        for p in players:p.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
    elif phase=='grant':u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.GrantStationPermit')
    elif phase in ('take_credential','swipe','read','dismiss'):
        (SAVED/'StationFlowAction.txt').write_text({'take_credential':'take'}.get(phase,phase),encoding='utf8');result['legacy']=__import__('runpy').run_path(str(Path(__file__).with_name('rebuild_mars_station_flow.py')))['probe']()
    elif phase=='aim':
        target=by[action['target']];kind=action['target']
        if kind=='MC_CoreSupplyRack':
            i=action.get('slot',0);y=-60+i*40;aim(target,(155,y,98),(10,y,85))
        elif kind.startswith('MC_Sector_'):aim(target,(140,0,98),(44,-30,100))
        elif kind=='MC_R_EcologyEntranceDoor':aim(target,(180,-160,98),(45,-220,155))
        elif kind=='MC_R_Guide_DomeOperations':aim(target,(0,125,98),(0,0,2))
        elif kind=='MC_R_StationAccessDoor':aim(target,(170,0,98),(20,0,145))
        else:aim(target,(150,0,98),(0,0,30))
        result['target']=target.get_actor_label();result['prompt']=str(target.get_interaction_prompt(player));result['executable']=target.can_execute_interaction(player)
    elif phase=='press':
        pawn=owner_pawn(player);component=pawn.get_component_by_class(u.InteractionComponent);component.refresh_interactable();target=component.get_current_interactable()
        result.update(intent_world=pawn.get_world().get_name(),intent_authority=pawn.has_authority(),target=target.get_actor_label() if target else None,prompt=str(component.get_current_interaction_prompt()),intent_queued=True,dispatched=bool(target and target.can_execute_interaction(pawn)))
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.Input press '+str(pid(player)))
        if isinstance(target,u.JTSItemSupplyRack) and target.get_supply_count()==1 and result['dispatched']:
            rack=by['MC_CoreSupplyRack'];samples=[];watch={'began':None}
            def watch_refill(_):
                count=rack.get_supply_count()
                if watch['began'] is None:
                    if count==0:watch['began']=u.GameplayStatics.get_time_seconds(w)
                    return
                elapsed=u.GameplayStatics.get_time_seconds(w)-watch['began'];samples.append([elapsed,count])
                if count or elapsed>12:
                    (SAVED/'MarsPowerRefillTiming.json').write_text(json.dumps(dict(first_stock_after_seconds=elapsed,stock=count,early_stock=any(t<9.99 and n>0 for t,n in samples),samples=samples)),encoding='utf8');u.unregister_slate_post_tick_callback(watch['handle'])
            watch['handle']=u.register_slate_post_tick_callback(watch_refill)
        elif isinstance(target,u.JTSFacilityController) and result['dispatched'] and target.get_actor_label().startswith('MC_Sector_'):
            began=u.GameplayStatics.get_time_seconds(w);samples=[];watch={};label=target.get_actor_label()
            def watch_replacement(_):
                elapsed=u.GameplayStatics.get_time_seconds(w)-began;states=[]
                for view in worlds:
                    device=next(a for a in u.GameplayStatics.get_all_actors_of_class(view,u.JTSFacilityController) if a.get_actor_label()==label)
                    components=device.get_components_by_class(u.StaticMeshComponent);door=next(c for c in components if c.get_name().startswith('CabinetDoor'))
                    states.append(dict(authority=device.has_authority(),active=device.get_editor_property('active'),animating=device.get_editor_property('RepairAnimating'),door_yaw=door.get_editor_property('relative_rotation').yaw,light=device.status_light.get_material(0).get_name()))
                samples.append(dict(elapsed=elapsed,states=states))
                if elapsed>2.3:
                    (SAVED/('MarsPowerReplacementTiming_'+label+'.json')).write_text(json.dumps(samples,indent=2),encoding='utf8');u.unregister_slate_post_tick_callback(watch['handle'])
            watch['handle']=u.register_slate_post_tick_callback(watch_replacement)
    elif phase=='drop':
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.Input drop '+str(pid(player)));result['intent_queued']=True
    elif phase in ('drop_slot','select_slot'):
        u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.Input '+phase+' '+str(pid(player))+' '+str(action.get('slot',1)));result['intent_queued']=True
    elif phase=='pickup_aim':
        target=next(a for a in aa if isinstance(a,u.JTSWorldPickupActor) and a.get_item_instance().item_id==u.JTSItemId.COMBINER_CORE_MODULE);aim(target,(100,0,98),(0,0,10));result['target']=target.get_actor_label()
    elif phase=='death':
        result['death_position']=vec(player.get_actor_location())
        health=player.get_component_by_class(u.JTSHealthComponent);health.set_invulnerable(False)
        result['damage']=health.apply_damage(100000,player.get_controller(),player,True)
        players=[a for a in u.GameplayStatics.get_all_actors_of_class(w,u.JTSCharacter) if a.get_controller()]
        players.sort(key=lambda a:not a.get_controller().is_local_controller())
    elif phase=='inventory':
        result['items']=[dict(player=pid(p),capacity=p.get_component_by_class(u.JTSInventoryComponent).get_inventory_capacity(),items=[dict(id=str(i.item_id),count=i.stack_count) for i in p.get_component_by_class(u.JTSInventoryComponent).get_editor_property('item_slots')]) for p in players]
        result['stack_limits']={str(i):player.get_component_by_class(u.JTSInventoryComponent).get_effective_stack_limit(i) for i in (u.JTSItemId.ROCK,u.JTSItemId.ORE,u.JTSItemId.COMBINER_CORE_MODULE)}
    elif phase=='ground':
        result['pickups']=[dict(position=vec(a.get_actor_location()),count=a.get_item_instance().stack_count,player_distance=(a.get_actor_location()-player.get_actor_location()).length()/100) for a in aa if isinstance(a,u.JTSWorldPickupActor) and a.get_item_instance().item_id==u.JTSItemId.COMBINER_CORE_MODULE]
    elif phase=='held':
        result['held']=[]
        for p in players:
            body=next(c for c in p.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='WeaponVisualBody')
            mesh=p.get_component_by_class(u.SkeletalMeshComponent);wrist=mesh.get_socket_location('Wrist_R');finger=mesh.get_socket_location('Index1_R');palm=wrist+(finger-wrist)*1.35
            grip=body.get_world_transform().transform_location(u.Vector(0,0,28.3))
            result['held'].append(dict(player=pid(p),active=str(p.get_component_by_class(u.JTSInventoryComponent).get_active_item_id()),mesh=body.static_mesh.get_name() if body.static_mesh else None,visible=body.is_visible(),grip_distance_cm=(grip-palm).length(),upright_dot=body.get_up_vector().dot(p.get_actor_up_vector())))
    elif phase=='lift':
        result['transits']=[dict(label=a.get_actor_label(),prompt=str(a.get_interaction_prompt(player)),executable=a.can_execute_interaction(player),distance_m=(a.get_actor_location()-player.get_actor_location()).length()/100) for a in aa if isinstance(a,u.JTSInteriorTransit)]
    elif phase=='presentation':
        result['presentation']=[]
        for view in worlds:
            actors=u.GameplayStatics.get_all_actors_of_class(view,u.Actor);devices={a.get_actor_label():a for a in actors}
            result['presentation'].append(dict(authority=u.GameplayStatics.get_game_state(view).has_authority(),rack=[dict(name=c.get_name(),visible=c.is_visible()) for c in devices['MC_CoreSupplyRack'].get_components_by_class(u.StaticMeshComponent) if c.get_name().startswith(('Supply','SlotLamp'))],sectors={s:dict(animating=devices['MC_Sector_'+s].get_editor_property('RepairAnimating'),light=devices['MC_Sector_'+s].status_light.get_material(0).get_name(),components=[dict(name=c.get_name(),visible=c.is_visible(),collision=str(c.get_collision_enabled())) for c in devices['MC_Sector_'+s].get_components_by_class(u.StaticMeshComponent) if c.get_name().startswith('Core')]) for s in 'ABC'}))
    elif phase=='save_load':
        setp(exp,save_slot_prefix='MarsValidationSolar',save_slot_name='MarsValidationSolar');u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.SaveReload')
        saved=u.GameplayStatics.load_game_from_slot('MarsValidationSolar_04',0);result['saved_loadable']=bool(saved);assert saved
    elif phase=='reload_world':
        u.SystemLibrary.execute_console_command(w,'net.AllowPIESeamlessTravel 1')
        u.SystemLibrary.execute_console_command(w,'servertravel /Game/Space/Maps/L_SpaceWorld?listen',players[0].get_controller())
    elif phase=='reopen_listen':u.GameplayStatics.open_level(w,'/Game/Space/Maps/L_SpaceWorld',True,'listen')
    result['players']=[dict(name=p.get_name(),id=pid(p),local=p.get_controller().is_local_controller(),active=str(p.get_component_by_class(u.JTSInventoryComponent).get_active_item_id()),cores=p.get_component_by_class(u.JTSInventoryComponent).get_item_count(u.JTSItemId.COMBINER_CORE_MODULE),stacks=[i.stack_count for i in p.get_component_by_class(u.JTSInventoryComponent).get_editor_property('item_slots') if i.item_id==u.JTSItemId.COMBINER_CORE_MODULE]) for p in players]
    result['worlds']=[]
    for v in worlds:
        state=u.GameplayStatics.get_game_state(v);actors=u.GameplayStatics.get_all_actors_of_class(v,u.Actor);lookup={a.get_actor_label():a for a in actors};rack=lookup.get('MC_CoreSupplyRack')
        result['worlds'].append(dict(world=v.get_name(),authority=state.has_authority(),flags=[str(f) for f in state.get_editor_property('facility_flag_ids')],rack_count=rack.get_supply_count() if rack else None,slots=list(rack.get_editor_property('slots')) if rack else [],ground_cores=sum(isinstance(a,u.JTSWorldPickupActor) and a.get_item_instance().item_id==u.JTSItemId.COMBINER_CORE_MODULE for a in actors),sectors={s:dict(active=lookup['MC_Sector_'+s].get_editor_property('active'),door=str(next(c for c in lookup['MC_Sector_'+s].get_components_by_class(u.StaticMeshComponent) if c.get_name().startswith('CabinetDoor')).get_editor_property('relative_rotation'))) for s in 'ABC'}))
    path=SAVED/('MarsPowerPIE_'+phase+'_'+str(action.get('player',0))+'_'+str(action.get('slot',0))+'.json');path.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    return result
