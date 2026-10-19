"""Level-owned flight tuning and native owner-RPC verification at normal game time."""
import json
import math
import runpy
from pathlib import Path
import unreal

OUT=Path(unreal.Paths.project_saved_dir())/'SpaceDebug'
TUNING={
    'Moon':dict(reference=14000,exponent=.1,expansion=0),
    'MarII':dict(reference=21000,exponent=.1,expansion=0),
    'EarthFragments':dict(reference=21000,exponent=.1,expansion=0),
}


def run(operation,world,actors):
    planets={str(a.get_planet_id()):a for a in actors if isinstance(a,unreal.JTSPlanetAnchor)}
    manager=next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
    c=manager.get_editor_property('compressed_space')
    if operation in ['configure','preview']:
        assert bool(world)==(operation=='preview'),'Configure requires stopped PIE; preview requires PIE'
        before={}
        for key,t in TUNING.items():
            p=planets[key]
            props=dict(near_flight_reference_radius=t['reference'],near_flight_size_exponent=.5,
                near_flight_max_speed_multiplier=3,cruise_approach_exponent_override=t['exponent'],
                cruise_approach_expansion_degrees_per_second=t['expansion'])
            before[key]={prop:p.get_editor_property(prop) for prop in props}
            p.modify()
            for prop,value in props.items():p.set_editor_property(prop,value)
        if operation=='configure':
            assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.')
            assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
            (OUT/'planet_flight_balance_configuration.json').write_text(json.dumps(dict(before=before,after=TUNING),indent=2),encoding='utf8')
        return json.dumps(dict(operation=operation,tuning=TUNING))
    if operation=='inspect':
        if not world:c.capture_authored_bodies(False)
        return json.dumps({key:dict(radius=c.authored_radius(p),
            scale=p.get_near_flight_speed_multiplier(c.authored_radius(p)),
            reference=p.get_editor_property('near_flight_reference_radius'),
            exponent=p.get_editor_property('cruise_approach_exponent_override'),
            expansion=p.get_editor_property('cruise_approach_expansion_degrees_per_second')) for key,p in planets.items()})
    assert world,'PIE required'
    if operation=='native':
        unreal._planet_flight_balance_probe=BalanceProbe(world,actors,planets,manager)
        return 'Normal-time native flight: both Earth/Moon directions, ordinary/Shift, three outer approaches, fragment-gap controls'
    if operation=='status':
        p=unreal._planet_flight_balance_probe
        p.save()
        return json.dumps(dict(status=p.result['status'],stage=p.stage,legs=p.result['legs'],errors=p.result['errors'],samples=len(p.result['samples'])))
    if operation=='stop':
        unreal._planet_flight_balance_probe.finish('stopped')
        return 'Native balance probe stopped'
    if operation=='marker_view':
        setup=runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run']
        setup('setup_EarthFragments_from_Moon_depart',world,actors)
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        p=planets['EarthFragments'];center=p.get_planet_center();radial=(planets['Moon'].get_planet_center()-center).normal()
        low,high=c.bubble_radius(p),2500000
        for _ in range(50):
            mid=(low+high)*.5
            if (c.navigation_position(center+radial*mid)-c.navigation_position(center)).length()<40000:low=mid
            else:high=mid
        ship.set_actor_location(center+radial*((low+high)*.5),False,True)
        ship.activate_flight_camera_third_person()
        return 'Native driving camera facing Earth at 40000 navigation km; stationary visual check'
    if operation=='estimate':
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        stats=ship.get_flight_movement_component().get_effective_stats()
        rows=[]
        for source,target in [('Moon','EarthFragments'),('EarthFragments','Moon')]:
            origin=planets[source];p=planets[target];center=p.get_planet_center()
            radial=(origin.get_planet_center()-center).normal()
            start=(origin.get_planet_center()-radial*(c.bubble_radius(origin)+1000)-center).length()
            end=c.bubble_radius(p)
            for boost in [False,True]:
                command=radial*-stats.max_move_speed*(stats.boost_multiplier if boost else 1)
                seconds=0;milestones=[]
                for i in range(600):
                    r=start-(start-end)*(i+.5)/600
                    x=center+radial*r
                    mult=c.cruise_approach_multiplier(x,command,stats.cruise_speed_multiplier,stats.cruise_approach_near_speed_multiplier,stats.cruise_approach_exponent)
                    seconds+=(start-end)/600/(command.length()*mult)
                    if i in [0,300,540,594,599]:milestones.append(dict(progress=(i+.5)/600,speed_cm_s=command.length()*mult))
                rows.append(dict(source=source,target=target,boost=boost,seconds=seconds,milestones=milestones))
        return json.dumps(rows)
    raise ValueError(operation)


class BalanceProbe:
    def __init__(self,world,actors,planets,manager):
        self.world=world;self.actors=actors;self.planets=planets;self.manager=manager
        self.c=manager.get_editor_property('compressed_space')
        self.ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        self.move=self.ship.get_flight_movement_component()
        self.stats=self.move.get_effective_stats()
        self.approach=runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run']
        self.result=dict(status='running',legs=[],samples=[],errors=[])
        self.stages=[(target,source,boost,'transfer') for source,target in [('Moon','EarthFragments'),('EarthFragments','Moon')] for boost in [False,True]]
        self.stages += [(target,None,True,'approach') for target in ['EarthFragments','Moon','MarII']]
        self.stages += [('EarthFragments',None,boost,axis) for boost,axis in [(False,'forward'),(True,'forward'),(False,'strafe'),(False,'reverse'),(False,'lift')]]
        self.stage=-1;self.stopping=False;self.last_game_time=-1
        settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
        self.settings=settings;self.throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground')
        settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal.GameplayStatics.set_global_time_dilation(world,1)
        self.next_stage()
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def intent(self,stop=False):
        state=unreal.JTSSpacecraftInputState()
        target,source,boost,mode=self.stages[self.stage]
        forward=0 if stop or mode in ['strafe','lift'] else -1 if mode=='reverse' else 1
        assert state.import_text('(MoveForward=%f,MoveRight=%f,Lift=%f,bFlightAssistEnabled=True,bBoosting=%s,SpeedLimit=1.0)'%(forward,1 if mode=='strafe' and not stop else 0,1 if mode=='lift' and not stop else 0,'True' if boost and not stop else 'False'))
        self.ship.call_method('ServerSetFlightInput',(state,))

    def next_stage(self):
        self.stage+=1
        if self.stage==len(self.stages):self.finish();return
        target,source,boost,mode=self.stages[self.stage]
        key=target+('_from_'+source+'_depart' if source else '')
        self.approach('setup_'+key,self.world,self.actors)
        if mode not in ['transfer','approach']:
            # Initial pose in an existing open debris gap. All subsequent movement is native.
            self.ship.set_actor_location(self.planets[target].get_planet_center()+unreal.Vector(110000,0,0),False,True)
            self.ship.set_actor_rotation(unreal.Rotator(0,90,0),False)
        self.started=unreal.GameplayStatics.get_time_seconds(self.world)
        self.previous=self.ship.get_actor_location();self.path=0;self.stopping=False
        self.first_sample=len(self.result['samples']);self.intent()

    def tick(self,delta):
        try:
            if not unreal.SystemLibrary.is_valid(self.ship):self.finish('world_closed');return
            now=unreal.GameplayStatics.get_time_seconds(self.world)
            if now<=self.last_game_time:return
            self.last_game_time=now
            if self.stopping:
                self.intent(True)
                if now-self.stopped_at>=.15:
                    if self.ship.get_flight_velocity().length()>.1:self.result['errors'].append('release did not stop: '+str(self.stage))
                    self.next_stage()
                return
            target,source,boost,mode=self.stages[self.stage]
            p=self.planets[target];position=self.ship.get_actor_location();velocity=self.ship.get_flight_velocity()
            self.path+=(position-self.previous).length();self.previous=position
            boom=self.ship.get_flight_camera_boom();camera=self.ship.get_flight_camera()
            pivot=camera.get_world_location()+camera.get_forward_vector()*boom.target_arm_length-unreal.MathLibrary.quat_rotate_vector(camera.get_world_rotation().quaternion(),boom.socket_offset)
            containing=self.c.containing_planet(position)
            self.result['samples'].append(dict(stage=self.stage,seconds=now-self.started,world_speed_cm_s=velocity.length(),
                nav_speed_cm_s=self.c.navigation_velocity(position,velocity).length(),radius_cm=(position-p.get_planet_center()).length(),
                range_km=(self.c.navigation_position(position)-self.c.navigation_position(p.get_planet_center())).length(),
                position=[position.x,position.y,position.z],containing=str(containing.get_planet_id()) if containing else None,
                camera_pivot_error_cm=(pivot-boom.get_world_location()-boom.target_offset).length(),ship_tick=self.ship.is_actor_tick_enabled()))
            arrived=containing==p if mode in ['transfer','approach'] else now-self.started>=1
            if arrived or now-self.started>75:
                if not arrived:self.result['errors'].append('timeout: '+str(self.stage))
                samples=self.result['samples'][self.first_sample:]
                self.result['legs'].append(dict(target=target,source=source,boost=boost,mode=mode,seconds=now-self.started,
                    path_cm=self.path,entry_speed_cm_s=velocity.length(),samples=len(samples),
                    max_camera_error_cm=max(r['camera_pivot_error_cm'] for r in samples)))
                self.intent(True);self.stopping=True;self.stopped_at=now;self.save()
            else:self.intent()
        except Exception as exc:
            self.result['errors'].append(str(exc));self.finish('error')

    def save(self):
        (OUT/'planet_flight_balance_native.json').write_text(json.dumps(self.result,indent=2),encoding='utf8')

    def finish(self,status='complete'):
        if self.stage<len(self.stages):self.intent(True)
        self.result['status']=status
        if hasattr(self,'handle'):unreal.unregister_slate_post_tick_callback(self.handle)
        self.settings.set_editor_property('bThrottleCPUWhenNotForeground',self.throttle)
        self.save()
