"""Import independently replaceable ship parts and author the production Blueprint configuration."""
import unreal, os, json, wave, math, struct, random
ROOT='/Game/Space/Ships/Refit';SRC='D:/projects/space/SourceArt/ShipRefit'
at=unreal.AssetToolsHelpers.get_asset_tools()
def save(o):
 if isinstance(o,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(o)
 unreal.EditorAssetLibrary.save_loaded_asset(o,False)
def asset(n,cls,fac,folder=ROOT):
 p=folder+'/'+n
 return unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else at.create_asset(n,folder,cls,fac)
colors={'Ceramic':(.43,.48,.47),'Frame':(.022,.036,.046),'Metal':(.16,.22,.25),'Panel':(.085,.13,.15),'Teal':(.025,.29,.28),'Orange':(.9,.24,.045),'Glass':(.018,.075,.095),'Glow':(.035,.85,.68),'Amber':(1,.38,.045),'White':(.68,.85,.85),'Screen':(.003,.009,.018)}
# Original, short mechanical and laser cues keep the physical modules self-contained.
sounds={}
for name,length in [('Laser',.23),('MainLaser',.42),('Lever',.34),('Lock',.38)]:
 path=SRC+'/S_Refit_'+name+'.wav';rng=random.Random(73);samples=[];phase=0
 for i in range(int(44100*length)):
  t=i/44100;q=t/length
  if name in ['Laser','MainLaser']:
   base=155 if name=='MainLaser' else 340;phase+=2*math.pi*(base+1550*math.exp(-q*13))/44100
   value=(math.sin(phase)*.58+math.sin(phase*.501)*.23+rng.uniform(-1,1)*.16)*math.exp(-q*6)*min(1,t*1500)
  elif name=='Lever':
   value=(rng.uniform(-1,1)*.65+math.sin(2*math.pi*95*t)*.2)*(math.exp(-q*32)+.65*math.exp(-((q-.78)/.06)**2))
  else:
   value=(math.sin(2*math.pi*880*t)*.3+math.sin(2*math.pi*1320*t)*.19)*math.exp(-q*7)+rng.uniform(-1,1)*.25*math.exp(-q*65)
  samples.append(struct.pack('<h',int(max(-1,min(1,value))*.8*32767)))
 with wave.open(path,'wb') as f:f.setnchannels(1);f.setsampwidth(2);f.setframerate(44100);f.writeframes(b''.join(samples))
 task=unreal.AssetImportTask();task.filename=path;task.destination_path=ROOT+'/Audio';task.automated=True;task.replace_existing=True;task.save=True
 at.import_asset_tasks([task]);sounds[name]=unreal.load_asset(ROOT+'/Audio/S_Refit_'+name)
mats={}
for n,c in colors.items():
 m=asset('M_Refit_'+n,unreal.Material,unreal.MaterialFactoryNew(),ROOT+'/Materials');unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
 e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-300,0);e.constant=unreal.LinearColor(*(v*2 if n in ['Glow','Amber'] else v for v in c),1)
 glow=n in ['Glow','Amber','White','Screen']
 if glow:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 unreal.MaterialEditingLibrary.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
 if not glow:
  for prop,val in [(unreal.MaterialProperty.MP_METALLIC,.7 if n in ['Metal','Frame','Panel'] else .35),(unreal.MaterialProperty.MP_ROUGHNESS,.2 if n=='Glass' else .38)]:
   x=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-300,170);x.r=val;unreal.MaterialEditingLibrary.connect_material_property(x,'',prop)
 unreal.MaterialEditingLibrary.recompile_material(m);save(m);mats['Refit_'+n]=m
meshes={};sm_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
for f in os.listdir(SRC):
 if not f.endswith('.fbx'):continue
 n=f[:-4];t=unreal.AssetImportTask();t.filename=SRC+'/'+f;t.destination_path=ROOT+'/Meshes';t.destination_name=n;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.automated_import_should_detect_type=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 d=o.static_mesh_import_data;d.combine_meshes=True;d.generate_lightmap_u_vs=False;d.auto_generate_collision=False;d.convert_scene=True;d.force_front_x_axis=False;d.import_rotation=unreal.Rotator(yaw=180);t.options=o
 at.import_asset_tasks([t]);m=unreal.load_asset(ROOT+'/Meshes/'+n)
 if not m:raise RuntimeError('Import failed '+n)
 for i,s in enumerate(m.static_materials):
  key=next((k for k in mats if str(s.material_slot_name).startswith(k)),None)
  if key:m.set_material(i,mats[key])
 if '_Col_' in n:
  sm_editor.remove_collisions(m);sm_editor.add_simple_collisions(m,unreal.ScriptingCollisionShapeType.NDOP26)
 save(m);meshes[n]=m

def bp(n,cls):
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',cls);return asset(n,unreal.Blueprint,f)
def loc(c,v):c.set_editor_property('relative_location',unreal.Vector(*v))
def rot(c,p=0,y=0,r=0):c.set_editor_property('relative_rotation',unreal.Rotator(pitch=p,yaw=y,roll=r))
def mesh(c,n):
 c.set_static_mesh(meshes[n])
 c.set_editor_property('override_materials',[])
 # FBX's handedness conversion reverses the authored longitudinal axis.
 c.set_editor_property('relative_scale3d',unreal.Vector(-1,1,1))

# Shared library pieces remain the same laser class; the main gun receives a larger installation scale.
weapon=bp('BP_ShipLaser',unreal.JTSShipWeaponModule);w=unreal.get_default_object(weapon.generated_class())
mesh(w.mount,'SM_Laser_Mount');mesh(w.turret,'SM_Laser_Cradle');mesh(w.gun,'SM_Laser_Gun')
loc(w.pitch_pivot,(0,0,48));loc(w.muzzle,(-119,0,0));mesh(w.beam,'SM_Laser_BeamUnit');mesh(w.impact,'SM_Laser_ImpactUnit');w.set_editor_property('damage',18);w.set_editor_property('fire_interval',.24)
w.set_editor_property('fire_sound',sounds['Laser']);save(weapon)
main=bp('BP_ShipMainLaser',unreal.JTSShipWeaponModule);c=unreal.get_default_object(main.generated_class())
for part,n in [('mount','SM_Laser_Mount'),('turret','SM_Laser_Cradle'),('gun','SM_Laser_Gun'),('beam','SM_Laser_BeamUnit'),('impact','SM_Laser_ImpactUnit')]:mesh(c.get_editor_property(part),n)
loc(c.pitch_pivot,(0,0,48));loc(c.muzzle,(-119,0,0))
for p,v in [('damage',65),('fire_interval',.7),('beam_radius',4.5),('recoil_travel',10),('fire_sound',sounds['MainLaser'])]:c.set_editor_property(p,v)
save(main)
exchange=bp('BP_Dock_StellarExchange',unreal.JTSStellarExchangeDock);c=unreal.get_default_object(exchange.generated_class())
c.set_editor_property('display_name','星际商店');c.set_editor_property('deployable',False)
mesh(c.body,'SM_StellarDock_Body');mesh(c.lever,'SM_StellarDock_Lever')
loc(c.screen_hinge,(0,0,0));loc(c.screen_anchor,(-17,35,5));c.screen_target.set_box_extent(unreal.Vector(59,5,19),False)
loc(c.lever_pivot,(67,43,-19));loc(c.lever_target,(0,0,35));c.lever_target.set_box_extent(unreal.Vector(19,19,28),False)
rot(c.display,y=90);c.display.set_draw_size(unreal.Vector2D(930,222));c.display.set_editor_property('relative_scale3d',unreal.Vector(.123,.123,.13))
c.set_editor_property('pull_sound',sounds['Lever']);c.set_editor_property('lock_sound',sounds['Lock'])
# Display local Y and Z map to the physical glass width and height.
save(exchange)
f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.JTSShipDockDefinition)
definition=asset('DA_Dock_StellarExchange',unreal.JTSShipDockDefinition,f);definition.set_editor_property('module_id','StellarExchange');definition.set_editor_property('actor_class',exchange.generated_class())
definition.set_editor_property('mount_transform',unreal.Transform(location=unreal.Vector(360,394,-5),rotation=unreal.Rotator(yaw=180)));save(definition)

hardpoints=[]
for side,cy in [('L',-520),('R',520)]:
 for i,(x,z,scale) in enumerate([(648,6,.95),(130,130,1),(-320,135,1)],1):
  h=unreal.JTSShipHardpoint();h.id=f'{side}{i}';h.weapon_class=weapon.generated_class();h.mount_transform=unreal.Transform(location=unreal.Vector(x,cy,z),scale=unreal.Vector(scale,scale,scale));hardpoints.append(h)
h=unreal.JTSShipHardpoint();h.id='Main';h.weapon_class=main.generated_class();h.mount_transform=unreal.Transform(location=unreal.Vector(270,0,-65),rotation=unreal.Rotator(roll=180),scale=unreal.Vector(1.4,1.4,1.4));hardpoints.append(h)
h=unreal.JTSShipHardpoint();h.id='Turret';h.weapon_class=weapon.generated_class();h.full_rotation=True;h.mount_transform=unreal.Transform(location=unreal.Vector(-120,0,238),scale=unreal.Vector(1.2,1.2,1.2));hardpoints.append(h)

sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
summary={}
for path in ['/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1','/Game/Space/Blueprints/Ships/BP_Spacecraft']:
 ship=unreal.load_asset(path);cdo=unreal.get_default_object(ship.generated_class());templates={}
 for handle in sub.k2_gather_subobject_data_for_blueprint(ship):
  data=lib.get_data(handle);obj=lib.get_object_for_blueprint(data,ship)
  if obj:templates[obj.get_name().removesuffix('_GEN_VARIABLE')]=obj
 unreal.log('REFIT_TEMPLATES '+str(list(templates)))
 # Native subobjects use the CDO, SCS components use the editable Blueprint template.
 mesh(cdo.get_editor_property('spacecraft_mesh'),'SM_Refit_Armor')
 for name,mn in [('Hull','SM_Refit_Frame'),('ColLeft','SM_Refit_Col_Left'),('ColRight','SM_Refit_Col_Right'),('ColCabin','SM_Refit_Col_Cabin'),('ColBridge','SM_Refit_Col_Bridge')]:
  if name not in templates:raise RuntimeError('Missing Blueprint component '+name)
  mesh(templates[name],mn)
 templates['Hull'].set_visibility(True);templates['Hull'].set_hidden_in_game(False)
 # Collision proxies are already widened in their own mesh geometry.
 for name,o in templates.items():
  if not isinstance(o,unreal.SceneComponent):continue
  p=o.relative_location
  if name.startswith('Cap_'):
   if isinstance(o,unreal.StaticMeshComponent):o.set_static_mesh(None)
  elif name.startswith(('Gear_','Cove_','FX_Lift')) and abs(p.y)>200:
   loc(o,(p.x,math.copysign(555 if name.startswith('Gear_') else 520,p.y),p.z))
  elif name in ['Door','BoardingPoint','ExitPoint']:
   loc(o,(p.x,-186 if name=='Door' else -267,p.z))
  elif name=='Step':loc(o,(p.x,-610,p.z))
  elif name in ['HP_L1','HP_L2','HP_L3','HP_R1','HP_R2','HP_R3']:
   hh=next(h for h in hardpoints if h.id==name.removeprefix('HP_'));o.set_editor_property('relative_location',hh.mount_transform.translation)
 cdo.get_editor_property('flight_collision').set_box_extent(unreal.Vector(655,135,210),False)
 cdo.get_editor_property('armament').set_editor_property('hardpoints',hardpoints)
 docks=cdo.get_editor_property('dock_component');catalog=list(docks.catalog)
 catalog=[d for d in catalog if str(d.module_id)!='StellarExchange']+[definition];docks.set_editor_property('catalog',catalog)
 installed=list(docks.initial_installed_ids)
 if 'StellarExchange' not in [str(x) for x in installed]:installed.append('StellarExchange')
 docks.set_editor_property('initial_installed_ids',installed)
 save(ship);summary[path]={'components':list(templates),'hardpoints':len(hardpoints)}
for kind in ['Material','Weapon']:
 d=unreal.load_asset('/Game/Space/Ships/Docks/DA_Dock_'+kind);t=d.mount_transform;t.translation=unreal.Vector(-65 if kind=='Material' else 125,646,-5);d.set_editor_property('mount_transform',t);save(d)
d=unreal.load_asset('/Game/Space/Ships/Docks/DA_Ship_CentralTerminal');t=d.mount_transform;t.translation=unreal.Vector(308,0,5);d.set_editor_property('mount_transform',t);save(d)
open('D:/projects/space/Saved/ship_refit_assets.json','w').write(json.dumps(summary,indent=2))
unreal.log('SHIP_REFIT_ASSETS_COMPLETE')
