import bpy
import json
from mathutils import Vector
from pathlib import Path
root = Path("D:/projects/space/SourceArt/SkeletonAstronaut")
o = bpy.data.objects["RaeTheRedPanda"]
edges = {v.index: [] for v in o.data.vertices}
for e in o.data.edges:
    a,b = e.vertices
    edges[a].append(b); edges[b].append(a)
todo = set(edges)
parts = []
while todo:
    seed = min(todo); todo.remove(seed)
    island = {seed}; stack = [seed]
    while stack:
        for other in edges[stack.pop()]:
            if other in todo:
                todo.remove(other); island.add(other); stack.append(other)
    verts = [o.data.vertices[i] for i in island]
    coords = [o.matrix_world @ v.co for v in verts]
    weights = {}
    for v in verts:
        for g in v.groups:
            name = o.vertex_groups[g.group].name
            weights[name] = weights.get(name,0) + g.weight
    uvs = [o.data.uv_layers["UVMap"].data[l.index].uv for l in o.data.loops if l.vertex_index in island]
    parts.append({"id":seed,"count":len(island), "min":[min(c[i] for c in coords) for i in range(3)],"max":[max(c[i] for c in coords) for i in range(3)],"weights":sorted(weights.items(),key=lambda x:-x[1])[:4],"uv":[sum(v[i] for v in uvs)/len(uvs) for i in range(2)] if uvs else []})
(root / "suit_islands.json").write_text(json.dumps(parts,indent=2))
print(json.dumps(parts))
