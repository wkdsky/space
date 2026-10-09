"""Recognize connected low-poly crater rings from the current authored marker."""
import json
from pathlib import Path
import numpy as np

root=Path("D:/projects/space/SourceArt/MoonAntNests")
source=json.loads((root/"moon_crater_source.json").read_text())
ids=sorted(map(int,source["vertices_local_cm"]))
points=np.array([source["vertices_local_cm"][str(i)] for i in ids])
axis=np.array(source["anchor_local_cm"]);axis/=np.linalg.norm(axis)
radius=np.linalg.norm(points,axis=1)
height=points@axis
tangent=np.linalg.norm(points-height[:,None]*axis,axis=1)
selected=height/radius>0.65
bands=np.unique(np.round(radius[selected],2))
gaps=np.diff(bands)
floor_gap=int(np.argmax(gaps))
floor_cut=float((bands[floor_gap]+bands[floor_gap+1])/2)
rim_gap=floor_gap+1+int(np.argmax(gaps[floor_gap+1:]))
rim_cut=float((bands[rim_gap]+bands[rim_gap+1])/2)
from collections import defaultdict,deque
lookup={i:p for i,p in zip(ids,points)}
triangles=source["triangles"]
floor={i for i,t in enumerate(triangles) if all(np.linalg.norm(lookup[v])<floor_cut for v in t)}
edge_faces=defaultdict(list)
for i,t in enumerate(triangles):
    for j in range(3):edge_faces[tuple(sorted((t[j],t[(j+1)%3])))].append(i)
neighbors=defaultdict(set)
for fs in edge_faces.values():
    if len(fs)==2:
        a,b=fs
        if a in floor and b in floor:neighbors[a].add(b);neighbors[b].add(a)
components=[]
while floor:
    seed=floor.pop();component={seed};queue=[seed]
    while queue:
        for n in neighbors[queue.pop()]:
            if n in floor:floor.remove(n);component.add(n);queue.append(n)
    components.append(component)
chosen=min(components,key=lambda fs:min(np.linalg.norm(sum((lookup[j] for j in triangles[i]))/3-np.array(source["anchor_local_cm"])) for i in fs))
boundary=[(edge,next(f for f in fs if f in chosen),next(f for f in fs if f not in chosen))
          for edge,fs in edge_faces.items() if len(fs)==2 and sum(f in chosen for f in fs)==1]
links=defaultdict(list)
for (a,b),_,_ in boundary:links[a].append(b);links[b].append(a)
assert all(len(link)==2 for link in links.values()),dict(links)
chain=[min(links)];previous=None
while True:
    current=chain[-1];nxt=next(n for n in links[current] if n!=previous)
    if nxt==chain[0]:break
    chain.append(nxt);previous=current
assert len(chain)==len(links)
ring=np.array([lookup[i] for i in chain]);origin=ring.mean(axis=0)
_,_,basis=np.linalg.svd(ring-origin)
normal=basis[2]
if normal@origin<0:normal=-normal
uv=(ring-origin)@basis[:2].T
solution=np.linalg.lstsq(np.column_stack([2*uv,np.ones(len(uv))]),np.sum(uv*uv,axis=1),rcond=None)[0]
center=origin+basis[:2].T@solution[:2]
foot_radius=float(np.sqrt(solution[2]+sum(solution[:2]**2)))
def face_normal(index):
    a,b,c=(lookup[j] for j in triangles[index]);n=np.cross(b-a,c-a);n/=np.linalg.norm(n)
    return n if n@((a+b+c)/3)>0 else -n
edges=[]
for edge,floor_face,wall_face in boundary:
    nf=face_normal(floor_face);nw=face_normal(wall_face)
    edges.append({"vertices":list(edge),"floor_triangle":floor_face,"wall_triangle":wall_face,
                  "floor_normal":nf.tolist(),"wall_normal":nw.tolist(),
                  "slope_break_degrees":float(np.degrees(np.arccos(np.clip(nf@nw,-1,1))))})
report={"floor_triangle_count":len(chosen),"foot_vertex_ids":chain,"foot_vertices_local_cm":ring.tolist(),
        "foot_center_local_cm":center.tolist(),"foot_normal":normal.tolist(),"foot_radius_local_cm":foot_radius,
        "foot_radius_world_m":foot_radius*source["moon_transform"]["scale"][0]/100,
        "foot_plane_max_error_local_cm":float(np.max(np.abs((ring-origin)@normal))),"edges":edges,
        "detected_floor_radius_cut_cm":floor_cut,"detected_rim_radius_cut_cm":rim_cut}
enclosed={i for i,t in enumerate(triangles) if all(np.linalg.norm(lookup[v])<rim_cut for v in t)}
region=set(chosen);queue=list(chosen)
adjacency=defaultdict(set)
for fs in edge_faces.values():
    if len(fs)==2:
        a,b=fs;adjacency[a].add(b);adjacency[b].add(a)
while queue:
    for n in adjacency[queue.pop()]:
        if n in enclosed and n not in region:region.add(n);queue.append(n)
rim_edges=[list(edge) for edge,fs in edge_faces.items() if len(fs)==2 and sum(f in region for f in fs)==1]
rim_ids=sorted({i for edge in rim_edges for i in edge})
report.update({"rim_edges":rim_edges,"rim_vertices_local_cm":[lookup[i].tolist() for i in rim_ids]})
(root/"moon_crater_foot.json").write_text(json.dumps(report,indent=2))
print("RECOGNIZED_CRATER_FOOT",{k:v for k,v in report.items() if k not in ("foot_vertices_local_cm","edges")})
print("SLOPE_BREAKS",[round(e["slope_break_degrees"],2) for e in edges])
