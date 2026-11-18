"""Level-only terrain placement repair and player-camera interaction verification."""
import unreal as u, math, json
from pathlib import Path

def vec(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]
def context():
 sub=u.get_editor_subsystem(u.EditorActorSubsystem)
 aa={a.get_actor_label():a for a in sub.get_all_level_actors()}
 p=next(a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
 return sub,aa,p

def sample(p,basis,x,y):
 h=p.project_point_to_surface(basis.transform_location(u.Vector(x,y,0)))
 return h

def scan(p,basis):
 rows=[]
 for x in range(-6500,6501,500):
  for y in range(-6500,6501,500):
   h=sample(p,basis,x,y)
   if not h.blocking_hit:continue
   normal=h.impact_normal;radial=p.get_radial_up_vector(h.impact_point)
   slope=math.degrees(math.acos(max(-1,min(1,normal.dot(radial)))))
   if slope>9:continue
   rot=u.MathLibrary.make_rot_from_zx(normal,basis.rotation.get_axis_x())
   t=u.Transform(location=h.impact_point,rotation=rot,scale=u.Vector(1,1,1))
   heights=[];slopes=[];bad=False
   for r,count in ((1000,8),(2100,16),(2650,16)):
    for i in range(count):
     v=t.transform_location(u.Vector(r*math.cos(i*math.tau/count),r*math.sin(i*math.tau/count),0))
     s=p.project_point_to_surface(v)
     if not s.blocking_hit:bad=True;break
     heights.append((s.impact_point-h.impact_point).dot(normal));slopes.append(s.impact_normal.dot(normal))
   if bad or not heights:continue
   spread=max(heights)-min(heights)
   if spread>130 or min(slopes)<.97:continue
   rows.append({'x':x,'y':y,'height_min':min(heights),'height_max':max(heights),'slope':slope,'spread':spread,'world':vec(h.impact_point),'rotation':str(rot),'score':math.hypot(x,y)+spread*10})
 rows.sort(key=lambda r:r['score'])
 path=Path(u.Paths.project_saved_dir())/'MarsSitingCandidates.json';path.write_text(json.dumps(rows,indent=2),encoding='utf8')
 return {'action':'scan','candidates':rows[:12],'count':len(rows)}

def place(a,t,p):
 a.detach_from_actor(u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD)
 a.set_actor_transform(t,False,False)
 a.attach_to_actor(p,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)

def terrain_fit(p,t,hx,hy,step=100):
 values=[];slope=[]
 for x in (-hx,-hx*.5,0,hx*.5,hx):
  for y in (-hy,-hy*.5,0,hy*.5,hy):
   v=t.transform_location(u.Vector(x,y,0));h=p.project_point_to_surface(v)
   if not h.blocking_hit:return None
   values.append((h.impact_point-t.translation).dot(t.rotation.get_axis_z()))
   slope.append(h.impact_normal.dot(t.rotation.get_axis_z()))
 return (min(values),max(values),min(slope))

def choose_small_site(p,basis,origin,hx,hy,occupied):
 forward=basis.rotation.get_axis_x();rows=[]
 for dx in range(-2100,2101,150):
  for dy in range(-2100,2101,150):
   x,y=origin.x+dx,origin.y+dy
   if any(abs(x-a)<hx+rx+55 and abs(y-b)<hy+ry+55 for a,b,rx,ry in occupied):continue
   h=sample(p,basis,x,y)
   if not h.blocking_hit:continue
   n=h.impact_normal;rad=p.get_radial_up_vector(h.impact_point)
   if n.dot(rad)<.985:continue
   rot=u.MathLibrary.make_rot_from_zx(n,forward);t=u.Transform(location=h.impact_point,rotation=rot,scale=u.Vector(1,1,1))
   fit=terrain_fit(p,t,hx+85,hy+85)
   if not fit or fit[1]-fit[0]>28 or fit[2]<.995:continue
   # Keep a continuous level work area, not just one supported centre point.
   rows.append((math.hypot(dx,dy)+(fit[1]-fit[0])*3,t,fit,x,y))
 if not rows:raise RuntimeError('No shallow, pit-free site near '+str(vec(origin)))
 rows.sort(key=lambda r:r[0]);_,t,fit,x,y=rows[0];t.translation=t.translation+t.rotation.get_axis_z()*(fit[1]+1)
 occupied.append((x,y,hx,hy));return t,fit,math.hypot(x-origin.x,y-origin.y)

def plan_siting(aa,p,basis):
 h=sample(p,basis,-2500,2500);assert h.blocking_hit
 # Face the landing clearing, away from the old front crater.
 rot=u.MathLibrary.make_rot_from_zx(h.impact_normal,basis.rotation.get_axis_y())
 new=u.Transform(location=h.impact_point+h.impact_normal*10,rotation=rot,scale=u.Vector(1,1,1))
 occupied=[(-2500,2500,2050,2050),(2484,2904,900,900)];modules=[];combiners=[]
 solar=sorted([(k,a) for k,a in aa.items() if k.startswith('MC_Solar_') and isinstance(a,u.StaticMeshActor)])
 for k,a in solar:
  origin=basis.inverse_transform_location(a.get_actor_location());t,fit,shift=choose_small_site(p,basis,origin,280,550,occupied)
  modules.append((k,t,fit,shift))
 for sector in 'ABC':
  a=aa['MC_Sector_'+sector];origin=basis.inverse_transform_location(a.get_actor_location());t,fit,shift=choose_small_site(p,basis,origin,150,170,occupied)
  heading=new.translation-t.translation;n=t.rotation.get_axis_z();t.rotation=u.MathLibrary.make_rot_from_zx(n,heading).quaternion()
  combiners.append((sector,t,fit,shift))
 return new,modules,combiners

def repair_siting(sub,aa,p,basis):
 if 'MarsSitingV2' in [str(x) for x in aa['MC_R_Lift_Housing'].tags]:return {'action':'apply','already_applied':True}
 # Calculate every supported footprint before changing any level actor.
 new,solar_plan,combiner_plan=plan_siting(aa,p,basis)
 removed=[];moved=[]
 retired=('MC_R_Dome_Pier_','MC_R_Dome_Stair_','MC_R_SolarFoot_','MC_R_Dome_Breach_','MC_R_DomeLift_FeedCover_')
 for k,a in list(aa.items()):
  if k.startswith(retired) or k in ('MC_R_Flow_BreachBarrier','MC_R_Dome_Airlock','MC_R_Dome_Entry'):
   sub.destroy_actor(a);aa.pop(k);removed.append(k)
 for k,a in aa.items():
  is_dome=k.startswith(('MC_R_Dome','MC_R_Flow_','MC_R_Sign_Dome')) or k in ('MC_R_Lift_Housing','MC_R_EcologyEntranceDoor','MC_R_StationAccessDoor','MC_R_StationAccessLamp','MC_Station_Elevator','MC_Station_Arrival','MC_Power','MC_Dome_Lantern','MC_R_Guide_DomeOperations','MC_R_Guide_Arrival','MC_R_GuideStand_Arrival','MC_R_Recovery_ServiceParts','MC_R_Recovery_SeedReserve','MC_R_Recovery_WaterReserve')
  if not is_dome:continue
  old=a.get_actor_transform();local=basis.inverse_transform_location(old.translation)
  relative_rotation=basis.rotation.inversed()*old.rotation
  t=u.Transform(location=new.transform_location(local),rotation=(new.rotation*relative_rotation).rotator(),scale=old.scale3d)
  place(a,t,p);moved.append(k)
 root='/Game/Space/Environment/MarsSystem/Chapter/Refined'
 for label,key in (('MC_R_Dome_Ribs','Flow_DomeRibs'),('MC_R_Dome_Glazing','Flow_DomeGlazing'),('MC_R_Dome_Footing','Flow_DomeFooting'),('MC_R_Flow_DomeSeal','Flow_DomeSeal')):
  a=aa[label];a.static_mesh_component.set_static_mesh(u.load_asset(root+'/SM_MP_'+key));a.set_actor_location(new.translation,False,False)
 for label,key in (('MC_R_Flow_ConcreteFoundation','Flow_ConcreteFoundation'),('MC_R_Flow_EntryRamp','Flow_EntryRamp')):
  a=aa.get(label) or sub.spawn_actor_from_class(u.StaticMeshActor,new.translation,new.rotation.rotator());a.set_actor_label(label);a.set_folder_path('MarsChapter/StationFlow')
  a.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE);a.static_mesh_component.set_static_mesh(u.load_asset(root+'/SM_MP_'+key));a.static_mesh_component.set_collision_profile_name('BlockAll');place(a,new,p);a.set_owner(p);a.tags=['PlanetSurfaceContent'];aa[label]=a
 concrete=u.load_asset(root+'/M_MP_Concrete')
 if not concrete:
  concrete=u.AssetToolsHelpers.get_asset_tools().create_asset('M_MP_Concrete',root,u.Material,u.MaterialFactoryNew())
  color=u.MaterialEditingLibrary.create_material_expression(concrete,u.MaterialExpressionConstant3Vector,-300,0);color.set_editor_property('constant',u.LinearColor(.29,.30,.27,1));u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
  rough=u.MaterialEditingLibrary.create_material_expression(concrete,u.MaterialExpressionConstant,-300,140);rough.set_editor_property('r',.94);u.MaterialEditingLibrary.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS);u.MaterialEditingLibrary.recompile_material(concrete);u.EditorAssetLibrary.save_loaded_asset(concrete,False)
 for label in ('MC_R_Flow_ConcreteFoundation','MC_R_Flow_EntryRamp'):aa[label].static_mesh_component.set_material(0,concrete)
 modules=[]
 for k,t,fit,shift in solar_plan:
  place(aa[k],t,p)
  modules.append({'label':k,'shift_cm':round(shift,1),'terrain_spread_cm':round(fit[1]-fit[0],1)})
 combiners=[]
 for sector,t,fit,shift in combiner_plan:
  a=aa['MC_Sector_'+sector];place(a,t,p)
  combiners.append({'sector':sector,'shift_cm':round(shift,1),'terrain_spread_cm':round(fit[1]-fit[0],1),'new_local':vec(basis.inverse_transform_location(a.get_actor_location()))})
  # Keep the sector's guide, stand and sign next to its accessible work area.
  for name,offset in (('MC_R_Guide_Sector'+sector,u.Vector(0,250,110)),('MC_R_GuideStand_Sector'+sector,u.Vector(0,250,0)),('MC_R_Sign_Sector'+sector,u.Vector(0,-230,240))):
   if name in aa:
    target=aa[name];q=u.Transform(location=t.transform_location(offset),rotation=t.rotation.rotator(),scale=target.get_actor_scale3d());place(target,q,p)
 aa['MC_R_Lift_Housing'].tags=list(aa['MC_R_Lift_Housing'].tags)+['MarsSitingV2']
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return {'action':'apply','dome_shift_cm':round((new.translation-basis.translation).length(),1),'removed_piers_stairs_and_grille':len(removed),'moved_dome_actors':len(moved),'solar':modules,'combiners':combiners}

def run(action):
 if action.startswith('runtime_'):return runtime(action[8:])
 sub,aa,p=context();basis=aa['MC_R_Lift_Housing'].get_actor_transform()
 if action=='entry_objects':
  gate=aa['MC_R_EcologyEntranceDoor'];t=gate.get_actor_transform()
  return {'action':action,'actors':[{'label':k,'local':vec(t.inverse_transform_location(a.get_actor_location())),'mesh':a.get_component_by_class(u.StaticMeshComponent).static_mesh.get_name() if a.get_component_by_class(u.StaticMeshComponent) and a.get_component_by_class(u.StaticMeshComponent).static_mesh else None} for k,a in aa.items() if (a.get_actor_location()-gate.get_actor_location()).length()<650]}
 if action=='entry_layout':
  # Keep the existing narrative props, clear of the complete entrance approach.
  gate=aa['MC_R_EcologyEntranceDoor'].get_actor_transform()
  for label,offset in (('MC_SupervisorRemains_5',(410,680,0)),('MC_R_LogStand_MC_Log_DoorSkeleton',(320,520,0)),('MC_Log_DoorSkeleton',(320,520,110))):
   hit=p.project_point_to_surface(gate.transform_location(u.Vector(*offset)));rot=u.MathLibrary.make_rot_from_zx(hit.impact_normal,gate.rotation.get_axis_x());pos=hit.impact_point+hit.impact_normal*(offset[2]+1)
   place(aa[label],u.Transform(location=pos,rotation=rot,scale=aa[label].get_actor_scale3d()),p)
  for label,offset in (('MC_R_LogStand_MC_Log_Director',(-830,-1370,0)),('MC_Log_Director',(-830,-1370,110))):
   place(aa[label],u.Transform(location=basis.transform_location(u.Vector(*offset)),rotation=basis.rotation.rotator(),scale=aa[label].get_actor_scale3d()),p)
  u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level();return {'action':action,'entrance_clear_width_cm':580}
 if action=='assets':
  import runpy
  source=runpy.run_path(str(Path(__file__).with_name('rebuild_mars_station_flow.py')));meshes=source['import_props']()
  bp=u.load_asset('/Game/Space/Ships/Docks/BP_Ship_CentralTerminal');cdo=u.get_default_object(bp.generated_class())
  cdo.set_editor_property('credential_dock_transform',u.Transform(location=u.Vector(110,30,-10),rotation=u.Rotator(yaw=90)))
  u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,False)
  return {'action':action,'imported':list(meshes)}
 if action=='inspect':
  return {'action':action,'planet':{'center':vec(p.get_planet_center()),'radius':p.get_approximate_radius(),'surface':str(p.get_gameplay_surface_actor())},'objects':[
   {'label':k,'class':a.get_class().get_name(),'world':vec(a.get_actor_location()),'local':vec(basis.inverse_transform_location(a.get_actor_location())),'rotation':str(a.get_actor_rotation()),'scale':vec(a.get_actor_scale3d()),'bounds':str(a.get_actor_bounds(False)), 'meshes':[(c.get_name(),c.static_mesh.get_name() if c.static_mesh else None,vec(c.get_world_location()),str(c.get_collision_enabled())) for c in a.get_components_by_class(u.StaticMeshComponent)]}
   for k,a in aa.items() if (k.startswith('MC_') and any(x in k for x in ('Dome','Foundation','Support','Stair','Sector_','Solar','Landing','Lift_Housing'))) or k.startswith('Mars_Site')], 'pie':bool(u.EditorLevelLibrary.get_pie_worlds(True))}
 if action=='scan':return scan(p,basis)
 if action=='apply':return repair_siting(sub,aa,p,basis)
 if action=='plan':
  new,solar,combiners=plan_siting(aa,p,basis)
  return {'action':action,'solar_count':len(solar),'solar_max_shift_cm':max(s[3] for s in solar),'combiners':[{'sector':s[0],'shift_cm':s[3],'fit':s[2]} for s in combiners]}
 if action=='audit':
  heights=[];entry=[]
  for x in range(-2000,2001,200):
   for y in range(-2000,2001,200):
    if x*x+y*y>2030**2:continue
    hit=sample(p,basis,x,y);heights.append(basis.inverse_transform_location(hit.impact_point).z)
  for y in range(-3500,-1799,100):
   hit=sample(p,basis,0,y);entry.append({'y':y,'z':round(basis.inverse_transform_location(hit.impact_point).z,1),'slope':round(math.degrees(math.acos(max(-1,min(1,hit.impact_normal.dot(p.get_radial_up_vector(hit.impact_point)))))),1)})
  props=[]
  for k,a in aa.items():
   if k.startswith(('MC_Solar_','MC_Sector_')) and isinstance(a,(u.StaticMeshActor,u.JTSFacilityController)):
    hit=p.project_point_to_surface(a.get_actor_location());props.append({'label':k,'ground_gap_cm':round((a.get_actor_location()-hit.impact_point).dot(a.get_actor_up_vector()),1),'slope':round(math.degrees(math.acos(max(-1,min(1,hit.impact_normal.dot(p.get_radial_up_vector(hit.impact_point)))))),1)})
  return {'action':action,'dome_ground_min_max_cm':[min(heights),max(heights)],'entry_terrain':entry,'props':props,'retired_remaining':[k for k in aa if k.startswith(('MC_R_Dome_Pier_','MC_R_Dome_Stair_','MC_R_SolarFoot_')) or k=='MC_R_Flow_BreachBarrier']}
 if action.startswith('view_'):
  key=action[5:];target=aa['MC_Sector_'+key] if key in 'ABC' else aa['MC_R_Lift_Housing']
  t=target.get_actor_transform();offset=u.Vector(900,900,650) if key in 'ABC' else u.Vector(4700,-3700,3200)
  pos=t.transform_location(offset);aim=target.get_actor_location()+target.get_actor_up_vector()*170
  u.EditorLevelLibrary.set_level_viewport_camera_info(pos,u.MathLibrary.make_rot_from_xz(aim-pos,target.get_actor_up_vector()));u.EditorLevelLibrary.editor_set_game_view(True)
  return {'action':action}
 raise ValueError(action)

def runtime(action):
 worlds=u.EditorLevelLibrary.get_pie_worlds(True);w=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
 aa=u.GameplayStatics.get_all_actors_of_class(w,u.Actor);pc=u.GameplayStatics.get_player_controller(w,0);pawn=next(a for a in aa if isinstance(a,u.JTSCharacter) and a.get_controller()==pc);device=next(a for a in aa if isinstance(a,u.JTSPortableCredentialDock));ship=u.GameplayStatics.get_game_state(w).get_active_spacecraft()
 camera=u.GameplayStatics.get_player_camera_manager(w,0);center=device.get_interaction_target_world_location();up=device.get_actor_up_vector();normal=device.get_owner().screen_anchor.get_right_vector();planet=pawn.get_gameplay_planet()
 gate=next(a for a in aa if a.get_actor_label()=='MC_R_EcologyEntranceDoor')
 input_sub=next((s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if isinstance(s.get_outer(),u.LocalPlayer) and s.get_outer().get_world()==w),None)
 if action.startswith('debug_'):
  widget=next(t for t in u.ObjectIterator(u.JTSSpaceDebugWidget) if t.get_world()==w and t.is_in_viewport()==False and t.get_owning_player()==pc)
  if action=='debug_open':widget.call_method('ToggleSidebar',())
  elif action=='debug_invalid':pc.call_method('ServerRequestDebugAccessPermit',('UnconfiguredPermit',))
  elif action=='debug_grant':
   button=next(t for t in u.ObjectIterator(u.JTSDebugPermitButton) if t.get_world()==w and t.get_name()=='DebugPermit_MarsEcologyAccess' and t.get_outer()==widget.get_editor_property('widget_tree'))
   button.call_method('Activate',())
  return {'action':action,'buttons':[{'name':t.get_name(),'enabled':t.get_is_enabled(),'world':t.get_world().get_path_name()} for t in u.ObjectIterator(u.Button) if t.get_world() in worlds and t.get_name() in ('DebugSidebarToggle','DebugSidebarClose','DebugPermit_MarsEcologyAccess')], 'shared_permit':u.GameplayStatics.get_game_state(w).has_important_item('MarsEcologyAccess'),'unconfigured_granted':u.GameplayStatics.get_game_state(w).has_important_item('UnconfiguredPermit'),'payload':[str(k) for k in pawn.get_inventory_component().get_active_item().credential_ids], 'replicas':[{'authority':u.GameplayStatics.get_game_state(v).has_authority(),'player':p.player_state.get_online_identity_string(),'payload':[str(k) for k in p.get_inventory_component().get_active_item().credential_ids]} for v in worlds for p in u.GameplayStatics.get_all_actors_of_class(v,u.JTSCharacter) if p.player_state and p.player_state.get_online_identity_string()==pawn.player_state.get_online_identity_string()]}
 if action=='hud':return {'action':action,'prompts':[{'text':str(t.get_text()),'visible':t.is_visible(),'visibility':str(t.get_visibility()),'position':str(t.slot.get_position()),'anchors':str(t.slot.get_anchors()),'alignment':str(t.slot.get_alignment()),'device_anchor':str(device.get_interaction_prompt_anchor()),'projected_device_anchor':str(u.WidgetLayoutLibrary.project_world_location_to_widget_position(pc,device.get_interaction_prompt_anchor(),True))} for t in u.ObjectIterator(u.TextBlock) if t.get_world()==w and t.get_name()=='InteractionPromptText'],'notice_count':len([n for n in u.ObjectIterator(u.JTSExpeditionNoticeWidget) if n.get_world()==w and n.is_in_viewport()])}
 if action=='reader_status':
  interaction=pawn.get_component_by_class(u.InteractionComponent);interaction.refresh_interactable();target=interaction.get_current_interactable()
  return {'action':action,'target':target.get_name() if target else None,'prompt':str(interaction.get_current_interaction_prompt()),'can_scan':gate.can_execute_interaction(pawn),'state':str(gate.get_editor_property('reader_state')),'open':gate.get_editor_property('open'),'payload':[str(k) for k in pawn.get_component_by_class(u.JTSInventoryComponent).get_active_item().credential_ids],'shared_permit':u.GameplayStatics.get_game_state(w).has_important_item('MarsEcologyAccess'),'anchor':str(gate.get_interaction_prompt_anchor()),'projected':str(u.WidgetLayoutLibrary.project_world_location_to_widget_position(pc,gate.get_interaction_prompt_anchor(),True)),'hud':[(str(t.get_text()),str(t.slot.get_position())) for t in u.ObjectIterator(u.TextBlock) if t.get_world()==w and t.get_name()=='InteractionPromptText'],'worlds':[{'authority':u.GameplayStatics.get_game_state(v).has_authority(),'state':str(a.get_editor_property('reader_state')),'open':a.get_editor_property('open'),'status_material':a.status_light.get_material(0).get_name()} for v in worlds for a in u.GameplayStatics.get_all_actors_of_class(v,u.JTSAutomaticAccessDoor) if a.get_actor_label()=='MC_R_EcologyEntranceDoor']}
 if action in ('input_key','input_tap','input_reader_tap'):
  began=u.GameplayStatics.get_time_seconds(w);state={'handle':None,'ticks':0};key=u.find_object(pawn,'InteractAction')
  history=[]
  def step(dt):
   held=u.GameplayStatics.get_time_seconds(w)-began<1.8 if action=='input_key' else state['ticks']==0
   input_sub.inject_input_vector_for_action(key,u.Vector(1 if held else 0,0,0),[],[])
   state['ticks']+=1
   if action=='input_reader_tap':
    c=pawn.get_component_by_class(u.InteractionComponent);history.append({'elapsed':u.GameplayStatics.get_time_seconds(w)-began,'state':str(gate.get_editor_property('reader_state')),'prompt':str(c.get_current_interaction_prompt()),'open':gate.get_editor_property('open'),'worlds':[{'authority':u.GameplayStatics.get_game_state(v).has_authority(),'state':str(a.get_editor_property('reader_state')),'open':a.get_editor_property('open'),'material':a.status_light.get_material(0).get_name()} for v in worlds for a in u.GameplayStatics.get_all_actors_of_class(v,u.JTSAutomaticAccessDoor) if a.get_actor_label()=='MC_R_EcologyEntranceDoor']})
    (Path(u.Paths.project_saved_dir())/'ReaderInputTrace.json').write_text(json.dumps(history,ensure_ascii=False,indent=2),encoding='utf8')
   if not held and (action!='input_reader_tap' or u.GameplayStatics.get_time_seconds(w)-began>3):u.unregister_slate_post_tick_callback(state['handle'])
  state['handle']=u.register_slate_post_tick_callback(step)
 if action=='approach_reader':
  p=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars');pos=gate.get_actor_transform().transform_location(u.Vector(180,-220,98));rot=u.MathLibrary.make_rot_from_zx(p.get_radial_up_vector(pos),u.SystemLibrary.get_component_bounds(gate.reader)[0]-pos)
  pawn.set_gameplay_planet(p);pawn.set_actor_location_and_rotation(pos,rot,False,False);pawn.initialize_planet_frame();pawn.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately();pc.set_view_target_with_blend(pawn,0)
 elif action.startswith('approach'):
  side=0 if action=='approach' else -55 if action=='approach_dual' else int(action.split('_')[-1]);p=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars');hit=p.project_point_to_surface(center+normal*(100 if action=='approach_dual' else 130)+device.get_owner().get_actor_forward_vector()*side)
  pos=hit.impact_point+p.get_radial_up_vector(hit.impact_point)*96;rot=u.MathLibrary.make_rot_from_zx(p.get_radial_up_vector(pos),center-pos)
  pawn.set_gameplay_planet(p);pawn.set_actor_location_and_rotation(pos,rot,False,False);pawn.initialize_planet_frame();pawn.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately();pc.set_view_target_with_blend(pawn,0)
 if action in ('aim','aim_terminal','aim_reader'):
  # Inject the same local look actions as the mouse; keep the real third-person camera.
  if action=='aim_terminal':center=device.get_owner().get_screen_world_location()
  if action=='aim_reader':center=u.SystemLibrary.get_component_bounds(gate.reader)[0]
  view=camera.get_camera_location();f=camera.get_camera_rotation().get_forward_vector();n=pawn.get_actor_up_vector();want=(center-view).normal();ft=(f-n*f.dot(n)).normal();wt=(want-n*want.dot(n)).normal()
  yaw=math.degrees(math.atan2(n.dot(ft.cross(wt)),ft.dot(wt)));pitch=math.degrees(math.asin(max(-1,min(1,want.dot(n))))-math.asin(max(-1,min(1,f.dot(n)))))
  for name,value in (('LookYawAction',yaw/.07),('LookPitchAction',pitch/.07)):
   a=u.find_object(pawn,name);input_sub.inject_input_vector_for_action(a,u.Vector(value,0,0),[],[])
 if action=='walk_entry':
  gate=next(a for a in aa if a.get_actor_label()=='MC_R_EcologyEntranceDoor');frame=gate.get_actor_transform();p=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
  ground=p.project_point_to_surface(frame.transform_location(u.Vector(1400,0,0)));start=ground.impact_point+p.get_radial_up_vector(ground.impact_point)*96;destination=frame.transform_location(u.Vector(-500,0,100));rot=u.MathLibrary.make_rot_from_zx(p.get_radial_up_vector(start),destination-start)
  pawn.set_gameplay_planet(p);pawn.set_actor_location_and_rotation(start,rot,False,False);pawn.initialize_planet_frame();pawn.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately();pc.set_control_rotation(rot);pc.set_view_target_with_blend(pawn,0)
  began=u.GameplayStatics.get_time_seconds(w);state={'handle':None};path=Path(u.Paths.project_saved_dir())/'MarsEntryWalk.json'
  def step(dt):
   elapsed=u.GameplayStatics.get_time_seconds(w)-began;pos=pawn.get_actor_location();local=frame.inverse_transform_location(pos);delta=destination-pos;n=p.get_radial_up_vector(pos);delta=delta-n*delta.dot(n);arrived=delta.length()<60
   path.write_text(json.dumps({'elapsed':elapsed,'gate_local':vec(local),'arrived':arrived,'view_target':pc.get_view_target().get_name()}),encoding='utf8')
   if arrived or elapsed>12:u.unregister_slate_post_tick_callback(state['handle']);return
   pawn.add_movement_input(delta.normal(),1,True)
  state['handle']=u.register_slate_post_tick_callback(step)
  return {'action':action,'walk_distance_cm':1900,'uses_character_movement':True}
 if action=='terminal_status':return {'action':action,'can_use':device.get_owner().can_use_dock(pawn),'screen_distance':(pawn.get_actor_location()-device.get_owner().get_screen_world_location()).length(),'front':normal.dot(pawn.get_actor_location()-device.get_owner().get_screen_world_location()),'device_use':device.can_interact(pawn)}
 if action=='trace':
  hits={};view=camera.get_camera_location()
  for name,ignore in (('client_los',[pawn]),('without_ship',[pawn,ship]),('without_terminal',[pawn,device.get_owner()])):
   hit=u.SystemLibrary.line_trace_single(w,view,center,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,ignore,u.DrawDebugTrace.NONE,True)
   hits[name]=[(str(hit.to_tuple()[i])) for i in (0,5,9,10)] if hit else None
  hits['pawn_distance']=(center-pawn.get_actor_location()).length();hits['bounds']=str(device.get_actor_bounds(False));hits['components']=[(c.get_name(),str(c.get_collision_enabled()),str(c.get_collision_object_type()),str(c.get_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY))) for c in device.get_components_by_class(u.PrimitiveComponent)]
  return {'action':action,'trace':hits}
 if action=='reader_trace':
  center=u.SystemLibrary.get_component_bounds(gate.reader)[0];result={'action':action,'distance':(center-pawn.get_actor_location()).length(),'can_scan':gate.can_execute_interaction(pawn)}
  for name,point in (('camera',camera.get_camera_location()),('server_eyes',pawn.get_actor_eyes_view_point()[0])):
   hit=u.SystemLibrary.line_trace_single(w,point,center,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn],u.DrawDebugTrace.NONE,True)
   result[name]=[(str(hit.to_tuple()[i])) for i in (0,5,9,10)] if hit else None
  return result
 c=pawn.get_component_by_class(u.InteractionComponent);c.refresh_interactable();target=c.get_current_interactable();view=camera.get_camera_location();f=camera.get_camera_rotation().get_forward_vector();hit=u.SystemLibrary.line_trace_single(w,view,view+f*1000,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[pawn,ship],u.DrawDebugTrace.NONE,True);actor=hit.to_tuple()[9] if hit else None
 return {'action':action,'view_target':pc.get_view_target().get_name(),'target':target.get_actor_label() if target else None,'prompt':str(c.get_current_interaction_prompt()),'can_interact':device.can_interact(pawn),'can_take':device.can_execute_interaction(pawn),'device_available':device.get_editor_property('available'),'alignment':round(f.dot((center-view).normal()),5),'aim_hit':actor.get_actor_label() if actor else None,'active_item':str(pawn.get_component_by_class(u.JTSInventoryComponent).get_active_item_id()),'progress':c.get_interaction_hold_progress()}

