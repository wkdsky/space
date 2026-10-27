"""Measured presentation corrections; assets and transforms remain editor configuration."""
import json
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
ART=ROOT/'SourceArt/FrontEnd'
DEST='/Game/Space/FrontEnd'
job=globals().get('AUTHORING_JOB') or json.loads((ROOT/'Saved/FrontEndJob.json').read_text())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EA=unreal.EditorAssetLibrary
operation=job.get('operation','probe')
vec=lambda v:[round(v.x,3),round(v.y,3),round(v.z,3)]
REPORT={'success':True,'operation':operation}

if operation=='animation_basis':
    import runpy
    helper=runpy.run_path(str(ROOT/'Tools/Art/frontend_animation_basis_unreal.py'))
    REPORT['basis']=helper['adapt_relaxed_idle'](unreal.load_asset(DEST+'/Animations/A_FE_Idle'))
elif operation=='anim_probe':
    REPORT['classes']=[name for name in dir(unreal) if 'Animation' in name and ('Library' in name or 'Controller' in name)]
    lib=unreal.AnimationLibrary
    REPORT['poses']={}
    for name in ['Idle','LookShip']:
        asset=unreal.load_asset(DEST+'/Animations/A_FE_'+name)
        REPORT['poses'][name]={bone:str(lib.get_bone_pose_for_frame(asset,bone,0,False)) for bone in ['Root','Body','Head']}
        REPORT['poses'][name]['import_data']=str(asset.get_editor_property('asset_import_data'))
    idle=unreal.load_asset(DEST+'/Animations/A_FE_Idle')
    opts=unreal.AnimPoseEvaluationOptions();opts.set_editor_property('should_retarget',False);opts.set_editor_property('evaluation_type',unreal.AnimDataEvalType.SOURCE)
    REPORT['raw_body']={}
    for t in [0,3,6,9,12]:
        pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(idle,t,opts)
        REPORT['raw_body'][t]=str(unreal.AnimPoseExtensions.get_bone_pose(pose,'Body',unreal.AnimPoseSpaces.LOCAL))
    REPORT['bone_tree']=[str(x) for x in idle.get_editor_property('skeleton').get_editor_property('bone_tree')][:10]
    REPORT['track_names']=[str(n) for n in idle.get_editor_property('data_model_interface').get_bone_track_names()]
    for prop in ['controller','data_model_interface']:
        obj=idle.get_editor_property(prop)
        REPORT[prop]={'type':str(type(obj)),'object':str(obj),'methods':[n for n in dir(obj) if any(s in n for s in ['bone','bracket','number','model'])]}
elif operation=='probe':
    REPORT['actors']=[]
    for a in actors.get_all_level_actors():
        name=a.get_actor_label()
        if any(s in name for s in ['Twinfork','Wall','Door','Frame','Pad','Presentation','AlienThreat','Hill']):
            center,extent=a.get_actor_bounds(False)
            item={'name':name,'location':vec(a.get_actor_location()),'scale':vec(a.get_actor_scale3d()),'center':vec(center),'extent':vec(extent)}
            if isinstance(a,unreal.StaticMeshActor):item['mesh']=str(a.static_mesh_component.static_mesh)
            if name=='FE_MainPresentation':
                mesh=a.get_editor_property('preview_mesh')
                item['preview_transform']=str(mesh.get_relative_transform())
                item['views']={str(k):{'transform':str(v.get_editor_property('transform')),'fov':v.get_editor_property('fov')} for k,v in a.get_editor_property('views').items()}
            REPORT['actors'].append(item)

elif operation=='import':
    at=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.MaterialEditingLibrary
    imported=[]
    skeleton=unreal.load_asset(DEST+'/Characters/SK_FE_Engineer_Skeleton')
    for filename,folder in [('Animations/A_FE_Idle.fbx','Animations'),('Textures/T_FE_ThreatHeatCloud.png','Textures')]+[
            ('Models/SM_FE_Threat'+p+'.fbx','Models') for p in ['Core','Halo','FrontRings']]:
        source=ART/filename;task=unreal.AssetImportTask()
        for k,v in {'filename':str(source),'destination_path':DEST+'/'+folder,'destination_name':source.stem,
                    'automated':True,'replace_existing':True,'save':True}.items():task.set_editor_property(k,v)
        if folder!='Textures':
            options=unreal.FbxImportUI()
            for k,v in {'import_materials':False,'import_textures':False,'automated_import_should_detect_type':False}.items():options.set_editor_property(k,v)
            if folder=='Animations':
                for k,v in {'import_mesh':False,'import_as_skeletal':True,'skeleton':skeleton,'import_animations':True,
                            'mesh_type_to_import':unreal.FBXImportType.FBXIT_ANIMATION}.items():options.set_editor_property(k,v)
                options.anim_sequence_import_data.set_editor_property('import_uniform_scale',1.0)
            else:
                options.set_editor_property('import_as_skeletal',False);options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
                options.static_mesh_import_data.set_editor_property('combine_meshes',True)
            task.set_editor_property('options',options);task.set_editor_property('factory',unreal.FbxFactory())
        at.import_asset_tasks([task]);asset=unreal.load_asset(DEST+'/'+folder+'/'+source.stem);assert asset,source
        if folder=='Textures':
            asset.set_editor_property('srgb',True)
            asset.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);asset.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
        EA.save_loaded_asset(asset,only_if_is_dirty=False);imported.append(asset.get_path_name())
    def constant(mat,value):
        n=me.create_material_expression(mat,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n
    def multiply(mat,a,aout,b,bout):
        n=me.create_material_expression(mat,unreal.MaterialExpressionMultiply)
        me.connect_material_expressions(a,aout,n,'A');me.connect_material_expressions(b,bout,n,'B');return n
    import runpy
    REPORT['basis']=runpy.run_path(str(ROOT/'Tools/Art/frontend_animation_basis_unreal.py'))['adapt_relaxed_idle'](unreal.load_asset(DEST+'/Animations/A_FE_Idle'))
    for part,color,energy,opacity in [('Core',(1,.63,.28),1500,None),('Halo',(1,.018,.003),650,.10),('FrontRings',(1,.06,.008),1100,.24)]:
        mat=unreal.load_asset(DEST+'/Materials/M_FE_Threat'+part) or at.create_asset('M_FE_Threat'+part,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        me.delete_all_material_expressions(mat)
        mat.set_editor_property('two_sided',True);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if opacity is not None else unreal.BlendMode.BLEND_OPAQUE)
        tint=me.create_material_expression(mat,unreal.MaterialExpressionVectorParameter)
        tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(*color,1))
        emit=multiply(mat,tint,'',constant(mat,energy),'');me.connect_material_property(emit,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if opacity is not None:me.connect_material_property(constant(mat,opacity),'',unreal.MaterialProperty.MP_OPACITY)
        me.recompile_material(mat);EA.save_loaded_asset(mat,only_if_is_dirty=False)
    mat=unreal.load_asset(DEST+'/Materials/M_FE_ThreatHeat') or at.create_asset('M_FE_ThreatHeat',DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    me.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',True);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    tex=me.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D)
    tex.set_editor_property('parameter_name','AtmosphericHeat');tex.set_editor_property('texture',unreal.load_asset(DEST+'/Textures/T_FE_ThreatHeatCloud'))
    emit=multiply(mat,tex,'RGB',constant(mat,180),'');me.connect_material_property(emit,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=multiply(mat,tex,'A',constant(mat,2.4),'');me.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    me.recompile_material(mat);EA.save_loaded_asset(mat,only_if_is_dirty=False)
    # A presentation-only child pawn avoids playing gameplay locomotion in the lobby.
    path=DEST+'/Blueprints/BP_FE_LobbyAstronaut'
    bp=unreal.load_asset(path)
    if not bp:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.load_class(None,'/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2.BP_JTSPlayer_Casual_2_C'))
        bp=at.create_asset('BP_FE_LobbyAstronaut',DEST+'/Blueprints',unreal.Blueprint,factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    subobjects=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
    idle=unreal.load_asset(DEST+'/Animations/A_FE_Idle')
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        obj=lib.get_object_for_blueprint(lib.get_data(handle),bp)
        if isinstance(obj,unreal.SkeletalMeshComponent):
            obj.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            data=obj.get_editor_property('animation_data');data.set_editor_property('anim_to_play',idle)
            data.set_editor_property('saved_looping',True);data.set_editor_property('saved_playing',True)
            obj.set_editor_property('animation_data',data)
    EA.save_loaded_asset(bp,only_if_is_dirty=False)
    REPORT['imported']=imported;REPORT['lobby_pawn']=path

elif operation=='scene':
    assert json.loads((ART/'asset_gate.json').read_text())['passed']
    kind=job.get('map','entry');path=DEST+'/Maps/'+('L_Entry' if kind=='entry' else 'L_PreLaunchLobby')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world.get_outermost().get_name()!=path:assert levels.load_level(path)
    by_name={a.get_actor_label():a for a in actors.get_all_level_actors()}
    def ensure_mesh(name,mesh):
        obj=by_name.get(name)
        if not obj:
            obj=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector());obj.set_actor_label(name);by_name[name]=obj
        obj.static_mesh_component.set_static_mesh(unreal.load_asset(DEST+'/Models/'+mesh))
        obj.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        return obj
    ship=by_name['FE_TwinforkDisplay'];ship.set_actor_location(unreal.Vector(-120,1000,0),False,False)
    ship.set_actor_rotation(unreal.Rotator(yaw=35),False);ship.set_actor_scale3d(unreal.Vector(.34,.34,.34))
    center,extent=ship.get_actor_bounds(False)
    ship.set_actor_location(ship.get_actor_location()+unreal.Vector(0,0,1.4-(center.z-extent.z)),False,False)
    ground=ensure_mesh('FE_DistantApron','SM_FE_Floor')
    ground.set_actor_location(unreal.Vector(0,7000,-35),False,False);ground.set_actor_scale3d(unreal.Vector(50,50,1))
    for a in list(by_name.values()):
        if a.get_actor_label().startswith('FE_AlienThreat_'):
            a.set_actor_location(unreal.Vector(-100000,400000,56000),False,False)
            a.set_actor_scale3d(unreal.Vector(100,100,100))
            a.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_Threat'+a.get_actor_label().removeprefix('FE_AlienThreat_')))
    cloud=ensure_mesh('FE_ThreatHeatedAtmosphere','SM_FE_FXCard')
    cloud.set_actor_location(unreal.Vector(-100000,400000,62000),False,False)
    cloud.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(unreal.Vector(100000,-400000,0)),False)
    cloud.set_actor_scale3d(unreal.Vector(900,350,1))
    cloud.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_FE_ThreatHeat'))
    cloud.static_mesh_component.set_cast_shadow(False)
    pp=by_name['FE_PostProcess'];settings=pp.get_editor_property('settings')
    settings.set_editor_property('override_bloom_intensity',True);settings.set_editor_property('bloom_intensity',.65)
    settings.set_editor_property('override_bloom_threshold',True);settings.set_editor_property('bloom_threshold',1.2)
    pp.set_editor_property('settings',settings)
    if kind=='entry':
        stage=by_name['FE_MainPresentation'];mesh=stage.get_editor_property('preview_mesh')
        mesh.set_relative_location(unreal.Vector(-50,-80,5),False,False)
        views=dict(stage.get_editor_property('views'))
        for name,position,target,fov in [('Home',(75,-650,140),(75,300,80),50),('Full',(75,-650,140),(75,300,80),50),('Head',(45,-340,185),(-5,-80,167),35)]:
            view=unreal.JTSMenuCameraView();view.set_editor_property('transform',unreal.Transform(location=unreal.Vector(*position),rotation=unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*position),unreal.Vector(*target))))
            view.set_editor_property('fov',fov);views[name]=view
        stage.set_editor_property('views',views);stage.show_view('Home')
        for i in range(4):
            pad=by_name['FE_CrewPad_'+str(i)];pad.set_actor_hidden_in_game(i!=1)
            if i==1:pad.set_actor_location(unreal.Vector(-50,-80,0),False,False)
    else:
        stage=by_name['FE_LobbyPresentation']
        pawn_class=unreal.load_class(None,DEST+'/Blueprints/BP_FE_LobbyAstronaut.BP_FE_LobbyAstronaut_C')
        mesh_z=unreal.get_default_object(pawn_class).mesh.get_relative_transform().translation.z
        pad_center,pad_extent=by_name['FE_CrewPad_0'].get_actor_bounds(False)
        capsule_z=pad_center.z+pad_extent.z-mesh_z
        stage.set_editor_property('presentation_offsets',[unreal.Vector(240-i*160,-80,capsule_z) for i in range(4)])
        camera=stage.get_editor_property('lobby_camera')
        position=unreal.Vector(0,-1100,180)
        camera.set_relative_transform(unreal.Transform(location=position,rotation=unreal.MathLibrary.find_look_at_rotation(position,unreal.Vector(0,40,110))),False,False)
        camera.set_field_of_view(48)
        bp=unreal.load_asset(DEST+'/Blueprints/BP_FE_LobbyGameMode');cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('default_pawn_class',pawn_class)
        cdo.set_editor_property('require_matching_build_version',True);EA.save_loaded_asset(bp,only_if_is_dirty=False)
    assert levels.save_current_level()
    REPORT.update(map=path,ship_floor_clearance=1.0,hero_pad_aligned=True)

(ART/'presentation_polish_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
