import unreal,os,json
ROOT='/Game/Space/Ships/Docks';SRC='D:/projects/space/SourceArt/ShipDocks'
at=unreal.AssetToolsHelpers.get_asset_tools()
def save(o):
 if isinstance(o,unreal.Blueprint):unreal.BlueprintEditorLibrary.compile_blueprint(o)
 unreal.EditorAssetLibrary.save_loaded_asset(o,False)
def asset(n,cls,factory,folder=ROOT):
 p=folder+'/'+n
 return unreal.load_asset(p) if unreal.EditorAssetLibrary.does_asset_exist(p) else at.create_asset(n,folder,cls,factory)
colors={'Ivory':(.55,.58,.56),'Graphite':(.022,.035,.046),'Steel':(.19,.24,.27),'Screen':(.002,.012,.023),'Teal':(.015,.5,.43),'Orange':(.85,.26,.045),'GlowTeal':(.025,.8,.61),'GlowOrange':(1,.32,.035),'Ink':(.14,.25,.28),'White':(.65,.85,.87)}
mats={}
for n,c in colors.items():
 m=asset('M_Dock_'+n,unreal.Material,unreal.MaterialFactoryNew(),ROOT+'/Materials')
 unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
 p=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-300,0);p.set_editor_property('constant',unreal.LinearColor(*(v*2 if n.startswith('Glow') else v for v in c),1))
 emissive=n.startswith('Glow') or n in ['White','Screen']
 if emissive:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 unreal.MaterialEditingLibrary.connect_material_property(p,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if emissive else unreal.MaterialProperty.MP_BASE_COLOR)
 if not emissive:
  for prop,value in [(unreal.MaterialProperty.MP_METALLIC,.65 if n in ['Steel','Graphite'] else .3),(unreal.MaterialProperty.MP_ROUGHNESS,.32)]:
   v=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant,-300,150+int(value*100));v.set_editor_property('r',value);unreal.MaterialEditingLibrary.connect_material_property(v,'',prop)
 unreal.MaterialEditingLibrary.recompile_material(m);save(m);mats['Dock_'+n]=m
meshes={}
for f in os.listdir(SRC):
 if not f.endswith('.fbx'):continue
 name=f[:-4];t=unreal.AssetImportTask();t.filename=SRC+'/'+f;t.destination_path=ROOT+'/Meshes';t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
 opts=unreal.FbxImportUI();opts.set_editor_property('import_mesh',True);opts.set_editor_property('import_materials',False);opts.set_editor_property('import_textures',False);opts.set_editor_property('import_as_skeletal',False);opts.set_editor_property('automated_import_should_detect_type',False);opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
 data=opts.get_editor_property('static_mesh_import_data');data.set_editor_property('combine_meshes',True);data.set_editor_property('generate_lightmap_u_vs',False);data.set_editor_property('auto_generate_collision',False);data.set_editor_property('convert_scene',True);data.set_editor_property('force_front_x_axis',False)
 t.options=opts;at.import_asset_tasks([t]);m=unreal.load_asset(ROOT+'/Meshes/'+name)
 if not m:raise RuntimeError('IMPORT FAILED '+name)
 for i,s in enumerate(m.get_editor_property('static_materials')):
  slot=str(s.material_slot_name);key=next((k for k in mats if slot==k or slot.startswith(k+'.')),None)
  if key:m.set_material(i,mats[key])
 if name.endswith(('_Body','_Screen','_Tray')):
  collision_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
  collision_editor.remove_collisions(m)
  collision_editor.add_simple_collisions(m,unreal.ScriptingCollisionShapeType.BOX)
 save(m);meshes[name]=m
 unreal.log('DOCK_MESH '+name+' '+str(m.get_bounds()))
defs=[]
for kind,label,service,x in [('Material','物料处理',unreal.JTSShipDockService.MATERIAL_PROCESSING,-65),('Weapon','武器改造',unreal.JTSShipDockService.WEAPON_MODIFICATION,125)]:
 fac=unreal.BlueprintFactory();fac.set_editor_property('parent_class',unreal.JTSShipDockActor)
 bp=asset('BP_Dock_'+kind,unreal.Blueprint,fac)
 cdo=unreal.get_default_object(bp.generated_class());cdo.set_editor_property('display_name',label);cdo.set_editor_property('service',service);cdo.set_editor_property('strut_spacing',53 if kind=='Material' else 62)
 for p in ['Mount','Body','Screen','Tray']:
  comp=cdo.get_editor_property(p.lower());comp.set_static_mesh(meshes['SM_Dock_'+kind+'_'+p]);comp.set_relative_scale3d(unreal.Vector(-1,-1,1))
 for leaf in ['Upper','Lower']:
  c=cdo.get_editor_property(leaf.lower()+'_bay_door');c.set_static_mesh(meshes['SM_Dock_'+kind+'_'+leaf+'Door']);c.set_relative_scale3d(unreal.Vector(-1,-1,1))
 cdo.set_editor_property('deployment_duration',3.2);cdo.set_editor_property('retraction_duration',2.8)
 for p in ['left_strut','right_strut']:cdo.get_editor_property(p).set_static_mesh(meshes['SM_Dock_Strut'])
 cdo.screen_anchor.set_relative_location(unreal.Vector(0,9,31 if kind=='Material' else 29.5),False,False)
 save(bp)
 df=unreal.DataAssetFactory();df.set_editor_property('data_asset_class',unreal.JTSShipDockDefinition)
 d=asset('DA_Dock_'+kind,unreal.JTSShipDockDefinition,df)
 d.set_editor_property('module_id',kind);d.set_editor_property('actor_class',bp.generated_class());d.set_editor_property('mount_transform',unreal.Transform(location=unreal.Vector(x,451,-5),rotation=unreal.Rotator(pitch=0,yaw=0,roll=0)))
 save(d);defs.append(d)
fac=unreal.BlueprintFactory();fac.set_editor_property('parent_class',unreal.JTSShipDockActor)
terminal_bp=asset('BP_Ship_CentralTerminal',unreal.Blueprint,fac)
tcdo=unreal.get_default_object(terminal_bp.generated_class())
tcdo.set_editor_property('display_name','飞船终端');tcdo.set_editor_property('service',unreal.JTSShipDockService.SHIP_TERMINAL);tcdo.set_editor_property('deployable',False)
tcdo.screen.set_static_mesh(meshes['SM_Ship_CentralTerminal']);tcdo.screen.set_relative_scale3d(unreal.Vector(-1,-1,1))
tcdo.screen_hinge.set_relative_location(unreal.Vector(0,0,0),False,False)
tcdo.screen_anchor.set_relative_location(unreal.Vector(0,10,0),False,False)
tcdo.screen_target.set_box_extent(unreal.Vector(61,5,42),False)
save(terminal_bp)
df=unreal.DataAssetFactory();df.set_editor_property('data_asset_class',unreal.JTSShipDockDefinition)
terminal_def=asset('DA_Ship_CentralTerminal',unreal.JTSShipDockDefinition,df)
terminal_def.set_editor_property('module_id','ShipTerminal');terminal_def.set_editor_property('actor_class',terminal_bp.generated_class())
terminal_def.set_editor_property('mount_transform',unreal.Transform(location=unreal.Vector(273,0,5),rotation=unreal.Rotator(pitch=0,yaw=-90,roll=0)))
save(terminal_def)
for path in ['/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1','/Game/Space/Blueprints/Ships/BP_Spacecraft']:
 bp=unreal.load_asset(path);cdo=unreal.get_default_object(bp.generated_class());comp=cdo.get_editor_property('dock_component');comp.set_editor_property('terminal_definition',terminal_def);comp.set_editor_property('catalog',defs);comp.set_editor_property('initial_installed_ids',['Material','Weapon']);save(bp)
open('D:/projects/space/Saved/dock_assets_done.txt','w').write('OK')
unreal.log('SHIP_DOCK_ASSETS COMPLETE')
