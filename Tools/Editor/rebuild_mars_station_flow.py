import unreal as u,json
from pathlib import Path
def inspect():
 aa=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors();house=next(a for a in aa if a.get_actor_label()=='MC_R_Lift_Housing');basis=house.get_actor_transform()
 return {'lights':[{'label':a.get_actor_label(),'intensity':c.get_editor_property('intensity')} for a in aa for c in a.get_components_by_class(u.LightComponent) if a.get_actor_label().startswith('MC_')], 'objects':[{'label':a.get_actor_label(),'local':str(basis.inverse_transform_location(a.get_actor_location())),'meshes':[(c.get_name(),c.static_mesh.get_name() if c.static_mesh else None,str(c.get_editor_property('relative_location'))) for c in a.get_components_by_class(u.StaticMeshComponent)]} for a in aa if a.get_actor_label() in ('MC_Power','MC_R_Lift_Housing','MC_R_EcologyEntranceDoor','MC_R_Dome_Lab','MC_R_Guide_DomeOperations')], 'terminal':[(c.get_name(),str(c.get_editor_property('relative_location')),str(c.static_mesh.get_bounds()) if isinstance(c,u.StaticMeshComponent) and c.static_mesh else None) for c in u.get_default_object(u.load_asset('/Game/Space/Ships/Docks/BP_Ship_CentralTerminal').generated_class()).get_components_by_class(u.SceneComponent)]}
def close():
 setp(u.get_default_object(u.JTSExpeditionSubsystem),save_slot_prefix='JumpToSpaceExpedition',save_slot_name='JumpToSpaceExpedition')
 settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'));setp(settings,PlayNumberOfClients=1,RunUnderOneProcess=True)
 u.SystemLibrary.quit_editor();return {'closing':True}

def debug_menu():
 import runpy
 debug=next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a,u.JTSSpaceDebugTravelManager))
 labels=runpy.run_path(str(Path(u.Paths.project_dir())/'Tools/Editor/configure_space_debug.py'))['configure_access_permits'](debug)
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return {'permits':labels}

ROOT='/Game/Space/Environment/MarsSystem/Chapter/Refined'
BP='/Game/Space/Blueprints/Planets/Mars/Chapter'
ITEMS='/Game/Space/Data/Items'
GATE_PROMPTS=dict(display_name='门禁',activation_prompt='[E] 刷门禁',missing_device_prompt='拿通讯器',missing_credential_prompt='没有许可',locked_prompt='未就绪',authorized_prompt='',swipe_seconds=.55,close_delay=3.)
LIFT_PROMPTS=dict(display_name='电梯门',activation_prompt='[长按 E] 开门',missing_device_prompt='未通电',locked_prompt='未通电',authorized_prompt='')

def repair_prompts(actor,sector):
 setp(actor,display_name=sector+' 区汇流器',locked_prompt='待检修',completed_prompt='已修复',show_progress_on_hud=False)

def prompts():
 _,aa,_=ctx()
 for name,values,label in (('BP_Mars_EcologyEntranceDoor',GATE_PROMPTS,'MC_R_EcologyEntranceDoor'),('BP_Mars_StationAccessDoor',LIFT_PROMPTS,'MC_R_StationAccessDoor')):
  if name=='BP_Mars_EcologyEntranceDoor':
   values=dict(values,reading_material=reader_material('Reading',u.LinearColor(.04,.9,1,1)),denied_material=reader_material('Denied',u.LinearColor(1,.025,.01,1)))
  bp=u.load_asset(BP+'/'+name);setp(u.get_default_object(bp.generated_class()),**values);u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp);setp(aa[label],**values)
 for sector in 'ABC':repair_prompts(aa['MC_Sector_'+sector],sector)
 setp(aa['MC_R_Flow_PowerReveal'],notice='电梯已通电')
 setp(aa['MC_Station_Elevator'],locked_prompt='未就绪',display_name='下行',prompt_label='电梯')
 item=u.load_asset(ITEMS+'/DA_Item_PortableCommunicator');setp(item,description='记录访问许可的移动通讯器。');save(item)
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return {'prompts':'concise','communicator':'single press take / return'}

def reader_material(state,tint):
 name='M_MP_Reader'+state;path=ROOT+'/'+name;mat=u.load_asset(path)
 if not mat:
  mat=u.AssetToolsHelpers.get_asset_tools().create_asset(name,ROOT,u.Material,u.MaterialFactoryNew())
  color=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionConstant3Vector,0,0)
  color.set_editor_property('constant',tint)
  u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
  u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_BASE_COLOR)
  u.MaterialEditingLibrary.recompile_material(mat);save(mat)
 return mat

def setp(o,**kw):
 for k,v in kw.items():o.set_editor_property(k,v)
 return o

def save(a):u.EditorAssetLibrary.save_loaded_asset(a,False);return a

def ctx():
 sub=u.get_editor_subsystem(u.EditorActorSubsystem);aa={a.get_actor_label():a for a in sub.get_all_level_actors()}
 planet=next(a for a in aa.values() if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
 return sub,aa,planet

def req(id,source=None):return setp(u.JTSFacilityRequirement(),source=source or u.JTSFacilityFact.FLAG,id=id)
def conditions(o,ids):setp(o.conditions,requirements=[req(i) for i in ids],mode=u.JTSConditionMode.ALL)

def blueprint(name,parent):
 path=BP+'/'+name;a=u.load_asset(path)
 if not a:a=u.AssetToolsHelpers.get_asset_tools().create_asset(name,BP,u.Blueprint,setp(u.BlueprintFactory(),parent_class=parent))
 return a

def reader_asset():return {key:mesh.get_path_name() for key,mesh in import_props({'Flow_GateReader'}).items()}

def import_props(keys=None):
 src=Path(u.Paths.project_dir())/'SourceArt/MarsStation/Refined';manifest=json.loads((src/'manifest.json').read_text(encoding='utf8'));result={};tasks=[]
 for key,info in manifest.items():
  if not key.startswith('Flow_') or keys is not None and key not in keys:continue
  name=info['mesh']
  ui=setp(u.FbxImportUI(),import_mesh=True,import_materials=False,import_textures=False,import_animations=False,import_as_skeletal=False,mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH,automated_import_should_detect_type=False)
  setp(ui.static_mesh_import_data,combine_meshes=True,auto_generate_collision=info['collision']!='none',generate_lightmap_u_vs=False)
  tasks.append(setp(u.AssetImportTask(),filename=str(src/(name+'.fbx')),destination_path=ROOT,destination_name=name,automated=True,save=True,replace_existing=True,replace_existing_settings=True,options=ui))
 u.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
 for key,info in manifest.items():
  if not key.startswith('Flow_') or keys is not None and key not in keys:continue
  m=u.load_asset(ROOT+'/'+info['mesh']);assert m,key
  for i,slot in enumerate(m.get_editor_property('static_materials')):
   mat={'MP_Palette':'M_MP_Palette','MP_Luminous':'M_MP_Luminous','MP_Glass':'M_MP_ETFE'}[str(slot.material_slot_name)]
   m.set_material(i,u.load_asset(ROOT+'/'+mat))
  if info['collision']=='complex':setp(m.get_editor_property('body_setup'),collision_trace_flag=u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,double_sided_geometry=True)
  save(m);result[key]=m
 return result

REPORT='''赤原生态站 · 设备组故障交接单 017
记录：陈岚　复核：周勉

站内主电源已切断。中央电梯停止运行，门锁与下行控制均被断电联锁锁住，不能手动强行启动。

外部 A、B、C 三个太阳能扇区的汇流器均已损坏：接触器被细尘侵入，触点烧蚀。阵列与储能柜本体正常，请不要拆电池板。

维修顺序：
① 从门后封存检修箱领取备件，材料会登记到飞船共享仓库。
② 前往 A／B／C 三区，逐台更换汇流器接触组件。每台需金属导电件 6 份、绝缘晶体 2 份。
③ 三台修复后，供电系统会自动完成自检，接通电梯动力馈线。观察轿厢上方的电源灯，绿灯亮起才可开门下行。

门禁由独立蓄电池供电。月球取得的访问许可需先在补给船中心终端扫描进移动通讯器，拿在手里到门口刷卡。门禁授权不等于恢复主电源。

电梯恢复后先到地下分配大厅。四个部门的主管工牌用于种子库授权，不能代替生态球入口许可。

抢修被东侧失压警报打断。工具还在台上，备用件封条未拆。下一班从这里接。——岚'''

def author():
 sub,aa,planet=ctx();meshes=import_props();at=u.AssetToolsHelpers.get_asset_tools()
 house=aa['MC_R_Lift_Housing'];basis=house.get_actor_transform();rotation=house.get_actor_rotation();up=house.get_actor_up_vector()
 outward=-house.get_actor_right_vector();gate_rot=u.MathLibrary.make_rot_from_xz(outward,up)
 def point(local):return basis.transform_location(u.Vector(*local))
 def place(a,local,rot=None):
  a.detach_from_actor(u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD,u.DetachmentRule.KEEP_WORLD)
  a.set_actor_scale3d(u.Vector(1,1,1));a.set_actor_location_and_rotation(point(local),rot or rotation,False,False)
  a.attach_to_actor(planet,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
  a.set_folder_path('MarsChapter/StationFlow');return a
 def prop(label,mesh,local,rot=None):
  a=aa.get(label) or sub.spawn_actor_from_class(u.StaticMeshActor,point(local),rot or rotation)
  a.set_actor_label(label);aa[label]=a;c=a.static_mesh_component;c.set_mobility(u.ComponentMobility.MOVABLE);c.set_static_mesh(mesh);setp(c,override_materials=[]);c.set_collision_profile_name('BlockAll');place(a,local,rot)
  a.set_owner(planet);setp(a,tags=list({str(t) for t in a.get_editor_property('tags')}|{'PlanetSurfaceContent'}));return a
 # Physical portable inventory item with authored hand grip, not a progression flag.
 item=u.load_asset(ITEMS+'/DA_Item_PortableCommunicator')
 if not item:item=at.create_asset('DA_Item_PortableCommunicator',ITEMS,u.JTSItemDefinition,setp(u.DataAssetFactory(),data_asset_class=u.JTSItemDefinition))
 held=setp(item.get_editor_property('held_presentation'),override_prototype_profile=True,mesh=meshes['Flow_Communicator'],mesh_scale=.82,grip_transform=u.Transform(location=u.Vector(-2.2,0,1.5)),body_scale=u.Vector(.1,.1,.1),barrel_scale=u.Vector(0,0,0),sight_scale=u.Vector(0,0,0))
 setp(item,item_id=u.JTSItemId.PORTABLE_COMMUNICATOR,display_name='移动通讯器',description='记录访问许可的移动通讯器。',primary_category=u.JTSItemCategory.UTILITY,capability_mask=1,max_stack_size=1,combat_damage=0,world_pickup_mesh=meshes['Flow_Communicator'],world_pickup_width=22,held_presentation=held,accent_color=u.LinearColor(.25,.8,.65,1));save(item)
 # The enum-to-definition mapping is project configuration, not a C++ asset path.
 # DefaultGame.ini maps the enum to this asset through JTSItemDefinitionSettings.
 commbp=blueprint('BP_Ship_PortableCommunicatorDock',u.JTSPortableCredentialDock);cdo=u.get_default_object(commbp.generated_class())
 cdo.mount.set_static_mesh(meshes['Flow_CommsDock']);cdo.device.set_static_mesh(meshes['Flow_Communicator']);cdo.status.set_static_mesh(meshes['Flow_Status'])
 cdo.status.set_relative_location(u.Vector(10.7,17,4),False,False)
 setp(cdo,scannable_permit_ids=['MarsEcologyAccess'],empty_material=u.load_asset(ROOT+'/M_MP_Amber'),ready_material=u.load_asset(ROOT+'/M_MP_Green'))
 u.BlueprintEditorLibrary.compile_blueprint(commbp);save(commbp)
 terminal=u.load_asset('/Game/Space/Ships/Docks/BP_Ship_CentralTerminal');tcdo=u.get_default_object(terminal.generated_class())
 setp(tcdo,credential_dock_class=commbp.generated_class(),credential_dock_transform=u.Transform(location=u.Vector(110,30,-10),rotation=u.Rotator(yaw=90)))
 u.BlueprintEditorLibrary.compile_blueprint(terminal);save(terminal)
 # Exterior ecology airlock, distinct from the central lift doors.
 outerbp=blueprint('BP_Mars_EcologyEntranceDoor',u.JTSAutomaticAccessDoor);ocdo=u.get_default_object(outerbp.generated_class())
 def configure_outer(o):
  for c,key in ((o.frame,'Access_Frame'),(o.left_leaf,'Access_Leaf'),(o.right_leaf,'Access_Leaf'),(o.reader,'Flow_GateReader'),(o.status_light,'Access_Status')):
   c.set_static_mesh(meshes.get(key) or u.load_asset(ROOT+'/SM_MP_'+key));setp(c,override_materials=[])
  o.left_leaf.set_relative_location(u.Vector(0,-70,0),False,False);o.right_leaf.set_relative_location(u.Vector(0,70,0),False,False)
  o.reader.set_relative_location(u.Vector(46,-220,0),False,False);o.status_light.set_relative_location(u.Vector(65,-220,184),False,False)
  setp(o,require_manual_activation=True,required_held_item=u.JTSItemId.PORTABLE_COMMUNICATOR,required_credential_id='MarsEcologyAccess',unlocked_flag='Mars_EcologyGate_Opened',locked_material=u.load_asset(ROOT+'/M_MP_Amber'),authorized_material=u.load_asset(ROOT+'/M_MP_Green'),reading_material=reader_material('Reading',u.LinearColor(.04,.9,1,1)),denied_material=reader_material('Denied',u.LinearColor(1,.025,.01,1)),leaf_travel=u.Vector(0,150,0),**GATE_PROMPTS)
  conditions(o,[])
 configure_outer(ocdo);u.BlueprintEditorLibrary.compile_blueprint(outerbp);save(outerbp)
 gate=aa.get('MC_R_EcologyEntranceDoor') or sub.spawn_actor_from_class(outerbp.generated_class(),point((0,-1840,0)),gate_rot)
 gate.set_actor_label('MC_R_EcologyEntranceDoor');configure_outer(gate);place(gate,(0,-1840,0),gate_rot)
 prop('MC_R_Flow_EntryVestibule',meshes['Flow_EntryVestibule'],(0,-1840,0),gate_rot)
 # The completed hemisphere has no side breach; access is through the airlock.
 # Authored shell pieces use the actual airlock orientation after FBX handedness conversion.
 ribs=aa['MC_R_Dome_Ribs'];ribs.static_mesh_component.set_static_mesh(meshes['Flow_DomeRibs']);ribs.static_mesh_component.set_collision_profile_name('BlockAll')
 aa['MC_R_Dome_Glazing'].static_mesh_component.set_static_mesh(meshes['Flow_DomeGlazing'])
 seal=prop('MC_R_Flow_DomeSeal',meshes['Flow_DomeSeal'],(0,0,0));seal.set_actor_transform(ribs.get_actor_transform(),False,False);seal.static_mesh_component.set_visibility(False,True);seal.set_actor_hidden_in_game(True)
 # A folder sits on the first desk after the gate, before the dead lift.
 table=prop('MC_R_Flow_DiagnosisDesk',u.load_asset(ROOT+'/SM_MP_Lab_Bench'),(480,-1370,0))
 top=108.5 # Desktop, excluding the raised monitor and bottles.
 doc=aa['MC_R_Guide_DomeOperations'];docmesh=doc.get_component_by_class(u.StaticMeshComponent);docmesh.set_static_mesh(meshes['Flow_ServiceFolder']);setp(docmesh,override_materials=[])
 place(doc,(472,-1388,top+1));setp(doc,read_flag='Mars_PowerDiagnosis_Read')
 data=doc.get_editor_property('document');setp(data,title='赤原生态站｜主电源故障 · 维修交接文件夹',body=REPORT);save(data)
 # A/B/C cannot be repaired before the player discovers the fault report.
 for sector in 'ABC':
  a=aa['MC_Sector_'+sector];a.mesh.set_static_mesh(meshes['Flow_Combiner']);setp(a.mesh,override_materials=[])
  conditions(a,['Mars_EcologyGate_Opened','Mars_PowerDiagnosis_Read'])
  repair_prompts(a,sector);setp(a,repair_seconds=3.5)
  a.status_light.set_static_mesh(meshes['Flow_Status']);a.status_light.set_relative_location(u.Vector(54,0,143),False,False);a.status_light.set_relative_scale3d(u.Vector(1,6,4))
 power=aa['MC_Power'];place(power,(0,245,418));conditions(power,['Mars_Sector_'+s+'_Repaired' for s in 'ABC'])
 setp(power,status_activation_delay=1.1,display_name='生态站主电源 · 三路维修联锁',inactive_material=u.load_asset(ROOT+'/M_MP_Dust'),active_material=u.load_asset(ROOT+'/M_MP_Green'))
 # The actual lift door has a separate, power-dependent latch and never uses the lunar credential.
 door=aa['MC_R_StationAccessDoor']
 def configure_lift(o):
  conditions(o,['Mars_Power','Mars_EcologyGate_Opened'])
  setp(o,require_manual_activation=True,required_held_item=u.JTSItemId.NONE,required_credential_id='',unlocked_flag='Mars_LiftDoor_Opened',swipe_seconds=.45,**LIFT_PROMPTS)
 configure_lift(door)
 liftbp=u.load_asset(BP+'/BP_Mars_StationAccessDoor');configure_lift(u.get_default_object(liftbp.generated_class()));u.BlueprintEditorLibrary.compile_blueprint(liftbp);save(liftbp)
 lift=aa['MC_Station_Elevator'];conditions(lift,['Mars_Power','Mars_EcologyGate_Opened','Mars_LiftDoor_Opened'])
 setp(lift,locked_prompt='未就绪',display_name='下行')
 # Light and camera animate the final handover from the outdoor repairs to the lift power source.
 glow=aa.get('MC_R_Flow_LiftPowerGlow') or sub.spawn_actor_from_class(u.PointLight,point((0,0,405)),rotation)
 glow.set_actor_label('MC_R_Flow_LiftPowerGlow');place(glow,(0,0,405));glow.point_light_component.set_mobility(u.ComponentMobility.MOVABLE)
 glow.point_light_component.set_light_color(u.LinearColor(.12,1,.55,1));glow.point_light_component.set_intensity(900);glow.point_light_component.set_attenuation_radius(650);glow.point_light_component.set_cast_shadows(False)
 powered=[glow,aa['MC_R_StationAccessLamp']]+[a for label,a in aa.items() if label in ('MC_Dome_Lantern','MC_R_Dome_Light_-350','MC_R_Dome_Light_350','MC_Hub_Light','MC_B1_Light','MC_B2_Light','MC_B3_Light','MC_B4_Light') or label.startswith(('MC_Hall_','MC_R_B1_WorkLight','MC_R_B2_WorkLight','MC_R_B3_WorkLight','MC_R_B4_WorkLight')) and isinstance(a,u.Light)]
 setp(power,powered_actors=powered)
 for a in powered:a.set_actor_hidden_in_game(True)
 # Independent amber battery lamp makes the entrance reader legible during the blackout.
 aux=aa.get('MC_R_Flow_AuxDoorLamp') or sub.spawn_actor_from_class(u.PointLight,point((0,-2220,290)),rotation)
 aux.set_actor_label('MC_R_Flow_AuxDoorLamp');place(aux,(0,-2220,290));aux.point_light_component.set_mobility(u.ComponentMobility.MOVABLE);aux.point_light_component.set_light_color(u.LinearColor(1,.65,.32,1));aux.point_light_component.set_intensity(500);aux.point_light_component.set_attenuation_radius(1000);aux.point_light_component.set_cast_shadows(False)
 aa['MC_R_StationAccessLamp'].set_actor_hidden_in_game(True)
 reveal=aa.get('MC_R_Flow_PowerReveal') or sub.spawn_actor_from_class(u.JTSFacilityPowerReveal,point((0,0,0)),rotation)
 reveal.set_actor_label('MC_R_Flow_PowerReveal');place(reveal,(0,0,0))
 setp(reveal,source=power,camera_start=u.Vector(650,1050,620),camera_end=u.Vector(300,650,510),focus=u.Vector(0,280,455),duration=4.1,audience_radius=18000,notice='电梯已通电')
 audio_path='/Game/Space/Environment/MarsSystem/Chapter/Audio/S_Mars_LiftPowerOn'
 if not u.load_asset(audio_path):
  task=setp(u.AssetImportTask(),filename=str(Path(u.Paths.project_dir())/'SourceArt/MarsSystem/Audio/S_Mars_LiftPowerOn.wav'),destination_path='/Game/Space/Environment/MarsSystem/Chapter/Audio',automated=True,save=True)
  at.import_asset_tasks([task])
 setp(reveal,power_on_sound=u.load_asset(audio_path))
 # Remove instructions that described the discarded proximity-permit shortcut.
 arrival=aa['MC_R_Guide_Arrival'].get_editor_property('document')
 setp(arrival,body='进入生态球前，请在飞船中心终端扫描月球通行卡，拿起移动通讯器。手持通讯器到大门读卡器长按 E 刷卡。\n\n门后的第一张工作台上放着设备组维修文件夹。中央电梯当前断电，门禁许可不能启动电梯。\n\n三区汇流器修复后，系统会自动接通电梯供电，并提示返回。');save(arrival)
 for name in ('Director','Power'):
  data=u.load_asset('/Game/Space/Data/Planets/Mars/Logs/DA_Log_'+name)
  if data:setp(data,body='主电源已隔离，中央电梯停运。详细检修交接文件夹留在生态球大门后的工作台。先读取交接单，再修复 A/B/C 三区汇流器；三路自检通过后电梯恢复供电。');save(data)
 finish()
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
 return audit()

def audit():
 _,aa,_=ctx();g=aa['MC_R_EcologyEntranceDoor'];d=aa['MC_R_StationAccessDoor'];power=aa['MC_Power']
 assert g!=d and g.get_editor_property('require_manual_activation') and d.get_editor_property('require_manual_activation')
 assert 1900 < (g.get_actor_location()-d.get_actor_location()).length() < 2300,'Entrance and lift must share the dome placement frame'
 assert str(g.get_editor_property('required_credential_id'))=='MarsEcologyAccess'
 assert str(d.get_editor_property('required_credential_id'))=='None'
 assert {str(r.id) for r in d.conditions.requirements}=={'Mars_Power','Mars_EcologyGate_Opened'}
 assert {str(r.id) for r in power.conditions.requirements}=={'Mars_Sector_'+s+'_Repaired' for s in 'ABC'}
 return {'gate':g.get_actor_label(),'lift_door':d.get_actor_label(),'distance_between_doors_cm':(g.get_actor_location()-d.get_actor_location()).length(),'power_conditions':[str(r.id) for r in power.conditions.requirements],'folder_read_flag':str(aa['MC_R_Guide_DomeOperations'].get_editor_property('read_flag')),'repair_conditions':{s:[str(r.id) for r in aa['MC_Sector_'+s].conditions.requirements] for s in 'ABC'},'communicator_definition':bool(u.load_asset(ITEMS+'/DA_Item_PortableCommunicator')),'reveal':aa['MC_R_Flow_PowerReveal'].get_class().get_name()}

def finish():
 sub,aa,planet=ctx()
 house=aa['MC_R_Lift_Housing'];t=house.get_actor_transform();parts=aa['MC_R_Recovery_ServiceParts'];old=t.inverse_transform_location(parts.get_actor_location())
 parts.set_actor_location_and_rotation(t.transform_location(u.Vector(-480,-1350,0)),house.get_actor_rotation(),False,False)
 u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level();return {'removed_airlock_obstruction':True,'parts_previous':str(old),'parts_now':'left of first desk, inside the main gate'}

def view(label,camera,target):
 _,aa,_=ctx();a=aa[label];t=a.get_actor_transform();p=t.transform_location(u.Vector(*camera));rot=u.MathLibrary.make_rot_from_xz(t.transform_location(u.Vector(*target))-p,a.get_actor_up_vector())
 u.EditorLevelLibrary.set_level_viewport_camera_info(p,rot);u.EditorLevelLibrary.editor_set_game_view(True);u.EditorLevelLibrary.editor_invalidate_viewports();return {'view':label}
def view_entry():return view('MC_R_EcologyEntranceDoor',(1100,600,480),(0,0,205))
def view_lift():return view('MC_R_Lift_Housing',(650,1050,560),(0,-60,300))
def view_folder():return view('MC_R_Guide_DomeOperations',(50,-80,80),(0,0,2))

def prepare():
 defaults=u.get_default_object(u.JTSExpeditionSubsystem);setp(defaults,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation')
 settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'));setp(settings,PlayNumberOfClients=2,RunUnderOneProcess=True)
 return {'isolated_prefix':'MarsAccessValidation','clients':2}

def probe():
 import runpy
 path=Path(u.Paths.project_dir())/'Saved/StationFlowAction.txt';action=path.read_text(encoding='utf8').strip() if path.exists() else 'status'
 if action.startswith('siting_'):
  return runpy.run_path(str(Path(__file__).with_name('fix_mars_station_siting.py')))['run'](action[7:])
 if action in ('travel','connect_test_clients','resume_test_phase','save_reload'):
  (path.parent/'MarsProbeAction.txt').write_text(action,encoding='utf8');return runpy.run_path(str(Path(__file__).with_name('probe_mars_chapter.py')))['run']()
 worlds=u.EditorLevelLibrary.get_pie_worlds(True)
 if not worlds:return {'playing':False}
 w=next(w for w in worlds if u.GameplayStatics.get_game_state(w) and u.GameplayStatics.get_game_state(w).has_authority())
 aa=u.GameplayStatics.get_all_actors_of_class(w,u.Actor);by={a.get_actor_label():a for a in aa};gs=u.GameplayStatics.get_game_state(w)
 players=[a for a in aa if isinstance(a,u.JTSCharacter) and a.get_controller() and not a.get_component_by_class(u.JTSHealthComponent).call_method('IsDead',())];p=players[0]
 exp=next(s for s in u.ObjectIterator(u.JTSExpeditionSubsystem) if s.get_outer()==u.GameplayStatics.get_game_instance(w));ship=gs.get_active_spacecraft()
 planet=next(a for a in aa if isinstance(a,u.JTSPlanetAnchor) and str(a.get_planet_id())=='Mars')
 inv=p.get_component_by_class(u.JTSInventoryComponent);gate=by['MC_R_EcologyEntranceDoor'];door=by['MC_R_StationAccessDoor'];power=by['MC_Power']
 dock=next((a for a in aa if isinstance(a,u.JTSPortableCredentialDock)),None)
 def aim_fixture(aim):
  up=planet.get_radial_up_vector(p.get_actor_location());loc=p.get_actor_location()+up*64;rot=u.MathLibrary.make_rot_from_xz(aim-loc,up)
  cam=by.get('StationFixtureAimCamera')
  if not cam:
   transform=u.Transform(location=loc,rotation=rot,scale=u.Vector(1,1,1));api=u.get_default_object(u.GameplayStatics);cam=api.call_method('BeginDeferredActorSpawnFromClass',(w,u.CameraActor.static_class(),transform,u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));api.call_method('FinishSpawningActor',(cam,transform,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));by['StationFixtureAimCamera']=cam
  cam.set_actor_label('StationFixtureAimCamera');cam.set_actor_location_and_rotation(loc,rot,False,False);p.get_controller().set_view_target_with_blend(cam,0)
 def move(a,local,target=(0,0,150),player=p):
  player.set_gameplay_planet(planet);pos=a.get_actor_transform().transform_location(u.Vector(*local));aim=a.get_actor_transform().transform_location(u.Vector(*target))
  rot=u.MathLibrary.make_rot_from_xz(aim-(pos+a.get_actor_up_vector()*64),a.get_actor_up_vector());player.set_actor_location_and_rotation(pos,rot,False,False);player.initialize_planet_frame();player.get_controller().set_control_rotation(rot);player.get_component_by_class(u.CharacterMovementComponent).stop_movement_immediately()
  if player==p:aim_fixture(aim)
 def approach_dock():
  owner=dock.get_owner();s=owner.get_screen_world_location();up=owner.get_actor_up_vector();normal=owner.screen_anchor.get_right_vector();pos=s+owner.get_actor_forward_vector()*91+normal*95-up*109
  p.set_actor_location_and_rotation(pos,u.MathLibrary.make_rot_from_xz(dock.get_actor_location()+up*20-pos-up*64,up),False,False);p.initialize_planet_frame();p.get_controller().set_control_rotation(p.get_actor_rotation())
  center,_=dock.get_actor_bounds(False);aim_fixture(center)
 result={'action':action}
 if action=='isolate':
  setp(exp,save_slot_prefix='MarsAccessValidation',save_slot_name='MarsAccessValidation');result['new_slot']=exp.begin_new_expedition_in_slot(4,'Station physical access verification')
  for player in players:player.get_component_by_class(u.JTSHealthComponent).set_invulnerable(True)
 elif action=='grant':
  assert exp.get_editor_property('save_slot_prefix')=='MarsAccessValidation' and exp.get_active_save_slot()==4
  u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.GrantStationPermit')
 elif action=='dock':approach_dock()
 elif action=='aim_dock':
  center,_=dock.get_actor_bounds(False);aim_fixture(center)
 elif action=='input_api':
  result['api']={'library':[x for x in dir(u.EnhancedInputLibrary) if 'value' in x.lower() or 'axis' in x.lower()],'subsystems':[(str(s.get_outer()),s.get_path_name()) for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)]}
 elif action=='take':
  approach_dock();result['can_scan']=dock.can_execute_interaction(p);dock.interact(p)
 elif action=='gate':move(gate,(180,-160,98),(45,-220,155))
 elif action=='swipe':
  move(gate,(180,-160,98),(45,-220,155));result['can_swipe']=gate.can_execute_interaction(p);gate.interact(p)
 elif action=='read':
  doc=by['MC_R_Guide_DomeOperations'];move(doc,(0,125,42),(0,0,2));result['can_read']=doc.can_interact(p);doc.interact(p)
 elif action=='parts':
  a=by['MC_R_Recovery_ServiceParts'];move(a,(150,0,98));result['can_claim']=a.can_execute_interaction(p);a.interact(p)
 elif action.startswith('repair'):
  a=by['MC_Sector_'+action[-1]];move(a,(175,0,98),(40,0,100));result['can_repair']=a.can_execute_interaction(p);result['prompt']=str(a.get_interaction_prompt(p))
  if action.startswith('repair_now'):a.interact(p)
 elif action=='lift':move(door,(170,0,98),(20,0,145))
 elif action=='open_lift':
  move(door,(170,0,98),(20,0,145));result['can_open']=door.can_execute_interaction(p);door.interact(p)
 elif action=='transit':
  a=by['MC_Station_Elevator'];move(a,(0,120,98),(0,0,30));result['can_transit']=a.can_execute_interaction(p);a.interact(p)
 elif action=='hold':
  c=p.get_component_by_class(u.InteractionComponent);c.refresh_interactable();c.try_interact()
 elif action=='cancel':p.get_component_by_class(u.InteractionComponent).cancel_interaction()
 elif action=='dismiss':
  p.get_controller().call_method('ClientReadDocument',(by['MC_R_Guide_DomeOperations'].get_editor_property('document'),))
  for widget in u.ObjectIterator(u.load_class(None,'/Script/space.JTSReadableDocumentWidget')):
   if not widget.get_name().startswith('Default__'):widget.call_method('Close',())
 elif action=='capture':u.SystemLibrary.execute_console_command(w,'HighResShot 1280x720',p.get_controller())
 elif action=='drop':result['dropped']=inv.drop_item_at_slot(inv.get_selected_quickbar_slot())
 elif action=='handoff':
  a=next(a for a in aa if isinstance(a,u.JTSWorldPickupActor) and a.get_item_instance().item_id==u.JTSItemId.PORTABLE_COMMUNICATOR);other=players[1];other_inv=other.get_component_by_class(u.JTSInventoryComponent);other_inv.drop_item_at_slot(1);move(a,(100,0,98),(0,0,10),other);result['can_pickup']=a.can_interact(other);a.interact(other)
  for i,item in enumerate(other_inv.get_editor_property('item_slots')):
   if item.item_id==u.JTSItemId.PORTABLE_COMMUNICATOR:other_inv.select_quickbar_slot(i)
  result['remote_payload']=str(other_inv.get_active_item())
 elif action=='return_device':
  other=players[1];other_inv=other.get_component_by_class(u.JTSInventoryComponent);result['dropped']=other_inv.drop_item_at_slot(other_inv.get_selected_quickbar_slot())
 elif action=='save_device':
  setp(exp,save_slot_prefix='MarsValidationStation',save_slot_name='MarsValidationStation');u.SystemLibrary.execute_console_command(w,'jts.Editor.Mars.SaveReload')
  saved=u.GameplayStatics.load_game_from_slot('MarsValidationStation_04',0);result['saved_loadable']=bool(saved);assert saved
 elif action=='passage':
  result['gate_passage']=[]
  for x in (-75,0,75):
   for z in (45,100,165):
    t=gate.get_actor_transform();start=t.transform_location(u.Vector(350,x,z));end=t.transform_location(u.Vector(-350,x,z));hit=u.SystemLibrary.sphere_trace_single(w,start,end,34,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,players,u.DrawDebugTrace.NONE,True)
    actor=hit.to_tuple()[9] if hit else None;result['gate_passage'].append({'lane':x,'height':z,'hit':actor.get_actor_label() if actor else None,'impact_in_dome':str(by['MC_R_Dome_Ribs'].get_actor_transform().inverse_transform_location(hit.to_tuple()[5])) if hit else None})
  import math
  result['dome_boundaries']=[];t=by['MC_R_Dome_Ribs'].get_actor_transform()
  for degrees in range(0,360,45):
   angle=math.radians(degrees);direction=u.Vector(math.cos(angle),math.sin(angle),0);start=t.transform_location(direction*2150+u.Vector(0,0,120));end=t.transform_location(direction*1700+u.Vector(0,0,120));hit=u.SystemLibrary.sphere_trace_single(w,start,end,34,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,players,u.DrawDebugTrace.NONE,True);actor=hit.to_tuple()[9] if hit else None
   result['dome_boundaries'].append({'direction':degrees,'blocker':actor.get_actor_label() if actor else None})
 elif action=='restore_camera':p.get_controller().set_view_target_with_blend(p,0)
 elif action=='visual':
  result['held_meshes']=[{'name':c.get_name(),'mesh':c.static_mesh.get_name() if c.static_mesh else None,'visible':c.is_visible(),'scale':str(c.get_world_scale())} for c in p.get_components_by_class(u.StaticMeshComponent) if 'Weapon' in c.get_name()]
  body=next(c for c in p.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='WeaponVisualBody');focus=u.SystemLibrary.get_component_bounds(body)[0]
  for v in worlds:
   pc=u.GameplayStatics.get_player_controller(v,0)
   if not pc:continue
   loc=p.get_actor_location()+p.get_actor_forward_vector()*235+p.get_actor_right_vector()*140+p.get_actor_up_vector()*90;rot=u.MathLibrary.make_rot_from_xz(focus-loc,p.get_actor_up_vector());t=u.Transform(location=loc,rotation=rot,scale=u.Vector(1,1,1));api=u.get_default_object(u.GameplayStatics);cam=api.call_method('BeginDeferredActorSpawnFromClass',(v,u.CameraActor.static_class(),t,u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));api.call_method('FinishSpawningActor',(cam,t,u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT));cam.get_component_by_class(u.CameraComponent).set_field_of_view(48);pc.set_view_target_with_blend(cam,0)
 elif action=='space':
  result['dropped']=inv.drop_item_at_slot(1)
 elif action=='pickup':
  pickups=[a for a in aa if isinstance(a,u.JTSWorldPickupActor) and a.get_item_instance().item_id==u.JTSItemId.PORTABLE_COMMUNICATOR]
  a=pickups[0];move(a,(100,0,98),(0,0,10));result['can_pickup']=a.can_interact(p);a.interact(p)
 result['players']=len(players);result['shared_permit']=gs.has_important_item('MarsEcologyAccess');result['flags']=[str(x) for x in gs.get_editor_property('facility_flag_ids')]
 result['player_items']=[{'name':player.get_name(),'items':[str(i) for i in player.get_component_by_class(u.JTSInventoryComponent).get_editor_property('item_slots')]} for player in players]
 result['gates']=[{'label':a.get_actor_label(),'open':a.get_editor_property('open'),'authorized':a.get_editor_property('authorized'),'can_execute':a.can_execute_interaction(p),'prompt':str(a.get_interaction_prompt(p))} for a in (gate,door)]
 if action in ('gate','hold'):
  cam=u.GameplayStatics.get_player_camera_manager(w,0);center,_=gate.get_actor_bounds(False);reader=gate.reader.get_world_location();view=cam.get_camera_location();f=cam.get_camera_rotation().get_forward_vector()
  result['gate_trace']={'center_local':str(gate.get_actor_transform().inverse_transform_location(center)),'reader_local':str(gate.get_actor_transform().inverse_transform_location(reader)),'center_distance':(p.get_actor_location()-center).length(),'center_alignment':f.dot((center-view).normal())}
  for key,target in (('center',center),('aim',view+f*1000)):
   hit=u.SystemLibrary.line_trace_single(w,view,target,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p],u.DrawDebugTrace.NONE,True);result['gate_trace'][key]=[str(hit.to_tuple()[i]) for i in (0,5,9,10)] if hit else None
 result['item']={'id':str(inv.get_active_item_id()),'payload':str(inv.get_active_item()),'slots':[(str(i.item_id),i.stack_count) for i in inv.get_editor_property('item_slots')],'capacity':inv.get_inventory_capacity()}
 if dock:result['dock']={'available':dock.get_editor_property('available'),'can_inspect':dock.can_interact(p),'can_execute':dock.can_execute_interaction(p),'prompt':str(dock.get_interaction_prompt(p)),'owner':dock.get_owner().get_name(),'local':str(dock.get_owner().get_actor_transform().inverse_transform_location(dock.get_actor_location())),'screen':str(dock.get_owner().get_screen_world_location())}
 if dock:
  cam=u.GameplayStatics.get_player_camera_manager(w,0);center,_=dock.get_actor_bounds(False);loc=cam.get_camera_location();direction=cam.get_camera_rotation().get_forward_vector()
  hit=u.SystemLibrary.line_trace_single(w,loc,center,u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[p,ship],u.DrawDebugTrace.NONE,True)
  target=hit.to_tuple()[9];result['aim']={'camera_local':str(dock.get_actor_transform().inverse_transform_location(loc)),'alignment':direction.dot((center-loc).normal()),'hit_actor':target.get_name() if target else None,'center':str(center),'pawn':str(p.get_actor_location())}
 result['cameras']=[{'world':v.get_name(),'view':u.GameplayStatics.get_player_controller(v,0).get_view_target().get_name(),'move_ignored':u.GameplayStatics.get_player_controller(v,0).is_move_input_ignored()} for v in worlds if u.GameplayStatics.get_player_controller(v,0)]
 c=p.get_component_by_class(u.InteractionComponent);a=c.get_current_interactable();result['interaction']={'target':a.get_actor_label() if a else None,'prompt':str(c.get_current_interaction_prompt()),'progress':c.get_interaction_hold_progress()}
 result['worlds']=[{'world':v.get_name(),'authority':u.GameplayStatics.get_game_state(v).has_authority(),'flags':[str(f) for f in u.GameplayStatics.get_game_state(v).get_editor_property('facility_flag_ids')],'gates':[(a.get_actor_label(),a.get_editor_property('open'),a.get_editor_property('authorized'),str(a.get_actor_scale3d())) for a in u.GameplayStatics.get_all_actors_of_class(v,u.JTSAutomaticAccessDoor)]} for v in worlds if isinstance(u.GameplayStatics.get_game_state(v),u.JTSGameState)]
 for vr,v in zip(result['worlds'],worlds):
  vr['communicators']=[str(a.get_component_by_class(u.JTSInventoryComponent).get_active_item()) for a in u.GameplayStatics.get_all_actors_of_class(v,u.JTSCharacter) if a.get_component_by_class(u.JTSInventoryComponent).get_active_item_id()==u.JTSItemId.PORTABLE_COMMUNICATOR]
  vr['cradles']=[{'available':d.get_editor_property('available'),'visible':d.device.is_visible()} for d in u.GameplayStatics.get_all_actors_of_class(v,u.JTSPortableCredentialDock)]
 return result
