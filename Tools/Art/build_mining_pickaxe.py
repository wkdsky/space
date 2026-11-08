"""Original expedition impulse pickaxe, centimetre coordinates, modular powered head."""
import bpy, math, json
from pathlib import Path
from mathutils import Vector

root=Path('D:/projects/space/SourceArt/MiningPickaxe');root.mkdir(parents=True,exist_ok=True)
collection=bpy.data.collections.get('MiningPickaxe')
if collection:
    for o in list(collection.objects):bpy.data.objects.remove(o,do_unlink=True)
else:
    collection=bpy.data.collections.new('MiningPickaxe');bpy.context.scene.collection.children.link(collection)
def material(name,color,metallic,roughness):
    m=bpy.data.materials.get(name) or bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    p=next((n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED'),None) or m.node_tree.nodes.new('ShaderNodeBsdfPrincipled');out=next((n for n in m.node_tree.nodes if n.type=='OUTPUT_MATERIAL'),None) or m.node_tree.nodes.new('ShaderNodeOutputMaterial');m.node_tree.links.new(p.outputs['BSDF'],out.inputs['Surface']);p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metallic;p.inputs['Roughness'].default_value=roughness
    return m
frame=material('Pickaxe_Frame',(.035,.065,.085),.72,.40)
shell=material('Pickaxe_Shell',(.52,.65,.70),.48,.34)
rubber=material('Pickaxe_Grip',(.018,.028,.036),.05,.83)
steel=material('Pickaxe_Steel',(.13,.21,.27),.8,.35)
edge=material('Pickaxe_Edge',(.55,.72,.78),.9,.24)
accent=material('Pickaxe_Accent',(.95,.23,.035),.35,.42)
energy=material('Pickaxe_Energy',(.015,.75,1),.1,.25)
p=next(n for n in energy.node_tree.nodes if n.type=='BSDF_PRINCIPLED');p.inputs['Emission Color'].default_value=(.015,.8,1,1);p.inputs['Emission Strength'].default_value=3
pieces=[]
def own(o,mat):
    for c in list(o.users_collection):c.objects.unlink(o)
    collection.objects.link(o);o.data.materials.append(mat);pieces.append(o)
    return o
def shaft(name,start,end,radius,mat):
    a,b=Vector(start),Vector(end);v=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=radius,depth=v.length,location=(a+b)*.5)
    o=bpy.context.object;o.name=name;o.rotation_mode='QUATERNION';o.rotation_quaternion=v.to_track_quat('Z','Y')
    own(o,mat)
def box(name,center,size,mat,bevel=.6):
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    o=bpy.context.object;o.name=name;o.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    own(o,mat)
    if bevel:
        mod=o.modifiers.new('MachinedCorners','BEVEL');mod.width=bevel;mod.segments=1
        bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=mod.name)
    return o

def plate(name,outline,y,thickness,mat):
    vertices=[(x,y+d,z) for d in (-thickness*.5,thickness*.5) for x,z in outline]
    n=len(outline);faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);collection.objects.link(o);o.data.materials.append(mat);pieces.append(o)

shaft('CompositeSpine',(-14,0,0),(105,0,0),2.4,frame)
shaft('PolymerGrip',(-12,0,0),(26,0,0),2.9,rubber)
for x in range(-10,27,4):shaft('GripRib',(x-.4,0,0),(x+.4,0,0),3.03,frame)
shaft('GripStop',(26,0,0),(29,0,0),3.6,shell)
shaft('Pommel',(-16,0,0),(-12,0,0),4.1,frame)
shaft('PommelStatus',(-16.3,0,0),(-15.9,0,0),2.9,energy)
box('LowerArmor',(43,0,0),(26,7,6),shell)
box('UpperSpine',(70,0,0),(26,5.5,5),frame)
for sign in (-1,1):
    box('EnergyConduit',(65,sign*2.95,0),(34,.55,1.0),energy,.15)
    box('ShaftOrangeMark',(37,sign*3.55,0),(4,.3,4),accent,.1)
    box('ShaftOrangeMark',(44,sign*3.55,0),(2,.3,4),accent,.1)
    plate('HeadShoulder',[(86,-4),(89,8),(99,13),(110,9),(115,0),(108,-8),(98,-6)],sign*4.5,2.2,shell)
    plate('ShoulderInset',[(91,0),(94,7),(101,9),(108,5),(108,0),(101,-2)],sign*5.7,.25,frame)
    plate('CoreWindow',[(96,0),(96,5),(101,7),(105,4),(104,0)],sign*5.95,.25,energy)
    plate('JawChannel',[(111,-10),(110,-17),(104,-26),(99,-30),(105,-19),(107,-12)],sign*3.05,.35,energy)
    for z in (3,6,9):box('BatteryVent',(81,sign*4.1,z),(7,.4,1.2),frame,.1)
box('ImpulseBattery',(82,0,5),(17,8,11),accent,1.8)
box('BatterySpine',(82,0,11),(12,5,1.8),frame,.4)
shaft('HeadCore',(101,-4.5,3),(101,4.5,3),6.3,steel)
shaft('HeadSocket',(92,0,0),(107,0,0),3.9,steel)
for sign in (-1,1):
    for x,z in ((90,4),(109,3),(101,-5)):
        shaft('TitaniumFastener',(x,sign*5.7,z),(x,sign*6.2,z),.85,edge)
def blade(name,rings,mat):
    vertices=[];faces=[]
    for x,z,wx,wy in rings:
        x+=14
        vertices.extend([(x-wx,-wy,z),(x+wx,-wy,z),(x+wx,wy,z),(x-wx,wy,z)])
    faces.append((3,2,1,0))
    for i in range(len(rings)-1):
        for j in range(4):a=i*4+j;b=i*4+(j+1)%4;faces.append((a,b,b+4,a+4))
    n=(len(rings)-1)*4;faces.append((n,n+1,n+2,n+3))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);collection.objects.link(o);o.data.materials.append(mat);pieces.append(o)
blade('ImpulseJaw',[(93,-4,6,3.4),(97,-12,5.1,3.2),(96,-20,4.1,2.8),(90,-29,2.6,1.8),(81,-36,.12,.12)],steel)
blade('TungstenContactTip',[(90,-29,2.62,1.82),(81,-36,.12,.12)],edge)
plate('RearArmorFin',[(93,10),(98,20),(105,18),(108,9)],0,6,shell)
box('RearCap',(100,0,18),(7,5.5,2),accent,.4)
bpy.ops.object.select_all(action='DESELECT')
for o in pieces:o.select_set(True)
bpy.context.view_layer.objects.active=pieces[0];bpy.ops.object.join()
o=bpy.context.object;o.name='SM_MiningPickaxe'
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
bpy.ops.wm.save_as_mainfile(filepath=str(root/'MiningPickaxe.blend'))
bpy.ops.export_scene.fbx(filepath=str(root/'SM_MiningPickaxe.fbx'),use_selection=True,apply_unit_scale=True,
    bake_space_transform=False,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,object_types={'MESH'},mesh_smooth_type='FACE')
(root/'profile.json').write_text(json.dumps(dict(mesh='SM_MiningPickaxe',grip=[0,0,0],tip=[95,0,-36],length_cm=135)),encoding='utf-8')
print(json.dumps(dict(mesh=o.name,vertices=len(o.data.vertices),fbx=str(root/'SM_MiningPickaxe.fbx'))))
