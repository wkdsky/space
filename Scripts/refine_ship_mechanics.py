import unreal, json
from pathlib import Path
ROOT=Path('D:/projects/space')
SHIP='/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1'
ASSETS='/Game/Space/Ships/Refit'
EA=unreal.EditorAssetLibrary
def save(bp):
    if isinstance(bp,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EA.save_loaded_asset(bp,False)
def templates(bp):
    sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib=unreal.SubobjectDataBlueprintFunctionLibrary
    result={}
    for h in sub.k2_gather_subobject_data_for_blueprint(bp):
        o=lib.get_object_for_blueprint(lib.get_data(h),bp)
        if o:result[o.get_name().removesuffix('_GEN_VARIABLE')]=(h,o)
    return result
def apply():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    at=unreal.AssetToolsHelpers.get_asset_tools()
    meshes={}
    for f in (ROOT/'SourceArt/ShipRefit/Mechanics').glob('*.fbx'):
        path=ASSETS+'/Meshes/'+f.stem
        m=unreal.load_asset(path)
        if not m:
            t=unreal.AssetImportTask();t.filename=str(f);t.destination_path=ASSETS+'/Meshes';t.destination_name=f.stem
            t.automated=True;t.replace_existing=True;t.save=True
            o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False
            o.automated_import_should_detect_type=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
            d=o.static_mesh_import_data;d.combine_meshes=True;d.generate_lightmap_u_vs=False;d.auto_generate_collision=False
            d.convert_scene=True;d.force_front_x_axis=False;d.import_rotation=unreal.Rotator(yaw=180);t.options=o
            at.import_asset_tasks([t]);m=unreal.load_asset(path)
            assert m, str(f)
        for i,slot in enumerate(m.static_materials):
            name='_'.join(str(slot.material_slot_name).split('_')[:2])
            mat=unreal.load_asset((ASSETS if name.startswith('Refit_') else '/Game/Space/Ships/Docks')+'/Materials/M_'+name)
            assert mat,name
            m.set_material(i,mat)
        save(m)
        meshes[f.stem]=m
    bp=unreal.load_asset(SHIP);sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    ts=templates(bp)
    def add(name,mn,parent='SceneRoot',loc=(0,0,0),yaw=0):
        nonlocal ts
        if name not in ts:
            h,reason=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=ts[parent][0],new_class=unreal.StaticMeshComponent,blueprint_context=bp))
            assert not str(reason),str(reason)
            assert sub.rename_subobject(h,unreal.Text(name))
            ts=templates(bp)
        c=ts[name][1];c.set_static_mesh(meshes[mn]);c.set_editor_property('override_materials',[])
        c.set_editor_property('relative_location',unreal.Vector(*loc));c.set_editor_property('relative_rotation',unreal.Rotator(yaw=yaw))
        c.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
        c.set_visibility(True);c.set_hidden_in_game(False);c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        return c
    add('ForkArmor','SM_Mech_ForkArmor')
    add('CabinArmor','SM_Mech_CabinArmor','CabinAssemblyRoot')
    add('CabinFrame','SM_Mech_CabinFrame','CabinAssemblyRoot')
    add('CabinSocketLeft','SM_Mech_CabinSocket','CabinAssemblyRoot',(0,-191,32),180)
    add('CabinSocketRight','SM_Mech_CabinSocket','CabinAssemblyRoot',(0,191,32),0)
    for side,sign in [('Left',-1),('Right',1)]:
        # Local -Y points into the aisle; 201cm covers y=395 -> 194.
        yaw=180 if sign<0 else 0
        add('Coupler'+side+'Housing','SM_Mech_CouplerHousing',loc=(0,sign*402,32),yaw=yaw)
        add('Coupler'+side+'Slide','SM_Mech_CouplerSlide',loc=(0,sign*402,32),yaw=yaw)
        add('Coupler'+side+'Pins','SM_Mech_CouplerPins','Coupler'+side+'Slide',(0,-193,32))
    add('RearCaptureCradle','SM_Mech_RearCradle',loc=(-510,0,35))
    add('RearEjectorPiston','SM_Mech_RearPiston',loc=(-560,0,35))
    # This hidden bounds source retains the existing flight/camera/boarding envelope.
    ts=templates(bp)
    ts['SpacecraftMesh'][1].set_visibility(False);ts['SpacecraftMesh'][1].set_hidden_in_game(True)
    ts['Hull'][1].set_static_mesh(meshes['SM_Mech_ForkFrame']);ts['Hull'][1].set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
    ts['Hull'][1].set_visibility(True);ts['Hull'][1].set_hidden_in_game(False)
    for name in ['Door','Step']:
        ts[name][1].set_static_mesh(None)
    feet=[]
    for leg,x,y in [('FL',250,-555),('FR',250,555),('BL',-470,-555),('BR',-470,555)]:
        name='Gear_'+leg
        assert sub.attach_subobject(ts['SceneRoot'][0],ts[name][0])
        for n,mn,loc in [(name,'SM_Mech_GearUpper',(x,y,-62)),(name+'_Shin','SM_Mech_GearShin',(0,0,-70)),(name+'_Foot','SM_Mech_GearFoot',(0,0,-40))]:
            c=ts[n][1];c.set_static_mesh(meshes[mn]);c.set_editor_property('override_materials',[])
            c.set_editor_property('relative_location',unreal.Vector(*loc));c.set_editor_property('relative_rotation',unreal.Rotator())
            c.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));c.set_editor_property('absolute_location',False)
            c.set_editor_property('absolute_rotation',False);c.set_editor_property('absolute_scale',False)
            c.set_visibility(True);c.set_hidden_in_game(False)
        f=unreal.JTSLandingFootDefinition();f.foot_component_name=name+'_Foot';f.strut_component_name=name+'_Shin'
        f.sole_centre_local=unreal.Vector(0,0,-20);f.strut_rest_length=40
        f.deployed_contact_frame=unreal.Transform(location=unreal.Vector(x,y,-192));f.pad_half_extent=unreal.Vector2D(47,32)
        f.max_compression=18;f.max_extension=28;feet.append(f)
    cdo=unreal.get_default_object(bp.generated_class());cdo.get_landing_support_component().set_editor_property('feet',feet)
    p=cdo.get_component_by_class(unreal.JTSSpacecraftPresentationComponent)
    for n,v in [('gear_transition_duration',2.8),('gear_deployment_duration',1.65),('gear_shin_retraction_travel',34),('gear_fold_pitch',92),('gear_fold_roll',0),('gear_fold_yaw',0),('gear_foot_stow_pitch',-92)]:p.set_editor_property(n,v)
    coupling=cdo.cabin_coupling
    coupling.set_editor_property('cabin_root_name','CabinAssemblyRoot')
    coupling.set_editor_property('cabin_component_names',['CabinArmor','CabinFrame','CabinSocketLeft','CabinSocketRight','ColCabin','BoardingPoint','ExitPoint','CameraBoom','Muzzle_Main'])
    links=[]
    for side,sign in [('Left',-1),('Right',1)]:
        for suffix,offset,pin in [('Slide',(0,sign*158,0),False),('Pins',(0,0,32),True)]:
            link=unreal.JTSCabinCouplingLink();link.component_name='Coupler'+side+suffix;link.retracted_offset=unreal.Vector(*offset);link.lock_pin=pin;links.append(link)
    coupling.set_editor_property('side_links',links);coupling.set_editor_property('rear_piston_name','RearEjectorPiston')
    coupling.set_editor_property('ejection_offset',unreal.Vector(1000,0,0));coupling.set_editor_property('rear_piston_travel',unreal.Vector(120,0,0))
    coupling.set_editor_property('clearance_centre',unreal.Vector(-85,0,42));coupling.set_editor_property('clearance_extent',unreal.Vector(385,194,198))
    hs=list(cdo.armament.hardpoints)
    for h in hs:
        if str(h.id)=='Main':h.mount_transform=unreal.Transform(location=unreal.Vector(0,0,-65));h.parent_component_name='CabinAssemblyRoot'
        elif str(h.id)=='Turret':h.parent_component_name='CabinAssemblyRoot'
    cdo.armament.set_editor_property('hardpoints',hs)
    save(bp)
    main=unreal.load_asset(ASSETS+'/BP_ShipMainLaser');c=unreal.get_default_object(main.generated_class())
    c.mount.set_static_mesh(meshes['SM_Mech_MainCannon']);c.mount.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));c.mount.set_editor_property('override_materials',[])
    c.turret.set_static_mesh(None);c.gun.set_static_mesh(None);c.pitch_pivot.set_editor_property('relative_location',unreal.Vector())
    for component in [c.gun,c.turret,c.yaw_pivot,c.pitch_pivot,c.muzzle]:
        component.set_editor_property('relative_scale3d',unreal.Vector(1,1,1))
        component.set_editor_property('relative_rotation',unreal.Rotator())
    c.muzzle.set_editor_property('relative_location',unreal.Vector(282,0,-37));c.set_editor_property('fixed_forward',True);c.set_editor_property('recoil_travel',0)
    save(main)
    terminal=unreal.load_asset('/Game/Space/Ships/Docks/DA_Ship_CentralTerminal');terminal.set_editor_property('parent_component_name','CabinAssemblyRoot');save(terminal)
    for kind in ['Material','Weapon']:
        dock=unreal.load_asset('/Game/Space/Ships/Docks/BP_Dock_'+kind);dc=unreal.get_default_object(dock.generated_class())
        dc.mount.set_static_mesh(meshes['SM_Mech_Dock'+kind+'Mount']);dc.mount.set_editor_property('relative_scale3d',unreal.Vector(1,1,1));dc.mount.set_editor_property('override_materials',[]);save(dock)
    # Existing savegames use the child class; explicitly remove stale visual/landing overrides.
    legacy=unreal.load_asset('/Game/Space/Blueprints/Ships/BP_Spacecraft');lc=unreal.get_default_object(legacy.generated_class())
    lc.get_landing_support_component().set_editor_property('feet',feet)
    lc.armament.set_editor_property('hardpoints',hs)
    lp=lc.get_component_by_class(unreal.JTSSpacecraftPresentationComponent)
    for n in ['gear_transition_duration','gear_deployment_duration','gear_shin_retraction_travel','gear_fold_pitch','gear_fold_roll','gear_fold_yaw','gear_foot_stow_pitch']:lp.set_editor_property(n,p.get_editor_property(n))
    lco=lc.cabin_coupling
    for n in ['cabin_root_name','cabin_component_names','side_links','rear_piston_name','ejection_offset','rear_piston_travel','clearance_centre','clearance_extent']:lco.set_editor_property(n,coupling.get_editor_property(n))
    save(legacy)
    return dict(imported=list(meshes),configured=SHIP)
def inspect():
    es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    bp=unreal.load_asset(SHIP)
    a=es.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,0,100000),transient=True)
    out=[]
    for c in a.get_components_by_class(unreal.SceneComponent):
        row=dict(name=c.get_name(),parent=c.get_attach_parent().get_name() if c.get_attach_parent() else None,loc=str(c.relative_location),rot=str(c.relative_rotation),scale=str(c.relative_scale3d))
        if isinstance(c,unreal.StaticMeshComponent):row.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,visible=c.visible,hidden=c.hidden_in_game)
        out.append(row)
    result=dict(components=out,feet=str(a.get_landing_support_component().feet))
    es.destroy_actor(a)
    (ROOT/'Saved/mechanics_inspect.json').write_text(json.dumps(result,indent=2))
    return result
def close():
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    unreal.SystemLibrary.execute_console_command(None,'QUIT_EDITOR')
    return 'closing'
def validate():
    es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);out={}
    for path in [SHIP,'/Game/Space/Blueprints/Ships/BP_Spacecraft']:
        bp=unreal.load_asset(path);a=es.spawn_actor_from_class(bp.generated_class(),unreal.Vector(0,0,100000),transient=True)
        try:
            cs={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)};rows=[]
            for f in a.get_landing_support_component().feet:
                foot=cs[str(f.foot_component_name)]
                sole=a.get_actor_transform().inverse_transform_location(foot.get_world_transform().transform_location(f.sole_centre_local))
                distance=unreal.MathLibrary.vector_distance(sole,f.deployed_contact_frame.translation)
                assert distance<.1,(path,foot.get_name(),str(sole),str(f.deployed_contact_frame))
                assert foot.get_world_scale().x>0,(path,'negative gear scale')
                rows.append(dict(name=foot.get_name(),sole=str(sole),error=distance,mesh=foot.static_mesh.get_name()))
            assert cs['Door'].static_mesh is None
            assert cs['Gear_FL'].get_attach_parent().get_name()=='SceneRoot'
            assert cs['CabinArmor'].get_attach_parent().get_name()=='CabinAssemblyRoot'
            out[path]=rows
        finally:es.destroy_actor(a)
    out['main_bounds']=str(unreal.load_asset(ASSETS+'/Meshes/SM_Mech_MainCannon').get_bounding_box())
    out['slider_bounds']=str(unreal.load_asset(ASSETS+'/Meshes/SM_Mech_CouplerSlide').get_bounding_box())
    (ROOT/'Saved/mechanics_validation.json').write_text(json.dumps(out,indent=2))
    return out
def preview():
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.JTSGameplayGameModeBase)
    a=es.spawn_actor_from_class(unreal.load_asset(SHIP).generated_class(),unreal.Vector(0,0,192));a.set_actor_label('Ship mechanical review')
    sun=es.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,1600),unreal.Rotator(pitch=-42,yaw=-35));sun.light_component.set_editor_property('intensity',4)
    sky=es.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,1500));sky.light_component.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.light_component.set_editor_property('intensity',.7)
    floor=es.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-5));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'));floor.set_actor_scale3d(unreal.Vector(60,60,.1))
    cam=unreal.Vector(1700,-1800,1120)
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(cam,unreal.MathLibrary.find_look_at_rotation(cam,unreal.Vector(0,0,225)))
    unreal.EditorLevelLibrary.editor_set_game_view(True)
    return a.get_path_name()
def _runtime_ship():
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world,'Start Simulate first'
    return unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSSpacecraftActor)[0]
def detach():
    a=_runtime_ship();return dict(accepted=a.cabin_coupling.release_cabin(),state=str(a.cabin_coupling.get_coupling_state()))
def redock():
    a=_runtime_ship();return dict(accepted=a.cabin_coupling.capture_cabin(),state=str(a.cabin_coupling.get_coupling_state()))
def capture():
    a=_runtime_ship()
    return dict(state=str(a.cabin_coupling.get_coupling_state()),cabin=str(a.cabin_coupling.get_cabin_world_transform()),weapons=len(a.armament.get_modules()))
