"""MCP configuration and runtime checks for physical mining strikes."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
FX = '/Game/Space/Effects/Mining'


def sample_strike(pawn):
    strike=pawn.get_component_by_class(unreal.JTSMiningStrikeComponent)
    tip=next(c for c in pawn.get_components_by_class(unreal.SceneComponent) if c.get_name()=='WeaponMuzzle').get_world_location()
    offset=tip-pawn.get_actor_location()
    up=pawn.get_actor_up_vector()
    contact=strike.get_last_impact_location()-pawn.get_actor_location()
    forward=pawn.get_actor_forward_vector();right=pawn.get_actor_right_vector()
    dot=lambda a,b:a.x*b.x+a.y*b.y+a.z*b.z
    return dict(impact_count=strike.get_confirmed_impact_count(),impact_feedback=strike.get_impact_feedback(),
        tip_contact_error=(tip-strike.get_last_impact_location()).length(),
        tip_height=dot(offset,up),contact_local=[dot(contact,forward),dot(contact,right),dot(contact,up)],
        tip_local=[dot(offset,forward),dot(offset,right),dot(offset,up)],
        tip=dict(x=tip.x,y=tip.y,z=tip.z))


def resource_probe():
    import time
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    pawn=unreal.GameplayStatics.get_player_pawn(world,0); pc=unreal.GameplayStatics.get_player_controller(world,0)
    inventory=pawn.get_component_by_class(unreal.JTSInventoryComponent)
    interaction=pawn.get_component_by_class(unreal.InteractionComponent)
    melee=pawn.get_component_by_class(unreal.JTSMeleeComponent)
    health=pawn.get_component_by_class(unreal.JTSHealthComponent); old_health=health.get_max_health(); health.set_max_health(10000,True)
    saved=pawn.get_actor_transform(); old_view=pc.get_view_target(); movement=pawn.character_movement
    subsystem=unreal.load_object(None,'/Script/Engine.Default__SubsystemBlueprintLibrary')
    expedition=subsystem.call_method('GetGameInstanceSubsystem',(world,unreal.JTSExpeditionSubsystem.static_class()))
    old_prefix=expedition.get_editor_property('save_slot_prefix')
    expedition.set_editor_property('save_slot_prefix','MiningContactQA_'+str(int(time.time())))
    assert expedition.begin_new_expedition_in_slot(1,'Temporary mining contact validation')
    pickups_before={a.get_name() for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSWorldPickupActor)}
    mode=movement.get_editor_property('movement_mode'); movement.disable_movement()
    node=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonResourceActor)
        if a.get_node_size()==unreal.JTSMoonResourceNodeSize.LARGE_METAL_ROCK)
    mesh=node.get_editor_property('resource_mesh'); origin,extent,radius=unreal.SystemLibrary.get_component_bounds(mesh)
    up=node.get_actor_up_vector(); forward=node.get_actor_forward_vector(); right=node.get_actor_right_vector()
    planet=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetAnchor) if str(a.get_planet_id())=='Moon')
    ground=planet.project_point_to_surface(origin-forward*(radius+23))
    pos=ground.impact_point+ground.impact_normal*88
    pawn.set_actor_location(pos,False,True); pawn.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(forward,up),True)
    lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
    def spawn(cls,point,aim):
        pose=unreal.Transform(location=point,rotation=unreal.MathLibrary.make_rot_from_xz(aim-point,up))
        a=lib.call_method('BeginDeferredActorSpawnFromClass',(world,cls.static_class(),pose,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        return lib.call_method('FinishSpawningActor',(a,pose,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
    camera=spawn(unreal.CameraActor,pos+up*40,origin)
    pc.set_view_target_with_blend(camera,0)
    capture=spawn(unreal.SceneCapture2D,pos-forward*170+right*280+up*140,pos+forward*50)
    lamp=spawn(unreal.PointLight,pos+right*150+up*160,origin)
    lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
    lamp.point_light_component.set_intensity(150)
    lamp.point_light_component.set_attenuation_radius(1000)
    c=capture.capture_component2d
    tex=unreal.RenderingLibrary.create_render_target2d(world,1280,800,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    c.set_editor_property('texture_target',tex); c.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    c.set_editor_property('capture_every_frame',False); c.set_editor_property('always_persist_rendering_state',True)
    p=c.get_editor_property('post_process_settings'); p.set_editor_property('override_auto_exposure_method',True)
    p.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    p.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True); p.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    c.set_editor_property('post_process_settings',p); c.set_editor_property('post_process_blend_weight',1)
    hud=next(o for o in unreal.ObjectIterator(unreal.JTSPrototypeHUDWidget) if o.get_owning_player_pawn()==pawn)
    label=next(o for o in unreal.ObjectIterator(unreal.TextBlock) if o.get_name()=='InteractionPromptText' and o.get_path_name().startswith(hud.get_path_name()+'.'))
    result=dict(pending=True,node=node.get_name(),before=node.get_remaining_mining_work(),frames=[])
    unreal._mining_resource_result=result
    class Pending:
        def __init__(self):
            self.phase='name'; self.at=unreal.GameplayStatics.get_time_seconds(world)
            inventory.select_quickbar_slot(next(i for i in range(inventory.get_inventory_capacity()) if inventory.get_item_at_slot(i).get_editor_property('item_id')!=unreal.JTSItemId.PICKAXE))
            self.handle=unreal.register_slate_post_tick_callback(self.tick)
            melee.on_attack_started.add_callable(self.swing); self.last_swing=-100
        def swing(self,kind): self.last_swing=unreal.GameplayStatics.get_time_seconds(world)
        def image(self,name,hud_image=False):
            folder=ROOT/'Saved/MoonSurfaceV3'; folder.mkdir(exist_ok=True)
            if hud_image: unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+str(folder/(name+'.png'))+'" -nosuffix',pc)
            else:
                c.capture_scene(); unreal.RenderingLibrary.export_render_target(world,tex,str(folder),name+'.png')
            result[name]=str(folder/(name+'.png'))
        def finish(self):
            melee.attack_released(); melee.stop_attack(); melee.on_attack_started.remove_callable(self.swing)
            pc.set_view_target_with_blend(old_view,0); pawn.set_actor_transform(saved,False,True); movement.set_movement_mode(mode)
            health.set_max_health(old_health,False)
            expedition.delete_expedition_slot(1);expedition.set_editor_property('save_slot_prefix',old_prefix)
            for a in (camera,capture,lamp): a.destroy_actor()
            unreal.unregister_slate_post_tick_callback(self.handle); result['pending']=False
        def tick(self,dt):
            try:
                now=unreal.GameplayStatics.get_time_seconds(world); health.heal(100)
                if self.phase=='name':
                    if now-self.at<.5:return
                    interaction.refresh_interactable()
                    result['name_label']=str(label.get_text()); result['focused']=str(interaction.get_current_interactable())
                    assert interaction.get_current_interactable()==node,result
                    assert str(label.get_text())==str(node.get_interaction_display_name()),result
                    assert not interaction.try_interact()
                    assert node.get_remaining_mining_work()==result['before']
                    self.image('mining_solid_name',True)
                    if not inventory.get_item_count(unreal.JTSItemId.PICKAXE):
                        if not inventory.can_add_item(unreal.JTSItemId.PICKAXE,1):
                            old=inventory.get_item_at_slot(0)
                            inventory.call_method('ServerDestroyItemQuantityAtSlot',(0,old.get_editor_property('stack_count')))
                        assert inventory.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
                    inventory.select_quickbar_slot(next(i for i in range(inventory.get_inventory_capacity()) if inventory.get_item_at_slot(i).get_editor_property('item_id')==unreal.JTSItemId.PICKAXE))
                    camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(right,up),True)
                    self.at=now; self.phase='miss_wait'; return
                if self.phase in ('miss_wait','aim_wait'):
                    if now-self.at<.2:return
                    if self.phase=='aim_wait':
                        hit=unreal.SystemLibrary.line_trace_single(world,camera.get_actor_location(),origin,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
                        if hit:
                            point=lib.call_method('BreakHitResult',(hit,))[5];delta=point-pawn.get_actor_location()
                            dot=lambda a,b:a.x*b.x+a.y*b.y+a.z*b.z
                            result['aim_local']=[dot(delta,forward),dot(delta,right),dot(delta,up)]
                    self.phase='miss' if self.phase=='miss_wait' else 'strike'
                    self.at=now; melee.attack_pressed(); return
                if self.phase=='miss':
                    if now-self.at<.9:return
                    melee.attack_released(); melee.stop_attack()
                    assert node.get_remaining_mining_work()==result['before'],'Swinging away mined the node'
                    result['aimed_miss_preserves_work']=True
                    camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(origin-camera.get_actor_location(),up),True)
                    self.at=now; self.phase='aim_wait'; return
                if self.phase=='strike':
                    frame=sample_strike(pawn);frame['seconds']=now;result['frames'].append(frame)
                    phase=now-self.last_swing
                    for name,start,end in [('windup',.04,.10),('cut',.13,.18),('contact',.19,.23),('recovery',.28,.34)]:
                        key='mining_'+name
                        if start<phase<end and key not in result:self.image(key)
                    if not unreal.SystemLibrary.is_valid(node) or node.is_actor_being_destroyed():
                        result['broken']=True; melee.attack_released();melee.stop_attack()
                        self.at=now;self.phase='pickup_wait';return
                    result['remaining']=node.get_remaining_mining_work()
                    if now-self.at>3 and 'mining_chipped' not in result:self.image('mining_chipped')
                    assert now-self.at<18, 'Aimed pickaxe never completed mining'
                if self.phase=='pickup_wait':
                    if now-self.at<1:return
                    drops=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSWorldPickupActor)
                        if a.get_name() not in pickups_before and (a.get_actor_location()-origin).length()<350]
                    result['drops']=len(drops);assert drops
                    result['drop_models']=[dict(name=str(a.get_item_display_name()),item=str(a.get_item_instance().get_editor_property('item_id')),
                        mesh=a.get_component_by_class(unreal.StaticMeshComponent).get_editor_property('static_mesh').get_name()) for a in drops]
                    assert all(d['name'] in ('石头','矿物') for d in result['drop_models'])
                    assert any(d['name']=='石头' for d in result['drop_models']) and any(d['name']=='矿物' for d in result['drop_models'])
                    assert all('_Metal_Loose_' in d['mesh'] for d in result['drop_models'] if d['name']=='矿物')
                    self.pickup=min(drops,key=lambda a:(a.get_actor_location()-origin).length())
                    inventory.call_method('ServerDestroyItemQuantityAtSlot',(inventory.get_selected_quickbar_slot(),1))
                    point=self.pickup.get_actor_location()
                    pawn.set_actor_location(point-forward*100+up*70,False,True)
                    camera.set_actor_location(pawn.get_actor_location()+up*40,False,True)
                    camera.set_actor_rotation(unreal.MathLibrary.make_rot_from_xz(point-camera.get_actor_location(),up),True)
                    self.at=now;self.phase='pickup_label';return
                if self.phase=='pickup_label':
                    if now-self.at<.4:return
                    interaction.refresh_interactable();result['pickup_label']=str(label.get_text())
                    assert interaction.get_current_interactable()==self.pickup,result['pickup_label']
                    assert '[E]' in result['pickup_label'] and '\u62fe\u53d6' in result['pickup_label'],result['pickup_label']
                    self.image('mining_loose_pickup',True)
                    self.at=now;self.phase='pickup_confirm';return
                if self.phase=='pickup_confirm':
                    if now-self.at<.3:return
                    assert interaction.try_interact()
                    result['pickup_succeeds']=True;self.finish();return
            except Exception:
                import traceback
                result['error']=traceback.format_exc();self.finish()
    unreal._mining_resource_probe=Pending()
    return result


def configure_loose_minerals():
    import moon_surface_unreal as s
    import moon_surface_refinement as resources
    rows=json.loads((ROOT/'SourceArt/MoonResources/loose_minerals_manifest.json').read_text())['assets']
    s.import_tasks([s.fbx_task(ROOT/'SourceArt/MoonResources/FBX'/(r['name']+'.fbx'),resources.MESHES,r['name']) for r in rows])
    sm=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in rows:
        mesh=unreal.load_asset(resources.MESHES+'/'+row['name']);assert mesh
        slots=list(mesh.get_editor_property('static_materials'));assert len(slots)==1
        role='Metal' if '_Metal_' in row['name'] else 'Crystal'
        slots[0].set_editor_property('material_interface',unreal.load_asset(resources.MATERIALS+'/M_MR_'+role))
        mesh.set_editor_property('static_materials',slots)
        mesh.set_editor_property('allow_cpu_access',True)
        sm.remove_collisions(mesh);assert sm.set_convex_decomposition_collisions(mesh,1,16,100000)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    for item,kind in (('Rock','Rock'),('Ore','Metal'),('Crystal','Crystal')):
        data=unreal.load_asset('/Game/Space/Data/Items/DA_Item_'+item)
        meshes=resources.variants(kind,'Small' if item=='Rock' else 'Loose')
        data.set_editor_property('world_pickup_mesh',meshes[0]);data.set_editor_property('world_pickup_mesh_variants',meshes)
        data.set_editor_property('world_pickup_width',32 if item=='Rock' else 23)
        data.set_editor_property('display_name','石头' if item=='Rock' else '矿物')
        data.set_editor_property('description','凿开石头后散落的独立小石块。' if item=='Rock' else '从含矿石头中凿出的独立矿物块，不含石头底座。')
        assert unreal.EditorAssetLibrary.save_loaded_asset(data)
    for item,name in (('Fuel','燃料'),('Water','水'),('Food','食物'),('MoonAntCorpse','镜虫尸体')):
        data=unreal.load_asset('/Game/Space/Data/Items/DA_Item_'+item)
        if data:
            data.set_editor_property('display_name',name);assert unreal.EditorAssetLibrary.save_loaded_asset(data)
    resources.check_protected()
    return dict(pure_mineral_variants=len(rows))


def configure():
    assert not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    # Existing loose mineral assets are independent from strike presentation.
    tasks=[]
    for file in sorted((ROOT/'SourceAssets/Mining').iterdir()):
        task=unreal.AssetImportTask(); task.set_editor_property('filename',str(file))
        task.set_editor_property('destination_path',FX if file.suffix=='.png' else '/Game/Space/Audio/Mining')
        task.set_editor_property('automated',True); task.set_editor_property('replace_existing',True); task.set_editor_property('save',True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    texture=unreal.load_asset(FX+'/T_MiningContactScar')
    texture.set_editor_property('srgb',False)
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    material=unreal.load_asset(FX+'/M_MiningContactScar')
    lib=unreal.MaterialEditingLibrary
    if not material:
        material=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MiningContactScar',FX,unreal.Material,unreal.MaterialFactoryNew())
        material.set_editor_property('material_domain',unreal.MaterialDomain.MD_DEFERRED_DECAL)
        material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        mask=lib.create_material_expression(material,unreal.MaterialExpressionTextureSample)
        mask.set_editor_property('texture',texture)
        mask.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        color=lib.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant',unreal.LinearColor(.045,.036,.027))
        lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
        lib.connect_material_property(mask,'A',unreal.MaterialProperty.MP_OPACITY)
        lib.recompile_material(material)
    attenuation=unreal.load_asset(FX+'/SA_MiningContact')
    if not attenuation:
        attenuation=unreal.AssetToolsHelpers.get_asset_tools().create_asset('SA_MiningContact',FX,unreal.SoundAttenuation,unreal.SoundAttenuationFactory())
    settings=attenuation.get_editor_property('attenuation')
    settings.set_editor_property('attenuation_shape_extents',unreal.Vector(800,0,0))
    settings.set_editor_property('falloff_distance',1800.)
    settings.set_editor_property('spatialize',True)
    attenuation.set_editor_property('attenuation',settings)
    spark=unreal.load_asset(FX+'/M_MiningSpark')
    if not spark:
        spark=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MiningSpark',FX,unreal.Material,unreal.MaterialFactoryNew())
        spark.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        glow=lib.create_material_expression(spark,unreal.MaterialExpressionConstant3Vector)
        glow.set_editor_property('constant',unreal.LinearColor(18,7,1.2))
        lib.connect_material_property(glow,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.recompile_material(spark)
    dust=unreal.load_asset(FX+'/NS_MiningContactDust')
    if not dust: dust=unreal.EditorAssetLibrary.duplicate_asset('/Game/Space/Effects/Moon/NS_EntranceCollapseDust',FX+'/NS_MiningContactDust')
    item=unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe')
    item.set_editor_property('directed_mining_swing',True)
    item.set_editor_property('mining_swing_duration',.32)
    item.set_editor_property('mining_contact_fraction',.48)
    item.set_editor_property('mining_reach',180.)
    item.set_editor_property('melee_attack_interval',.32)
    item.set_editor_property('mining_swing_sound',unreal.load_asset('/Game/Space/Audio/Mining/S_PickaxeWhoosh'))
    item.set_editor_property('mining_swing_attenuation',attenuation)
    profile=item.get_editor_property('held_presentation')
    if not profile.get_editor_property('mesh'):
        profile.set_editor_property('body_scale',unreal.Vector(.96,.045,.045))
        profile.set_editor_property('body_location',unreal.Vector(22,0,0))
        profile.set_editor_property('barrel_scale',unreal.Vector(.08,.08,.44))
        profile.set_editor_property('barrel_location',unreal.Vector(70,0,0))
        profile.set_editor_property('muzzle_transform',unreal.Transform(location=unreal.Vector(70,0,-22)))
    item.set_editor_property('held_presentation',profile)
    chip=unreal.load_asset('/Game/Space/Environment/MoonResources/Meshes/SM_MR_Rock_Medium_01')
    if not chip: chip=unreal.load_asset('/Engine/BasicShapes/Cube')
    def feedback(actor,soil):
        impact=actor.get_component_by_class(unreal.JTSMiningImpactComponent)
        assert impact,actor.get_path_name()
        prefix='S_SoilStrike_' if soil else 'S_StoneChip_'
        impact.set_editor_property('impact_sounds',[unreal.load_asset('/Game/Space/Audio/Mining/'+prefix+s) for s in ('A','B')])
        impact.set_editor_property('break_sound',unreal.load_asset('/Game/Space/Audio/Mining/'+('S_SoilCollapse' if soil else 'S_RockBreak')))
        impact.set_editor_property('impact_attenuation',attenuation)
        impact.set_editor_property('impact_dust',dust)
        impact.set_editor_property('dust_scale',.13 if soil else .10)
        impact.set_editor_property('scar_material',material)
        impact.set_editor_property('fragment_mesh',chip)
        impact.set_editor_property('spark_mesh',unreal.load_asset('/Engine/BasicShapes/Cube'))
        impact.set_editor_property('spark_material',spark)
        impact.set_editor_property('spark_count',3 if soil else 10)
        mesh=actor.get_editor_property('refuge_mesh' if soil else 'resource_mesh')
        tags=list(mesh.get_editor_property('component_tags'))
        if 'MiningSurface' not in [str(t) for t in tags]: tags.append('MiningSurface')
        mesh.set_editor_property('component_tags',tags)
    for path,soil in [('/Game/Space/Blueprints/Modes/Moon/BP_MoonResourceActor',False),
            ('/Game/Space/Blueprints/Modes/BP_MoonResourceActor',False),
            ('/Game/Space/Blueprints/Planets/Moon/EcologyV3/BP_MoonHabitat',True)]:
        cls=unreal.EditorAssetLibrary.load_blueprint_class(path)
        if not cls and path == '/Game/Space/Blueprints/Modes/BP_MoonResourceActor': continue
        assert cls,path
        feedback(unreal.get_default_object(cls),soil)
        assert unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(path))
    nests=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label().startswith('Moon_Nest_')]
    assert len(nests)==9
    for nest in nests: nest.modify(); feedback(nest,True)
    for asset in (texture,material,attenuation,dust,item,spark): assert unreal.EditorAssetLibrary.save_loaded_asset(asset)
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
    return dict(configured=True,nests=9,stroke_seconds=.32)


def run(stage):
    if stage == 'pickaxe_configure':
        import importlib, mining_pickaxe_unreal
        importlib.reload(mining_pickaxe_unreal)
        try: return json.dumps(mining_pickaxe_unreal.configure())
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
    if stage.startswith('forearm_'):
        import importlib, mining_forearm_probe
        importlib.reload(mining_forearm_probe)
        try: return json.dumps(mining_forearm_probe.run(stage[8:]))
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
    if stage == 'item':
        i=unreal.load_asset('/Game/Space/Data/Items/DA_Item_Pickaxe');p=i.get_editor_property('held_presentation')
        return json.dumps(dict(profile=str(p),directed=i.get_editor_property('directed_mining_swing')))
    if stage == 'resource_probe': return json.dumps(resource_probe())
    if stage == 'resource_result': return json.dumps(getattr(unreal,'_mining_resource_result',None))
    if stage == 'configure':
        try: return json.dumps(configure())
        except Exception:
            import traceback
            return json.dumps(dict(error=traceback.format_exc()))
    if stage == 'inspect':
        assets = unreal.EditorAssetLibrary.list_assets('/Game/Space', recursive=True)
        paths = [p for p in assets if any(k in p for k in ('Pickaxe', 'MoonResourceActor', 'MoonHabitat', 'Punch', 'Impact'))]
        return json.dumps(dict(assets=paths))
    if stage == 'bones':
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        mesh = pawn.mesh
        inv = pawn.get_component_by_class(unreal.JTSInventoryComponent)
        return json.dumps(dict(origin=str(pawn.get_actor_location()), up=str(pawn.get_actor_up_vector()),
            bones={n:str(mesh.get_socket_location(n)-pawn.get_actor_location()) for n in ('UpperArm_R','LowerArm_R','Wrist_R','Index1_R')},
            active=str(inv.get_active_item_id())))
    if stage == 'contact_debug':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0);pc=unreal.GameplayStatics.get_player_controller(world,0)
        cam=pc.player_camera_manager
        hit=unreal.SystemLibrary.line_trace_single(world,cam.get_camera_location(),cam.get_camera_location()+cam.get_camera_rotation().get_forward_vector()*1400,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        return json.dumps(dict(pawn=str(pawn.get_actor_transform()),camera=str(cam.get_camera_location()),hit=str(lib.call_method('BreakHitResult',(hit,))) if hit else None))
    if stage == 'debug_nest':
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        inv=pawn.get_component_by_class(unreal.JTSInventoryComponent)
        if not inv.get_item_count(unreal.JTSItemId.PICKAXE): assert inv.try_add_item_by_id(unreal.JTSItemId.PICKAXE,1)
        inv.select_quickbar_slot(next(i for i in range(inv.get_inventory_capacity()) if inv.get_item_at_slot(i).get_editor_property('item_id')==unreal.JTSItemId.PICKAXE))
        nest=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label()=='Moon_Nest_01')
        mesh=nest.get_editor_property('refuge_mesh'); saved=pawn.get_actor_transform(); rows=[]
        origin,extent,radius=unreal.SystemLibrary.get_component_bounds(mesh)
        lib=unreal.get_default_object(unreal.GameplayStatics.static_class())
        for distance in (210,180,160,140,120):
            pos=mesh.get_world_location()-mesh.get_forward_vector()*distance+mesh.get_up_vector()*100
            pawn.set_actor_location(pos,False,True)
            start=pos+pawn.get_actor_up_vector()*35
            hit=unreal.SystemLibrary.line_trace_single(world,start,origin,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,True,[pawn],unreal.DrawDebugTrace.NONE,True)
            rows.append(dict(distance=distance,can_execute=nest.call_method('CanExecuteInteraction',(pawn,)),hit=str(lib.call_method('BreakHitResult',(hit,))) if hit else None,
                center_distance=(origin-pos).length()))
        pawn.set_actor_transform(saved,False,True)
        return json.dumps(dict(rows=rows,bounds=str((origin,extent,radius)),mesh=mesh.get_editor_property('static_mesh').get_path_name()))
    raise ValueError(stage)
