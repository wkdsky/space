"""Opt-in Unreal MCP authoring and PIE checks for the new Moon corpse landmark."""
import json
from functools import wraps
from pathlib import Path
import unreal
import toolset_registry
from toolset_registry.registration import Registration

ROOT=Path(unreal.Paths.project_dir()).resolve()/"SourceArt/SkeletonAstronaut"
BLUEPRINT="/Game/Space/Blueprints/Planets/Moon/BP_MoonSkeletonAstronautCorpse"
MESH="/Game/Space/Characters/SkeletonAstronaut/SM_SkeletonAstronaut_Corpse"
SURFACE_CLEARANCE_CM=0.3

def legacy_fbx_import(function):
    """Keep FbxImportUI effective and restore the editor's importer preference."""
    @wraps(function)
    def wrapped(*args,**kwargs):
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        flag="Interchange.FeatureFlags.Import.FBX"
        previous=unreal.SystemLibrary.get_console_variable_int_value(flag)
        unreal.SystemLibrary.execute_console_command(world,flag+" 0")
        try:
            return function(*args,**kwargs)
        finally:
            unreal.SystemLibrary.execute_console_command(world,flag+" "+str(previous))
    return wrapped

def level_actors():
    return {a.get_actor_label():a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}

def vector(value):
    return [value.x,value.y,value.z]

def describe(corpse,anchor):
    mesh=corpse.get_editor_property("corpse_mesh")
    surface=anchor.get_surface_transform()
    up=surface.rotation.rotate_vector(unreal.Vector(0,0,1))
    delta=corpse.get_actor_location()-surface.translation
    return {"actor":corpse.get_path_name(),"class":corpse.get_class().get_path_name(),
            "location":vector(corpse.get_actor_location()),"surface_location":vector(surface.translation),
            "root_clearance_cm":unreal.MathLibrary.dot_vector_vector(delta,up),
            "alignment_dot":unreal.MathLibrary.dot_vector_vector(corpse.get_actor_up_vector(),up),
            "surface_placement":corpse.is_using_real_planet_surface_placement(),
            "replicates":corpse.get_editor_property("replicates"),
            "components":[c.get_name() for c in corpse.get_components_by_class(unreal.ActorComponent)],
            "mesh":mesh.static_mesh.get_path_name(),
            "materials":[mesh.get_material(i).get_path_name() for i in range(mesh.get_num_materials())]}

@unreal.uclass()
class MoonCorpseAuthoringTools(unreal.ToolsetDefinition):
    """Author the skeleton astronaut corpse and check its existing surface-controller path."""

    @toolset_registry.tool_call
    @staticmethod
    @legacy_fbx_import
    def reimport_corrected_models() -> str:
        """Reimport the corrected symmetric boots and settled corpse pose, preserving player skeleton and materials."""
        if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
            raise RuntimeError("Stop PIE before reimporting character models")
        skeletal_path="/Game/Space/Characters/SkeletonAstronaut/SK_SkeletonAstronaut"
        skeleton=unreal.load_asset(skeletal_path).skeleton
        imported=[]
        for path,skeletal in ((skeletal_path,True),(MESH,False)):
            existing=unreal.load_asset(path)
            options=unreal.FbxImportUI()
            options.set_editor_property("automated_import_should_detect_type",False)
            options.set_editor_property("import_as_skeletal",skeletal)
            options.set_editor_property("mesh_type_to_import",unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
            options.set_editor_property("import_mesh",True)
            options.set_editor_property("import_animations",False)
            options.set_editor_property("import_materials",False)
            options.set_editor_property("import_textures",False)
            if skeletal:
                options.set_editor_property("skeleton",skeleton)
                options.set_editor_property("create_physics_asset",False)
                if isinstance(existing.get_editor_property("asset_import_data"),unreal.FbxSkeletalMeshImportData):
                    options.set_editor_property("skeletal_mesh_import_data",existing.get_editor_property("asset_import_data"))
                options.skeletal_mesh_import_data.set_editor_property("update_skeleton_reference_pose",False)
            else:
                if isinstance(existing.get_editor_property("asset_import_data"),unreal.FbxStaticMeshImportData):
                    options.set_editor_property("static_mesh_import_data",existing.get_editor_property("asset_import_data"))
                options.static_mesh_import_data.set_editor_property("auto_generate_collision",False)
            task=unreal.AssetImportTask()
            task.filename=str(ROOT/(path.rsplit("/",1)[1]+".fbx"))
            task.destination_path=path.rsplit("/",1)[0]
            task.destination_name=path.rsplit("/",1)[1]
            task.automated=True;task.replace_existing=True;task.save=False;task.options=options
            unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            mesh=unreal.load_asset(path)
            assert mesh and path+"."+mesh.get_name() in task.imported_object_paths, list(task.imported_object_paths)
            slots=list(mesh.materials if skeletal else mesh.static_materials)
            for slot in slots:
                name=str(slot.material_slot_name).removeprefix("M_SA_")
                if name=="Socket":name="Graphite"
                material=unreal.load_asset("/Game/Space/Characters/SkeletonAstronaut/Materials/MI_SA_"+name)
                assert material, str(slot.material_slot_name)
                slot.material_interface=material
            mesh.set_editor_property("materials" if skeletal else "static_materials",slots)
            if skeletal:assert mesh.skeleton==skeleton
            assert unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
            imported.append(mesh.get_path_name())
        return json.dumps({"imported":imported,"shared_skeleton":skeleton.get_path_name()})

    @toolset_registry.tool_call
    @staticmethod
    def fix_grounding() -> str:
        """Apply the settled corpse model and a small surface clearance before moving the landmark."""
        actors=level_actors();anchor=actors["Moon_CorpseAnchor_A"]
        corpse=actors["Moon_Corpse_SkeletonAstronaut"]
        bp=unreal.load_asset(BLUEPRINT)
        unreal.get_default_object(bp.generated_class()).set_editor_property("planet_surface_clearance",SURFACE_CLEARANCE_CM)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        corpse.modify()
        corpse.get_editor_property("corpse_mesh").set_static_mesh(unreal.load_asset(MESH))
        assert corpse.snap_to_planet_surface_anchor(anchor)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
        return json.dumps(describe(corpse,anchor))

    @toolset_registry.tool_call
    @staticmethod
    def center_corpse_and_nests() -> str:
        """Center the landmark on the fitted crater and reuse the nest generator within six metres of that same center."""
        actors=level_actors();anchor=actors["Moon_CorpseAnchor_A"]
        corpse=actors["Moon_Corpse_SkeletonAstronaut"]
        controller=actors["BP_MoonSurfaceController"]
        fit=json.loads((ROOT/"moon_crater_fit.json").read_text())
        component=actors["MoonPlanet"].static_mesh_component
        center=component.get_world_transform().transform_location(unreal.Vector(*[c*100 for c in fit["center_surface_local_m"]]))
        anchor.modify();anchor.set_editor_property("surface_clearance",0.0)
        anchor.set_actor_location(center,False,False)
        assert anchor.snap_to_planet_surface()
        corpse.modify();assert corpse.snap_to_planet_surface_anchor(anchor)
        controller.modify();controller.set_editor_property("corpse_surface_anchor",anchor)
        controller.set_editor_property("placed_corpse_landmark",corpse)
        data=controller.get_editor_property("moon_gameplay_data") or unreal.load_asset("/Game/Space/Data/Planets/Moon/PDA_MoonSurfaceGameplay")
        assert data
        data.modify()
        data.set_editor_property("moon_ant_nest_outer_radius_around_corpse",600.0)
        assert unreal.EditorAssetLibrary.save_loaded_asset(data,only_if_is_dirty=False)
        result=describe(corpse,anchor)
        result.update({"crater_center_world":vector(center),"nest_data":data.get_path_name(),
                       "nest_outer_radius_cm":600.0,"nest_count":data.get_editor_property("moon_ant_nest_count")})
        (ROOT/"moon_corpse_corrected_layout.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def camera_transform(close_up: bool) -> str:
        """Return a camera pose looking at the corpse, or at the whole enclosing crater."""
        actors=level_actors();corpse=actors["Moon_Corpse_SkeletonAstronaut"]
        offset=unreal.Vector(250,-280,340) if close_up else unreal.Vector(0,-5500,19500)
        camera=corpse.get_actor_location()+corpse.get_actor_transform().rotation.rotate_vector(offset)
        target=corpse.get_actor_location()+corpse.get_actor_up_vector()*25
        forward=unreal.MathLibrary.normal(target-camera)
        right=unreal.MathLibrary.normal(unreal.MathLibrary.cross_vector_vector(corpse.get_actor_up_vector(),forward))
        up=unreal.MathLibrary.cross_vector_vector(forward,right)
        rotation=unreal.MathLibrary.make_rotation_from_axes(forward,right,up)
        return json.dumps({"location":{"x":camera.x,"y":camera.y,"z":camera.z},
                           "rotation":{"pitch":rotation.pitch,"yaw":rotation.yaw,"roll":rotation.roll},
                           "scale":{"x":1,"y":1,"z":1}})

    @toolset_registry.tool_call
    @staticmethod
    def configure_scene() -> str:
        """Configure this project's Moon corpse Blueprint and a single placed landmark in L_SpaceWorld."""
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if world.get_path_name()!="/Game/Space/Maps/L_SpaceWorld.L_SpaceWorld":
            raise RuntimeError("Open L_SpaceWorld before authoring this landmark")
        mesh=unreal.load_asset(MESH)
        if not mesh:raise RuntimeError("Import the complete posed corpse mesh first")
        bp=unreal.load_asset(BLUEPRINT)
        if not bp:
            factory=unreal.BlueprintFactory()
            factory.set_editor_property("parent_class",unreal.JTSMoonCorpseActor)
            bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                "BP_MoonSkeletonAstronautCorpse","/Game/Space/Blueprints/Planets/Moon",unreal.Blueprint,factory)
        cdo=unreal.get_default_object(bp.generated_class())
        cdo.get_editor_property("corpse_mesh").set_static_mesh(mesh)
        cdo.set_editor_property("planet_surface_clearance",SURFACE_CLEARANCE_CM)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        controller_bp=unreal.load_asset("/Game/Space/Blueprints/Planets/Moon/BP_MoonSurfaceController")
        unreal.get_default_object(controller_bp.generated_class()).set_editor_property("moon_corpse_class",bp.generated_class())
        unreal.BlueprintEditorLibrary.compile_blueprint(controller_bp)
        actors=level_actors();anchor=actors["Moon_CorpseAnchor_A"]
        controller=actors["BP_MoonSurfaceController"]
        candidates=[a for a in actors.values() if isinstance(a,unreal.JTSMoonCorpseActor)]
        # The requested replacement removes any old human corpse instances, preserving the anchor.
        subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        corpse=next((a for a in candidates if a.get_class()==bp.generated_class()),None)
        removed=[]
        for old in candidates:
            if old!=corpse:
                removed.append(old.get_path_name())
                if not subsystem.destroy_actor(old):raise RuntimeError("Could not remove old corpse")
        if not corpse:
            corpse=subsystem.spawn_actor_from_class(bp.generated_class(),anchor.get_actor_location(),anchor.get_actor_rotation())
        corpse.set_actor_label("Moon_Corpse_SkeletonAstronaut")
        corpse.set_folder_path("Planets/Moon/Landmarks")
        assert corpse.snap_to_planet_surface_anchor(anchor)
        controller.modify()
        controller.set_editor_property("placed_corpse_landmark",corpse)
        result=describe(corpse,anchor);result["removed_old_actors"]=removed
        (ROOT/"moon_corpse_authored.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def validate_pie() -> str:
        """Verify the server reuses one placed corpse on repeated requests, with no legacy primitives."""
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if not world:raise RuntimeError("Start PIE first")
        controllers=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonSurfaceController)
        controller=next(c for c in controllers if str(c.get_planet_id())=="Moon")
        anchor=controller.get_editor_property("corpse_surface_anchor")
        placed=controller.get_editor_property("placed_corpse_landmark")
        first=controller.spawn_configured_corpse_at_planet_surface_anchor()
        assert first and first==placed
        for _ in range(3):assert controller.spawn_configured_corpse_at_planet_surface_anchor()==first
        corpses=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSMoonCorpseActor)
        assert len(corpses)==1
        result=describe(first,anchor)
        assert result["mesh"].startswith(MESH+".")
        assert result["components"]==["SceneRoot","CorpseMesh"] or set(result["components"])=={"SceneRoot","CorpseMesh"}
        assert abs(result["root_clearance_cm"]-SURFACE_CLEARANCE_CM)<0.1
        assert result["alignment_dot"]>0.99999 and result["replicates"]
        result.update({"corpse_count":len(corpses),"repeated_requests_reused_placed_actor":True,
                       "controller_landmark_reference_matches":first==placed})
        (ROOT/"moon_corpse_pie_validation.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        return json.dumps(result)

_registration=Registration([MoonCorpseAuthoringTools])
_registration.register()
unreal.log("MOON_CORPSE_MCP_REGISTERED")
