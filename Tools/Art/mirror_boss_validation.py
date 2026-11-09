"""Disposable PIE checks; does not save runtime mutations to authored assets."""
import unreal, json, math
from pathlib import Path
OUT=Path(unreal.Paths.project_dir()).resolve()/'Saved/MirrorBoss'
GS=unreal.GameplayStatics
def worlds():return unreal.EditorLevelLibrary.get_pie_worlds(True)
def get(world,cls):return GS.get_all_actors_of_class(world,cls)
def server():return next(w for w in worlds() if any(a.has_authority() for a in get(w,unreal.JTSNestBossEncounter)))
def summary():
    rows=[]
    for w in worlds():
        encounters=get(w,unreal.JTSNestBossEncounter);bosses=get(w,unreal.JTSPlanetBoss)
        rows.append(dict(world=w.get_path_name(),authority=bool(GS.get_game_mode(w)),encounters=len(encounters),nests=len([a for a in get(w,unreal.JTSPlanetEcologyHabitat) if a.get_actor_label().startswith('Moon_Nest_')]),
            players=[dict(name=a.get_name(),health=a.get_component_by_class(unreal.JTSHealthComponent).get_health()) for a in get(w,unreal.JTSCharacter)],
            bosses=[dict(authority=b.has_authority(),name=str(b.get_editor_property('display_name')),health=b.health.get_health(),max_health=b.health.get_max_health(),action=str(b.combat.get_action()),parts=len(b.get_components_by_class(unreal.BoxComponent)),location=[b.get_actor_location().x,b.get_actor_location().y,b.get_actor_location().z]) for b in bosses]))
    return rows
def position_players(distance=750):
    w=server();enc=get(w,unreal.JTSNestBossEncounter)[0];p=enc.get_editor_property('planet');center=enc.active_boss.get_actor_location() if enc.active_boss else enc.get_actor_location();forward=enc.active_boss.get_actor_forward_vector() if enc.active_boss else enc.get_actor_forward_vector();up=p.get_radial_up_vector(center)
    players=get(w,unreal.JTSCharacter)
    for i,pawn in enumerate(players):
        hit=p.project_point_to_surface(center+forward*distance+up.cross(forward)*(i*220))
        pawn.character_movement.disable_movement();pawn.set_actor_location(hit.impact_point+up*96,False,True)
        pawn.set_actor_rotation(p.make_surface_aligned_rotation(hit.impact_point,center-hit.impact_point),False)
        pc=pawn.get_controller()
        if pc:pc.set_control_rotation(p.make_surface_aligned_rotation(hit.impact_point,center-hit.impact_point))
        pawn.get_component_by_class(unreal.JTSHealthComponent).set_max_health(1000,True)
    return dict(players=len(players))
def run(stage):
    if stage=='boss_qa_summary':return summary()
    if stage=='boss_qa_position':return position_players()
    if stage=='boss_qa_near':return position_players(300)
    if stage=='boss_qa_client_damage':
        w=next(w for w in worlds() if not GS.get_game_mode(w));b=get(w,unreal.JTSPlanetBoss)[0]
        before=b.health.get_health();applied=b.health.apply_damage(500,None,None,True)
        assert applied==0 and b.health.get_health()==before
        return dict(client_rejected=True,health=before)
    if stage=='boss_qa_prepare_players':
        from pickaxe_combat_validation import spawn
        w=server();enc=get(w,unreal.JTSNestBossEncounter)[0]
        controllers=[a for a in get(w,unreal.PlayerController) if a.has_authority()]
        assert len(controllers)==2,len(controllers)
        cls=unreal.load_asset('/Game/Space/Blueprints/Player/BP_JTSPlayer_Casual_2').generated_class()
        for pc in controllers:
            old=pc.get_controlled_pawn()
            pawn=spawn(w,cls,enc.get_actor_location()+enc.get_actor_up_vector()*500,enc.get_actor_forward_vector(),enc.get_actor_up_vector())
            pc.possess(pawn)
            if old:old.destroy_actor()
        return position_players()
    if stage=='boss_qa_view':
        from pickaxe_combat_validation import spawn
        w=server();enc=get(w,unreal.JTSNestBossEncounter)[0];p=enc.get_editor_property('planet')
        center=enc.active_boss.get_actor_location() if enc.active_boss else enc.get_actor_location()
        up=p.get_radial_up_vector(center);f=enc.get_actor_forward_vector();r=up.cross(f)
        for pc in get(w,unreal.PlayerController):
            loc=center+f*850+r*550+up*450
            camera=spawn(w,unreal.CameraActor,loc,center+up*90-loc,up)
            pc.set_view_target_with_blend(camera)
            GS.set_game_paused(w,True)
        return summary()
    if stage=='boss_qa_shot':
        w=server();unreal.SystemLibrary.execute_console_command(w,'Shot SHOWUI',GS.get_player_controller(w,0))
        return dict(screenshot=True)
    if stage=='boss_qa_resume':
        GS.set_game_paused(server(),False)
        return dict(resumed=True)
    if stage in ('boss_qa_six','boss_qa_seven'):
        enc=get(server(),unreal.JTSNestBossEncounter)[0];nests=list(enc.get_editor_property('nests'))
        for a in nests[:6 if stage.endswith('six') else 7]:
            hp=a.get_component_by_class(unreal.JTSHealthComponent)
            if hp.get_health()>0:hp.apply_damage(10000,None,None,True)
        return dict(blocked=[a.is_entrance_blocked() for a in nests],boss=bool(enc.active_boss))
    if stage=='boss_qa_damage':
        b=get(server(),unreal.JTSPlanetBoss)[0];before=b.health.get_health()
        GS.apply_damage(b,50,None,GS.get_player_pawn(server(),0),None)
        return dict(before=before,after=b.health.get_health())
    if stage=='boss_qa_death':
        b=get(server(),unreal.JTSPlanetBoss)[0];b.health.apply_damage(100000,None,None,True);return summary()
    if stage=='boss_qa_capture':
        from pickaxe_combat_validation import capture
        w=server();enc=get(w,unreal.JTSNestBossEncounter)[0];center=enc.get_actor_location();p=enc.get_editor_property('planet');up=p.get_radial_up_vector(center);forward=enc.get_actor_forward_vector();right=up.cross(forward)
        return dict(overview=capture(w,center+up*7500,center,forward,'boss_crater_overview'),close=capture(w,center+forward*850+right*550+up*450,center+up*90,up,'boss_in_crater'))
    raise ValueError(stage)
