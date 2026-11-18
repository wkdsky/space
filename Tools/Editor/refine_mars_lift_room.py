"""Replace the exterior lift hall with a compact lift centred on the dome floor."""
from pathlib import Path
import json, math
import unreal as u

ROOT='/Game/Space/Environment/MarsSystem/Chapter/Refined'
REVISION='1'
NAMES=('Station_LiftShaft','Station_LiftCab','Station_LiftDoorFrame','Station_LiftDoorLeaf','Station_LiftFeed')
RETIRED=('Station_Shell','Station_Deck','Station_LiftCore','Station_PowerRoute','Station_ServiceCabinet','Station_Ramp','Lift_Housing')

def setp(obj,**values):
 for k,v in values.items():obj.set_editor_property(k,v)
 return obj

def context():
 sub=u.get_editor_subsystem(u.EditorActorSubsystem)
 aa={a.get_actor_label():a for a in sub.get_all_level_actors()}
 planet=next(a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
 return sub,aa,planet

def inspect():
 _,aa,planet=context();dome=aa['MC_R_Dome_Footing'].get_actor_transform()
 labels=['MC_R_Lift_Housing','MC_Station_Elevator','MC_Station_Arrival','MC_Hub_Elevator','MC_R_StationAccessDoor','MC_Power']
 return {'actors':{n:{'local_cm':dome.inverse_transform_location(aa[n].get_actor_location()).to_tuple(),'destination':str(aa[n].get_editor_property('destination')) if isinstance(aa[n],u.JTSInteriorTransit) else None} for n in labels},'old_hall_actors':sum(n.startswith('MC_R_LiftRoom_') for n in aa),'zones':[a.get_actor_label() for a in aa.values() if isinstance(a,u.JTSInteriorZone)]}

def import_assets():
 source=Path(u.Paths.project_dir())/'SourceArt/MarsStation/Refined';manifest=json.loads((source/'manifest.json').read_text(encoding='utf8'))
 at=u.AssetToolsHelpers.get_asset_tools();meshes={}
 for key in NAMES:
  item=manifest[key];mesh=u.load_asset(ROOT+'/'+item['mesh'])
  if not mesh or u.EditorAssetLibrary.get_metadata_tag(mesh,'MarsDomeLiftRevision')!=REVISION:
   ui=setp(u.FbxImportUI(),import_mesh=True,import_materials=False,import_textures=False,import_animations=False,import_as_skeletal=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH,automated_import_should_detect_type=False)
   setp(ui.static_mesh_import_data,combine_meshes=True,auto_generate_collision=item['collision']!='none',generate_lightmap_u_vs=False)
   task=setp(u.AssetImportTask(),filename=str(source/(item['mesh']+'.fbx')),destination_path=ROOT,destination_name=item['mesh'],automated=True,save=True,replace_existing=True,replace_existing_settings=True,options=ui)
   at.import_asset_tasks([task]);mesh=u.load_asset(ROOT+'/'+item['mesh'])
  assert mesh,key
  for i,slot in enumerate(mesh.get_editor_property('static_materials')):
   mesh.set_material(i,u.load_asset(ROOT+'/'+{'MP_Palette':'M_MP_Palette','MP_Luminous':'M_MP_Luminous','MP_Glass':'M_MP_ETFE'}[str(slot.material_slot_name)]))
  if item['collision']=='complex':setp(mesh.get_editor_property('body_setup'),collision_trace_flag=u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  u.EditorAssetLibrary.set_metadata_tag(mesh,'MarsDomeLiftRevision',REVISION)
  u.EditorAssetLibrary.save_loaded_asset(mesh,False);meshes[key]=mesh
 for key in ('Station_PowerBox','Station_ReadyIndicator','Station_LiftPanel','Station_TrenchTrunk'):
  meshes[key]=u.load_asset(ROOT+'/SM_MP_'+key);assert meshes[key],key
 return meshes

def rounded(points,radius=160):
 p=[u.Vector(x,y,0) for x,y in points];out=[p[0]]
 for a,b,c in zip(p,p[1:],p[2:]):
  incoming=(b-a).normal();outgoing=(c-b).normal();distance=min(radius,(b-a).length()*.35,(c-b).length()*.35)
  entry=b-incoming*distance;exit=b+outgoing*distance;out.append(entry)
  for j in range(1,9):
   t=j/8;out.append(entry*((1-t)**2)+b*(2*t*(1-t))+exit*t*t)
 out.append(p[-1]);return out

def configure_door(door,meshes):
 for component,key in ((door.frame,'Station_LiftDoorFrame'),(door.left_leaf,'Station_LiftDoorLeaf'),(door.right_leaf,'Station_LiftDoorLeaf')):
  component.set_static_mesh(meshes[key]);setp(component,override_materials=[])
  component.set_relative_scale3d(u.Vector(1,1,1));component.set_collision_profile_name('BlockAll')
 door.left_leaf.set_relative_location(u.Vector(0,-60,0),False,False)
 door.right_leaf.set_relative_location(u.Vector(0,60,0),False,False)
 door.reader.set_relative_location(u.Vector(19,-178,0),False,False)
 door.reader.set_relative_scale3d(u.Vector(.85,.85,.85))
 door.status_light.set_relative_location(u.Vector(21,-178,175),False,False)
 door.status_light.set_relative_scale3d(u.Vector(.8,.8,.8))
 setp(door,leaf_travel=u.Vector(0,120,0),detection_center=u.Vector(0,0,165),activation_half_extents=u.Vector(300,150,180),safety_half_extents=u.Vector(100,120,180),locked_prompt='生态球中央电梯 · 未识别到地外访问许可',authorized_prompt='生态球中央电梯 · 访问许可已核验，靠近自动开门')

def refine():
 sub,aa,planet=context();meshes=import_assets()
 dome=aa['MC_R_Dome_Footing'].get_actor_transform()
 # The central tile defines the walking plane. The planet and circular deck stay intact.
 deck=aa['MC_R_DomeDeck_0_0'];local=dome.inverse_transform_location(deck.get_actor_location())
 top=deck.static_mesh_component.static_mesh.get_bounds();floor=local.z+top.origin.z+top.box_extent.z
 origin=dome.transform_location(u.Vector(0,0,floor));rotation=dome.rotation.rotator()
 transform=u.Transform(rotation=rotation,location=origin,scale=u.Vector(1,1,1))
 up=aa['MC_R_Dome_Footing'].get_actor_up_vector();front=aa['MC_R_Dome_Footing'].get_actor_right_vector()
 def p(local):return transform.transform_location(u.Vector(*local))
 def place(actor,local=(0,0,0),rot=None,scale=(1,1,1)):
  actor.detach_from_actor(u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD)
  actor.set_actor_scale3d(u.Vector(*scale));actor.set_actor_location_and_rotation(p(local),rot or rotation,False,False)
  actor.attach_to_actor(planet,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
  if isinstance(actor,(u.StaticMeshActor,u.PointLight)):
   actor.set_owner(planet)
   setp(actor,tags=list({str(tag) for tag in actor.get_editor_property('tags')}|{'PlanetSurfaceContent'}))
  return actor
 def prop(name,key,local=(0,0,0),rot=None,scale=(1,1,1),collision=True):
  label='MC_R_DomeLift_'+name;actor=aa.get(label)
  if not actor:actor=sub.spawn_actor_from_class(u.StaticMeshActor,p(local),rot or rotation);aa[label]=actor
  actor.set_actor_label(label);actor.set_folder_path('MarsChapter/Refined/Mars/DomeLift')
  mesh=actor.static_mesh_component;mesh.set_mobility(u.ComponentMobility.MOVABLE);mesh.set_static_mesh(meshes[key]);setp(mesh,override_materials=[])
  mesh.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
  return place(actor,local,rot,scale)
 # Remove the entire hall assembly. Keep only the solar network up to its collector.
 removed=[]
 for label in list(aa):
  actor=aa[label]
  if label=='MC_R_LiftRoom_PVCollector' or label.startswith(('MC_R_LiftRoom_Trench_A_','MC_R_LiftRoom_Trench_B_','MC_R_LiftRoom_Trench_C_')):
   new=label.replace('MC_R_LiftRoom_','MC_R_PV_');actor.set_actor_label(new);actor.set_folder_path('MarsChapter/Refined/Mars/PowerFeed');aa[new]=aa.pop(label)
  elif label.startswith(('MC_R_LiftRoom_','MC_R_Cable_','MC_R_Entry_Walk_','MC_Station_Lift_','MC_ElevatorBuilding_','MC_R_DomeLift_FeedCover_')):
   assert sub.destroy_actor(actor),label;aa.pop(label);removed.append(label)
 house=aa['MC_R_Lift_Housing'];house.static_mesh_component.set_static_mesh(meshes['Station_LiftShaft']);setp(house.static_mesh_component,override_materials=[])
 house.static_mesh_component.set_collision_profile_name('BlockAll');house.set_folder_path('MarsChapter/Refined/Mars/DomeLift');place(house)
 prop('Cab','Station_LiftCab');prop('Feed','Station_LiftFeed',collision=False)
 lift=aa['MC_Station_Elevator'];lift.mesh.set_static_mesh(meshes['Station_LiftPanel']);setp(lift.mesh,override_materials=[])
 place(lift,(155,150,100));lift.set_folder_path('MarsChapter/Refined/Mars/DomeLift')
 setp(lift,display_name='乘坐生态球中央电梯 · 地下分配大厅',locked_prompt='电梯停运 · 需地外访问许可与 A/B/C 三区供电')
 place(aa['MC_Station_Arrival'],(0,25,125),u.MathLibrary.make_rot_from_xz(front,up))
 door=aa['MC_R_StationAccessDoor'];configure_door(door,meshes)
 place(door,(0,232,0),u.MathLibrary.make_rot_from_xz(front,up));door.set_folder_path('MarsChapter/Refined/Mars/DomeLift')
 bp=u.load_asset('/Game/Space/Blueprints/Planets/Mars/Chapter/BP_Mars_StationAccessDoor')
 configure_door(u.get_default_object(bp.generated_class()),meshes)
 u.BlueprintEditorLibrary.compile_blueprint(bp);u.EditorAssetLibrary.save_loaded_asset(bp,False)
 power=aa['MC_Power'];power.mesh.set_static_mesh(meshes['Station_PowerBox']);power.mesh.set_collision_enabled(u.CollisionEnabled.NO_COLLISION);setp(power.mesh,override_materials=[])
 place(power,(0,-80,418));power.set_folder_path('MarsChapter/Refined/Mars/DomeLift')
 power.status_light.set_static_mesh(meshes['Station_ReadyIndicator']);power.status_light.set_relative_scale3d(u.Vector(1,1,1));power.status_light.set_relative_location(u.Vector(0,48,24),False,False)
 setp(power,inactive_material=u.load_asset(ROOT+'/M_MP_Amber'),active_material=u.load_asset(ROOT+'/M_MP_Green'),hide_mesh_when_active=False)
 power.status_light.set_material(0,power.get_editor_property('inactive_material'))
 lamp=aa['MC_R_StationAccessLamp'];place(lamp,(0,0,335));lamp.set_folder_path('MarsChapter/Refined/Mars/DomeLift')
 c=lamp.point_light_component;c.set_intensity(160);c.set_attenuation_radius(450);c.set_cast_shadows(False)
 # Open the approach between the original benches without altering the growing areas.
 for x in (-450,450):
  bench=aa['MC_R_Dome_Bench_'+str(-350 if x<0 else 350)]
  old=bench.get_actor_rotation();place(bench,(x,650,-3),old)
 # Reroute the final collector feed to the dome; all covers are separate props.
 station=planet.get_surface_frame_at(aa['Mars_Site_Station'].get_actor_location(),u.Vector(1,0,0))
 collector=aa.get('MC_R_PV_PVCollector')
 if collector:
  start=station.transform.inverse_transform_location(collector.get_actor_location())
  end=station.transform.inverse_transform_location(p((-1800,-180,0)))
  points=rounded([(start.x,start.y),(-2000,end.y),(end.x,end.y)])
  index=0
  def cover(first,last):
   nonlocal index
   delta=last-first
   if delta.length()<1:return
   actor=prop('FeedCover_'+str(index),'Station_TrenchTrunk',rot=u.MathLibrary.make_rot_from_xz(delta,up),scale=(delta.length()/100,1,1),collision=False)
   actor.set_actor_location(first,False,False);index+=1
  def ground(point):
   world=station.transform.transform_location(point);hit=planet.project_point_to_surface(world);assert hit.blocking_hit
   return hit.impact_point+planet.get_radial_up_vector(hit.impact_point)*1.5
  for a,b in zip(points,points[1:]):
   count=max(1,math.ceil((b-a).length()/120))
   for j in range(count):cover(ground(a+(b-a)*(j/count)),ground(a+(b-a)*((j+1)/count)))
  cover(ground(points[-1]),p((-1700,-180,.6)))
  first=p((-1700,-180,.6));last=p((-234,-180,.6));count=math.ceil((last-first).length()/120)
  for j in range(count):cover(first+(last-first)*(j/count),first+(last-first)*((j+1)/count))
 guide=aa['MC_R_Guide_Arrival'];place(guide,(345,440,111))
 stand=aa.get('MC_R_GuideStand_Arrival')
 if stand:place(stand,(345,440,0))
 document=guide.get_editor_property('document')
 setp(document,body='地下入口位于生态球圆形地板正中央。靠近电梯门时，门禁会自动核验地外访问许可。\n\n修复 A、B、C 三个汇流箱，并等待清扫风恢复板阵输出。金属检修盖将三路供电汇入生态球，馈线沿井壁接入轿厢上方的动力箱。三路就绪后，在轿厢内操作下行面板。\n\n故障交接夹板留在温室工作台，请先核对设备组记录和领料单。地下主管工牌用于部门与种子库，不能代替站区访问许可。\n\n值班台代留　周勉')
 u.EditorAssetLibrary.save_loaded_asset(document,False)
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 result=audit();result['removed_hall_actors']=len(removed);return result

def audit():
 _,aa,planet=context();house=aa['MC_R_Lift_Housing'];t=house.get_actor_transform();dome=aa['MC_R_Dome_Footing'].get_actor_transform()
 centre=dome.inverse_transform_location(house.get_actor_location())
 assert abs(centre.x)<.01 and abs(centre.y)<.01,'Lift must be centred on the circular dome floor'
 assert not any(n.startswith('MC_R_LiftRoom_') for n in aa),'Exterior hall remnants remain'
 bounds=house.static_mesh_component.static_mesh.get_bounds()
 assert bounds.box_extent.x<=240.1 and bounds.box_extent.y<=250.1,'Shaft must closely surround the cab'
 local={n:t.inverse_transform_location(aa[n].get_actor_location()).to_tuple() for n in ('MC_Station_Elevator','MC_Station_Arrival','MC_R_StationAccessDoor','MC_Power')}
 assert abs(local['MC_Station_Arrival'][0])<150 and abs(local['MC_Station_Arrival'][1])<150
 assert aa['MC_Hub_Elevator'].get_editor_property('destination')==aa['MC_Station_Arrival'],'Return destination disconnected'
 assert aa['MC_Station_Elevator'].get_editor_property('destination')==aa['MC_Hub_Arrival'],'Downward destination disconnected'
 assert aa['MC_Power'].mesh.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
 door=aa['MC_R_StationAccessDoor']
 def trace(a,b,ignore):
  hit=u.SystemLibrary.line_trace_single(house.get_world(),t.transform_location(u.Vector(*a)),t.transform_location(u.Vector(*b)),u.TraceTypeQuery.TRACE_TYPE_QUERY1,True,ignore,u.DrawDebugTrace.NONE,True)
  actor=hit.to_tuple()[9] if hit else None
  return actor.get_actor_label() if actor else None
 passage=trace((0,450,150),(0,0,150),[door]);assert passage is None,passage
 closed=trace((0,450,150),(0,0,150),[]);assert closed==door.get_actor_label(),closed
 floor=trace((0,0,150),(0,0,-30),[]);assert floor=='MC_R_DomeLift_Cab',floor
 for name in ('MC_R_Lift_Housing','MC_R_DomeLift_Cab','MC_R_StationAccessDoor','MC_Station_Elevator','MC_Station_Arrival','MC_Power'):
  assert aa[name].get_attach_parent_actor()==planet,name
 return {'layout':'4.8 m x 5.0 m compact shaft and cab inside ecology dome','dome_centre_offset_cm':centre.to_tuple(),'local_positions_cm':local,'clear_open_passage':passage is None,'closed_door_blocks':closed,'floor_hit':floor,'lift_requirements':str(aa['MC_Station_Elevator'].conditions.get_editor_property('requirements')),'power_rule':str(aa['MC_Power'].get_editor_property('completed_flag'))}

def retire_assets():
 audit();removed=[];retained={}
 for name in RETIRED:
  path=ROOT+'/SM_MP_'+name
  if not u.EditorAssetLibrary.does_asset_exist(path):continue
  refs=u.EditorAssetLibrary.find_package_referencers_for_asset(path,True)
  if refs:retained[name]=list(refs);continue
  if u.EditorAssetLibrary.delete_asset(path):removed.append(name)
 return {'deleted_unused_hall_meshes':removed,'retained_referenced_meshes':retained}

def view_inside():return view((0,70,175),(100,-170,180))
def view_services():return view((-650,600,580),(-100,-100,280))
def view_outside():return view((850,1200,700),(0,0,210))
def view_overview():return view((650,1250,1050),(0,-100,100))
def view(camera,target):
 _,aa,_=context();house=aa['MC_R_Lift_Housing'];t=house.get_actor_transform()
 position=t.transform_location(u.Vector(*camera));rotation=u.MathLibrary.make_rot_from_xz(t.transform_location(u.Vector(*target))-position,house.get_actor_up_vector())
 u.EditorLevelLibrary.set_level_viewport_camera_info(position,rotation);u.EditorLevelLibrary.editor_set_game_view(True);u.EditorLevelLibrary.editor_invalidate_viewports()
 return audit()

