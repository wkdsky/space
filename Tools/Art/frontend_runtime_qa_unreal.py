"""PIE smoke tests and runtime evidence, driven by Saved/FrontEndJob.json via MCP."""
import unreal
import json
from pathlib import Path
ROOT=Path(unreal.Paths.project_dir()).resolve()
job=json.loads((ROOT/'Saved/FrontEndJob.json').read_text())
worlds=list(unreal.EditorLevelLibrary.get_pie_worlds(False))
REPORT={'worlds':[]}
if job.get('defaults'):
    REPORT['defaults']={}
    for path in ['/Game/Space/FrontEnd/Widgets/WBP_FE_Main','/Game/Space/FrontEnd/Widgets/WBP_FE_Lobby']:
        REPORT['defaults'][path]=unreal.get_default_object(unreal.load_class(None,path+'.'+path.rsplit('/',1)[1]+'_C')).get_editor_property('lobby')
shell_class=unreal.load_class(None,'/Script/space.JTSFrontEndShellWidget')
for world in worlds:
    if len(worlds)>1:
        unreal.SystemLibrary.execute_console_command(world,'net.AllowPIESeamlessTravel 1',unreal.GameplayStatics.get_player_controller(world,0))
    widgets=list(unreal.WidgetLibrary.get_all_widgets_of_class(world,shell_class,False))
    record={'world':world.get_path_name(),'widgets':[],'time':unreal.GameplayStatics.get_time_seconds(world),'paused':unreal.GameplayStatics.is_game_paused(world)}
    for command in job.get('console',[]):unreal.SystemLibrary.execute_console_command(world,command,unreal.GameplayStatics.get_player_controller(world,0))
    if job.get('presentation'):
        record['presentation']=[]
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            name=actor.get_actor_label()
            if name in ['FE_TwinforkDisplay','FE_MainPresentation','FE_LobbyPresentation','FE_ThreatHeatedAtmosphere'] or name.startswith('FE_CrewPad_'):
                center,extent=actor.get_actor_bounds(False)
                data={'name':name,'location':str(actor.get_actor_location()),'center':str(center),'extent':str(extent),'hidden':actor.get_editor_property('hidden')}
                if name=='FE_MainPresentation':
                    mesh=actor.get_editor_property('preview_mesh')
                    if 'pose_time' in job:
                        mesh.set_position(job['pose_time'],False);mesh.set_play_rate(0)
                    data.update(anim=str(mesh.get_anim_instance()),playing=mesh.is_playing(),mesh_transform=str(mesh.get_relative_transform()),
                                bones={b:str(mesh.get_socket_location(b)) for b in ['Body','Head','Wrist_L','Wrist_R','Foot_L','Foot_R']})
                    data['bone_transforms']={b:str(mesh.get_socket_transform(b,unreal.RelativeTransformSpace.RTS_WORLD)) for b in ['Root','Body','Head','Wrist_L','Wrist_R','Foot_L','Foot_R']}
                record['presentation'].append(data)
    if job.get('scene'):
        record['scene']=[]
        for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
            if 'MaintenanceRobot' in a.get_actor_label() or isinstance(a,unreal.LevelSequenceActor):
                record['scene'].append({'name':a.get_actor_label(),'position':str(a.get_actor_location()),'playing':a.get_editor_property('sequence_player').is_playing() if isinstance(a,unreal.LevelSequenceActor) else None})
                if isinstance(a,unreal.LevelSequenceActor) and job.get('bindings'):
                    seq=a.get_sequence();player=a.get_editor_property('sequence_player')
                    record['bindings']=[{'name':str(b.get_display_name()),'objects':[str(o) for o in player.get_bound_objects(seq.get_binding_id(b))]} for b in seq.get_bindings()]
    for w in widgets:
        if job.get('world_index') is not None and len(REPORT['worlds'])!=job['world_index']:continue
        entry={'class':w.get_class().get_path_name(),'page':str(w.get_current_page()),'lobby':w.get_editor_property('lobby')}
        owner=w.get_owning_player();entry['owner']=str(owner);entry['owner_role']=str(owner.get_local_role());entry['owner_state']=str(owner.player_state);entry['local_controller']=owner.is_local_controller()
        # Python reflection forces editor script callspace. A native timer invokes the
        # actual button on the next game tick, so client RPCs use their real net driver.
        for name in job.get('click_next_tick',[]):
            button=w.find_control(name);assert button,name
            unreal.SystemLibrary.set_timer(button,'Invoke',.1,False)
        if job.get('ready_rpc') is not None:owner.call_method('ServerSetReady',args=(job['ready_rpc'],))
        for a in job.get('actions',[]):
            w.dispatch_action(a['action'],a.get('index',0))
        if job.get('navigate'):w.navigate(job['navigate'])
        for name,value in job.get('inputs',{}).items():
            control=w.find_control(name);assert control,name;control.set_text(value)
        for a in job.get('after_actions',[]):w.dispatch_action(a['action'],a.get('index',0))
        entry['page_after']=str(w.get_current_page())
        entry['labels']={name:str(w.find_control(name).get_text()) for name in job.get('labels',[]) if w.find_control(name)}
        entry['controls']={name:{'enabled':w.find_control(name).get_is_enabled(),'visible':w.find_control(name).is_visible()} for name in job.get('controls',[]) if w.find_control(name)}
        if job.get('expect_page'):assert entry['page_after']==job['expect_page'],entry
        record['widgets'].append(entry)
    gs=unreal.GameplayStatics.get_game_state(world)
    if gs:
        record['phase']=str(gs.get_gameplay_phase()) if hasattr(gs,'get_gameplay_phase') else str(gs)
        if hasattr(gs,'get_launch_deadline'):record['launch_deadline']=gs.get_launch_deadline();record['accepting_players']=gs.is_accepting_new_players()
        record['player_count']=len(gs.player_array)
        record['players']=[]
        for p in gs.player_array:
            item={'name':p.get_player_name(),'id':p.get_editor_property('player_id')}
            if hasattr(p,'get_appearance'):
                item.update(ready=p.call_method('IsReady'),host=p.call_method('IsExpeditionHost'),appearance=str(p.call_method('GetAppearance')))
            pawn=p.get_pawn()
            if pawn and hasattr(pawn,'mesh'):
                ac=pawn.get_component_by_class(unreal.load_class(None,'/Script/space.JTSAvatarAppearanceComponent'))
                item['catalog']=str(ac.get_editor_property('catalog')) if ac else None
                if job.get('freeze'):pawn.character_movement.disable_movement()
                item['location']=str(pawn.get_actor_location());item['movement_mode']=str(pawn.character_movement.movement_mode);item['velocity']=str(pawn.get_velocity())
                item['mesh']=str(pawn.mesh.get_editor_property('skeletal_mesh_asset'))
                item['head']=str(pawn.mesh.get_socket_location('Head'))
                item['anim_instance']=str(pawn.mesh.get_anim_instance())
                if job.get('presentation'):
                    item['bones']={b:str(pawn.mesh.get_socket_location(b)) for b in ['Body','Head','Wrist_L','Wrist_R','Foot_L','Foot_R']}
                    item['playing']=pawn.mesh.is_playing()
            record['players'].append(item)
    REPORT['worlds'].append(record)
    if job.get('screenshot'):
        filename=str(ROOT/('Saved/FrontEndQA/'+job.get('screenshot_name','QA')+'.png')).replace('\\','/')
        unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename='+filename+' -nosuffix',unreal.GameplayStatics.get_player_controller(world,0))
REPORT['success']=True
(ROOT/'SourceArt/FrontEnd/runtime_qa_report.json').write_text(json.dumps(REPORT,ensure_ascii=False,indent=2),encoding='utf-8')
