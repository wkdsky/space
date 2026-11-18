"""Lightweight local-space membrane and segmented body motion, authored as assets."""
import unreal as u
ROOT='/Game/Space/Environment/MarsSystem/Chapter/Refined'
def refine():
 at=u.AssetToolsHelpers.get_asset_tools();mel=u.MaterialEditingLibrary
 def make(name,code):
  m=u.load_asset(ROOT+'/'+name)
  if m and u.EditorAssetLibrary.get_metadata_tag(m,'MarsMotionRevision')=='1':return m
  if not m:m=at.create_asset(name,ROOT,u.Material,u.MaterialFactoryNew())
  else:mel.delete_all_material_expressions(m)
  m.set_editor_property('two_sided',True);m.set_editor_property('used_with_instanced_static_meshes',True)
  def node(cls,**props):
   n=mel.create_material_expression(m,cls)
   for k,v in props.items():n.set_editor_property(k,v)
   return n
  def wire(a,b,pin,out=''):
   inputs=mel.get_material_expression_input_names(b)
   if pin not in inputs and len(inputs)==1:pin=inputs[0]
   assert mel.connect_material_expressions(a,out,b,pin),(str(type(b)),pin,list(inputs))
  tex=node(u.MaterialExpressionTextureSample,texture=u.load_asset(ROOT+'/T_MP_Palette'))
  mel.connect_material_property(tex,'RGB',u.MaterialProperty.MP_BASE_COLOR)
  r=node(u.MaterialExpressionConstant,r=.85);mel.connect_material_property(r,'',u.MaterialProperty.MP_ROUGHNESS)
  p=node(u.MaterialExpressionPreSkinnedPosition);t=node(u.MaterialExpressionTime)
  inputs=[]
  for key in ('P','T'):
   inp=u.CustomInput();inp.set_editor_property('input_name',key);inputs.append(inp)
  deform=node(u.MaterialExpressionCustom,code=code,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,inputs=inputs,description='Local membrane/body motion')
  wire(p,deform,'P');wire(t,deform,'T')
  transform=node(u.MaterialExpressionTransformPosition,transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL,transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
  wire(deform,transform,'Position')
  world=node(u.MaterialExpressionWorldPosition)
  difference=node(u.MaterialExpressionSubtract);wire(transform,difference,'A');wire(world,difference,'B')
  mel.connect_material_property(difference,'',u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
  mel.recompile_material(m);u.EditorAssetLibrary.set_metadata_tag(m,'MarsMotionRevision','1');u.EditorAssetLibrary.save_loaded_asset(m,False);return m
 configs=[('Dust_Moth','M_MP_MothMotion','float w=saturate((abs(P.y)-3)/20); return P+float3(0,0,sin(T*36+P.x*.05)*w*12);'),
 ('Light_Sail_Ray','M_MP_SailMotion','float w=saturate((abs(P.y)-60)/620); return P+float3(0,0,sin(T*2.8+P.x*.006)*w*95);'),
 ('Burrow_Worm','M_MP_WormMotion','float w=saturate(P.z/300);return P+float3(sin(T*4-P.z*.018)*w*20,cos(T*3-P.z*.02)*w*10,0);')]
 for key,name,code in configs:
  m=make(name,code);mesh=u.load_asset(ROOT+'/SM_MP_'+key)
  for i,slot in enumerate(mesh.get_editor_property('static_materials')):
   if str(slot.material_slot_name)=='MP_Palette':mesh.set_material(i,m)
  u.EditorAssetLibrary.save_loaded_asset(mesh,False)
 return [name for _,name,_ in configs]
