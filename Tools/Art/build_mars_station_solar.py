"""M2.1 solar modules, metres/+Z up. Run in Blender MCP; never touches planet art."""
import bpy
import math
import json
from pathlib import Path
from mathutils import Vector, Matrix

OUT=Path('D:/projects/space/SourceArt/MarsStation/Refined')
OUT.mkdir(parents=True,exist_ok=True)
COLLECTION='MarsStationSolar25'
old=bpy.data.collections.get(COLLECTION)
if old:
    for o in list(old.objects):bpy.data.objects.remove(o,do_unlink=True)
    bpy.data.collections.remove(old)
collection=bpy.data.collections.new(COLLECTION)
bpy.context.scene.collection.children.link(collection)
materials={}
for name,color in {'SolarDust':(.008,.022,.052,1),'Frame':(.76,.78,.72,1),
                   'Orange':(.9,.25,.045,1),'Metal':(.22,.28,.28,1),'Graphite':(.04,.055,.06,1),
                   'RedDust':(.095,.029,.016,1)}.items():
    m=bpy.data.materials.get('MP_'+name) or bpy.data.materials.new('MP_'+name)
    m.use_nodes=True
    shader=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    shader.inputs['Base Color'].default_value=color
    shader.inputs['Roughness'].default_value=.9
    m.diffuse_color=color
    materials[name]=m
manifest={}

class Module:
    def __init__(self,name):
        self.name='SM_MP_'+name;self.vertices=[];self.faces=[];self.slots=[];self.collision=[]
    def box(self,loc,size,mat='Metal',rotation=None,collision=False):
        rotation=rotation or Matrix.Identity(3)
        corners=[Vector((a*size[0]/2,b*size[1]/2,c*size[2]/2)) for a,b,c in
                 [(-1,-1,-1),(-1,-1,1),(-1,1,-1),(-1,1,1),(1,-1,-1),(1,-1,1),(1,1,-1),(1,1,1)]]
        corners=[rotation@v+Vector(loc) for v in corners]
        fs=[(0,2,6,4),(1,5,7,3),(0,4,5,1),(2,3,7,6),(0,1,3,2),(4,6,7,5)]
        start=len(self.vertices);self.vertices+=corners
        self.faces += [tuple(start+i for i in f) for f in fs]
        self.slots += [list(materials).index(mat)]*6
        if collision:self.collision.append((corners,fs))
    def beam(self,a,b,width=.1,mat='Metal',collision=False):
        a,b=Vector(a),Vector(b);direction=b-a
        self.box((a+b)/2,(width,width,direction.length),mat,
                 direction.to_track_quat('Z','Y').to_matrix(),collision)
    def faceplate(self,loc,size,mat,rotation):
        start=len(self.vertices)
        self.vertices += [rotation@Vector((x*size[0]/2,y*size[1]/2,0))+Vector(loc)
                          for x,y in ((-1,-1),(1,-1),(1,1),(-1,1))]
        self.faces.append(tuple(start+i for i in range(4)))
        self.slots.append(list(materials).index(mat))
    def export(self,**spec):
        d=bpy.data.meshes.new(self.name);d.from_pydata(self.vertices,[],self.faces);d.update()
        o=bpy.data.objects.new(self.name,d);collection.objects.link(o)
        for m in materials.values():d.materials.append(m)
        for p,i in zip(d.polygons,self.slots):p.material_index=i
        exported=[o]
        for i,(v,f) in enumerate(self.collision):
            mesh=bpy.data.meshes.new('UCX');mesh.from_pydata(v,[],f);mesh.update()
            c=bpy.data.objects.new('UCX_'+self.name+'_'+str(i).zfill(2),mesh)
            collection.objects.link(c);exported.append(c)
        bpy.ops.object.select_all(action='DESELECT')
        for a in exported:a.select_set(True)
        bpy.context.view_layer.objects.active=o
        bpy.ops.export_scene.fbx(filepath=str(OUT/(self.name+'.fbx')),use_selection=True,
            object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False)
        triangles=sum(len(f)-2 for f in self.faces)
        manifest[self.name]=dict(triangles=triangles,collision_hulls=len(self.collision),**spec)
        for c in exported[1:]:c.hide_set(True)
        return o

angle=math.radians(25)
rotation=Matrix.Rotation(-angle,3,'Y')
def panel(m,center,w,length,cols,rows):
    center=Vector(center)
    point=lambda x,y,z:center+rotation@Vector((x,y,z))
    m.box(center,(w,length,.12),'Graphite',rotation,True)
    for x in (-w/2,w/2):m.box(point(x,0,.09),(.06,length,.1),'Frame',rotation)
    for y in (-length/2,length/2):m.box(point(0,y,.09),(w,.06,.1),'Frame',rotation)
    for col in range(cols):
        for row in range(rows):
            m.faceplate(point(-w/2+(col+.5)*w/cols,-length/2+(row+.5)*length/rows,.09),
                  (w/cols-.035,length/rows-.035),'SolarDust',rotation)
            # Narrow reddish deposits leave the dark blue cell face exposed.
            if col==0 or row in (0,rows-1):
                m.faceplate(point(-w/2+(col+.5)*w/cols,-length/2+(row+.07)*length/rows,.092),
                      (w/cols-.07,.035),'RedDust',rotation)
            # Silver cell busbars, inset into each photovoltaic cell.
            for offset in (-.25,.25):
                m.faceplate(point(-w/2+(col+.5)*w/cols+offset,-length/2+(row+.5)*length/rows,.093),
                      (.006,length/rows-.07),'Frame',rotation)

m=Module('Solar_Tracker')
panel(m,(0,0,3.2),6,12,5,10)
m.beam((0,-6.2,3.05),(0,6.2,3.05),.22,'Orange')
for y in (-3.6,3.6):
    m.box((0,y,.1),(.75,.75,.2),'Graphite',collision=True)
    m.box((0,y,1.6),(.2,.2,3.0),'Metal',collision=True)
    m.beam((0,y,.3),(-1.5,y,2.45),.09)
    m.beam((0,y,.3),(1.5,y,3.6),.09)
m.box((0,-6.3,2.9),(.6,.5,.55),'Graphite',collision=True)
m.export(panel_area_m2=72,tilt_deg=25,axis_height_m=3.2,low_edge_m=3.2-3*math.sin(angle),high_edge_m=3.2+3*math.sin(angle))

m=Module('Solar_Canopy')
row_width=5*math.cos(angle)
for row in range(4):
    for col in range(2):
        panel(m,((row-1.5)*row_width,(col-.5)*7,4.5+2.5*math.sin(angle)),5,7,5,7)
for x in (-8.1,0,8.1):
    for y in (-5.5,5.5):
        m.box((x,y,.12),(.9,.9,.24),'Graphite',collision=True)
        m.box((x,y,2.28),(.3,.3,4.56),'Metal',collision=True)
        m.beam((x,y,3.8),(x,y+(-1 if y>0 else 1)*1.3,4.42),.1)
for y in (-5.5,5.5):
    m.beam((-8.8,y,4.38),(8.8,y,4.38),.18)
    m.beam((-8.8,y,4.05),(8.8,y,4.05),.12)
    for i in range(9):m.beam((-8.8+i*1.95,y,4.05),(-6.85+i*1.95,y,4.38),.065)
for x in (-7,-4.5,4.5,7):
    for y in (-3.4,3.4):
        m.box((x,y,1.1),(1.8,1.0,2.2),'Graphite',collision=True)
        m.box((x+.85,y-.6,1.1),(1.7,.05,2.05),'Metal',Matrix.Rotation(.28,3,'Z'))
m.export(panel_area_m2=280,tilt_deg=25,rows=4,columns=2,columns_supports=6,
         low_edge_m=4.5,high_edge_m=4.5+5*math.sin(angle),footprint_m=[4*row_width,14])

# Independent blocked collapse crater: no hole is cut in Mars.
m=Module('Station_CollapsePit')
for i in range(32):
    a,b=i*math.tau/32,(i+1)*math.tau/32
    # An attached rubble lip and blocked bowl, entirely above the untouched mesh.
    for r0,z0,r1,z1 in ((1.45,.06,2.05,1.5),(2.05,1.5,2.5,.06)):
        corners=[Vector((r*math.cos(theta),r*math.sin(theta),z))
                 for r,theta,z in ((r0,a,.0),(r0,a,z0),(r0,b,.0),(r0,b,z0),
                                   (r1,a,.0),(r1,a,z1),(r1,b,.0),(r1,b,z1))]
        faces=[(0,2,6,4),(1,5,7,3),(0,4,5,1),(2,3,7,6),(0,1,3,2),(4,6,7,5)]
        start=len(m.vertices);m.vertices+=corners
        m.faces += [tuple(start+j for j in face) for face in faces]
        m.slots += [list(materials).index('SolarDust')]*6
        m.collision.append((corners,faces))
m.box((0,0,.03),(2.9,2.9,.06),'SolarDust',collision=True)
m.export(diameter_m=5,apparent_depth_m=1.5,blocked=True)

(OUT/'MarsStationSolar25.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MarsStationSolar25.blend'))
print(json.dumps(manifest,indent=2))
