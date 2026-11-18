"""Author the wide TwinFork hull and separate replaceable equipment in centimetres."""
import bpy, math, os
from mathutils import Vector

OUT='D:/projects/space/SourceArt/ShipRefit/AlphaArcade'
os.makedirs(OUT,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
M={};parts={};group=[]
COLORS={'Ceramic':(.43,.48,.47),'Frame':(.022,.036,.046),'Metal':(.16,.22,.25),'Panel':(.085,.13,.15),'Teal':(.025,.29,.28),'Orange':(.9,.24,.045),'Glass':(.018,.075,.095),'Glow':(.035,.85,.68),'Amber':(1,.38,.045),'White':(.68,.85,.85),'Screen':(.003,.009,.018)}
for n,c in COLORS.items():
 m=bpy.data.materials.new('Refit_'+n);m.diffuse_color=(*c,1);m.use_nodes=True
 p=next(x for x in m.node_tree.nodes if x.type=='BSDF_PRINCIPLED')
 p.inputs['Base Color'].default_value=(*c,1);p.inputs['Metallic'].default_value=.65 if n in ['Metal','Frame','Panel'] else .3;p.inputs['Roughness'].default_value=.31
 if n in ['Glow','Amber','White']:p.inputs['Emission Color'].default_value=(*c,1);p.inputs['Emission Strength'].default_value=2 if n!='White' else .6
 M[n]=m
def finish(o,n,ma,bev=0):
 o.name=n;o.data.materials.append(M[ma]);group.append(o)
 if bev:
  b=o.modifiers.new('Edge radii','BEVEL');b.width=bev;b.segments=3
  bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=b.name)
  w=o.modifiers.new('Weighted normals','WEIGHTED_NORMAL')
  try:bpy.ops.object.modifier_apply(modifier=w.name)
  except:pass
 return o
def box(n,loc,dim,ma='Ceramic',bevel=1):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.dimensions=dim;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);return finish(o,n,ma,bevel)
def cyl(n,loc,r,d,ma='Metal',axis='Z',verts=32):
 bpy.ops.mesh.primitive_cylinder_add(vertices=verts,radius=r,depth=d,location=loc);o=bpy.context.object
 if axis=='X':o.rotation_euler[1]=math.pi/2
 if axis=='Y':o.rotation_euler[0]=math.pi/2
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);return finish(o,n,ma,.5)
def line(n,a,b,w,ma='Metal'):
 v=Vector(b)-Vector(a);o=box(n,(Vector(a)+Vector(b))/2,(w,w,v.length),ma,.4);o.rotation_euler=v.to_track_quat('Z','Y').to_euler();return o
def text(n,s,loc,size,ma='White',rot=(0,0,0)):
 bpy.ops.object.text_add(location=loc,rotation=rot);o=bpy.context.object;o.data.body=s;o.data.size=size;o.data.align_x='CENTER';o.data.align_y='CENTER';o.data.extrude=.02
 bpy.ops.object.convert(target='MESH');return finish(o,n,ma)
def loft(n,rows,cy=0,ma='Ceramic',bevel=1):
 vs=[]
 for x,w,bot,top in rows:
  b=min(16,(top-bot)/5,w/4)
  vs.extend([(x,cy-w+b,bot),(x,cy+w-b,bot),(x,cy+w,bot+b),(x,cy+w,top-b),(x,cy+w-b,top),(x,cy-w+b,top),(x,cy-w,top-b),(x,cy-w,bot+b)])
 fs=[tuple(reversed(range(8))),tuple(range(len(vs)-8,len(vs)))]
 for i in range(len(rows)-1):
  for j in range(8):fs.append((i*8+j,i*8+(j+1)%8,(i+1)*8+(j+1)%8,(i+1)*8+j))
 me=bpy.data.meshes.new(n);me.from_pydata(vs,[],fs);me.update();o=bpy.data.objects.new(n,me);bpy.context.collection.objects.link(o);return finish(o,n,ma,bevel)
def savepart(n):
 global group
 bpy.ops.object.select_all(action='DESELECT')
 for o in group:o.select_set(True)
 bpy.context.view_layer.objects.active=group[0];bpy.ops.object.join();o=bpy.context.object;o.name=n
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
 bpy.ops.export_scene.fbx(filepath=OUT+'/'+n+'.fbx',use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False,add_leaf_bones=False)
 parts[n]=o;group=[];o.hide_set(True);o.hide_render=True;return o


# Independent arcade cabinet; original dock anchors and lever hinge remain compatible.
for n,c in {'Ink':(.035,.008,.085),'Pink':(.95,.025,.22),'Gold':(1,.59,.025),'Cyan':(.02,.7,.95)}.items():
 m=bpy.data.materials.new('Arcade_'+n);m.diffuse_color=(*c,1);m.use_nodes=True
 bs=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');bs.inputs['Base Color'].default_value=(*c,1);bs.inputs['Metallic'].default_value=.35;bs.inputs['Roughness'].default_value=.3
 if n in ['Pink','Cyan']:bs.inputs['Emission Color'].default_value=(*c,1);bs.inputs['Emission Strength'].default_value=2
 M[n]=m
box('Cabinet back',(0,-7,0),(174,22,88),'Ink',5)
box('Arcade body',(-12,12,-1),(151,34,87),'Ink',6)
box('Gold brow',(-17,22,30),(140,38,13),'Gold',3)
box('Pink bezel',(-17,27,5),(130,14,48),'Pink',3)
box('Inner black bevel',(-17,33,5),(121,3,39),'Ink',2)
box('Glass',(-17,34,5),(115,1,32),'Screen',1)
for x in [-83,49]:
 box('Vertical light rail',(x,33,2),(3,3,74),'Cyan',.8)
for x in range(-76,47,13):
 cyl('Marquee bulb',(x,36,32),2,2,'Pink' if x%2 else 'Cyan','Y',12)
box('Control deck',(-17,38,-28),(139,53,14),'Gold',3)
box('Prize chute',(-17,62,-27),(67,4,9),'Ink',1)
for x,ma in [(30,'Pink'),(43,'Cyan')]:cyl('Arcade button',(x,44,-19),5,4,ma,'Z',20)
box('Lever side housing',(67,20,-6),(28,40,90),'Pink',4)
cyl('Lever bearing',(67,42,-19),14,16,'Gold','Y')
savepart('SM_AlphaArcade_Body')
# Portable luminous chip rewards, their glints remain a separate effect.
box('Reward chassis',(0,0,0),(20,14,5),'Ink',1.6)
box('Crystal wafer',(0,0,3),(14,9,3),'Cyan',1)
for x in [-7,-3,1,5]:box('Golden contacts',(x,-7,0),(2,5,1),'Gold',.2)
savepart('SM_AlphaArcade_Prize')
for o in parts.values():o.hide_set(False);o.hide_render=False
bpy.ops.wm.save_as_mainfile(filepath=OUT+'/AlphaArcade.blend')
print('ALPHA_ARCADE_ART_COMPLETE')
