"""Level-owned flight fields and all-pairs verification, including visual-only near bodies."""
import json
import math
import runpy
from pathlib import Path
import unreal

OUT = Path(unreal.Paths.project_saved_dir()) / 'SpaceDebug'
TUNING = {key:(80.,1.) for key in ['EarthFragments','Moon','Mars','MarI','MarII']}
# Historical marker gates are only migration/verification inputs. Runtime flight never reads them.
RETAINED_GATES = {'EarthFragments':620000.,'Moon':200000.,'MarII':2400.}


def xyz(v): return [v.x,v.y,v.z]


def angle(c,position,body):
    d=(position-body['center']).length()
    return math.degrees(2*math.asin(min(.999999,c.authored_radius(body['actor'])*c.visual_scale(position,body['actor'])/max(1,d))))


def legacy_outer_bound(c,body,gate):
    """Separable monotonic NAV: the gate lies within these six axial coordinate bounds."""
    center,nav=body['center'],body['entry'].navigation_position_kilometers
    extents=[]
    for axis in [unreal.Vector(1,0,0),unreal.Vector(0,1,0),unreal.Vector(0,0,1)]:
        distances=[]
        for sign in [-1,1]:
            low,high=c.bubble_radius(body['actor']),max(c.bubble_radius(body['actor'])*2,100000.)
            for _ in range(40):
                if (c.navigation_position(center+axis*(high*sign))-nav).length()>=gate:break
                high*=2
            else:raise AssertionError('Unbounded historical gate')
            for _ in range(48):
                mid=(low+high)*.5
                if (c.navigation_position(center+axis*(mid*sign))-nav).length()<gate:low=mid
                else:high=mid
            distances.append(high)
        extents.append(max(distances))
    return math.sqrt(sum(v*v for v in extents))


def context():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor) if world else unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    manager = next(a for a in actors if isinstance(a,unreal.JTSSpaceWorldManager))
    c = manager.get_editor_property('compressed_space')
    if not world: c.capture_authored_bodies(False)
    bodies = {}
    for e in c.get_editor_property('bodies'):
        p = isinstance(e.actor,unreal.JTSPlanetAnchor)
        key = str(e.actor.get_planet_id()) if p else next(k for k in ['Mars','MarI'] if e.actor.actor_has_tag(k))
        bodies[key] = dict(actor=e.actor,entry=e,center=e.actor.get_planet_center() if p else e.actor.get_actor_bounds(False)[0])
    return world,actors,manager,c,bodies


def run(operation):
    world,actors,manager,c,bodies = context()
    if operation == 'report':
        result = {}
        for filename,count in [('adaptive_moon_native.json',4),('adaptive_all_pairs_native.json',20)]:
            data = json.loads((OUT/filename).read_text(encoding='utf8'))
            assert data['status']=='complete' and not data['errors'] and len(data['legs'])==count
            assert not any(any(v['background_collision']) for v in data['samples'])
            mismatch = max(abs(v['speed_cm_s']-v['expected_cm_s'])/max(1,v['expected_cm_s']) for v in data['samples'])
            assert mismatch<.005, (filename,mismatch)
            result[filename] = dict(legs=count,samples=len(data['samples']),time_dilation=data['time_dilation'],max_relative_speed_error=mismatch)
        matrix=json.loads((OUT/'adaptive_matrix.json').read_text(encoding='utf8'))
        automation=json.loads((OUT/'adaptive_automation.json').read_text(encoding='utf8'))
        envelope=json.loads((OUT/'adaptive_envelope_scan.json').read_text(encoding='utf8'))
        assert matrix['passed'] and len(matrix['routes'])==20 and automation['passed']==13 and automation['failed']==0 and envelope['passed']
        result.update(passed=True,matrix_samples=sum(v['samples'] for v in matrix['routes']),automation_passed=13,envelopes=envelope['bodies'])
        (OUT/'adaptive_validation.json').write_text(json.dumps(result,indent=2),encoding='utf8')
        return json.dumps(result)
    if operation == 'configure':
        assert not world
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().startswith('/Game/Space/Maps/L_SpaceWorld.')
        before = {k:dict(center=xyz(v['center']),radius=c.authored_radius(v['actor']),nav=xyz(v['entry'].navigation_position_kilometers),visual=v['entry'].visual_radius_multiplier,
            prior_width=c.flight_transition_width(v['actor']),
            legacy_outer_bound=legacy_outer_bound(c,v,RETAINED_GATES[k]) if k in RETAINED_GATES else 0) for k,v in bodies.items()}
        entries = c.get_editor_property('bodies')
        c.modify()
        for i,e in enumerate(entries):
            key = next(k for k,v in bodies.items() if v['actor']==e.actor)
            e.set_editor_property('bEnableNearFlight',True)
            e.flight_transition_distance_radii,e.near_flight_speed_multiplier = TUNING[key]
            e.flight_transition_minimum_width=max(e.flight_transition_minimum_width,before[key]['prior_width'],
                before[key]['legacy_outer_bound']-c.bubble_radius(e.actor),c.authored_radius(e.actor)*TUNING[key][0])
            e.flight_approach_exponent = .1
            e.near_flight_reference_radius=21000.
            e.near_flight_size_exponent=.5
            e.near_flight_max_speed_multiplier=3.
            e.flight_angular_change_degrees_per_second=8.
            e.flight_relative_angular_change_per_second=.65
            entries[i] = e
            if isinstance(e.actor,unreal.JTSPlanetAnchor):
                e.actor.modify()
                e.actor.set_editor_property('near_flight_reference_radius',14000 if key=='Moon' else 21000)
                e.actor.set_editor_property('cruise_approach_exponent_override',.1)
                e.actor.set_editor_property('cruise_approach_expansion_degrees_per_second',0.)
            else:
                assert not e.actor.get_actor_enable_collision()
        c.set_editor_property('bodies',entries)
        c.capture_authored_bodies(False)
        for k,v in bodies.items():
            assert xyz(v['center']) == before[k]['center'] and c.authored_radius(v['actor']) == before[k]['radius']
        assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
        config_path=OUT/'adaptive_configuration.json'
        if not config_path.exists():config_path.write_text(json.dumps(dict(before=before,tuning=TUNING),indent=2),encoding='utf8')
        return run('inspect')
    if operation == 'inspect':
        rows = {}
        for k,v in bodies.items():
            a,e = v['actor'],v['entry']
            rows[k] = dict(radius=c.authored_radius(a),bubble=c.bubble_radius(a),
                near_enabled=isinstance(a,unreal.JTSPlanetAnchor) or e.get_editor_property('bEnableNearFlight'),
                width=c.flight_transition_width(a),retained_width=e.flight_transition_minimum_width,
                near_scale=c.cruise_approach_multiplier(v['center'],unreal.Vector(0,0,4200),100,3,1),
                collision=a.get_actor_enable_collision(),visual_multiplier=e.visual_radius_multiplier,
                near_body=str(c.containing_near_flight_body(v['center'])),gameplay_planet=str(c.containing_planet(v['center'])))
        (OUT/'adaptive_inspect.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
        return json.dumps(rows)
    if operation == 'envelope_scan':
        rows={}
        for key,b in bodies.items():
            a=b['actor'];inner=c.bubble_radius(a);width=c.flight_transition_width(a)
            old=legacy_outer_bound(c,b,RETAINED_GATES[key]) if key in RETAINED_GATES else 0
            assert inner+width>=old and width>=c.authored_radius(a)*80-.01 and width>=b['entry'].flight_transition_minimum_width
            max_ratio=0.;rates=[];samples=0;worst_rate={};worst_ratio={}
            # Six axial and twenty diagonal directions, both growth and shrinkage.
            for x in [-1,0,1]:
                for y in [-1,0,1]:
                    for z in [-1,0,1]:
                        if x==y==z==0:continue
                        radial=unreal.Vector(x,y,z).normal();command=radial*-4200
                        last=None
                        for i in range(301):
                            p=b['center']+radial*(inner+width*(i/300)**3)
                            m=c.cruise_approach_multiplier(p,command,100,3,1)
                            reverse=c.cruise_approach_multiplier(p,-command,100,3,1)
                            assert math.isfinite(m) and 1-1e-8<=m<=100+1e-8 and abs(m-reverse)<1e-8
                            change=abs(angle(c,p+radial*10,b)-angle(c,p-radial*10,b))/20
                            rate=change*m*4200
                            if not rates or rate>max(rates):worst_rate=dict(radial=xyz(radial),position=xyz(p),progress=(i/300)**3,multiplier=m,angle=angle(c,p,b),angular_rate=rate)
                            rates.append(rate)
                            if last is not None and max(m,last)/min(m,last)>max_ratio:
                                max_ratio=max(m,last)/min(m,last);worst_ratio=dict(radial=xyz(radial),position=xyz(p),progress=(i/300)**3,multiplier=m,previous=last)
                            last=m;samples+=1
            assert max(rates)<30., (key,'radial visual growth spike',max(rates))
            rows[key]=dict(radius=c.authored_radius(a),width=width,outer=inner+width,legacy_outer_bound=old,samples=samples,
                max_adjacent_speed_ratio=max_ratio,peak_ordinary_angular_rate=max(rates),worst_rate=worst_rate,worst_ratio=worst_ratio)
        result=dict(passed=True,bodies=rows)
        (OUT/'adaptive_envelope_scan.json').write_text(json.dumps(result,indent=2),encoding='utf8')
        return json.dumps(result)
    if operation == 'matrix':
        rows = []
        for source,origin in bodies.items():
            for target,dest in bodies.items():
                if source == target: continue
                direction=(dest['center']-origin['center']).normal()
                start=origin['center']+direction*(c.bubble_radius(origin['actor'])+1000)
                end=dest['center']-direction*(c.bubble_radius(dest['actor'])-2)
                samples=[]
                last_range=float('inf')
                for i in range(1001):
                    p=start+(end-start)*(i/1000)
                    m=c.cruise_approach_multiplier(p,direction*4200,100,3,1)
                    reverse=c.cruise_approach_multiplier(p,-direction*4200,100,3,1)
                    assert 1-1e-8 <= m <= 100+1e-8 and abs(m-reverse)<1e-9
                    r=(c.navigation_position(p)-c.navigation_position(dest['center'])).length()
                    assert r<=last_range+.01
                    last_range=r
                    samples.append(m)
                rows.append(dict(source=source,target=target,min=min(samples),max=max(samples),samples=len(samples)))
        result=dict(passed=True,routes=rows)
        (OUT/'adaptive_matrix.json').write_text(json.dumps(result,indent=2),encoding='utf8')
        return json.dumps(result)
    if operation in ['native','moon_native']:
        assert world
        unreal._multi_body_probe=Probe(world,actors,manager,c,bodies,operation=='moon_native')
        return 'Native owner-RPC routes started: '+operation
    if operation == 'status':
        p=unreal._multi_body_probe;p.save()
        return json.dumps(dict(status=p.result['status'],stage=p.stage,legs=p.result['legs'],errors=p.result['errors'],samples=len(p.result['samples'])))
    if operation.startswith('view_'):
        assert world
        key=operation[len('view_'):]
        setup=runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run']
        setup('setup_Moon',world,actors)
        ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        ship.set_actor_location(bodies[key]['center']+unreal.Vector(0,0,c.authored_radius(bodies[key]['actor'])+1000),False,True)
        ship.set_actor_rotation(unreal.Rotator(-20,0,0),False)
        ship.activate_flight_camera_third_person()
        return run('inspect')
    raise ValueError(operation)


class Probe:
    def __init__(self,world,actors,manager,c,bodies,moon_only):
        self.world=world;self.c=c;self.bodies=bodies;self.manager=manager
        self.ship=next(a for a in actors if isinstance(a,unreal.JTSSpacecraftActor))
        runpy.run_path(str(Path(__file__).with_name('validate_cruise_approach.py')))['run']('setup_Moon',world,actors)
        self.stages=[(s,t,b) for s,t in [('Moon','EarthFragments'),('EarthFragments','Moon')] for b in [False,True]] if moon_only else [(s,t,True) for s in bodies for t in bodies if s!=t]
        self.output=OUT/('adaptive_moon_native.json' if moon_only else 'adaptive_all_pairs_native.json')
        self.result=dict(status='running',time_dilation=1 if moon_only else 4,legs=[],samples=[],errors=[])
        self.settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
        self.throttle=self.settings.get_editor_property('bThrottleCPUWhenNotForeground')
        self.settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
        unreal.GameplayStatics.set_global_time_dilation(world,self.result['time_dilation'])
        self.stage=-1;self.last_time=-1;self.next_stage()
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def intent(self,stop=False):
        state=unreal.JTSSpacecraftInputState()
        state.import_text('(MoveForward=%f,bFlightAssistEnabled=True,bBoosting=%s,SpeedLimit=1.0)'%(0 if stop else 1,'True' if self.stages[self.stage][2] and not stop else 'False'))
        self.ship.call_method('ServerSetFlightInput',(state,))

    def next_stage(self):
        self.stage+=1
        if self.stage==len(self.stages):self.finish();return
        source,target,boost=self.stages[self.stage]
        a,b=self.bodies[source],self.bodies[target]
        direction=(b['center']-a['center']).normal()
        self.ship.set_actor_location(a['center']+direction*(self.c.bubble_radius(a['actor'])+1000),False,True)
        right=unreal.MathLibrary.cross_vector_vector(unreal.Vector(0,0,1),direction).normal()
        self.ship.set_actor_rotation(unreal.MathLibrary.make_rotation_from_axes(direction,right,unreal.MathLibrary.cross_vector_vector(direction,right)),False)
        self.started=unreal.GameplayStatics.get_time_seconds(self.world)
        self.first=len(self.result['samples']);self.stopping=False;self.intent()

    def tick(self,delta):
        try:
            if not unreal.SystemLibrary.is_valid(self.ship):self.finish('world_closed');return
            now=unreal.GameplayStatics.get_time_seconds(self.world)
            if now<=self.last_time:return
            self.last_time=now
            if self.stopping:
                self.intent(True)
                if now-self.stopped_at>.15:
                    if self.ship.get_flight_velocity().length()>.1:self.result['errors'].append('stop failure '+str(self.stage))
                    self.next_stage()
                return
            source,target,boost=self.stages[self.stage]
            dest=self.bodies[target];pos=self.ship.get_actor_location();vel=self.ship.get_flight_velocity()
            radius=(pos-dest['center']).length()
            expected=self.c.cruise_approach_multiplier(pos,vel.normal()*4200,100,3,1)*4200*(1.8 if boost else 1)
            near=self.c.containing_near_flight_body(pos)
            self.result['samples'].append(dict(stage=self.stage,seconds=now-self.started,speed_cm_s=vel.length(),expected_cm_s=expected,
                position=xyz(pos),target_radius=radius,near_actor=near.get_name() if near else None,
                target_angle=angle(self.c,pos,dest),source_angle=angle(self.c,pos,self.bodies[source]),
                background_collision=[self.bodies[k]['actor'].get_actor_enable_collision() for k in ['Mars','MarI']]))
            arrived=radius<self.c.bubble_radius(dest['actor'])
            if arrived or now-self.started>240:
                if not arrived:self.result['errors'].append('timeout '+source+' -> '+target)
                samples=self.result['samples'][self.first:]
                if any(any(v['background_collision']) for v in samples):self.result['errors'].append('background collision enabled')
                self.result['legs'].append(dict(source=source,target=target,boost=boost,seconds=now-self.started,
                    entry_speed_cm_s=vel.length(),samples=len(samples)))
                self.intent(True);self.stopping=True;self.stopped_at=now;self.save()
            else:self.intent()
        except Exception as exc:
            self.result['errors'].append(str(exc));self.finish('error')

    def save(self):self.output.write_text(json.dumps(self.result,indent=2),encoding='utf8')

    def finish(self,status='complete'):
        if self.stage<len(self.stages):self.intent(True)
        self.result['status']=status
        unreal.GameplayStatics.set_global_time_dilation(self.world,1)
        self.settings.set_editor_property('bThrottleCPUWhenNotForeground',self.throttle)
        if hasattr(self,'handle'):unreal.unregister_slate_post_tick_callback(self.handle)
        self.save()
