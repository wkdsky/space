"""Original front-end models, rigged to the project's compatible player skeleton.

Execute through frontend_blender_client.py. Coordinates are metres, front is -Y.
The imported CC0 reference contributes bones only, never avatar mesh geometry.
"""
import bpy
import bmesh
import json
import math
from pathlib import Path
from mathutils import Vector, Matrix

ROOT = Path('D:/projects/space')
OUT = ROOT / 'SourceArt/FrontEnd'
PRODUCTION_FILES={e['file'] for e in json.loads((OUT/'asset_manifest.json').read_text(encoding='utf-8'))['assets']}
for folder in ['Models', 'Characters', 'Textures', 'Audio', 'UI']:
    (OUT / folder).mkdir(parents=True, exist_ok=True)
scene = bpy.data.scenes.new('JTS_FrontEnd_Source')
bpy.context.window.scene = scene
for previous in list(bpy.data.scenes):
    if previous != scene and previous.name.startswith('JTS_FrontEnd_Source'):
        for obj in list(previous.objects):
            if len(obj.users_scene)==1:bpy.data.objects.remove(obj,do_unlink=True)
        bpy.data.scenes.remove(previous)
scene.name='JTS_FrontEnd_Source'
scene.unit_settings.system = 'METRIC'
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.view_transform = 'AgX'
MATS = []
for name, color, metal, rough in [
    ('Helmet',(.81,.79,.69,1),.08,.42), ('Upper',(.93,.235,.045,1),.05,.53),
    ('Lower',(.81,.79,.69,1),.05,.55), ('Seal',(.025,.037,.044,1),.05,.78),
    ('Metal',(.16,.22,.26,1),.65,.36), ('Visor',(.006,.018,.026,1),.7,.18),
    ('Glow',(.14,.77,.83,1),.05,.3), ('Eye',(.94,.70,.25,1),0,.4),
    ('Mouth',(.94,.70,.25,1),0,.4), ('Concrete',(.19,.23,.27,1),0,.9),
    ('Marking',(.95,.48,.09,1),.05,.6), ('Glass',(.025,.12,.17,1),.65,.2),
]:
    mat = bpy.data.materials.get('FE_' + name) or bpy.data.materials.new('FE_' + name)
    mat.diffuse_color = color; mat.use_nodes = True
    mat.node_tree.nodes.clear()
    node = mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled')
    output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
    mat.node_tree.links.new(node.outputs['BSDF'],output.inputs['Surface'])
    node.inputs['Base Color'].default_value = color
    node.inputs['Metallic'].default_value = metal
    node.inputs['Roughness'].default_value = rough
    if name in ['Glow', 'Eye', 'Mouth']:
        node.inputs['Emission Color'].default_value = color
        node.inputs['Emission Strength'].default_value = .6
    MATS.append(mat)
NAMES = [m.name.removeprefix('FE_') for m in MATS]
parts = []
ASSETS = []


def finish(obj, name, material, bone=None, smooth=False):
    obj.name = name
    obj.data.materials.clear()
    for mat in MATS: obj.data.materials.append(mat)
    for face in obj.data.polygons:
        face.material_index = NAMES.index(material); face.use_smooth = smooth
    if bone:
        obj.vertex_groups.new(name=bone).add(list(range(len(obj.data.vertices))),1,'REPLACE')
    parts.append(obj)
    return obj


def box(name, pos, size, mat='Metal', bone=None, bevel=.025):
    bpy.ops.mesh.primitive_cube_add(size=1,location=pos)
    obj = bpy.context.object; obj.scale = size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        modifier = obj.modifiers.new('Manufactured edge','BEVEL')
        modifier.width = min(bevel,min(size)*.25); modifier.segments = 2
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        normal = obj.modifiers.new('Weighted corner normals','WEIGHTED_NORMAL')
        bpy.ops.object.modifier_apply(modifier=normal.name)
    return finish(obj,name,mat,bone)


def ellipsoid(name, pos, size, mat='Seal', bone=None, segments=24, rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=pos)
    obj=bpy.context.object;obj.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    return finish(obj,name,mat,bone,True)


def visor_panel(name,center,width,height,depth,radius,mat):
    vertices=[]
    for y in [center[1]-depth/2,center[1]+depth/2]:
        for cx,cz,start in [(width/2-radius,height/2-radius,0),(-width/2+radius,height/2-radius,90),(-width/2+radius,-height/2+radius,180),(width/2-radius,-height/2+radius,270)]:
            for i in range(7):
                angle=math.radians(start+i*15)
                vertices.append((center[0]+cx+radius*math.cos(angle),y,center[2]+cz+radius*math.sin(angle)))
    count=len(vertices)//2
    faces=[tuple(range(count-1,-1,-1)),tuple(range(count,count*2))]
    faces += [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj)
    return finish(obj,name,mat,'Head')


def tube(name, start, end, radius, mat='Metal', bone=None, radius2=None, vertices=16):
    start,end=Vector(start),Vector(end)
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=radius,radius2=radius2 or radius,depth=(end-start).length,location=(start+end)*.5)
    obj=bpy.context.object;obj.rotation_euler=(end-start).to_track_quat('Z','Y').to_euler()
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    return finish(obj,name,mat,bone,True)


def line(name, points, radius=.009, mat='Marking', bone=None):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.bevel_depth=radius;curve.bevel_resolution=2
    spline=curve.splines.new('POLY');spline.points.add(len(points)-1)
    for point,co in zip(spline.points,points):point.co=(*co,1)
    obj=bpy.data.objects.new(name,curve);scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj;obj.select_set(True)
    bpy.ops.object.convert(target='MESH');obj=bpy.context.object;obj.select_set(False)
    return finish(obj,name,mat,bone,True)


def loft(name, rings, mat, weights, sides=24):
    vertices=[];faces=[]
    for x,y,z,rx,ry in rings:
        for i in range(sides):
            angle=2*math.pi*i/sides
            vertices.append((x+rx*math.cos(angle),y+ry*math.sin(angle),z))
    for j in range(len(rings)-1):
        for i in range(sides):
            k=j*sides+i;n=j*sides+(i+1)%sides
            faces.append((k,n,n+sides,k+sides))
    faces.extend([tuple(range(sides-1,-1,-1)),tuple((len(rings)-1)*sides+i for i in range(sides))])
    data=bpy.data.meshes.new(name);data.from_pydata(vertices,[],faces);data.update()
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj)
    finish(obj,name,mat,smooth=True)
    for j,groups in enumerate(weights):
        for bone,weight in groups.items():
            group=obj.vertex_groups.get(bone) or obj.vertex_groups.new(name=bone)
            group.add(list(range(j*sides,(j+1)*sides)),weight,'REPLACE')
    return obj


def join(name, objects=None):
    objects=objects if objects is not None else list(parts)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join();obj=bpy.context.object;obj.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(island_margin=.025);bpy.ops.object.mode_set(mode='OBJECT')
    return obj


def export_static(name):
    obj=join(name);parts.clear()
    path=OUT/'Models'/(name+'.fbx')
    if path.relative_to(OUT).as_posix() not in PRODUCTION_FILES:
        obj.hide_render=True;obj.hide_set(True)
        return obj
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},bake_anim=False,axis_forward='-Y',axis_up='Z')
    obj.data.calc_loop_triangles()
    ASSETS.append({'id':name,'source':'original_blender','file':path.relative_to(OUT).as_posix(),'triangles':len(obj.data.loop_triangles),'dimensions_m':list(obj.dimensions)})
    obj.hide_render=True;obj.hide_set(True)
    return obj


def prepare_rig():
    bpy.ops.import_scene.fbx(filepath=str(OUT/'External/Casual_2_reference.fbx'))
    selected=list(bpy.context.selected_objects)
    rig=next(o for o in selected if o.type=='ARMATURE')
    rig.animation_data_clear();matrix=rig.matrix_world.copy();rig.parent=None;rig.matrix_world=matrix
    rig.data.pose_position='REST'
    for obj in selected:
        if obj!=rig:bpy.data.objects.remove(obj,do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    return rig


rig=prepare_rig()
rig.name='Armature' # UE strips Blender's Armature object; no extra skeleton root is imported.
characters=[]
for style in range(5):
    parts.clear()
    # Fitted pressure garment, with articulated waist rather than a single rigid torso.
    loft('PressureJacket',[(0,-.04,z,rx,ry) for z,rx,ry in [(.96,.15,.105),(1.01,.152,.108),(1.10,.143,.10),(1.19,.168,.115),(1.32,.193,.12),(1.41,.21,.115),(1.46,.15,.10)]],
         'Upper',[{'Hips':1},{'Hips':.5,'Abdomen':.5},{'Abdomen':1},{'Abdomen':.4,'Torso':.6},{'Torso':1},{'Chest':1},{'Chest':1}])
    loft('FlexiblePelvis',[(0,-.04,z,rx,ry) for z,rx,ry in [(.88,.15,.095),(.94,.164,.108),(1.0,.151,.108),(1.025,.145,.105)]], 'Lower',[{'Hips':1}]*4)
    for z in [1.035,1.058,1.08]:
        loft('WaistAccordion',[(0,-.04,z,.15,.108),(0,-.04,z+.009,.15,.108)],'Seal',[{'Abdomen':1}]*2)
    # Compact collar and sealed helmet; no skin or human head exists inside.
    tube('NeckSeal',(0,-.043,1.46),(0,-.043,1.53),.103,'Seal','Neck',vertices=32)
    tube('LockingCollar',(0,-.043,1.49),(0,-.043,1.516),.126,'Metal','Neck',vertices=32)
    ellipsoid('HelmetShell',(0,-.04,1.694),(.197,.184,.213),'Helmet','Head',32,20)
    visor_panel('VisorRim',(0,-.197,1.707),.346,.244,.076,.060,'Seal')
    visor_panel('VisorGlass',(0,-.239,1.707),.313,.205,.018,.045,'Visor')
    for side in [-1,1]:
        tube('CommsPort',(side*.183,-.04,1.68),(side*.208,-.04,1.68),.061,'Metal','Head',vertices=24)
        tube('CommsLamp',(side*.207,-.04,1.68),(side*.211,-.04,1.68),.026,'Glow','Head',vertices=16)
    # Face quads use individual UV0 ranges restored after automatic body unwrap.
    for name,pos,size in [('Eye',(0,-.250,1.735),(.235,.001,.067)),('Mouth',(0,-.250,1.662),(.12,.001,.035))]:
        box(name+'Display',pos,size,name,'Head',0)
    box('ChestInset',(0,-.168,1.328),(.217,.038,.16),'Seal','Torso',.018)
    box('ChestPlate',(0,-.190,1.33),(.197,.014,.136),'Helmet','Torso',.014)
    box('Telemetry',(0,-.200,1.357),(.151,.005,.028),'Visor','Torso',.004)
    for x in [-.05,0,.05]:box('TelemetryPixel',(x,-.204,1.357),(.032,.003,.009),'Glow','Torso',.001)
    for x in [-.064,-.032,0]:tube('PanelKey',(x,-.202,1.31),(x,-.212,1.31),.009,'Marking','Torso',vertices=12)
    box('PanelLabel',(.047,-.203,1.31),(.048,.004,.015),'Metal','Torso',.002)
    box('LifeSupport',(0,.105,1.28),(.245,.106,.29),'Metal','Torso',.025)
    box('LifeSupportCover',(0,.166,1.29),(.213,.032,.223),'Upper','Torso',.015)
    for x in [-.07,.07]:box('PackVent',(x,.187,1.30),(.043,.011,.126),'Seal','Torso',.005)
    for side,sign in [('L',1),('R',-1)]:
        shoulder=Vector((sign*.153,-.060,1.432));elbow=Vector((sign*.334,-.056,1.432));wrist=Vector((sign*.568,-.086,1.432))
        tube('UpperSleeve',shoulder,elbow,.073,'Upper','UpperArm_'+side,radius2=.057,vertices=24)
        ellipsoid('ShoulderJoint',shoulder,(.083,.079,.081),'Upper','UpperArm_'+side)
        ellipsoid('ElbowFlex',elbow,(.062,.062,.062),'Seal','LowerArm_'+side)
        tube('Forearm',elbow,wrist,.062,'Upper','LowerArm_'+side,radius2=.047,vertices=24)
        for t in [.14,.27]:
            c=elbow.lerp(wrist,t);d=(wrist-elbow).normalized()*.009
            tube('ElbowRib',c-d,c+d,.064,'Seal','LowerArm_'+side,vertices=24)
        tube('Cuff',wrist-Vector((sign*.035,0,0)),wrist,.053,'Metal','Wrist_'+side,vertices=24)
        ellipsoid('GlovePalm',(sign*.660,-.084,1.432),(.085,.055,.039),'Upper','Wrist_'+side)
        tube('ThumbWeb',(sign*.624,-.106,1.424),(sign*.650,-.149,1.408),.023,'Upper','Thumb1_'+side,vertices=12)
        for finger in ['Index','Middle','Ring','Pinky','Thumb']:
            for index in ([1,2] if finger=='Thumb' else [2,3]):
                b=rig.data.bones[finger+str(index)+'_'+side]
                h=rig.matrix_world@b.head_local;t=rig.matrix_world@b.tail_local
                if finger=='Thumb' and index==2:t=h+(t-h).normalized()*.027
                tube('Glove'+finger+str(index),h,t,.013 if finger!='Thumb' else .018,'Upper',b.name,vertices=10)
                ellipsoid('GloveKnuckle',h,(.014,.014,.014),'Upper',b.name,12,8)
                ellipsoid('GloveTip',t,(.013,.013,.013),'Upper',b.name,12,8)
        hip=Vector((sign*.121,-.045,.964));knee=Vector((sign*.121,-.087,.533));ankle=Vector((sign*.121,-.062,.105))
        tube('Thigh',hip,knee,.094,'Lower','UpperLeg_'+side,radius2=.068,vertices=24)
        ellipsoid('KneeJoint',knee,(.071,.073,.069),'Seal','LowerLeg_'+side)
        tube('Calf',knee,ankle,.073,'Lower','LowerLeg_'+side,radius2=.055,vertices=24)
        box('KneePlate',(sign*.121,-.146,.532),(.112,.034,.111),'Helmet','LowerLeg_'+side,.018)
        for z in [.573,.592]:box('KneeSignal',(sign*.121,-.153,z),(.065,.006,.009),'Marking','UpperLeg_'+side,.001)
        box('Boot',(sign*.121,-.123,.075),(.143,.265,.145),'Lower','Foot_'+side,.025)
        box('BootSole',(sign*.121,-.123,.020),(.147,.271,.038),'Seal','Foot_'+side,.006)
        box('ToeGuard',(sign*.121,-.231,.067),(.135,.045,.065),'Metal','Foot_'+side,.009)
        box('BeltPouch',(sign*.151,-.108,.973),(.064,.052,.096),'Metal','Hips',.01)
        # Suit-specific authored geometry, kept inside common collision and motion envelope.
        if style in [0,2,4]:
            box('ShoulderPlate',(sign*.193,-.054,1.487),(.116,.159,.04),'Helmet','UpperArm_'+side,.012)
        if style==1:
            line('ScoutStripe',[(sign*.12,-.163,1.40),(sign*.155,-.166,1.25),(sign*.13,-.148,1.18)],.009,'Glow','Torso')
            box('ScoutShin',(sign*.121,-.141,.34),(.047,.022,.17),'Metal','LowerLeg_'+side,.01)
        if style==2:
            box('ToolCarrier',(sign*.174,.02,.88),(.075,.063,.17),'Metal','UpperLeg_'+side,.01)
            for j in range(3):tube('EngineerTool',(sign*(.158+j*.018),-.02,.85),(sign*(.158+j*.018),-.02,.98),.006,'Marking','UpperLeg_'+side,vertices=8)
        if style==3:
            box('SurveyPad',(sign*.46,-.119,1.458),(.083,.031,.051),'Metal','LowerArm_'+side,.008)
            box('SurveyScreen',(sign*.46,-.138,1.458),(.060,.004,.034),'Glow','LowerArm_'+side,.004)
        if style==4:
            box('TacticalThigh',(sign*.122,-.119,.788),(.109,.035,.17),'Metal','UpperLeg_'+side,.016)
            box('TacticalChestWing',(sign*.125,-.162,1.34),(.047,.028,.188),'Helmet','Torso',.01)
    if style==3:
        tube('SurveyAntenna',(.10,.14,1.43),(.10,.14,1.74),.007,'Metal','Chest',vertices=10)
        ellipsoid('AntennaLamp',(.10,.14,1.74),(.014,.014,.014),'Glow','Chest',12,8)
    mesh=join('SK_FE_'+['Explorer','Scout','Engineer','Survey','Tactical'][style]);parts.clear()
    # Restore full 0..1 UVs for each face display; UV coordinates are generated from front projection.
    uv=mesh.data.uv_layers.active
    for poly in mesh.data.polygons:
        mat=mesh.data.materials[poly.material_index].name
        if mat not in ['FE_Eye','FE_Mouth']:continue
        width,height,center=(.235,.067,1.735) if mat=='FE_Eye' else (.12,.035,1.662)
        for li in poly.loop_indices:
            p=mesh.data.vertices[mesh.data.loops[li].vertex_index].co
            uv.data[li].uv=(p.x/width+.5,(p.z-center)/height+.5)
    world=mesh.matrix_world.copy()
    for v in mesh.data.vertices:v.co=rig.matrix_world.inverted()@world@v.co
    mesh.matrix_world=rig.matrix_world.copy();mesh.parent=rig;mesh.matrix_parent_inverse=Matrix.Identity(4);mesh.matrix_basis=Matrix.Identity(4)
    modifier=mesh.modifiers.new('CompatiblePlayerRig','ARMATURE');modifier.object=rig
    mesh.data.calc_loop_triangles()
    assert all(v.groups for v in mesh.data.vertices),'Unweighted avatar vertices'
    assert len(mesh.data.loop_triangles)<=35000
    ASSETS.append({'id':mesh.name,'source':'original_blender','file':'Characters/'+mesh.name+'.fbx','triangles':len(mesh.data.loop_triangles),'unweighted_vertices':0,'rig':'W3 compatible bones; original mesh and weights','material_slots':[m.name for m in mesh.data.materials]})
    characters.append(mesh)
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);mesh.select_set(True);bpy.context.view_layer.objects.active=mesh
    bpy.ops.export_scene.fbx(filepath=str(OUT/'Characters'/(mesh.name+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',use_armature_deform_only=False,mesh_smooth_type='FACE',apply_scale_options='FBX_SCALE_NONE')
    # Explicit reduction exports keep the same rig and material sections.
    for lod,ratio in [(1,.55),(2,.26)]:
        duplicate=mesh.copy();duplicate.data=mesh.data.copy();scene.collection.objects.link(duplicate)
        bpy.ops.object.select_all(action='DESELECT');duplicate.select_set(True);bpy.context.view_layer.objects.active=duplicate
        reduction=duplicate.modifiers.new('LOD reduction','DECIMATE');reduction.ratio=ratio;bpy.ops.object.modifier_apply(modifier=reduction.name)
        rig.select_set(True)
        bpy.ops.export_scene.fbx(filepath=str(OUT/'Characters'/(mesh.name+'_LOD'+str(lod)+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',use_armature_deform_only=False)
        bpy.data.objects.remove(duplicate,do_unlink=True)
    mesh.hide_render=True;mesh.hide_set(True)

# Environment kit: each piece has authored functional details and consistent 0.02m edge treatment.
rig.name='JTS_AvatarRig'
for kind in ['Floor','GrateFloor','Wall','ServiceWall','Column','Beam','Brace','DoorFrame','Door','PlatformEdge','Stairs','Rail']:
    parts.clear()
    if kind in ['Floor','GrateFloor']:
        box('Deck',(0,0,-.10),(4,4,.2),'Concrete')
        for x in [-1.98,1.98]:box('DeckJoint',(x,0,.002),(.025,4,.004),'Seal',bevel=0)
        if kind=='GrateFloor':
            box('GrateWell',(0,0,.01),(2.8,2.8,.03),'Seal')
            for i in range(-8,9):box('GrateBar',(i*.16,0,.045),(.05,2.8,.05),'Metal',bevel=.007)
    elif kind in ['Wall','ServiceWall']:
        box('WallPanel',(0,0,2),(4,.2,4),'Metal')
        for x in [-1.9,1.9]:box('WallPost',(x,-.15,2),(.12,.16,4),'Seal')
        for z in [.2,3.8]:box('WallTrim',(0,-.14,z),(3.8,.1,.08),'Marking')
        if kind=='ServiceWall':
            for x in [-1.3,-.9]:tube('Conduit',(x,-.24,.2),(x,-.24,3.5),.07,'Metal')
            box('Junction',(.4,-.3,1.8),(1.1,.35,1.3),'Helmet')
            for z in [1.4,1.6,1.8]:box('JunctionVent',(.4,-.49,z),(.76,.02,.05),'Seal')
    elif kind in ['Column','Beam','Brace']:
        height=5 if kind=='Column' else 4
        box('Core',(0,0,height*.5),(.26,.3,height),'Metal')
        for x in [-.23,.23]:box('Flange',(x,0,height*.5),(.08,.50,height),'Seal')
        for z in [.15,height-.15]:box('FastenerPlate',(0,-.27,z),(.48,.08,.22),'Marking')
        if kind=='Brace':
            for obj in parts:obj.rotation_euler.y=math.radians(35)
    elif kind=='DoorFrame':
        for x in [-1.65,1.65]:box('Jamb',(x,0,1.65),(.3,.5,3.3),'Metal')
        box('Lintel',(0,0,3.3),(3.6,.5,.3),'Marking')
    elif kind=='Door':
        box('DoorLeaf',(0,0,1.5),(3,.15,3),'Helmet')
        for x in [-.8,.8]:box('DoorRib',(x,-.12,1.5),(.1,.12,2.8),'Metal')
        box('Window',(0,-.10,2),(.9,.05,.55),'Glass')
    elif kind=='PlatformEdge':
        box('Curb',(0,0,.15),(4,.25,.3),'Metal')
        for x in range(-7,8):
            obj=box('Hazard',(x*.25,-.136,.15),(.1,.012,.24),'Marking',bevel=0);obj.rotation_euler.y=.5
    elif kind=='Stairs':
        for i in range(6):box('Step',(0,i*.28,(i+1)*.15), (1.6,.3,.15),'Metal')
    elif kind=='Rail':
        for x in [-1.9,0,1.9]:tube('Post',(x,0,0),(x,0,1.1),.04)
        for z in [.5,1.1]:tube('Handrail',(-2,0,z),(2,0,z),.04,'Marking')
    export_static('SM_FE_'+kind)

for kind in ['TowerBase','TowerMiddle','TowerTop','TowerArm','DistantBuilding','Antenna']:
    parts.clear()
    if kind.startswith('Tower'):
        height=4 if kind!='TowerTop' else 2
        for x in [-1,1]:
            for y in [-1,1]:box('TowerLeg',(x,y,height/2),(.22,.22,height),'Metal')
        for z in [.1,height-.1]:box('TowerDeck',(0,0,z),(2.4,2.4,.16),'Metal')
        for x in [-1,1]:
            tube('CrossBrace',(x,-1,.2),(x,1,height-.2),.055,'Marking')
            tube('CrossBrace',(x,1,.2),(x,-1,height-.2),.055,'Marking')
        if kind=='TowerArm':box('ServiceArm',(3,0,height-.3),(6,.8,.6),'Metal')
        if kind=='TowerTop':tube('Beacon',(0,0,2),(0,0,2.3),.15,'Glow')
    elif kind=='DistantBuilding':
        box('Building',(0,0,3),(12,8,6),'Concrete')
        box('Roof',(0,0,6.1),(12.5,8.5,.2),'Metal')
        for i in range(-4,5):box('Window',(i*1.1,-4.01,4),(.7,.02,.6),'Glow')
    else:
        tube('AntennaMast',(0,0,0),(0,0,5),.10)
        for z in [3,4,5]:tube('Aerial',(-1.3,0,z),(1.3,0,z),.035,'Marking')
        box('Transmitter',(0,0,2),(.5,.5,.8),'Helmet')
    export_static('SM_FE_'+kind)

for kind in ['CrateSmall','CrateLarge','RepairTerminal','PowerUnit','CableStraight','CableCoil','RepairCart','Toolbox','Barrier','LightPole','Vent','DockMarker','MaintenanceRobot','RiflePose','MiningToolPose']:
    parts.clear()
    if kind.startswith('Crate') or kind=='Toolbox':
        scale=.7 if kind=='CrateSmall' else 1.2 if kind=='CrateLarge' else .4
        box('Container',(0,0,scale*.4),(scale,scale*.75,scale*.8),'Helmet')
        for x in [-scale*.35,scale*.35]:box('Strap',(x,0,scale*.4),(.055,scale*.77,scale*.82),'Metal')
        box('Latch',(0,-scale*.39,scale*.4),(scale*.18,.035,scale*.12),'Marking')
        box('Label',(scale*.16,-scale*.383,scale*.57),(scale*.24,.005,scale*.1),'Seal',bevel=0)
    elif kind in ['RepairTerminal','PowerUnit']:
        box('Foot',(0,0,.06),(.8,.6,.12),'Metal')
        box('Cabinet',(0,0,.7),(.65,.45,1.3),'Helmet')
        box('Inset',(0,-.24,.94),(.48,.04,.35),'Seal')
        box('Screen',(0,-.267,.97),(.40,.01,.23),'Glass')
        for x in [-.14,-.07,0,.07]:box('Indicator',(x,-.274,1.0),(.035,.004,.018),'Glow',bevel=0)
        for z in [.3,.37,.44]:box('CoolingSlit',(0,-.24,z),(.4,.03,.025),'Seal')
    elif kind.startswith('Cable'):
        points=[(0,t*.15,.035) for t in range(21)] if kind=='CableStraight' else [(math.cos(t*.1)*(.4+t*.0015),math.sin(t*.1)*(.4+t*.0015),.04+t*.00025) for t in range(180)]
        line('InsulatedCable',points,.025,'Seal')
        for p in [points[0],points[-1]]:box('Plug',p,(.11,.06,.07),'Marking',bevel=.01)
    elif kind=='RepairCart':
        for z in [.24,.74]:box('Tray',(0,0,z),(.9,.6,.05),'Metal')
        for x in [-.4,.4]:
            for y in [-.25,.25]:
                tube('CartLeg',(x,y,.2),(x,y,.8),.025)
                tube('Wheel',(x-.04,y,.1),(x+.04,y,.1),.095,'Seal')
        box('CartDrawer',(0,0,.55),(.8,.5,.19),'Upper')
    elif kind=='Barrier':
        for x in [-.8,.8]:box('BarrierFoot',(x,0,.05),(.4,.7,.1),'Seal');tube('Stanchion',(x,0,.1),(x,0,1),.04)
        box('Barrier',(0,0,.85),(1.9,.08,.3),'Marking')
        for x in [-.6,-.2,.2,.6]:box('BlackStripe',(x,-.047,.85),(.14,.01,.28),'Seal',bevel=0)
    elif kind=='LightPole':
        tube('Pole',(0,0,0),(0,0,5),.055)
        box('LampHousing',(0,-.2,5),(.65,.45,.12),'Metal')
        box('LampFace',(0,-.2,4.93),(.56,.36,.02),'Glow')
    elif kind=='Vent':
        box('VentCase',(0,0,.3),(1,.6,.6),'Metal')
        for x in range(-5,6):box('VentSlat',(x*.073,-.32,.3),(.03,.06,.42),'Seal')
    elif kind=='DockMarker':
        tube('Pad',(0,0,.01),(0,0,.04),.42,'Seal',vertices=48)
        for i in range(12):
            a=i*math.tau/12;box('PadLight',(.36*math.cos(a),.36*math.sin(a),.045),(.07,.035,.01),'Glow',bevel=.003)
    elif kind=='MaintenanceRobot':
        box('RobotBody',(0,0,.38),(.65,.43,.48),'Helmet',bevel=.055)
        box('RobotBumper',(0,0,.19),(.73,.51,.10),'Marking')
        box('RobotScreen',(0,-.232,.45),(.47,.025,.17),'Visor',bevel=.03)
        for x in [-.12,.12]:ellipsoid('RobotEye',(x,-.252,.47),(.025,.01,.025),'Glow')
        for x in [-.34,.34]:
            for y in [-.14,.14]:tube('RobotWheel',(x-.04,y,.14),(x+.04,y,.14),.13,'Seal',vertices=24)
        tube('RobotAntenna',(.17,.08,.61),(.17,.08,.88),.008)
        ellipsoid('RobotBeacon',(.17,.08,.88),(.025,.025,.025),'Marking')
    elif kind=='RiflePose':
        box('Receiver',(0,0,0),(.12,.44,.14),'Metal');tube('Barrel',(0,-.2,0),(0,-.65,0),.035)
        box('Stock',(0,.31,0),(.08,.25,.10),'Seal');box('Grip',(0,.02,-.11),(.07,.10,.16),'Seal')
        box('Magazine',(0,-.11,-.13),(.07,.14,.19),'Upper');box('Sight',(0,-.11,.105),(.05,.12,.06),'Glow')
    else:
        tube('ToolShaft',(0,0,0),(0,0,.9),.022,'Metal')
        box('ToolHead',(0,0,.9),(.5,.08,.08),'Marking');box('Grip',(0,0,.3),(.06,.06,.25),'Seal')
    export_static('SM_FE_'+kind)

for i in range(3):
    parts.clear();ellipsoid('Hill',(0,0,-2),(30+i*10,15+i*3,8+i*2),'Concrete',segments=24,rings=8)
    export_static('SM_FE_Hill'+str(i+1))
parts.clear();ellipsoid('Moon',(0,0,0),(1,1,1),'Helmet',segments=48,rings=32);export_static('SM_FE_Moon')

# Adapt project ship into a separate display asset: weld, corrected normals, bevels, new palette.
ship_parts=[]
for filename in ['SM_Twinfork_R1_Hull.obj','SM_Twinfork_R1_Shell.obj','SM_Twinfork_Gear.obj']:
    bpy.ops.wm.obj_import(filepath=str(ROOT/'SourceArt/TwinforkR1'/filename),global_scale=.01,forward_axis='X',up_axis='Z')
    imported=[o for o in bpy.context.selected_objects if o.type=='MESH']
    for obj in imported:
        # Source material group names are mapped to the front-end's restrained palette.
        old=[m.name.lower() if m else '' for m in obj.data.materials]
        slots=[('Glass' if any(w in n for w in ['glass','window','cockpit']) else 'Glow' if any(w in n for w in ['light','emiss','cyan']) else 'Upper' if any(w in n for w in ['orange','accent']) else 'Metal' if any(w in n for w in ['dark','black','metal']) else 'Helmet') for n in old]
        indices=[p.material_index for p in obj.data.polygons]
        obj.data.materials.clear()
        for m in MATS:obj.data.materials.append(m)
        for p,index in zip(obj.data.polygons,indices):p.material_index=NAMES.index(slots[min(index,len(slots)-1)] if slots else 'Helmet')
        bm=bmesh.new();bm.from_mesh(obj.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.0005);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
        bevel=obj.modifiers.new('DisplayEdge','BEVEL');bevel.width=.015;bevel.segments=2
        bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=bevel.name)
        ship_parts.append(obj)
parts[:]=ship_parts
ship=export_static('SM_FE_TwinforkDisplay')
ASSETS[-1]['source']='project_twinfork_adapted'
ASSETS[-1]['adaptations']=['Separate display mesh','Welded vertices and recalculated normals','0.015m manufactured bevel','New industrial ivory/orange/graphite material palette','Removed gameplay-only internals and weapons']

# Studio and pose preview; original menu actions are exported separately below.
rig.data.pose_position='POSE'
for bone in rig.pose.bones:bone.rotation_mode='XYZ';bone.rotation_euler=(0,0,0)
for side,sign in [('L',1),('R',-1)]:
    rig.pose.bones['UpperArm_'+side].rotation_euler.y=math.radians(sign*68)
    rig.pose.bones['LowerArm_'+side].rotation_euler.z=math.radians(-sign*6)
rig.hide_render=False
characters[0].hide_render=False;characters[0].hide_set(False)
bpy.ops.object.camera_add(location=(2.8,-5.8,2.6));camera=bpy.context.object
camera.rotation_euler=(Vector((0,0,1.0))-camera.location).to_track_quat('-Z','Y').to_euler()
camera.data.type='ORTHO';camera.data.ortho_scale=2.35;scene.camera=camera
for location,power,size,color in [((-3,-4,5),650,4,(1,.79,.58)),((3,-1,4),500,3,(.55,.78,1)),((0,3,4),800,3,(1,.5,.23))]:
    bpy.ops.object.light_add(type='AREA',location=location);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.data.color=color
    light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.mesh.primitive_plane_add(size=200,location=(0,0,-.012));ground=bpy.context.object;ground.data.materials.append(MATS[3])
scene.world=bpy.data.worlds.new('FrontEndStudio');scene.world.color=(.12,.12,.12)
scene.render.resolution_x=900;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
(OUT/'models_manifest.json').write_text(json.dumps(ASSETS,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'FrontEnd_Models.blend'))
result={'assets':len(ASSETS),'characters':ASSETS[:5],'blend':bpy.data.filepath}
