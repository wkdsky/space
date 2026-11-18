"""Rigid armour/industrial rigs and authored combat clips for both Mars bosses."""
import bpy,bmesh,math,ast,json
from pathlib import Path
from mathutils import Vector
src=Path('D:/projects/space/Tools/Art/build_mars_refinement.py').read_text(encoding='utf8')
# Reuse dimensioned modelling helpers without rerunning environment generation.
tree=ast.parse(src);functions=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'add','box','uv','cyl','beam','ring','txt','mesh','bolt'}]
exec(compile(ast.Module(body=functions,type_ignores=[]),'<mars-model-helpers>','exec'))
OUT=Path('D:/projects/space/SourceArt/MarsBosses/Refined');OUT.mkdir(parents=True,exist_ok=True)
palette=json.loads(Path('D:/projects/space/SourceArt/MarsStation/Refined/manifest.json').read_text())['palette']
for old in list(bpy.data.objects):
 if old.name.startswith(('SK_MP_Tuantuan','SK_MP_Fabricator')) or (old.type=='ARMATURE' and old.data.name.startswith(('Tuantuan_Skeleton','Fabricator_Skeleton'))):bpy.data.objects.remove(old,do_unlink=True)
for old in list(bpy.data.actions):
 if old.name.startswith(('A_MP_Tuantuan_','A_MP_Fabricator_')):bpy.data.actions.remove(old)
MATS={n:bpy.data.materials.get('MP_'+n) for n in palette};parts=[];bones=[];bone='Root';records={}
original_add=add
def add(o,mat):
 original_add(o,mat);group=o.vertex_groups.new(name=bone);group.add(list(range(len(o.data.vertices))),1,'REPLACE');return o
def joint(name,head,tail,parent='Root'):
 global bone
 bones.append((name,head,tail,parent));bone=name
def compact(o):
 names=list(palette);old=[m.name.removeprefix('MP_').split('.')[0] for m in o.data.materials]
 for layer in list(o.data.uv_layers):o.data.uv_layers.remove(layer)
 uvdata=o.data.uv_layers.new(name='PaletteUV');slots=[]
 for p in o.data.polygons:
  n=old[p.material_index];k='MP_Luminous' if n in ('Glow','Amber','Red') else 'MP_Palette'
  if k not in slots:slots.append(k)
  p.material_index=slots.index(k)
  for i in p.loop_indices:uvdata.data[i].uv=((names.index(n)+.5)/len(names),.5)
 o.data.materials.clear()
 for k in slots:o.data.materials.append(bpy.data.materials[k])
def buildrig(name,style):
 global parts,bones,bone
 bpy.ops.object.select_all(action='DESELECT')
 for o in parts:o.select_set(True)
 bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();model=bpy.context.object;model.name='SK_MP_'+name
 bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR');bpy.ops.object.transform_apply(location=False,rotation=True,scale=True);compact(model)
 bpy.ops.object.select_all(action='DESELECT');data=bpy.data.armatures.new(name+'_Skeleton');rig=bpy.data.objects.new('Armature',data);bpy.context.collection.objects.link(rig);rig.select_set(True);bpy.context.view_layer.objects.active=rig
 bpy.ops.object.mode_set(mode='EDIT')
 root=data.edit_bones.new('Root');root.head=(0,0,0);root.tail=(0,0,.4)
 for n,h,t,parent in bones:
  b=data.edit_bones.new(n);b.head=h;b.tail=t;b.parent=data.edit_bones[parent]
 bpy.ops.object.mode_set(mode='OBJECT');mod=model.modifiers.new('Rigid armour rig','ARMATURE');mod.object=rig;model.parent=rig
 # Export metre metadata once. Scaling both parent rig and child mesh compounds units.
 bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
 def export(file,anim):
  bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);model.select_set(True);bpy.context.view_layer.objects.active=rig
  bpy.ops.export_scene.fbx(filepath=str(OUT/(file+'.fbx')),use_selection=True,object_types={'MESH','ARMATURE'},bake_anim=anim,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,add_leaf_bones=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',use_armature_deform_only=True)
 export(model.name,False)
 if name=='Tuantuan':
  full=model
  for variant,removed in [('ShedRear',range(3)),('Soft',range(7))]:
   model=full.copy();model.data=full.data.copy();bpy.context.collection.objects.link(model);model.name='SK_MP_Tuantuan_'+variant
   remove_indices={g.index for g in model.vertex_groups if g.name in {'Shell'+str(i) for i in removed}}
   indices={v.index for v in model.data.vertices if any(g.group in remove_indices for g in v.groups)}
   bm=bmesh.new();bm.from_mesh(model.data);bm.verts.ensure_lookup_table();bmesh.ops.delete(bm,geom=[v for v in bm.verts if v.index in indices],context='VERTS');bm.to_mesh(model.data);bm.free()
   export(model.name,False);model.hide_set(True)
  model=full
 clips=['Emerge','Idle','Walk','Slam','Sweep','Charge','Stagger','Death','TurnLeft','TurnRight']
 for clip in clips:
  rig.animation_data_clear();action=bpy.data.actions.new('A_MP_'+name+'_'+clip);rig.animation_data_create();rig.animation_data.action=action
  frames=61;bpy.context.scene.frame_start=1;bpy.context.scene.frame_end=frames;bpy.context.scene.render.fps=30
  for i in range(frames):
   t=i/(frames-1);wave=math.sin(t*math.tau);pulse=math.sin(t*math.pi);impact=max(0,1-abs(t-.48)*9)
   for pb in rig.pose.bones:pb.rotation_mode='XYZ';pb.rotation_euler=(0,0,0);pb.location=(0,0,0)
   body=rig.pose.bones['Body']
   if style=='bug':
    body.location.z=(.045*wave if clip in ('Walk','Idle') else .1*pulse)
    if clip=='Charge':body.rotation_euler.x=t*math.tau*2
    if clip=='Stagger':body.rotation_euler.y=.12*wave;body.rotation_euler.z=.06*wave
    if clip=='Slam':body.rotation_euler.y=-.28*pulse+.4*impact
    if clip=='Sweep':body.rotation_euler.z=.40*math.sin(t*math.tau)
    if clip=='Death':body.rotation_euler.x=t*1.7;body.location.z=-t*.30
    if clip=='Emerge':body.location.z=-1.2*(1-t)
    for pb in rig.pose.bones:
     if pb.name.startswith('Leg'):pb.rotation_euler.x=.28*math.sin(t*math.tau*2+(0 if 'L' in pb.name else math.pi)+int(pb.name[-1])*.8)*(1 if clip in ('Walk','Charge') else .15)
     if pb.name.startswith('Shell'):pb.rotation_euler.y=.025*math.sin(t*math.tau+int(pb.name[-1])*.6)
   else:
    body.rotation_euler.z=.03*wave;body.location.z=(.1*wave if clip=='Walk' else .025*wave)
    if clip=='Death':body.location.z=-2*t;body.rotation_euler.y=t*.8
    if clip=='Stagger':body.rotation_euler.y=.10*wave
    if clip=='Emerge':body.location.z=-2.5*(1-t)
    for pb in rig.pose.bones:
     if pb.name.startswith('Arm'):
      side=int(pb.name[-1]);pb.rotation_euler.y=(-.70*pulse+1.2*impact if clip=='Slam' else .25*wave);pb.rotation_euler.z=(.8*wave if clip=='Sweep' else .1*wave)*(1 if side%2 else -1)
     if pb.name.startswith('Leg'):pb.rotation_euler.x=.16*math.sin(t*math.tau*2+int(pb.name[-1])*math.pi/2) if clip=='Walk' else 0
   if clip in ('TurnLeft','TurnRight'):body.rotation_euler.z=wave*.09*(1 if clip=='TurnLeft' else -1)
   for pb in rig.pose.bones:
    pb.keyframe_insert('rotation_euler',frame=i+1);pb.keyframe_insert('location',frame=i+1)
  export(action.name,True)
 rig.animation_data_clear()
 for pb in rig.pose.bones:pb.rotation_euler=(0,0,0);pb.location=(0,0,0)
 records[name]={'mesh':model.name,'bones':len(data.bones),'triangles':sum(len(p.vertices)-2 for p in model.data.polygons),'clips':clips}
 parts=[];bones=[];bone='Root';model.hide_set(True);rig.hide_set(True);return model,rig

# Tuantuan is purely organic. Seven overlapping, broad rigid shell plates,
# fourteen jointed legs, brood belly and antennae remain readable at 5.8 m.
joint('Body',(0,0,1.35),(0,0,2.2));uv((0,0,1.1),(2.45,1.65,.85),'Belly',20,10)
for i in range(7):
 x=-2.25+i*.72;joint('Shell'+str(i),(x,0,1.35),(x+.3,0,1.35),'Body')
 uv((x,0,1.65),(.55,1.72,1.24),'Shell',20,10)
 for layer in (1,2):uv((x-.10*layer,0,1.65+.15*layer),(.49-layer*.04,1.72-layer*.08,1.24+.03*layer),'Shell',20,10)
 for side in (-1,1):
  beam((x-.32,side*1.12,2.45),(x+.30,side*1.12,2.45),.035,'Amber')
  joint('Leg'+('L' if side<0 else 'R')+str(i),(x,side*1.0,.75),(x,side*1.7,.3),'Body')
  beam((x,side*1.2,.75),(x+.15,side*1.85,.32),.105,'Bone');beam((x+.15,side*1.85,.32),(x+.32,side*1.95,.10),.085,'Shell')
joint('Head',(2.35,0,1.05),(2.95,0,1.05),'Body');uv((2.55,0,1.05),(.55,.95,.65),'Shell',20,10)
for side in (-1,1):
 uv((2.96,side*.43,1.3),(.09,.12,.10),'Graphite')
 joint('Antenna'+str(side),(2.8,side*.4,1.4),(3.1,side*.7,1.9),'Head')
 beam((2.8,side*.4,1.4),(3.15,side*.75,1.8),.055,'Bone');beam((3.15,side*.75,1.8),(2.9,side*.95,2),.035,'Bone')
bug,bugrig=buildrig('Tuantuan','bug')

# Fabricator: a human crucible with printed limbs, exposed cable bundles,
# industrial manipulators and reused solar panels. No alien silhouette.
joint('Body',(0,0,4.5),(0,0,7.5));cyl((0,0,4.4),2.6,5.8,'Graphite',16);cyl((0,0,4.8),2.35,4.6,'Orange',16)
for z in (2,3.2,6.5,7.2):ring((0,0,z),2.55,.18,'Ceramic',24)
for i in range(10):
 a=i*math.tau/10;x,y=2.4*math.cos(a),2.4*math.sin(a)
 box((x,y,4.5),(.3,1.2,3.4),'Ceramic',.12,(0,0,a));beam((x,y,2),(x*.6,y*.6,8.4),.13,'Graphite')
cyl((0,0,7.5),1.8,.5,'Metal');cyl((0,0,7.8),1.4,.18,'Amber');cyl((0,0,8.3),1,1,'Graphite');uv((0,0,9.2),(1.4,1.1,.8),'Ceramic',16,8)
txt('ORIGINAL FABRICATOR',(2.6,-.1,5.5),.22,rotation=(math.pi/2,0,math.pi/2));txt('UNIT 000',(2.6,-.1,5.1),.28,rotation=(math.pi/2,0,math.pi/2))
for i in range(4):
 a=i*math.tau/4+.4;d=Vector((math.cos(a),math.sin(a),0));root=d*1.8+Vector((0,0,3));elbow=d*3.8+Vector((0,0,2));foot=d*4.5+Vector((0,0,.25))
 joint('Leg'+str(i),root,elbow,'Body');uv(root,(.45,.45,.45),'Orange');beam(root,elbow,.35,'Ceramic');uv(elbow,(.5,.5,.5),'Graphite');beam(elbow,foot,.28,'Ceramic');box(foot,(1.8,1.2,.5),'Graphite',.12,(0,0,a))
 for side in (-1,1):beam(root+Vector((side*.2,0,0)),foot+Vector((side*.2,0,.5)),.06,'Metal')
 a=i*math.tau/4;d=Vector((math.cos(a),math.sin(a),0));root=d*1.4+Vector((0,0,7.4));elbow=d*3.4+Vector((0,0,10.8));hand=d*5.5+Vector((0,0,8.6))
 joint('Arm'+str(i),root,elbow,'Body');uv(root,(.6,.6,.6),'Graphite');beam(root,elbow,.28,'Orange');uv(elbow,(.55,.55,.55),'Metal');beam(elbow,hand,.24,'Ceramic');box(hand,(1,.9,1.1),'Orange',.12)
 for side in (-1,1):beam(hand+Vector((0,side*.35,0)),hand+Vector((.5,side*.35,-1)),.10,'Graphite')
 for n in range(3):beam(root+Vector((n*.12,0,.1)),elbow+Vector((n*.12,0,.2)),.045,'Graphite')
 # Salvaged tracker panels act as asymmetrical scales on the fabricator.
 bone='Body';p=d*2.5+Vector((0,0,5.9));box(p,(1.6,.10,1.7),'Metal',.03,(.15,0,a));box(p+Vector((0,-.07,0)),(1.4,.02,1.5),'Cell',.01,(.15,0,a))
fab,fabrig=buildrig('Fabricator','machine')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Mars_Bosses.blend'))
(OUT/'manifest.json').write_text(json.dumps(records,indent=2))
print(json.dumps(records))
