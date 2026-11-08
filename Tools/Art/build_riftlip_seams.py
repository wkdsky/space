"""Build narrow, walk-safe Riftlip seam overlays in centimetres. Never edits terrain."""
import bpy, math, random, json
from pathlib import Path
OUT=Path('D:/projects/space/SourceArt/MoonSurfaceV3/RiftlipSeams')
OUT.mkdir(parents=True,exist_ok=True)
scene=bpy.data.scenes.new('RiftlipSeamReview')
bpy.context.window.scene=scene
stone=bpy.data.materials.new('RegolithEdge');stone.diffuse_color=(.23,.25,.24,1)
void=bpy.data.materials.new('SeamShadow');void.diffuse_color=(.003,.004,.005,1)
for variant in range(4):
    rng=random.Random(981+variant)
    vertices=[]; faces=[]; ids=[]
    length=260+variant*22
    count=21
    # Cross section: low rubble shoulder -> inner wall -> dark bottom -> opposing wall.
    sections=[]
    for i in range(count):
        t=i/(count-1);x=(t-.5)*length
        center=5*math.sin(t*math.pi*3+variant)+rng.uniform(-2,2)
        taper=math.sin(math.pi*t)**.55
        gap=(15+variant*1.4)*taper+.8
        outer=gap+12*taper
        height=1.5+(5+rng.uniform(-1,2))*taper
        row=[]
        for y,z in ((-outer,.15),(-gap,height),(-gap*.83,1.05),(gap*.83,1.05),(gap,height),(outer,.15)):
            row.append(len(vertices));vertices.append((x,center+y,z))
        sections.append(row)
    for i in range(count-1):
        for j in range(5):
            a,b=sections[i][j],sections[i+1][j]
            c,d=sections[i+1][j+1],sections[i][j+1]
            faces.extend(((a,b,c),(a,c,d)));ids.extend((1 if j in (1,2,3) else 0,)*2)
    # Small irregular shards stay below ankle height; the centre remains a single visible slit.
    for i in range(20):
        x=rng.uniform(-length*.43,length*.43);t=x/length+.5
        taper=math.sin(math.pi*t)**.55
        side=rng.choice((-1,1));y=side*((15+variant*1.4)*taper+rng.uniform(7,16))
        radius=rng.uniform(2,5);z=rng.uniform(2,6)
        k=len(vertices);vertices.extend(((x-radius,y-radius,.2),(x+radius,y-radius,.2),(x+radius*.6,y+radius,.2),(x-radius*.8,y+radius,.2),(x,y,z)))
        for a,b,c in ((0,1,4),(1,2,4),(2,3,4),(3,0,4)):
            faces.append((k+a,k+b,k+c));ids.append(0)
    name='SM_MS3_RiftRim_'+str(variant)
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vertices,[],faces);mesh.update()
    obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj);obj.location.y=variant*110
    mesh.materials.append(stone);mesh.materials.append(void)
    for poly,mid in zip(mesh.polygons,ids):poly.material_index=mid
    data=dict(vertices=vertices,triangles=faces,material_ids=ids,length_cm=length,max_gap_cm=(15+variant*1.4)*2)
    (OUT/(name+'.json')).write_text(json.dumps(data),encoding='utf-8')
print('RIFTLIP_SEAMS_BUILT '+str(OUT))
