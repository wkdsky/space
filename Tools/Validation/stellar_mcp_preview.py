"""Opt-in PIE-only weapon QA tools. Load using `py <this file>` in a rebuilt editor.

Registers with the engine's Unreal MCP ToolsetRegistry. Never saves assets or levels.
Equipment and energy changes apply only to the current preview session.
"""
import unreal
import json
from pathlib import Path
import toolset_registry
from toolset_registry.registration import Registration

CATALOG = "/Game/Space/Data/Weapons/DA_StellarWeaponCatalog"
_preview = None
_performance = unreal.get_default_object(unreal.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
_original_throttle = _performance.get_editor_property("bThrottleCPUWhenNotForeground")
_performance.set_editor_property("bThrottleCPUWhenNotForeground", False)

def player():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        raise RuntimeError("Start PIE on an expedition gameplay map first.")
    cls = unreal.load_class(None, "/Script/space.JTSCharacter")
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
    for actor in actors:
        if actor.is_player_controlled():
            return actor
    raise RuntimeError("PIE has no controlled JTS player.")

def component(actor, name):
    return actor.get_component_by_class(unreal.load_class(None, "/Script/space." + name))

@unreal.uclass()
class StellarPreviewTools(unreal.ToolsetDefinition):
    """PIE-only stellar weapon equipment, input and raised-cast visual inspection."""

    @toolset_registry.tool_call
    @staticmethod
    def equip_weapon(attachment_id: str, primary: bool, secondary: bool) -> str:
        """Equip an authored stellar pair and submit actual player input in PIE.

        Args:
            attachment_id: Catalog attachment, such as FreezingTube or ExplosionTube.
            primary: Hold primary input.
            secondary: Hold secondary input.
        """
        global _preview
        actor = player()
        catalog = unreal.load_asset(CATALOG)
        definition = next((d for d in catalog.weapons if str(d.attachment_id) == attachment_id), None)
        if not definition:
            raise ValueError("Attachment must exist in the authored weapon catalog.")
        weapon = component(actor, "JTSStellarWeaponComponent")
        weapon.set_primary(False); weapon.set_secondary(False)
        state = actor.get_editor_property("player_state")
        loadout = component(state, "JTSStellarLoadoutComponent")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        ships = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.load_class(None, "/Script/space.JTSSpacecraftActor"))
        if not ships:
            raise RuntimeError("Preview requires a map with the expedition spacecraft.")
        ship = ships[0]
        original = actor.get_actor_location()
        controller = actor.get_controller()
        actor.set_actor_location(ship.get_boarding_interaction_center(), False, False)
        try:
            locker = state.get_editor_property("ShipLockerSlots")
            needed = [definition.core_id, definition.attachment_id]
            if any(str(loadout.get_slot(i).stellar_item_id) != str(item_id)
                   and not any(str(s.stellar_item_id) == str(item_id) for s in locker)
                   for i,item_id in zip((1,2),needed)):
                controller.call_method("ServerRequestDebugStellarItems", args=(ship,))
            for index,item_id in zip((1,2),needed):
                if str(loadout.get_slot(index).stellar_item_id) == str(item_id):
                    continue
                locker = state.get_editor_property("ShipLockerSlots")
                entry_index = next(i for i,s in enumerate(locker) if str(s.stellar_item_id) == str(item_id))
                loadout.call_method("ServerExchangeLocker", args=(ship,entry_index,locker[entry_index].slot_token,index,loadout.get_slot(index).instance_id))
        finally:
            actor.set_actor_location(original + ship.get_actor_right_vector() * (1100 if _preview is None else 0), False, False)
        loadout.call_method("ServerSelectWeapon", args=(1,))
        _preview = actor
        weapon.set_primary(primary); weapon.set_secondary(secondary)
        return actor.get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def set_input(primary: bool, secondary: bool) -> bool:
        """Release or hold actual weapon input without replacing the equipped pair."""
        actor = player()
        weapon = component(actor, "JTSStellarWeaponComponent")
        weapon.set_primary(primary); weapon.set_secondary(secondary)
        return weapon.is_casting()

    @toolset_registry.tool_call
    @staticmethod
    def get_preview_state() -> str:
        """Report the controlled player, equipment and actual raised-cast state."""
        actor = player()
        weapon = component(actor, "JTSStellarWeaponComponent")
        loadout = component(actor.get_editor_property("player_state"), "JTSStellarLoadoutComponent")
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        wrist = mesh.get_socket_location("Wrist_R")
        upper = mesh.get_socket_location("LowerArm_R") - mesh.get_socket_location("UpperArm_R")
        lower = wrist - mesh.get_socket_location("LowerArm_R")
        alignment = unreal.MathLibrary.dot_vector_vector(upper, lower) / max(.001, upper.length() * lower.length())
        state = {"actor": actor.get_path_name(), "location": str(actor.get_actor_location()),
                 "casting": weapon.is_casting(), "energy": loadout.get_energy(),
                 "wrist_height": unreal.MathLibrary.dot_vector_vector(wrist - actor.get_actor_location(), actor.get_actor_up_vector()),
                 "arm_alignment": alignment, "bindings": str(loadout.get_weapons())}
        return json.dumps(state)

    @toolset_registry.tool_call
    @staticmethod
    def prepare_target(distance: float, health: float) -> str:
        """Spawn one stationary, transient enemy in the PIE aiming line for visual QA.

        Uses the native enemy damage receiver without registering settlement AI.
        Never creates or saves a level actor. Distance is limited to 120–1000 cm.
        """
        actor = player()
        if not 120 <= distance <= 1000 or not 1 <= health <= 1000000:
            raise ValueError("Use a bounded distance and positive test health.")
        view, rotation = actor.get_actor_eyes_view_point()
        transform = unreal.Transform(location=view + unreal.MathLibrary.get_forward_vector(rotation) * distance)
        gameplay = unreal.get_default_object(unreal.GameplayStatics)
        # These BlueprintInternalUseOnly UFunctions are callable through reflection,
        # but Unreal intentionally omits ordinary Python wrapper methods for them.
        target = gameplay.call_method("BeginDeferredActorSpawnFromClass", args=(
            actor, unreal.load_class(None, "/Script/space.JTSMoonCubeEnemy"), transform,
            unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None,
            unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        gameplay.call_method("FinishSpawningActor", args=(target, transform,
            unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        component(target, "JTSHealthComponent").set_max_health(health, True)
        body = next(c for c in target.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "CubeBody")
        body.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
        return target.get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def capture_preview(name: str) -> str:
        """Capture the actual PIE player's game viewport to Saved, without changing assets."""
        if not name.isalnum():
            raise ValueError("Capture name must be alphanumeric.")
        actor = player()
        path = str(Path(unreal.Paths.project_saved_dir()).resolve() / ("Stellar" + name + ".png"))
        unreal.SystemLibrary.execute_console_command(actor, "Shot SHOWUI filename=" + path.replace("\\", "/") + " -nosuffix", actor.get_controller())
        return path

def maintain_preview(delta):
    global _preview
    if _preview:
        try:
            if not unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
                _preview = None
                _performance.set_editor_property("bThrottleCPUWhenNotForeground", _original_throttle)
                return
        except Exception:
            pass

_registration = Registration([StellarPreviewTools])
_registration.register()
_tick = unreal.register_slate_post_tick_callback(maintain_preview)
unreal.log("STELLAR_MCP_PREVIEW_REGISTERED")
