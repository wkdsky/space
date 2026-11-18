import unreal, os
ROOT='/Game/Space/Ships/Refit/AlphaArcade'
at=unreal.AssetToolsHelpers.get_asset_tools()
def save(o): unreal.EditorAssetLibrary.save_loaded_asset(o,False)
def asset(n,cls,f):return unreal.load_asset(ROOT+'/'+n) or at.create_asset(n,ROOT,cls,f)
def material(n,c,glow=False):
 m=asset('M_Arcade_'+n,unreal.Material,unreal.MaterialFactoryNew());unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
 x=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);x.constant=unreal.LinearColor(*c,1)
 unreal.MaterialEditingLibrary.connect_material_property(x,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if glow else unreal.MaterialProperty.MP_BASE_COLOR)
 if not glow:
  for prop,v in [(unreal.MaterialProperty.MP_METALLIC,.35),(unreal.MaterialProperty.MP_ROUGHNESS,.3)]:
   z=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant);z.r=v;unreal.MaterialEditingLibrary.connect_material_property(z,'',prop)
 unreal.MaterialEditingLibrary.recompile_material(m);save(m);return m
mats={n:material(n,c,n in ['Pink','Cyan']) for n,c in {'Ink':(.035,.008,.085),'Pink':(1.8,.045,.4),'Gold':(1,.59,.025),'Cyan':(.03,1.1,1.7)}.items()}
for n in ['SM_AlphaArcade_Body','SM_AlphaArcade_Prize']:
 t=unreal.AssetImportTask();t.filename='D:/projects/space/SourceArt/ShipRefit/AlphaArcade/'+n+'.fbx';t.destination_path=ROOT;t.destination_name=n;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.automated_import_should_detect_type=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 d=o.static_mesh_import_data;d.combine_meshes=True;d.generate_lightmap_u_vs=False;d.auto_generate_collision=False;d.import_rotation=unreal.Rotator(yaw=180);t.options=o;at.import_asset_tasks([t]);mesh=unreal.load_asset(ROOT+'/'+n)
 for i,s in enumerate(mesh.static_materials):
  name=str(s.material_slot_name)
  mat=next((v for k,v in mats.items() if name.startswith('Arcade_'+k)),None)
  if not mat and name.startswith('Refit_'):mat=unreal.load_asset('/Game/Space/Ships/Refit/Materials/M_Refit_'+name.removeprefix('Refit_'))
  if mat:mesh.set_material(i,mat)
 save(mesh)
# One tiny billboard, no point light and no permanent glow pillar.
m=asset('M_Pickup_StarGlint',unreal.Material,unreal.MaterialFactoryNew());m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
def node(cls):return unreal.MaterialEditingLibrary.create_material_expression(m,cls)
def wire(a,b,p):
 if not unreal.MaterialEditingLibrary.connect_material_expressions(a,'',b,p):raise RuntimeError('Cannot connect '+str(a.get_class().get_name())+' to '+str(b.get_class().get_name())+'.'+p)
def custom_input(name):
 c=unreal.CustomInput();c.set_editor_property('input_name',name);return c
uv=node(unreal.MaterialExpressionTextureCoordinate);tm=node(unreal.MaterialExpressionTime);pos=node(unreal.MaterialExpressionActorPositionWS)
star=node(unreal.MaterialExpressionCustom);star.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1);star.set_editor_property('inputs',[custom_input('UV'),custom_input('T'),custom_input('P')]);star.set_editor_property('code','''float cycle=frac(T/2.7+dot(P,float3(.00173,.00217,.00131)));
float pulse=smoothstep(0,.025,cycle)*(1-smoothstep(.045,.13,cycle));
float2 v=(UV-.5)*2; float size=.62+.34*sin(saturate(cycle/.13)*3.14159);v/=size;
float diamond=pow(abs(v.x),.48)+pow(abs(v.y),.48);
float rays=saturate((1-diamond)*18);float halo=exp(-dot(v,v)*55)*.35;
return (rays+halo)*pulse;''');wire(uv,star,'UV');wire(tm,star,'T');wire(pos,star,'P')
col=node(unreal.MaterialExpressionConstant3Vector);col.constant=unreal.LinearColor(7,6.3,4.8,1);mul=node(unreal.MaterialExpressionMultiply);wire(star,mul,'A');wire(col,mul,'B');unreal.MaterialEditingLibrary.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);unreal.MaterialEditingLibrary.connect_material_property(star,'',unreal.MaterialProperty.MP_OPACITY)
cam=node(unreal.MaterialExpressionCameraPositionWS);sub=node(unreal.MaterialExpressionSubtract);wire(cam,sub,'A');wire(pos,sub,'B');norm=node(unreal.MaterialExpressionNormalize);wire(sub,norm,'VectorInput');offset=node(unreal.MaterialExpressionMultiply);wire(norm,offset,'A');offset.set_editor_property('const_b',12);unreal.MaterialEditingLibrary.connect_material_property(offset,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
unreal.MaterialEditingLibrary.recompile_material(m);save(m)
bp=unreal.load_asset('/Game/Space/Ships/Refit/BP_Dock_StellarExchange');c=unreal.get_default_object(bp.generated_class());c.body.set_static_mesh(unreal.load_asset(ROOT+'/SM_AlphaArcade_Body'));c.body.set_editor_property('relative_scale3d',unreal.Vector(-1,1,1));c.body.set_editor_property('override_materials',[])
c.set_editor_property('display_name',unreal.Text('阿尔法快乐拉杆（活动专用）'));c.prize_eject_anchor.set_editor_property('relative_location',unreal.Vector(-17,66,-27));c.prize_eject_anchor.set_editor_property('relative_rotation',unreal.Rotator(yaw=90));save(bp)
# Data assets use the project's existing definition registry, all appearance is editable here.
for enum,n in [(unreal.JTSItemId.STELLAR_TEXT,'StellarText'),(unreal.JTSItemId.STELLAR_WEAPON,'StellarWeapon')]:
 path='/Game/Space/Data/Items/DA_Item_'+n;obj=unreal.load_asset(path)
 if not obj:
  f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.JTSItemDefinition);obj=at.create_asset('DA_Item_'+n,'/Game/Space/Data/Items',unreal.JTSItemDefinition,f)
 obj.set_editor_property('item_id',enum);obj.set_editor_property('display_name',unreal.Text('星际核心／配件' if n=='StellarText' else '星际武器'));obj.set_editor_property('world_pickup_mesh',unreal.load_asset(ROOT+'/SM_AlphaArcade_Prize'));obj.set_editor_property('world_pickup_width',24);obj.set_editor_property('pickup_glint_material',m);obj.set_editor_property('pickup_glint_size',20);save(obj)
for path in ['/Game/Space/Ships/TwinforkR1/BP_Twinfork_R1','/Game/Space/Blueprints/Ships/BP_Spacecraft']:
 bp=unreal.load_asset(path);c=unreal.get_default_object(bp.generated_class());c.scanner.set_editor_property('scan_coverage_scale',1.35);save(bp)
unreal.log('ALPHA_ARCADE_ASSETS_COMPLETE')
