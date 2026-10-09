"""Opt-in MCP tools for the level-authored crater-foot ant entrances."""
import json
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration

ROOT=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/MoonAntNests"
BP="/Game/Space/Blueprints/Planets/Moon/BP_MoonAntNestEntrance"
MESH_FOLDER="/Game/Space/Meshes/Creatures/MoonAnt"
VOID="/Game/Space/Materials/Moon/M_MoonAntNestVoid"

def moon_ant_level_actors():
    return {a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}

def vector(v):return [v.x,v.y,v.z]

@unreal.uclass()
class MoonAntNestAuthoringTools(unreal.ToolsetDefinition):
    """Create independent ant mouths on the recognized crater foot, reusing existing ant AI."""

    @toolset_registry.tool_call
    @staticmethod
    def author_crater_nests() -> str:
        """Replace the old corpse anchor with nine separate, fitted Moon_AntNest entrances."""
        editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        assert not editor.get_game_world(), "Stop PIE before authoring nests"
        assert editor.get_editor_world().get_path_name()=="/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld"
        state=moon_ant_level_actors();moon=state["MoonPlanet"].static_mesh_component
        plan=json.loads((ROOT/"moon_ant_nest_placements.json").read_text())
        asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
        material=unreal.load_asset(VOID)
        if not material:
            folder,name=VOID.rsplit("/",1)
            material=asset_tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
            material.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_UNLIT)
            material.set_editor_property("two_sided",True)
            color=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector,0,0)
            color.set_editor_property("constant",unreal.LinearColor(0.001,0.001,0.001,1))
            unreal.MaterialEditingLibrary.connect_material_property(color,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
            unreal.MaterialEditingLibrary.recompile_material(material)
            assert unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
        previous=unreal.SystemLibrary.get_console_variable_int_value("Interchange.FeatureFlags.Import.FBX")
        unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),"Interchange.FeatureFlags.Import.FBX 0")
        try:
            for item in plan["placements"]:
                options=unreal.FbxImportUI();options.automated_import_should_detect_type=False
                options.import_as_skeletal=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
                options.import_mesh=True;options.import_materials=False;options.import_textures=False
                options.static_mesh_import_data.auto_generate_collision=False
                options.static_mesh_import_data.combine_meshes=True
                task=unreal.AssetImportTask();task.filename=str(ROOT/(item["mesh"]+".fbx"))
                task.destination_path=MESH_FOLDER;task.destination_name=item["mesh"]
                task.automated=True;task.replace_existing=True;task.save=False;task.options=options
                asset_tools.import_asset_tasks([task])
                mesh=unreal.load_asset(MESH_FOLDER+"/"+item["mesh"])
                assert mesh and task.imported_object_paths
                slots=list(mesh.static_materials)
                for slot in slots:
                    slot.material_interface=material if str(slot.material_slot_name)=="M_AntNestVoid" else moon.get_material(0)
                mesh.set_editor_property("static_materials",slots)
                mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag",unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
        finally:
            unreal.SystemLibrary.execute_console_command(editor.get_editor_world(),"Interchange.FeatureFlags.Import.FBX "+str(previous))
        bp=unreal.load_asset(BP)
        if not bp:
            bp=unreal.EditorAssetLibrary.duplicate_asset("/Game/Space/Blueprints/Planets/Moon/BP_MoonAntNest",BP)
        assert bp
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property("base_moon_ant_nest_mesh_scale",unreal.Vector(1,1,1))
        cdo.set_editor_property("nest_tint",unreal.LinearColor(0,0,0,1))
        component=cdo.get_editor_property("nest_mesh")
        component.set_static_mesh(unreal.load_asset(MESH_FOLDER+"/"+plan["placements"][0]["mesh"]))
        component.set_relative_scale3d(unreal.Vector(1,1,1))
        component.set_editor_property("override_materials",[])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        placed=[];rows=[];transform=moon.get_world_transform()
        for item in plan["placements"]:
            point=transform.transform_location(unreal.Vector(*item["point_local_cm"]))
            axes=[transform.rotation.rotate_vector(unreal.Vector(*item[key])) for key in ("forward_local","right_local","up_local")]
            rotation=unreal.MathLibrary.make_rotation_from_axes(*axes)
            nest=state.get(item["name"])
            if not nest:nest=subsystem.spawn_actor_from_class(bp.generated_class(),point,rotation)
            assert isinstance(nest,unreal.JTSMoonAntNestActor)
            nest.modify();nest.set_actor_label(item["name"]);nest.set_folder_path("Planets/Moon/AntNests")
            nest.set_actor_location(point,False,False);nest.set_actor_rotation(rotation,False)
            visual=nest.get_editor_property("nest_mesh")
            visual.set_static_mesh(unreal.load_asset(MESH_FOLDER+"/"+item["mesh"]))
            visual.set_relative_scale3d(unreal.Vector(1,1,1));visual.set_editor_property("override_materials",[])
            placed.append(nest)
            rows.append({"name":item["name"],"actor":nest.get_path_name(),"location_cm":vector(point),
                         "rotation":{"pitch":rotation.pitch,"yaw":rotation.yaw,"roll":rotation.roll},
                         "mesh":visual.static_mesh.get_path_name()})
        controller=state["BP_MoonSurfaceController"]
        controller.modify();controller.set_editor_property("placed_moon_ant_nests",placed)
        controller.set_editor_property("corpse_surface_anchor",None)
        data=unreal.load_asset("/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay")
        data.modify();data.set_editor_property("moon_ant_nest_count",9)
        assert unreal.EditorAssetLibrary.save_loaded_asset(data,only_if_is_dirty=False)
        old=state.get("Moon_CorpseAnchor_A")
        if old:assert subsystem.destroy_actor(old)
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        result={"independent_nest_count":len(placed),"replaced_anchor":bool(old),"blueprint":BP,"nests":rows}
        (ROOT/"moon_ant_nests_authored.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def camera_transform(nest_index: int, overview: bool) -> str:
        """Return a local-gravity camera at an entrance or an overview of the complete foot ring."""
        state=moon_ant_level_actors();nest=state[f"Moon_AntNest_{nest_index:02}"]
        q=nest.get_actor_transform().rotation
        if overview:
            foot=json.loads((ROOT/"moon_crater_foot.json").read_text())
            moon=state["MoonPlanet"].static_mesh_component.get_world_transform()
            target=moon.transform_location(unreal.Vector(*foot["foot_center_local_cm"]))
            up=moon.rotation.rotate_vector(unreal.Vector(*foot["foot_normal"]))
            point=target+up*16000+nest.get_actor_forward_vector()*3500
        else:
            up=nest.get_actor_up_vector()
            point=nest.get_actor_location()+q.rotate_vector(unreal.Vector(180,125,140))
            target=nest.get_actor_location()+q.rotate_vector(unreal.Vector(-15,0,25))
        forward=unreal.MathLibrary.normal(target-point)
        right=unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(up,forward))
        rotation=unreal.MathLibrary.make_rotation_from_axes(forward,right,unreal.MathLibrary.cross_vector_vector(forward,right))
        return json.dumps({"location":{"x":point.x,"y":point.y,"z":point.z},
                          "rotation":{"pitch":rotation.pitch,"yaw":rotation.yaw,"roll":rotation.roll},
                          "scale":{"x":1,"y":1,"z":1}})

    @toolset_registry.tool_call
    @staticmethod
    def validate_pie() -> str:
        """Check that exactly nine placed nests survive initialization and spawn the existing ants."""
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        assert world
        nests=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonAntNestActor)
        assert len(nests)==9,len(nests)
        controller=next(c for c in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonSurfaceController)
                        if str(c.get_planet_id())=="Moon")
        assert controller.is_surface_gameplay_initialized()
        assert not controller.get_editor_property("corpse_surface_anchor")
        placed=list(controller.get_editor_property("placed_moon_ant_nests"))
        assert len(placed)==9 and set(placed)==set(nests)
        ants=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonAntActor)
        assert ants,"Wait for the nest spawn timers"
        for ant in ants:assert ant.get_owner() in nests
        original=json.loads((ROOT/"moon_ant_nests_authored.json").read_text())
        rows=[]
        for row in original["nests"]:
            name=row["actor"].rsplit(".",1)[-1]
            nest=next(n for n in nests if n.get_name()==name)
            assert (nest.get_actor_location()-unreal.Vector(*row["location_cm"])).length()<.1
            assert nest.get_editor_property("nest_mesh").get_editor_property("relative_scale3d")==unreal.Vector(1,1,1)
            rows.append({"name":row["name"],"active_ants":sum(a.get_owner()==nest for a in ants),
                         "transform_preserved":True,"replicates":nest.get_editor_property("replicates")})
        result={"nest_count":9,"live_ant_count":len(ants),"legacy_random_nests":0,"nests":rows}
        (ROOT/"moon_ant_nests_pie_validation.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        return json.dumps(result)

_ant_nest_registration=Registration([MoonAntNestAuthoringTools])
_ant_nest_registration.register()
unreal.log("MOON_ANT_NEST_TOOLS_REGISTERED")
