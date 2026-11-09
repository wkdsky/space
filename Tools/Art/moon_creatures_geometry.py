"""Author the four V3 Moon creatures in live Blender through MCP.

Coordinates are metres, +X forward and +Z surface normal. Shell pieces and
joint collars have rigid weights on their corresponding articulated bones.
No world actors, terrain or existing scene objects are edited.
"""
import bpy
import json
import math
from mathutils import Vector

SCENE = "JTS_MoonCreatures_V3"
KINDS = ("MirrorSkimmer", "Riftlip", "SpireWorker", "SpireSoldier")


def material(name, color, roughness=.65):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    node = next(n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    node.inputs["Base Color"].default_value = (*color, 1)
    node.inputs["Roughness"].default_value = roughness
    node.inputs["Metallic"].default_value = 0
    mat.diffuse_color = (*color, 1)
    return mat


def studio():
    scene = bpy.data.scenes.get(SCENE) or bpy.data.scenes.new(SCENE)
    bpy.context.window.scene = scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1
    scene.render.fps = 30
    scene.frame_start, scene.frame_end = 1, 61
    return scene


def palette(kind):
    colors = {
        "MirrorSkimmer": [( .79,.78,.70),(.57,.60,.60),(.24,.27,.28),(.88,.87,.79)],
        "Riftlip": [(.64,.66,.65),(.46,.49,.49),(.20,.23,.25),(.75,.76,.70)],
        "SpireWorker": [(.66,.61,.53),(.48,.45,.40),(.25,.26,.27),(.77,.73,.64)],
        "SpireSoldier": [(.60,.63,.65),(.42,.45,.48),(.19,.22,.25),(.74,.76,.75)],
    }[kind]
    return [material("M_MC_" + kind + "_" + label, c, .32 if kind=="MirrorSkimmer" and i==0 else .65)
            for i, (label, c) in enumerate(zip(("Shell", "Wear", "Joint", "Crown"), colors))]


class Builder:
    """Collect only the geometry and deformation data for one asset."""
    def __init__(self, kind):
        self.kind = kind
        self.scene = studio()
        assert not bpy.data.objects.get("RIG_"+kind), "Asset already exists: " + kind
        self.collection = bpy.data.collections.new("CREATURE_" + kind)
        self.scene.collection.children.link(self.collection)
        self.parts = []
        self.bones = []
        self.mats = palette(kind)

    def bone(self, name, head, tail, parent=None):
        self.bones.append((name, tuple(head), tuple(tail), parent))

    def mesh(self, name, vertices, faces, bone, mat=0):
        data = bpy.data.meshes.new(self.kind+"_"+name)
        data.from_pydata(vertices, [], faces)
        data.update()
        obj = bpy.data.objects.new(self.kind+"_"+name, data)
        self.collection.objects.link(obj)
        obj.data.materials.append(self.mats[mat])
        group = obj.vertex_groups.new(name=bone)
        group.add(list(range(len(vertices))), 1, "REPLACE")
        self.parts.append(obj)
        return obj

    def loft(self, name, outline, levels, bone, mat=0, crown=False):
        # Each level is z, xy scale and x offset. All rings have shared winding.
        n = len(outline)
        vs = [(x*s+dx, y*s, z) for z,s,dx in levels for x,y in outline]
        faces = [tuple(reversed(range(n)))]
        for k in range(len(levels)-1):
            for i in range(n):
                j=(i+1)%n
                faces.append((k*n+i,k*n+j,(k+1)*n+j,(k+1)*n+i))
        faces.append(tuple((len(levels)-1)*n+i for i in range(n)))
        obj = self.mesh(name,vs,faces,bone,mat)
        if crown:
            obj.data.materials.append(self.mats[3])
            obj.data.polygons[-1].material_index=1
        return obj

    def pebble(self, name, center, dims, bone, mat=0, power=2, n=12):
        outline=rounded(dims[0]/2,dims[1]/2,power,n)
        z=center[2]; h=dims[2]/2
        obj=self.loft(name,outline,[(z-h,.58,center[0]),(z-h*.45,1,center[0]),
                                  (z+h*.5,.94,center[0]),(z+h,.5,center[0])],bone,mat)
        for v in obj.data.vertices:
            v.co.y+=center[1]
        return obj

    def link(self, name, a, b, r0, r1, bone, mat=0, sides=6):
        a,b=Vector(a),Vector(b)
        direction=(b-a).normalized()
        across=direction.cross(Vector((1,0,0)))
        if across.length<.1:
            across=direction.cross(Vector((0,1,0)))
        across.normalize(); up=direction.cross(across).normalized()
        vs=[]
        # Tapered ends avoid the appearance of pipes or mechanical hinges.
        for p,r in ((a,r0*.65),(a.lerp(b,.18),r0),(a.lerp(b,.8),r1),(b,r1*.65)):
            for i in range(sides):
                angle=2*math.pi*i/sides
                vs.append(tuple(p+r*(math.cos(angle)*across+math.sin(angle)*up)))
        faces=[tuple(reversed(range(sides)))]
        for j in range(3):
            for i in range(sides):
                k=(i+1)%sides
                faces.append((j*sides+i,j*sides+k,(j+1)*sides+k,(j+1)*sides+i))
        faces.append(tuple(3*sides+i for i in range(sides)))
        return self.mesh(name,vs,faces,bone,mat)

    def finish(self, spec):
        bpy.ops.object.select_all(action="DESELECT")
        for part in self.parts:
            part.select_set(True)
        bpy.context.view_layer.objects.active=self.parts[0]
        bpy.ops.object.join()
        mesh=bpy.context.object
        mesh.name="SK_"+self.kind
        mesh.data.name=mesh.name+"_Geometry"
        if self.kind=="Riftlip":bind_riftlip_tail(mesh)
        # Flat broad faces, explicit triangulation, no subdivision dependency.
        tri=mesh.modifiers.new("ExportTriangles","TRIANGULATE")
        bpy.ops.object.modifier_apply(modifier=tri.name)
        armdata=bpy.data.armatures.new("SKEL_"+self.kind)
        rig=bpy.data.objects.new("RIG_"+self.kind,armdata)
        self.collection.objects.link(rig)
        bpy.context.view_layer.objects.active=rig
        mesh.select_set(False); rig.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        for name,h,t,parent in self.bones:
            bone=armdata.edit_bones.new(name)
            bone.head=h; bone.tail=t
            if parent:
                bone.parent=armdata.edit_bones[parent]
        bpy.ops.object.mode_set(mode="OBJECT")
        for bone in rig.pose.bones:
            bone.rotation_mode="QUATERNION"
        mesh.parent=rig
        mod=mesh.modifiers.new("BoundSkeleton","ARMATURE")
        mod.object=rig
        rig.show_in_front=True
        rig["species"]=self.kind
        rig["design_version"]="Moon v2 ecology / v3 low-poly"
        rig["forward_axis"]="+X"
        rig["surface_normal"]="+Z (actor aligns to spherical planet surface)"
        rig["motion_spec"]=json.dumps(spec)
        rig["asset_origin"]="Original geometry authored in Blender via MCP; no downloaded mesh content."
        mesh["skinning"]=("Rigid shell and legs; blended head/mouth weights at flexible lip roots."
                           if self.kind=="Riftlip" else
                           "Rigid exoskeleton plates, normalized deformation weights, articulated mouth and six legs.")
        return {"species":self.kind,"vertices":len(mesh.data.vertices),"triangles":len(mesh.data.polygons),
                "bones":len(armdata.bones),"materials":len(mesh.data.materials)}


def bind_riftlip_tail(mesh):
    """Let the existing abdomen bone bend the spindle without changing its silhouette or skeleton."""
    body=mesh.vertex_groups.get('body');abdomen=mesh.vertex_groups.get('abdomen')
    for vertex in mesh.data.vertices:
        if vertex.co.x>=-.04 or not any(g.group==body.index and g.weight>0 for g in vertex.groups):continue
        t=max(0,min(1,(-vertex.co.x-.04)/.15));weight=t*t*(3-2*t)
        body.add([vertex.index],1-weight,'REPLACE');abdomen.add([vertex.index],weight,'REPLACE')


def rounded(hx,hy,power=4,n=20):
    def signed(v):
        return math.copysign(abs(v)**(2/power),v)
    return [(hx*signed(math.cos(i*2*math.pi/n)),hy*signed(math.sin(i*2*math.pi/n))) for i in range(n)]


def base(builder, z):
    builder.bone("root",(0,0,0),(0,0,.05))
    builder.bone("body",(0,0,z),( .08,0,z),"root")
    builder.bone("head",(.10,0,z),(.20,0,z),"body")
    builder.bone("abdomen",(-.03,0,z),(-.15,0,z),"body")


def legs(builder, xs, width, z, reach, radius, shovels=False):
    spec=[]
    for side,sign in (("L",1),("R",-1)):
        for i,x in enumerate(xs):
            label=f"{side}{i+1}"
            h=Vector((x,sign*width,z))
            k=h+Vector((.025 if i==0 else -.015,sign*reach*.60,.008))
            f=h+Vector((.065 if i==0 else -.035,sign*reach,.012-z))
            toe=f+Vector((.045,sign*.008,-.002))
            upper,lower,tip=("leg_"+label+"_"+n for n in ("upper","lower","toe"))
            builder.bone(upper,h,k,"body")
            builder.bone(lower,k,f,upper)
            builder.bone(tip,f,toe,lower)
            front=shovels and i==0
            rad=radius*(1.28 if front else 1)
            builder.link(label+"_haunch",h,k,rad*1.1,rad,upper,1)
            builder.pebble(label+"_seal",k,(rad*1.6,rad*1.5,rad*1.4),lower,2,n=8)
            builder.link(label+"_shin",k,f,rad*.87,rad*.6,lower,0)
            if front:
                builder.pebble(label+"_spade",tuple(f+Vector((.024,sign*.006,.003))),
                                (.105,.074,.026),tip,0,power=3,n=8)
                builder.pebble(label+"_spade_worn_edge",tuple(toe+Vector((.018,0,.001))),
                                (.039,.074,.016),tip,1,power=3,n=8)
            else:
                builder.pebble(label+"_pad",tuple((f+toe)*.5),(.055,rad*1.8,.018),tip,1,n=8)
            spec.append(dict(label=label,h=list(h),k=list(k),f=list(f),toe=list(toe),
                             upper=upper,lower=lower,tip=tip,front=front))
    return spec


def mouths(builder, center, spread, length, width, z, lip=False):
    if lip:
        return riftlip_mouths(builder, center, spread, length, width, z)
    for side,sign in (("L",1),("R",-1)):
        bone="mouth_"+side
        h=(center,sign*spread,z)
        builder.bone(bone,h,(center+length,sign*spread,z),"head")
        builder.pebble("jaw_"+side,(center+length*.45,sign*spread,z),
                        (length,width,.035),bone,1,power=3,n=8)


def riftlip_mouths(builder, center, spread, length, width, z):
    # A recessed seal and rolled inner edges make the cleft read when closed.
    # Broad, blunt lobes form one split wedge rather than separate pincers.
    seal=len(builder.mats)
    builder.mats.append(material("M_MC_Riftlip_Seal",(.065,.085,.088),.83))
    rim=len(builder.mats)
    builder.mats.append(material("M_MC_Riftlip_LipRim",(.56,.58,.52),.70))
    floor=[(center-.027,-.019),(center+.045,-.030),(center+.078,-.019),
           (center+.084,0),(center+.078,.019),(center+.045,.030),(center-.027,.019)]
    builder.loft("recessed_seal_floor",floor,[(z-.027,.98,0),(z-.016,1,0)],"head",seal)
    builder.pebble("concealed_scraper",(center+.064,0,z-.013),(.058,.025,.009),"head",1,n=8)
    outline=[(-.025,.008),(.014,.006),(.050,.008),(.084,.012),(.119,.019),
             (.143,.030),(.148,.047),(.129,.095),(.092,.110),(.044,.107),
             (.000,.082),(-.025,.044)]
    for side,sign in (("L",1),("R",-1)):
        bone="mouth_"+side
        builder.bone(bone,(center,sign*spread,z),(center+length,sign*spread,z),"head")
        parts=[]
        ring=[(center+x,sign*y) for x,y in outline]
        if sign<0:ring.reverse()
        cx,cy=center+.058,sign*.057
        vs=[]
        for scale,dz in ((.77,-.013),(1,.002),(.98,.026),(.73,.041)):
            for x,y in ring:
                vx=cx+(x-cx)*scale;vy=cy+(y-cy)*scale
                vs.append((vx,vy,z+dz-.105*(vx-center)))
        n=len(ring)
        faces=[tuple(reversed(range(n)))]
        for k in range(3):
            for i in range(n):
                j=(i+1)%n
                faces.append((k*n+i,k*n+j,(k+1)*n+j,(k+1)*n+i))
        faces.append(tuple(3*n+i for i in range(n)))
        parts.append(builder.mesh("rounded_lip_lobe_"+side,vs,faces,bone,0))

        # Elliptical rolls vary in thickness and merge into the cheek at the
        # back. The dark lower roll reads as a deep fold under the pale edge.
        path=[(-.012,.012,.028,.004,.008),(.021,.015,.037,.009,.015),
              (.056,.018,.036,.011,.016),(.089,.023,.028,.012,.015),
              (.119,.034,.017,.012,.014),(.139,.047,.005,.005,.008)]
        for label,dy,dz,scale,mat in (("rolled_lip",0,0,1,rim),("inner_seal",-.005,-.015,.78,seal)):
            verts=[]
            for j,(dx,y,height,ry,rz) in enumerate(path):
                prev=path[max(0,j-1)];nxt=path[min(len(path)-1,j+1)]
                tangent=Vector((nxt[0]-prev[0],sign*(nxt[1]-prev[1]),0)).normalized()
                across=Vector((-tangent.y,tangent.x,0))
                for i in range(8):
                    a=i*math.tau/8
                    point=Vector((center+dx,sign*(y+dy),z+height+dz))
                    point+=across*(ry*scale*math.cos(a))+Vector((0,0,rz*scale*math.sin(a)))
                    verts.append(tuple(point))
            faces=[tuple(reversed(range(8)))]
            for j in range(len(path)-1):
                for i in range(8):
                    faces.append((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i))
            faces.append(tuple((len(path)-1)*8+i for i in range(8)))
            parts.append(builder.mesh(label+"_"+side,verts,faces,bone,mat))

        # The lip roots stay attached to the head while their ends curl out.
        # Keep the established skeleton: the existing two mouth bones suffice.
        for obj in parts:
            head=obj.vertex_groups.new(name="head")
            mouth=obj.vertex_groups[bone]
            for v in obj.data.vertices:
                u=max(0,min(1,(v.co.x-(center-.025))/.095))
                weight=.12+.88*u*u*(3-2*u)
                mouth.add([v.index],weight,"REPLACE")
                head.add([v.index],1-weight,"REPLACE")


def build(kind):
    b=Builder(kind)
    if kind=="MirrorSkimmer":
        base(b,.075)
        b.pebble("hidden_chassis",(0,0,.065),(.35,.26,.07),"body",2,power=3)
        outline=rounded(.245,.19,4.5,24)
        b.loft("single_porcelain_mantle",outline,[(.082,.91,0),(.097,1,0),(.139,.97,0),(.153,.73,-.008)],
               "body",0,crown=True)
        b.pebble("small_scraper_head",(.224,0,.066),(.070,.095,.043),"head",1,power=3)
        mouths(b,.232,.018,.034,.021,.047)
        ls=legs(b,(.135,0,-.135),.142,.065,.071,.012)
        spec=dict(legs=ls,body_z=.075,length=.52,height=.155,walk_speed=.10,run_speed=.55,
                  stride=.042,lift=.013,depth=.24,role="peaceful_surface_grazer")
    elif kind=="Riftlip":
        base(b,.088)
        outline=[(-.29,0),(-.20,-.056),(.06,-.07),(.145,-.038),(.16,0),
                 (.145,.038),(.06,.07),(-.20,.056)]
        b.loft("tapered_spindle",outline,[(.056,.84,0),(.09,1,0),(.135,.79,-.008)],"body",0)
        for i,(x,sx,sy) in enumerate(((-.17,.14,.104),(-.06,.145,.13),(.045,.13,.14))):
            b.pebble("shallow_dorsal_fold_"+str(i),(x,0,.124),(sx,sy,.043),
                     "abdomen" if i==0 else "body",0,power=2.8,n=8)
        b.pebble("wedge_head_base",(.139,0,.084),(.112,.182,.066),"head",1,power=2.8,n=8)
        mouths(b,.16,.039,.145,.102,.079,lip=True)
        ls=legs(b,(.105,-.055,-.19),.052,.074,.080,.012)
        spec=dict(legs=ls,body_z=.088,length=.60,height=.155,walk_speed=.20,run_speed=.80,
                  stride=.065,lift=.025,depth=.28,role="alert_fissure_scavenger",
                  mouth_design="Deep V cleft, rolled lips, recessed seal, blended lip roots")
    elif kind=="SpireWorker":
        base(b,.115)
        b.pebble("pottery_thorax",(.035,0,.14),(.26,.23,.19),"body",0,power=2.5,n=12)
        b.pebble("compact_abdomen",(-.145,0,.115),(.23,.20,.145),"abdomen",0,power=2.6,n=12)
        b.pebble("abdomen_overlap_rim",(-.065,0,.132),(.065,.213,.144),"body",1,n=12)
        outline=rounded(.09,.091,2.8,12)
        b.loft("short_working_head",outline,[(.072,.9,.16),(.133,1,.15),(.183,.52,.13)],"head",0)
        mouths(b,.205,.033,.060,.036,.081)
        ls=legs(b,(.103,-.02,-.16),.085,.095,.10,.021,shovels=True)
        spec=dict(legs=ls,body_z=.115,length=.53,height=.24,walk_speed=.14,run_speed=.34,
                  stride=.055,lift=.024,depth=.33,role="colony_builder_and_carrier")
    elif kind=="SpireSoldier":
        base(b,.125)
        # A low armoured thorax, narrow waist and compact abdomen read as a fighter at gameplay scale.
        b.pebble("sealed_muscular_thorax",(-.018,0,.14),(.27,.265,.155),"body",2,power=2.6,n=10)
        b.pebble("overlapping_scapular_shield",(-.035,0,.193),(.255,.30,.125),"body",0,power=3,n=10)
        b.pebble("dorsal_keel",(-.048,0,.251),(.19,.07,.035),"body",3,power=3,n=8)
        b.pebble("protected_waist",(-.159,0,.127),(.075,.11,.085),"abdomen",2,n=10)
        b.pebble("compact_armoured_gaster",(-.225,0,.126),(.17,.185,.14),"abdomen",0,power=2.5,n=10)
        b.pebble("gaster_overlapping_band",(-.19,0,.158),(.05,.19,.096),"abdomen",1,n=10)
        outline=[(-.015,-.085),(.12,-.137),(.215,-.115),(.257,-.062),
                 (.257,.062),(.215,.115),(.12,.137),(-.015,.085)]
        b.loft("sloped_head_carapace",outline,[(.095,.83,0),(.15,1,0),(.23,.88,-.016),
                                               (.255,.58,-.03)],"head",0,crown=True)
        b.pebble("recessed_mouth_seal",(.245,0,.116),(.06,.14,.073),"head",2,power=3,n=8)
        for side,sign in (("L",1),("R",-1)):
            bone="mouth_"+side
            b.bone(bone,(.218,sign*.079,.115),(.39,sign*.032,.115),"head")
            # Clearly exposed hooked mandibles sit in FRONT of the shield, with a visible biting gap.
            jaw=[(.205,.071),(.242,.115),(.31,.109),(.391,.055),(.407,.018),
                 (.367,.028),(.310,.062),(.265,.064),(.234,.043)]
            jaw=[(x,sign*y) for x,y in jaw]
            if sign>0:jaw.reverse()
            b.loft("hooked_battle_mandible_"+side,jaw,[(.082,.93,0),(.113,1,0),(.143,.95,0)],bone,1)
            b.link("pale_cutting_edge_"+side,(.307,sign*.065,.127),(.395,sign*.025,.116),.014,.007,bone,3,5)
            for j,x in enumerate((.275,.313,.347)):
                y=.057-(x-.275)*.25
                tooth=[(x-.014,sign*y),(x+.014,sign*y),(x+.008,sign*(y-.024))]
                if sign<0:tooth.reverse()
                b.loft("interlocking_tooth_"+side+str(j),tooth,[(.099,1,0),(.124,.93,0)],bone,3)
            b.pebble("protected_sensory_slit_"+side,(.187,sign*.113,.192),(.06,.018,.024),"head",2,power=3,n=8)
            b.link("swept_cheek_ridge_"+side,(.052,sign*.115,.204),(.204,sign*.092,.219),.019,.011,"head",3,5)
        ls=legs(b,(.095,-.035,-.19),.105,.116,.145,.023)
        # Forelegs carry broad shields and hooked contact claws, unlike the worker's flat digging spades.
        for leg in ls:
            sign=1 if leg['label'][0]=='L' else -1
            front=leg['label'].endswith('1')
            h,k,f=map(Vector,(leg['h'],leg['k'],leg['f']))
            if front:
                b.pebble("forearm_buckler_"+leg['label'],tuple(h.lerp(k,.58)+Vector((.012,0,.020))),
                         (.13,.09,.065),leg['upper'],0,power=3,n=8)
                b.link("foreleg_hook_"+leg['label'],tuple(f),tuple(f+Vector((.073,-sign*.012,.002))),.017,.004,leg['tip'],3,5)
            else:
                b.pebble("haunch_plate_"+leg['label'],tuple(h.lerp(k,.35)),(.07,.067,.045),leg['upper'],0,n=8)
        spec=dict(legs=ls,body_z=.125,length=.72,height=.28,walk_speed=.14,run_speed=.62,
                  stride=.071,lift=.026,depth=.41,role="armoured_colony_biter",
                  mouth_design="Exposed hooked mandibles with inner teeth; closed pressure seal behind mouth")
    else:
        raise ValueError(kind)
    return b.finish(spec)


def build_all():
    return [build(kind) for kind in KINDS]
