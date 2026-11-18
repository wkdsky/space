"""Blender MCP recipe: serviceable outdoor combiner, hand module and steel supply rack."""
import bpy,math,json
from pathlib import Path
from mathutils import Vector,Matrix

OUT=Path('D:/projects/space/SourceArt/MarsStation/Power');OUT.mkdir(parents=True,exist_ok=True)
COL='MarsPowerService'
old=bpy.data.collections.get(COL)
if old:
    for o in list(old.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.data.collections.remove(old)
collection=bpy.data.collections.new(COL);bpy.context.scene.collection.children.link(collection)
palette={'White':(.72,.76,.77,1),'Orange':(.9,.19,.026,1),'Metal':(.25,.3,.32,1),
         'Dark':(.012,.016,.019,1),'Concrete':(.26,.26,.25,1),'Gold':(.62,.37,.055,1),
         'Green':(.014,.8,.065,1),'Yellow':(.95,.57,.018,1),'Black':(.003,.004,.005,1)}
palette['CoreLabel']=(1,1,1,1)
mats={}
for k,c in palette.items():
    m=bpy.data.materials.get('Power_'+k) or bpy.data.materials.new('Power_'+k);m.use_nodes=True
    shader=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value=c;shader.inputs['Roughness'].default_value=.64
    if k in ('Metal','Gold'):shader.inputs['Metallic'].default_value=.7
    m.diffuse_color=c;mats[k]=m
label_image=bpy.data.images.load(str(OUT/'T_MP_CoreLabel.png'),check_existing=True)
label_node=mats['CoreLabel'].node_tree.nodes.new('ShaderNodeTexImage');label_node.image=label_image
mats['CoreLabel'].node_tree.links.new(label_node.outputs['Color'],next(n for n in mats['CoreLabel'].node_tree.nodes if n.type=='BSDF_PRINCIPLED').inputs['Base Color'])
manifest={}

class Part:
    def __init__(self,name,bevel=True):self.name='SM_MP_'+name;self.objects=[];self.bevel=bevel
    def keep(self,o,mat):
        for c in list(o.users_collection):c.objects.unlink(o)
        for i,uv in enumerate(o.data.uv_layers):uv.name='UVMap' if i==0 else 'UVMap'+str(i)
        collection.objects.link(o);o.data.materials.append(mats[mat]);self.objects.append(o);return o
    def box(self,loc,size,mat='Metal',rot=None,bevel=None):
        bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.scale=size
        if rot:o.rotation_euler=rot
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        # Small chamfers catch the light without softening the manufactured silhouette.
        if self.bevel if bevel is None else bevel:
            modifier=o.modifiers.new('Folded edges','BEVEL');modifier.width=min(.008,min(size)*.12);modifier.segments=1
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        return self.keep(o,mat)
    def rod(self,a,b,r=.02,mat='Metal',segments=12):
        a,b=Vector(a),Vector(b);d=b-a;bpy.ops.mesh.primitive_cylinder_add(vertices=segments,radius=r,depth=d.length,location=(a+b)/2)
        o=bpy.context.object;o.rotation_euler=d.to_track_quat('Z','Y').to_euler();return self.keep(o,mat)
    def text(self,text,loc,size,mat='Dark',rot=(math.pi/2,0,math.pi/2),font=None):
        data=bpy.data.curves.new('Equipment marking','FONT');data.body=text;data.size=size;data.extrude=.0004
        data.align_x='CENTER';data.align_y='CENTER'
        if font:data.font=font
        o=bpy.data.objects.new('Painted label',data);collection.objects.link(o);o.location=loc;o.rotation_euler=rot
        bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target='MESH')
        o.data.materials.append(mats[mat]);self.objects.append(o)
    def poly(self,verts,mat):
        d=bpy.data.meshes.new('Warning');d.from_pydata(verts,[],[list(range(len(verts)))]);d.update()
        o=bpy.data.objects.new('Warning',d);collection.objects.link(o);d.materials.append(mats[mat]);self.objects.append(o);return o
    def export(self,**spec):
        bpy.ops.object.select_all(action='DESELECT')
        for o in self.objects:o.select_set(True)
        bpy.context.view_layer.objects.active=self.objects[0];bpy.ops.object.join();o=bpy.context.object;o.name=self.name
        bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        bpy.ops.export_scene.fbx(filepath=str(OUT/(self.name+'.fbx')),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,bake_anim=False)
        manifest[self.name]=dict(triangles=sum(len(p.vertices)-2 for p in o.data.polygons),**spec)
        return o

fontpath=Path('C:/Windows/Fonts/msyh.ttc');font=bpy.data.fonts.load(str(fontpath)) if fontpath.exists() else None
p=Part('Power_CombinerBase');p.box((0,0,.075),(1.6,2,.15),'Concrete');p.export(dimensions_m=[1.6,2,.15])
p=Part('Power_CombinerBody')
for y in (-.585,.585):p.box((0,y,.95),(.8,.03,1.6),'White')
p.box((-.385,0,.95),(.03,1.2,1.6),'White');p.box((0,0,.175),(.8,1.2,.05),'White');p.box((0,0,1.735),(.8,1.2,.03),'White')
p.box((.04,0,1.81),(.98,1.38,.07),'White',rot=(0,.04,0))
p.box((.397,0,.95),(.022,1.14,1.48),'Metal')
# A visible recessed backplane and vertical guide rails inside the opening.
p.box((.415,.30,.95),(.018,.52,.82),'Black')
for y in (.53,.07):p.box((.435,y,.95),(.08,.025,.77),'Metal')
p.box((.44,.30,.535),(.16,.49,.04),'Metal')
for y,z,w,h in [(.58,1.22,.045,.17),(.045,.98,.06,.24),(.31,1.385,.32,.055),(.42,.51,.21,.065)]:
    p.box((.413,y,z),(.012,w,h),'Dark')
for y in (-.603,.603):
    for z in (.37,.43,.49,.55):p.box((-.11,y,z),(.36,.012,.018),'Dark')
for y in (-.48,.48):p.rod((.42,y,.21),(.45,y,.21),.023,'Metal')
p.rod((0,0,1.78),(0,0,1.91),.055,'Dark');p.export(dimensions_m=[.8,1.2,1.6],front_axis='+X')
p=Part('Power_CombinerDoor')
# Origin lies at the left hinge. Blueprint rotates this mesh around local Z.
p.box((.015,.57,.8),(.045,1.14,1.5),'Orange')
p.box((.047,.98,.83),(.04,.055,.18),'Dark')
p.rod((.08,.98,.77),(.08,.98,.9),.012,'Metal')
for z in (.25,1.27):p.rod((0,0,z),(0,0,z+.12),.024,'Metal')
p.box((.042,.57,1.45),(.009,1.02,.075),'Yellow')
for y in (.10,.28,.46,.64,.82,1.0):
    p.poly([(.048,y,1.4125),(.048,y+.08,1.4125),(.048,y+.035,1.4875),(.048,y-.045,1.4875)],'Black')
p.poly([(.052,.38,1.11),(.052,.76,1.11),(.052,.57,1.4)],'Yellow')
p.poly([(.055,.58,1.35),(.055,.48,1.23),(.055,.57,1.23),(.055,.54,1.14),(.055,.67,1.28),(.055,.59,1.28)],'Black')
p.export(hinge_local_m=[.44,.57,.15],closed_yaw=0,fault_yaw=55)
p=Part('Power_StatusLens');p.rod((0,0,0),(0,0,.095),.065,'White',20);p.export()
for letter in 'ABC':
    p=Part('Power_Sector'+letter)
    p.text(letter,(-.05,-.607,1.1),.52,'Orange',rot=(math.pi/2,0,0))
    p.text(letter,(-.05,.607,1.1),.52,'Orange',rot=(math.pi/2,0,math.pi))
    p.export()
for burnt in (False,True):
    p=Part('Power_CoreBurnt' if burnt else 'Power_CoreModule',bevel=False);body='Dark' if burnt else 'Metal'
    p.box((0,0,.12),(.12,.45,.24),body,bevel=True)
    for y in (-.21,.21):p.box((.062,y,.12),(.006,.025,.22),'Dark',bevel=True)
    for y in (-.13,.13):p.rod((0,y,.23),(0,y,.283),.016,'Dark' if burnt else 'Orange',segments=8)
    p.rod((0,-.13,.283),(0,.13,.283),.017,'Dark' if burnt else 'Orange',segments=8)
    for y in (-.15,-.1,-.05,0,.05,.1,.15):p.box((-.075,y,.055),(.03,.025,.045),'Dark' if burnt else 'Gold')
    p.box((.064,0,.16),(.003,.32,.075),'Dark' if burnt else 'White',bevel=True)
    label=p.poly([(.067,-.16,.1225),(.067,.16,.1225),(.067,.16,.1975),(.067,-.16,.1975)],'Dark' if burnt else 'CoreLabel')
    uv=label.data.uv_layers.new(name='UVMap')
    for loop,coord in zip(uv.data,[(0,0),(1,0),(1,1),(0,1)]):loop.uv=coord
    p.box((.065,0,.04),(.005,.36,.009),'Dark' if burnt else 'Green')
    p.export(dimensions_m=[.15,.45,.30],grip_m=[0,0,.283],pickable=not burnt)
p=Part('Power_SupplyRack')
for x in (-.28,.28):
    for y in (-.78,.78):
        p.box((x,y,.9),(.04,.045,1.8),'Metal');p.box((x,y,.02),(.1,.1,.04),'Dark')
for z in (.12,.65,1.18,1.73):
    p.box((0,0,z),(.6,1.6,.035),'Metal');p.box((.29,0,z-.035),(.025,1.6,.08),'Orange')
for y in (-.8,.8):p.rod((-.29,y,.2),(-.29,-y,1.65),.012,'Metal')
for i in range(4):
    y=-.6+i*.4;p.box((.307,y,.595),(.008,.32,.08),'White');p.text(str(i+1)+' / CORE',(.313,y,.595),.036,'Dark')
# Tool boxes with handles and two recognizable cable coils on other shelves.
for y,z in ((-.48,.17),(.43,.17),(-.43,1.23)):
    p.box((0,y,z+.14),(.39,.5,.28),'Dark');p.box((.204,y,z+.14),(.012,.4,.04),'Orange')
    p.rod((0,y-.09,z+.3),(0,y+.09,z+.3),.018,'Metal')
for y in (.3,.65):
    for k in range(3):
        bpy.ops.mesh.primitive_torus_add(major_segments=20,minor_segments=6,location=(0,y,1.31+k*.02),major_radius=.11,minor_radius=.015)
        p.keep(bpy.context.object,'Black')
p.export(dimensions_m=[.6,1.6,1.8],slot_centres_m=[[.1,-.6+i*.4,.66] for i in range(4)])
for name,size in [('ColumnJunction',(.2,.3,.4)),('WallJunction',(.3,.6,.8))]:
    p=Part('Power_'+name);p.box((0,0,size[2]/2),size,'White');p.box((size[0]/2+.006,0,size[2]/2),(.015,size[1]*.86,size[2]*.86),'Dark')
    p.poly([(size[0]/2+.016,-size[1]*.18,size[2]*.35),(size[0]/2+.016,size[1]*.18,size[2]*.35),(size[0]/2+.016,0,size[2]*.7)],'Yellow');p.export(dimensions_m=list(size))
p=Part('Power_CableMarker');p.rod((0,0,0),(0,0,.8),.025,'Orange');p.box((0,0,.72),(.09,.16,.14),'Orange');p.export(height_m=.8)
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MarsPowerService.blend'))
print(json.dumps(manifest,indent=2))
