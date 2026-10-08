"""Author local enemy status layers and downward hail. Run in the rebuilt UE editor.

Only saves the status/ice/flame assets, Freezing FX Blueprint and stellar catalog.
"""
from pathlib import Path
import math
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ART = ROOT / "SourceArt" / "StellarEffects"
ART.mkdir(parents=True, exist_ok=True)
BASE = "/Game/Space"
MAT = BASE + "/Materials/Weapons/StellarFX"
MESH = BASE + "/Meshes/Weapons/StellarFX"
FX = BASE + "/Blueprints/Weapons/Stellar"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary

def source_mesh(name, flame=False):
    vertices, uvs, faces = [], [], []
    if flame:
        # Crossed ribbons remain visible from every direction; the material cuts soft fire tongues.
        for plane in range(3):
            angle = plane * math.pi / 3
            start = len(vertices)
            for j in range(9):
                height = j / 8
                for side in (-1, 1):
                    vertices.append((side * 50 * math.cos(angle), side * 50 * math.sin(angle), height * 100 - 50))
                    uvs.append(((side + 1) / 2, height))
            for j in range(8):
                a = start + j * 2
                faces += [(a, a + 1, a + 2), (a + 1, a + 3, a + 2)]
    else:
        segments = 8
        rings = [(0, 0), (.25, 1), (1, 0)]
        for height, radius in rings:
            for i in range(segments + 1):
                angle = math.tau * i / segments
                vertices.append((50 * radius * math.cos(angle), 50 * radius * math.sin(angle), height * 100 - 50))
                uvs.append((i / segments, height))
        for j in range(len(rings) - 1):
            for i in range(segments):
                a = j * (segments + 1) + i
                faces += [(a, a + 1, a + segments + 1), (a + 1, a + segments + 2, a + segments + 1)]
    lines = ["# Faceted ice / soft flame ribbons; unit width 100cm, height 100cm"]
    lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in vertices]
    lines += [f"vt {u:.6f} {v:.6f}" for u, v in uvs]
    lines += ["f " + " ".join(f"{i+1}/{i+1}" for i in face) for face in faces]
    path = ART / (name + ".obj")
    path.write_text("\n".join(lines) + "\n", encoding="ascii")
    task = unreal.AssetImportTask()
    task.filename = str(path); task.destination_path = MESH; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.factory = unreal.FbxFactory()
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False; options.import_mesh = True
    options.import_as_skeletal = False; options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.original_import_type = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials = False; options.import_textures = False
    options.static_mesh_import_data.combine_meshes = True; options.static_mesh_import_data.convert_scene = False
    options.static_mesh_import_data.auto_generate_collision = False
    task.options = options; TOOLS.import_asset_tasks([task])
    return unreal.load_asset(MESH + "/" + name)

ice_mesh = source_mesh("SM_StellarStatusIce")
flame_mesh = source_mesh("SM_StellarStatusFlame", True)

def asset(name, folder, cls, factory):
    return unreal.load_asset(folder + "/" + name) or TOOLS.create_asset(name, folder, cls, factory)

def material(name, inputs, code, opaque=False):
    mat = asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE if opaque else unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    shader = MEL.create_material_expression(mat, unreal.MaterialExpressionCustom)
    shader.set_editor_property("code", code)
    shader.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom_inputs = []
    for key in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", key)
        custom_inputs.append(entry)
    shader.set_editor_property("inputs", custom_inputs)
    for key, value in inputs.items():
        if value == "world":
            node = MEL.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
        elif value == "uv":
            node = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
        elif value == "time":
            node = MEL.create_material_expression(mat, unreal.MaterialExpressionTime)
        elif isinstance(value, tuple):
            node = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
            node.set_editor_property("parameter_name", key)
            node.set_editor_property("default_value", unreal.LinearColor(*value, 1))
        else:
            node = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter)
            node.set_editor_property("parameter_name", key); node.set_editor_property("default_value", value)
        assert MEL.connect_material_expressions(node, "", shader, key)
    color = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    color.set_editor_property("r", True); color.set_editor_property("g", True); color.set_editor_property("b", True)
    assert MEL.connect_material_expressions(shader, "", color, MEL.get_material_expression_input_names(color)[0])
    assert MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if not opaque:
        opacity = MEL.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
        opacity.set_editor_property("r", False); opacity.set_editor_property("g", False); opacity.set_editor_property("b", False); opacity.set_editor_property("a", True)
        assert MEL.connect_material_expressions(shader, "", opacity, MEL.get_material_expression_input_names(opacity)[0])
        assert MEL.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.layout_material_expressions(mat)
    errors = MEL.recompile_material(mat)
    if errors: raise RuntimeError(f"{name}: {errors}")
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat

ice = material("M_StellarStatusIce", {"Color": (.22, .65, 1), "UV": "uv"}, """
float facet = .55 + .45 * abs(sin(UV.x * 25.1327));
float cap = pow(saturate(UV.y), 3);
return float4(Color * facet * 1.2 + float3(.4,.6,.65) * cap, 1);
""", True)
flame = material("M_StellarStatusFlame", {"Color": (1, .16, .01), "UV": "uv", "T": "time"}, """
// OBJ import flips the vertical texture coordinate: y rises from the flame base.
float y = 1-UV.y;
float sway = sin(y * 11 - T * 7) * .06 * y;
float width = .43 * pow(saturate(1-y), .7);
float noise = sin(y*29-T*13+UV.x*17) * sin(UV.x*23+T*8) * .045;
float distance = abs(UV.x-.5+sway);
float edge = saturate((width-distance+noise)*15);
float hot = saturate(1-distance/max(.01,width)) * pow(saturate(1-y),.8);
float3 rgb = lerp(Color * 1.5, float3(2,1.25,.16), hot);
float alpha = edge * (1-smoothstep(.86,1,y)) * smoothstep(0,.08,y) * .65;
return float4(rgb, alpha);
""")
stains = material("M_StellarStatusBodyTint", {
    "P": "world", "BodyCenter": (0,0,0), "BodyX": (1,0,0), "BodyY": (0,1,0), "BodyZ": (0,0,1), "BodyExtent": (45,45,45),
    "Frozen": 0., "Burning": 0., "Poisoned": 0., "IceColor": (.28,.72,1), "FireColor": (1,.19,.015), "PoisonColor": (.22,.8,.035)
}, """
float3 delta = P - BodyCenter;
float3 p = float3(dot(delta,BodyX),dot(delta,BodyY),dot(delta,BodyZ)) / max(BodyExtent,float3(1,1,1));
float noise = sin(p.x*19+p.z*13)*sin(p.y*17-p.z*23) * .06;
float d = min(length(p-float3(.38,-1,.38)),length(p-float3(-.4,-1,-.27)));
d = min(d,length(p-float3(-.42,-1,.5)));
d = min(d,length(p-float3(-.3,1,.4)));
d = min(d,length(p-float3(.4,1,-.25)));
d = min(d,length(p-float3(.46,1,.5)));
d = min(d,length(p-float3(-1,-.4,.38)));
d = min(d,length(p-float3(-1,.4,-.27)));
d = min(d,length(p-float3(-1,.42,.5)));
d = min(d,length(p-float3(1,-.4,.38)));
d = min(d,length(p-float3(1,.4,-.27)));
d = min(d,length(p-float3(1,.42,.5)));
float poison = (1-smoothstep(.22+noise,.34+noise,d)) * Poisoned;
float rim = smoothstep(.1,.26,d);
float3 poisonRGB = lerp(PoisonColor*.11,PoisonColor*1.1+float3(.12,.1,0),rim);
float3 a = abs(p);
float edge = max(min(a.x,a.y),max(min(a.x,a.z),min(a.y,a.z)));
float frost = smoothstep(.64+noise,.86,edge) * Frozen;
float burn = smoothstep(.65,.97,p.z) * Burning;
float total = max(.001,poison+frost+burn);
float3 rgb = (poisonRGB*poison + IceColor*1.05*frost + FireColor*.85*burn) / total;
return float4(rgb,saturate(poison*.94+frost*.78+burn*.5));
""")

parent = unreal.load_class(None, "/Script/space.JTSStellarStatusEffectActor")
factory = unreal.BlueprintFactory(); factory.set_editor_property("parent_class", parent)
bp = asset("BP_StellarTargetStatusFX", FX, unreal.Blueprint, factory)
status_cls = unreal.load_class(None, FX + "/BP_StellarTargetStatusFX.BP_StellarTargetStatusFX_C")
cdo = unreal.get_default_object(status_cls)
for component, mesh, mat in ((cdo.ice_shards, ice_mesh, ice), (cdo.flames, flame_mesh, flame), (cdo.embers, unreal.load_asset("/Engine/BasicShapes/Sphere"), flame)):
    component.set_static_mesh(mesh); component.set_material(0, mat); component.set_visibility(False)
    component.set_cast_shadow(False)
cdo.set_editor_property("body_tint_material", stains)
cdo.set_editor_property("head_socket", "head")
cdo.set_editor_property("ice_sockets", ["hand_l", "hand_r", "foot_l", "foot_r"])
unreal.BlueprintEditorLibrary.compile_blueprint(bp); unreal.EditorAssetLibrary.save_loaded_asset(bp)

hail_bp = unreal.load_asset(FX + "/BP_StellarFreezingFX")
unreal.BlueprintEditorLibrary.compile_blueprint(hail_bp)
hail_bp.modify()
hail = unreal.get_default_object(unreal.load_class(None, FX + "/BP_StellarFreezingFX.BP_StellarFreezingFX_C"))
for component, mesh, mat in (
    (hail.hailstones, ice_mesh, ice),
    (hail.hail_trails, unreal.load_asset("/Engine/BasicShapes/Cylinder"), unreal.load_asset(MAT + "/M_StellarAbilityBeam")),
    (hail.hail_impacts, unreal.load_asset(MESH + "/SM_StellarAbilityDisc"), unreal.load_asset(MAT + "/M_StellarAbilitySurface"))):
    component.modify()
    component.set_static_mesh(mesh); component.set_material(0, mat); component.set_visibility(False)
hail.modify()
hail.set_editor_property("hail_height", 900.)
hail.set_editor_property("hail_fall_speed", 1400.)
hail.set_editor_property("hail_count", 24)
unreal.EditorAssetLibrary.save_loaded_asset(hail_bp)

catalog = unreal.load_asset(BASE + "/Data/Weapons/DA_StellarWeaponCatalog")
weapons = list(catalog.weapons)
for definition in weapons: definition.set_editor_property("target_status_effect_class", status_cls)
catalog.set_editor_property("weapons", weapons); unreal.EditorAssetLibrary.save_loaded_asset(catalog)
for mesh in (ice_mesh, flame_mesh): unreal.EditorAssetLibrary.save_loaded_asset(mesh)
print("STELLAR_STATUS_AND_DOWNWARD_HAIL_CONFIGURED", len(weapons))
