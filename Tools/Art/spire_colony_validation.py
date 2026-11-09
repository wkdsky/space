"""Disposable server/client PIE probes for Spire patrols, provocation, pursuit and finite recovery."""
import json
import unreal
from pathlib import Path
from pickaxe_combat_validation import spawn, capture

GS=unreal.GameplayStatics
OUT=Path('D:/projects/space/Saved/SpireColony')
OUT.mkdir(parents=True,exist_ok=True)
state={}


def worlds(): return unreal.EditorLevelLibrary.get_pie_worlds(True)
def actors(w,cls): return GS.get_all_actors_of_class(w,cls)
def server(): return next(w for w in worlds() if any(a.has_authority() for a in actors(w,unreal.JTSPlanetColonyNest)))
def colony(w=None):
    return min(actors(w or server(),unreal.JTSPlanetColonyNest),key=lambda a:a.get_actor_label())
def residents(nest,kind=None):
    habitats=[nest.get_editor_property(key) for key in (('workers','guards') if not kind else (kind,))]
    return [a for a in actors(nest.get_world(),unreal.JTSPlanetSurfaceCreature) if a.get_editor_property('habitat') in habitats
            and not a.get_component_by_class(unreal.JTSHealthComponent).is_dead]


def summary():
    rows=[]
    for w in worlds():
        nests=[]
        for nest in actors(w,unreal.JTSPlanetColonyNest):
            c=nest.get_editor_property('colony_defense')
            bugs=residents(nest)
            visible=[a for a in bugs if a.get_editor_property('mesh').is_visible()]
            nests.append(dict(name=nest.get_actor_label(),authority=nest.has_authority(),
                workers=c.get_living_workers(),soldiers=c.get_living_soldiers(),
                outside_workers=c.get_outside_workers(),outside_soldiers=c.get_outside_soldiers(),
                visible_workers=sum(a.get_class().get_name().startswith('BP_SpireWorker') for a in visible),
                visible_soldiers=sum(a.get_class().get_name().startswith('BP_SpireSoldier') for a in visible),
                health=nest.get_editor_property('health').get_health()))
        pawns=actors(w,unreal.JTSCharacter)
        rows.append(dict(world=w.get_path_name(),nests=nests,players=[dict(name=p.get_name(),health=p.get_component_by_class(unreal.JTSHealthComponent).get_health()) for p in pawns]))
    return rows


def position(distance):
    w=server();nest=colony();p=nest.get_editor_property('planet')
    pc=next(c for c in actors(w,unreal.PlayerController) if c.has_authority())
    pawn=pc.get_controlled_pawn()
    if not isinstance(pawn,unreal.JTSCharacter):
        cls=unreal.load_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2').generated_class()
        pawn=spawn(w,cls,nest.get_actor_location()+nest.get_actor_up_vector()*500,nest.get_actor_forward_vector(),nest.get_actor_up_vector())
        pc.possess(pawn)
    ground=p.project_point_to_surface(nest.get_actor_location()+nest.get_actor_forward_vector()*distance)
    up=p.get_radial_up_vector(ground.impact_point)
    pawn.character_movement.disable_movement()
    pawn.set_actor_location(ground.impact_point+up*pawn.capsule_component.get_scaled_capsule_half_height(),False,True)
    pawn.set_actor_rotation(p.make_surface_aligned_rotation(ground.impact_point,-nest.get_actor_forward_vector()),False)
    pc.set_control_rotation(p.make_surface_aligned_rotation(ground.impact_point,-nest.get_actor_forward_vector()))
    state['pawn']=pawn
    return pawn


def run(stage):
    if stage=='setup_multiplayer':
        w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        state['play_settings']={k:settings.get_editor_property(k) for k in ('PlayNumberOfClients','RunUnderOneProcess','AdditionalServerGameOptions')}
        state['mode']=w.get_world_settings().get_editor_property('default_game_mode')
        settings.set_editor_property('PlayNumberOfClients',2);settings.set_editor_property('RunUnderOneProcess',True)
        settings.set_editor_property('AdditionalServerGameOptions','?game=/Script/space.JTSGameplayGameModeBase')
        w.get_world_settings().set_editor_property('default_game_mode',unreal.JTSGameplayGameModeBase)
        gm=unreal.get_default_object(unreal.JTSGameplayGameModeBase)
        state['build_check']=gm.get_editor_property('require_matching_build_version')
        gm.set_editor_property('require_matching_build_version',False)
        return dict(two_player_test_ready=True)
    if stage=='restore_multiplayer':
        w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
        for k,v in state['play_settings'].items():settings.set_editor_property(k,v)
        w.get_world_settings().set_editor_property('default_game_mode',state['mode'])
        unreal.get_default_object(unreal.JTSGameplayGameModeBase).set_editor_property('require_matching_build_version',state['build_check'])
        return dict(restored=True)
    if stage=='summary':return summary()
    if stage=='attack':
        pawn=position(150);hp=pawn.get_component_by_class(unreal.JTSHealthComponent);hp.set_max_health(10000,True)
        nest=colony();before=nest.get_editor_property('health').get_health()
        GS.apply_damage(nest,10000,pawn.get_controller(),pawn,None)
        assert nest.get_editor_property('health').get_health()==before
        position(950)
        state['health_before']=hp.get_health()
        return dict(attacked=True,nest_health=before,player_health=hp.get_health())
    if stage=='pursuit':
        pawn=state['pawn'];nest=colony();p=nest.get_editor_property('planet')
        point=p.project_point_to_surface(pawn.get_actor_location()).impact_point
        bugs=residents(nest,'guards')
        return dict(player_health=pawn.get_component_by_class(unreal.JTSHealthComponent).get_health(),before=state['health_before'],
                    soldiers=[dict(name=b.get_name(),distance=(b.get_actor_location()-point).length(),state=str(b.get_editor_property('state')),
                                   visible=b.get_editor_property('mesh').is_visible()) for b in bugs],summary=summary())
    if stage=='leave':
        position(3500);return dict(left=True)
    if stage=='deplete':
        nest=colony();bugs=residents(nest,'guards')
        for bug in bugs:bug.get_component_by_class(unreal.JTSHealthComponent).apply_damage(100000,None,None,True)
        pawn=state['pawn']
        for i in range(20):GS.apply_damage(nest,20,pawn.get_controller(),pawn,None)
        return dict(killed=len(bugs),after=summary())
    if stage=='accelerate_recovery':
        nest=colony()
        for key in ('workers','guards'):
            habitat=nest.get_editor_property(key);r=habitat.get_editor_property('recovery')
            for k,v in dict(delay=2,duration=12,batch_size=3,minimum_player_distance=0).items():r.set_editor_property(k,v)
            habitat.set_editor_property('recovery',r)
        return dict(accelerated=True)
    if stage=='client_authority':
        client=next(w for w in worlds() if not GS.get_game_mode(w));nest=colony(client)
        before=nest.get_editor_property('health').get_health();c=nest.get_editor_property('colony_defense')
        pawn=GS.get_player_pawn(client,0);c.raise_alarm(pawn)
        removed=nest.get_editor_property('health').apply_damage(10000,None,pawn,True)
        assert removed==0 and nest.get_editor_property('health').get_health()==before
        assert c.get_living_soldiers()==0
        return dict(client_mutations_rejected=True)
    if stage=='capture':
        nest=colony();w=server();p=nest.get_editor_property('planet');point=nest.get_actor_location()
        up=p.get_radial_up_vector(point);forward=nest.get_actor_forward_vector();right=up.cross(forward)
        lamp=spawn(w,unreal.PointLight,point+forward*450+right*400+up*600,forward,up)
        lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        lamp.point_light_component.set_intensity(1000000)
        lamp.point_light_component.set_attenuation_radius(1800)
        path=capture(w,point+forward*520+right*340+up*170,point+up*190,up,'SpireColony')
        lamp.destroy_actor()
        return dict(image=path)
    if stage=='capture_soldier':
        nest=colony();w=server();p=nest.get_editor_property('planet')
        bug=next(a for a in residents(nest,'guards') if a.get_editor_property('mesh').is_visible())
        point=bug.get_actor_location();up=p.get_radial_up_vector(point)
        forward=bug.get_actor_forward_vector();right=up.cross(forward)
        lamp=spawn(w,unreal.PointLight,point+forward*100+up*130,forward,up)
        lamp.point_light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        lamp.point_light_component.set_intensity(5000);lamp.point_light_component.set_attenuation_radius(500)
        path=capture(w,point+forward*70+right*75+up*55,point+up*8,up,'SpireSoldier_Runtime')
        lamp.destroy_actor()
        return dict(image=path)
    raise ValueError(stage)
