"""Targeted, repeatable station entrance and engineering handover authoring."""
from pathlib import Path
import json, runpy
import unreal as u

ROOT='/Game/Space/Environment/MarsSystem/Chapter/Refined'
BP='/Game/Space/Blueprints/Planets/Mars/Chapter'
DATA='/Game/Space/Data/Planets/Mars'
REPORT='''记录：陈岚／设备工程　复核：周勉／站务

地面保障组：昨晚的遥测包已上传，回执暂缺。请将下列情况并入下一次补给评估。

阵列端的抽测电压正常，储能柜也未触发内部故障。问题集中在 A、B、C 三个汇流箱：密封圈老化后细尘进入接触器，触点烧蚀，三路输出均已隔离。换保险件没有用，请后班不要重复拆电池板。

检修领料单已夹在本页后面。每箱需金属导电件 6 份、绝缘晶体 2 份，三区用量相同。温室靠入口的封存检修箱里还留着最后一批备件；领出后先在补给船货舱登记，别再挪去修巡视车。站内加工机已停，现有库存不够另做一套。

更换后仍需等清扫风过境。板面现在的积尘遮光很重，只修箱体不会马上有电。电梯复位联锁要收到三区的供电确认，少一路便无法复位；门禁有独立蓄电池，仍可核验地外访问许可。

抢修被东侧的失压警报打断，工具还留在台上。我们把种植床的水放掉了，回收桶里的净水还能用。地下各组未能按时交班，恢复电梯前不要单人下去。

手写补记：周工，检修箱的封条没动。夹板留在温室工作台，下一班从这里接。——岚'''

def setp(obj,**values):
 for name,value in values.items():obj.set_editor_property(name,value)
 return obj

def save(asset):
 u.EditorAssetLibrary.save_loaded_asset(asset,False)
 return asset

def repair_attachments():
 actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
 anchors={p.get_editor_property('gameplay_surface_actor'):p for p in actors if isinstance(p,u.JTSPlanetAnchor) and str(p.get_planet_id()) in ('Mars','MarI','MarII')}
 changed=[]
 for actor in actors:
  anchor=anchors.get(actor.get_attach_parent_actor())
  if actor.get_actor_label().startswith('MC_') and actor.get_editor_property('replicates') and anchor:
   # Planet meshes have enormous scales. FRepAttachment quantizes their inverse
   # child scale to zero; the unit-scale anchor preserves independent props.
   actor.attach_to_actor(anchor,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
   changed.append(actor.get_actor_label())
 return changed

def remove_recording():
 path='/Game/Space/Environment/MarsSystem/Chapter/Audio/S_Mars_Beacon'
 if u.EditorAssetLibrary.does_asset_exist(path):
  assert u.EditorAssetLibrary.delete_asset(path),'Could not delete retired station recording'
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return {'recording_asset_exists':u.EditorAssetLibrary.does_asset_exist(path)}

def refine():
 sub=u.get_editor_subsystem(u.EditorActorSubsystem)
 aa={a.get_actor_label():a for a in sub.get_all_level_actors()}
 at=u.AssetToolsHelpers.get_asset_tools()
 manifest=json.loads((Path(u.Paths.project_dir())/'SourceArt/MarsStation/Refined/manifest.json').read_text(encoding='utf8'))
 meshes={}
 for key in ('Access_Frame','Access_Leaf','Access_Reader','Access_Status','Incident_Clipboard'):
  info=manifest[key];name=info['mesh'];mesh=u.load_asset(ROOT+'/'+name)
  if not mesh:
   ui=setp(u.FbxImportUI(),import_mesh=True,import_materials=False,import_textures=False,import_animations=False,import_as_skeletal=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH,automated_import_should_detect_type=False)
   setp(ui.static_mesh_import_data,combine_meshes=True,auto_generate_collision=info['collision']!='none',generate_lightmap_u_vs=False)
   task=setp(u.AssetImportTask(),filename=str(Path(u.Paths.project_dir())/'SourceArt/MarsStation/Refined'/(name+'.fbx')),destination_path=ROOT,destination_name=name,automated=True,save=True,options=ui)
   at.import_asset_tasks([task]);mesh=u.load_asset(ROOT+'/'+name)
  assert mesh,name
  for i,slot in enumerate(mesh.get_editor_property('static_materials')):
   mat=u.load_asset(ROOT+'/'+{'MP_Palette':'M_MP_Palette','MP_Luminous':'M_MP_Luminous','MP_Glass':'M_MP_ETFE'}[str(slot.material_slot_name)])
   mesh.set_material(i,mat)
  if info['collision']=='complex':setp(mesh.get_editor_property('body_setup'),collision_trace_flag=u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
  u.EditorAssetLibrary.set_metadata_tag(mesh,'MarsRefinementRevision','2');save(mesh);meshes[key]=mesh
 def requirement(source,id):return setp(u.JTSFacilityRequirement(),source=source,id=id)
 permit=requirement(u.JTSFacilityFact.IMPORTANT_ITEM,'MarsEcologyAccess')
 bp=u.load_asset(BP+'/BP_Mars_StationAccessDoor')
 if not bp:
  factory=setp(u.BlueprintFactory(),parent_class=u.JTSAutomaticAccessDoor)
  bp=at.create_asset('BP_Mars_StationAccessDoor',BP,u.Blueprint,factory)
 cdo=u.get_default_object(bp.generated_class())
 for component,key in [(cdo.frame,'Access_Frame'),(cdo.left_leaf,'Access_Leaf'),(cdo.right_leaf,'Access_Leaf'),(cdo.reader,'Access_Reader'),(cdo.status_light,'Access_Status')]:
  component.set_static_mesh(meshes[key]);setp(component,override_materials=[])
 cdo.reader.set_relative_location(u.Vector(48,-220,0),False,False)
 cdo.status_light.set_relative_location(u.Vector(65,-220,149),False,False)
 setp(cdo.conditions,requirements=[permit],mode=u.JTSConditionMode.ALL)
 setp(cdo,locked_prompt='赤原站门禁：未识别到地外访问许可 · 月球取得的许可可用于此处',authorized_prompt='赤原站门禁：访问许可已核验 · 靠近自动开门',locked_material=u.load_asset(ROOT+'/M_MP_Amber'),authorized_material=u.load_asset(ROOT+'/M_MP_Green'))
 u.BlueprintEditorLibrary.compile_blueprint(bp)
 save(bp)

 housing=aa['MC_R_Lift_Housing'];t=housing.get_actor_transform()
 # Imported house front is local +Y; the door's passage is local X.
 location=t.transform_location(u.Vector(0,300,0))
 rotation=u.MathLibrary.make_rot_from_xz(housing.get_actor_right_vector(),housing.get_actor_up_vector())
 door=aa.get('MC_R_StationAccessDoor')
 if not door:door=sub.spawn_actor_from_class(bp.generated_class(),location,rotation)
 defaults=u.get_default_object(bp.generated_class())
 for key in ('locked_prompt','authorized_prompt','locked_material','authorized_material'):
  door.set_editor_property(key,defaults.get_editor_property(key))
 door.set_actor_label('MC_R_StationAccessDoor');door.set_folder_path('MarsChapter/Refined/Mars')
 door.set_actor_location_and_rotation(location,rotation,False,False)
 door.attach_to_actor(housing.get_attach_parent_actor(),'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
 lamp=aa.get('MC_R_StationAccessLamp')
 lamp_position=location+door.get_actor_forward_vector()*280+door.get_actor_up_vector()*330
 if not lamp:lamp=sub.spawn_actor_from_class(u.PointLight,lamp_position)
 lamp.set_actor_label('MC_R_StationAccessLamp');lamp.set_folder_path('MarsChapter/Refined/Mars')
 lamp.set_actor_location(lamp_position,False,False)
 light=lamp.point_light_component;light.set_mobility(u.ComponentMobility.MOVABLE)
 light.set_intensity(160);light.set_attenuation_radius(950);light.set_cast_shadows(False)
 light.set_light_color(u.LinearColor(.84,.91,.80,1))
 lamp.attach_to_actor(housing.get_attach_parent_actor(),'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
 # Place the console behind the credential barrier and independently guard its server action.
 lift=aa['MC_Station_Elevator']
 lift.set_actor_location(location-door.get_actor_forward_vector()*220-door.get_actor_right_vector()*205,False,False)
 setp(lift.conditions,requirements=[requirement(u.JTSFacilityFact.FLAG,'Mars_Power'),permit],mode=u.JTSConditionMode.ALL)
 setp(lift,locked_prompt='电梯锁定 · 需站区访问许可及 A/B/C 三区供电（修复后等待风扫清尘）')

 document=aa['MC_R_Guide_DomeOperations']
 data=document.get_editor_property('document')
 setp(data,title='赤原生态站｜设备组交接单 017',body=REPORT)
 save(data)
 document_mesh=document.get_component_by_class(u.StaticMeshComponent)
 document_mesh.set_static_mesh(meshes['Incident_Clipboard']);setp(document_mesh,override_materials=[])
 bench=aa['MC_R_Dome_Lab'];bt=bench.get_actor_transform()
 # Top of the authored bench is 108.5 cm; the clipboard's backed foot rests on it.
 document.set_actor_location_and_rotation(bt.transform_location(u.Vector(-8,20,109)),bench.get_actor_rotation(),False,False)
 arrival=aa['MC_R_Guide_Arrival'].get_editor_property('document')
 setp(arrival,title='赤原生态站｜入口值班便笺',body='访客请从升降站正门进入。门禁使用独立电源，接近读卡器即可核验地外访问许可，无需插卡。许可登记对本次同行人员有效。\n\n升降机停运，故障交接夹板放在温室工作台。三区汇流箱的排查结果和检修领料单都在那里，先看记录再拆设备。\n\n地下主管工牌只用于部门及种子库授权，不能代替站区访问许可。\n\n值班台代留　周勉')
 save(arrival)
 # This automatic rule has no physical console; its old cube blocks the entry
 # when planet streaming restores actor-level collision.
 if 'MC_Power' in aa:aa['MC_Power'].mesh.set_static_mesh(None)
 repair_attachments()
 remove_recording()
 if u.EditorAssetLibrary.does_asset_exist(ROOT+'/SM_MP_Station_LiftShaft') or u.EditorAssetLibrary.does_asset_exist(ROOT+'/SM_MP_Station_Shell'):
  runpy.run_path(str(Path(__file__).with_name('refine_mars_lift_room.py')))['refine']()
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return audit()

def audit():
 aa={a.get_actor_label():a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
 door=aa['MC_R_StationAccessDoor'];doc=aa['MC_R_Guide_DomeOperations'];lift=aa['MC_Station_Elevator']
 data=doc.get_editor_property('document')
 defaults=u.get_default_object(door.get_class())
 if not defaults.get_editor_property('authorized_material'):raise RuntimeError('Door Blueprint lost its configured status material')
 power_mesh=aa['MC_Power'].mesh
 assert not power_mesh.get_editor_property('static_mesh') or power_mesh.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION,'Power visual must not block the entrance'
 assert isinstance(door.get_attach_parent_actor(),u.JTSPlanetAnchor),'Replicated door must use the unit-scale anchor'
 return {'door':door.get_class().get_path_name(),'door_requirements':str(door.conditions.get_editor_property('requirements')),'lift_requirements':str(lift.conditions.get_editor_property('requirements')),'clipboard':doc.get_component_by_class(u.StaticMeshComponent).get_editor_property('static_mesh').get_name(),'report_characters':len(str(data.body)),'door_position':str(door.get_actor_location()),'console_door_local':str(door.get_actor_transform().inverse_transform_location(lift.get_actor_location())),'recording_asset_exists':u.EditorAssetLibrary.does_asset_exist('/Game/Space/Environment/MarsSystem/Chapter/Audio/S_Mars_Beacon')}

def door_view():
 aa={a.get_actor_label():a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
 door=aa['MC_R_StationAccessDoor'];up=door.get_actor_up_vector();right=door.get_actor_right_vector();front=door.get_actor_forward_vector()
 camera=door.get_actor_location()+front*900+right*100+up*260
 rotation=u.MathLibrary.make_rot_from_xz(door.get_actor_location()+up*210-camera,up)
 u.EditorLevelLibrary.set_level_viewport_camera_info(camera,rotation)
 u.EditorLevelLibrary.editor_set_game_view(True)
 return audit()

def clipboard_view():
 aa={a.get_actor_label():a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()}
 doc=aa['MC_R_Guide_DomeOperations'];up=doc.get_actor_up_vector()
 camera=doc.get_actor_location()+up*110-doc.get_actor_right_vector()*75
 rotation=u.MathLibrary.make_rot_from_xz(doc.get_actor_location()-camera,doc.get_actor_right_vector())
 u.EditorLevelLibrary.set_level_viewport_camera_info(camera,rotation)
 u.EditorLevelLibrary.editor_set_game_view(True)
 data=doc.get_editor_property('document')
 return {'title':str(data.title),'report':str(data.body)}

def probe(action,w,worlds,players):
 gs=u.GameplayStatics.get_game_state(w)
 aa={a.get_actor_label():a for a in u.GameplayStatics.get_all_actors_of_class(w,u.Actor)}
 door=aa['MC_R_StationAccessDoor'];lift=aa['MC_Station_Elevator'];doc=aa['MC_R_Guide_DomeOperations']
 instance=u.GameplayStatics.get_game_instance(w)
 expedition=next(s for s in u.ObjectIterator(u.JTSExpeditionSubsystem) if s.get_outer()==instance)
 def move(player,local):
  planet=next(a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
  if player.is_boarded():player.get_controller().call_method('ServerRequestDisembarkSpacecraft',(gs.get_active_spacecraft(),))
  player.set_gameplay_planet(planet)
  player.set_actor_location(door.get_actor_transform().transform_location(u.Vector(*local)),False,False)
  player.initialize_planet_frame()
  player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
 if action=='isolate':
  setp(expedition,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation')
  expedition.begin_new_expedition_in_slot(4,'Automatic station access verification')
  for player in players:player.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
 elif action=='approach':
  for i,player in enumerate(players):move(player,(250+i*15,(i-1.5)*65,150))
 elif action=='grant':
  # The test-only command calls the same authoritative recorder as the lunar pickup.
  assert expedition.get_editor_property('save_slot_prefix')=='MarsAccessValidation' and expedition.get_active_save_slot()==4
  u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.GrantStationPermit')
 elif action=='leave':
  for i,player in enumerate(players):move(player,(1400+i*100,0,150))
 elif action=='safety':
  for i,player in enumerate(players):move(player,(0,(i-1.5)*65,150))
 elif action=='power':expedition.set_facility_flag('Mars_Power')
 elif action=='console':
  player=players[-1]
  point=lift.get_actor_location()+lift.get_actor_right_vector()*120+lift.get_actor_up_vector()*50
  move(player,door.get_actor_transform().inverse_transform_location(point).to_tuple())
  return {'console_can_inspect':lift.can_interact(player),'console_can_execute':lift.can_execute_interaction(player),'prompt':str(lift.get_interaction_prompt(player))}
 elif action=='read':
  player=players[0]
  move(player,(1400,0,150))
  up=doc.get_actor_up_vector()
  point=doc.get_actor_location()-doc.get_actor_right_vector()*170+up*50
  player.set_actor_location_and_rotation(point,u.MathLibrary.make_rot_from_xz(doc.get_actor_location()-point,up),False,False)
  player.initialize_planet_frame()
  player.get_controller().set_control_rotation(player.get_actor_rotation())
  allowed=doc.can_interact(player)
  doc.interact(player)
  return {'can_read':allowed,'document_title':str(doc.get_editor_property('document').title)}
 elif action=='transit':
  player=players[-1]
  point=lift.get_actor_location()+lift.get_actor_right_vector()*120+lift.get_actor_up_vector()*50
  move(player,door.get_actor_transform().inverse_transform_location(point).to_tuple());lift.interact(player)
 elif action=='attachment_audit':
  return [{'world':world.get_path_name(),'zero_scale':[a.get_actor_label() for a in u.GameplayStatics.get_all_actors_of_class(world,u.Actor) if a.get_actor_label().startswith('MC_') and a.get_actor_scale3d().is_nearly_zero()]} for world in worlds]
 result={'action':action,'shared_permit':gs.has_important_item('MarsEcologyAccess'),'open':door.get_editor_property('open'),'authorized':door.get_editor_property('authorized'),'left_local':str(door.left_leaf.get_editor_property('relative_location')),'console_usable':lift.can_execute_interaction(players[-1]),'players':len(players),'worlds':[]}
 transform=door.get_actor_transform()
 hit=u.SystemLibrary.line_trace_single(w,transform.transform_location(u.Vector(250,0,150)),transform.transform_location(u.Vector(-200,0,150)),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,players,u.DrawDebugTrace.NONE,True)
 obstruction=hit.to_tuple()[9] if hit else None
 result['passage_obstruction']=obstruction.get_actor_label() if obstruction else None
 for world in worlds:
  doors=u.GameplayStatics.get_all_actors_of_class(world,u.JTSAutomaticAccessDoor)
  state=u.GameplayStatics.get_game_state(world)
  if not isinstance(state,u.JTSGameState):
   result['worlds'].append({'world':world.get_name(),'state_class':state.get_class().get_name() if state else None})
   continue
  door_states=[]
  for d in doors:
   dt=d.get_actor_transform()
   characters=u.GameplayStatics.get_all_actors_of_class(world,u.JTSCharacter)
   hit=u.SystemLibrary.line_trace_single(world,dt.transform_location(u.Vector(250,0,150)),dt.transform_location(u.Vector(-200,0,150)),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,characters,u.DrawDebugTrace.NONE,True)
   obstruction=hit.to_tuple()[9] if hit else None
   door_states.append({'path':d.get_path_name(),'scale':str(d.get_actor_scale3d()),'open':d.get_editor_property('open'),'authorized':d.get_editor_property('authorized'),'status_material':d.status_light.get_material(0).get_name(),'left_y':d.left_leaf.get_editor_property('relative_location').y,'blocking':str(d.left_leaf.get_collision_enabled()),'passage_obstruction':obstruction.get_actor_label() if obstruction else None})
  result['worlds'].append({'world':world.get_name(),'authority':state.has_authority(),'shared_permit':state.has_important_item('MarsEcologyAccess'),'doors':door_states})
 return result
