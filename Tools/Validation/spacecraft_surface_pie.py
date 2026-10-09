"""Run in SpaceWorld PIE: real Moon flight, boundary capture, locked intent, Space abort and touchdown.

The test disables only the pawn's camera/input heartbeat Tick, then submits normal owner RPCs.
Movement, gear presentation, physics, replication and planet queries continue to run normally.
"""
import json, math, time
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
world=unreal.EditorLevelLibrary.get_pie_worlds(True)[0]
ship=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSSpacecraftActor)[0]
planet=ship.get_flight_planet()
support=ship.get_landing_support_component()
movement=ship.get_flight_movement_component()
envelope=ship.get_surface_envelope_component()
presentation=ship.get_component_by_class(unreal.JTSSpacecraftPresentationComponent)
def v(x): return [x.x,x.y,x.z]
def dot(x,y): return unreal.MathLibrary.dot_vector_vector(x,y)
def intent(forward=0,right=0,lift=0,pitch=0,yaw=0,boost=False,held=False):
    s=unreal.JTSSpacecraftInputState()
    s.import_text(f'(MoveForward={forward},MoveRight={right},Lift={lift},Pitch={pitch},Yaw={yaw},bBoosting={boost},bDescentKeyHeld={held})')
    ship.call_method('ServerSetFlightInput',args=(s,))

assert ship.is_landed()
player=unreal.GameplayStatics.get_player_pawn(world,0)
player.set_actor_location(ship.get_boarding_interaction_center(),False,True)
assert ship.try_board_player(player)
ship.set_actor_tick_enabled(False)
assert ship.begin_surface_takeoff()
centre=planet.get_planet_center()
radius=(ship.get_actor_location()-centre).length()
report={'initial_landed':True,'landing_site_count':len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.JTSPlanetLandingSite)),'samples':[],'phases':[], 'cruise_following_every_sample':True}
# Find a valid actual footprint, not a LandingSite. Native tests cover refusal and corrected footprints.
picked=None
for pitch in [25,35,45,55,65]:
    for yaw in range(0,360,30):
        p,y=math.radians(pitch),math.radians(yaw)
        direction=unreal.Vector(math.sin(p)*math.cos(y),math.sin(p)*math.sin(y),math.cos(p))
        frame=planet.get_surface_frame_at(centre+direction*(radius+2000),unreal.Vector(1,0,0))
        if not frame: continue
        ship.set_actor_location_and_rotation(frame.location+frame.up*1200,frame.transform.rotation.rotator(),False,True)
        movement.stop_movement_immediately()
        r=support.find_landing(planet,True)
        if r and r.is_valid:
            picked=(frame,r)
            break
    if picked: break
assert picked,'No actual four-foot footprint found'
frame,plan=picked
report['test_ground']=v(frame.location)
report['planned_slope']=plan.ground_slope_degrees
report['four_feet']=len(plan.foot_contacts)
started=time.monotonic()
stage='settle'
stage_time=started
start_position=ship.get_actor_location()
landing_start=None
first_landing_position=None
attempt_capture_height=None
aborted=False
previous_rotation=None
max_turn=0
min_clearance=1e12
max_cruise_speed=0
last_sent=0
def hull_support(up):
    mesh=ship.get_editor_property('spacecraft_mesh')
    box=mesh.static_mesh.get_bounding_box()
    t=mesh.get_world_transform()
    support_distance=0
    for x in [box.min.x,box.max.x]:
        for y in [box.min.y,box.max.y]:
            for z in [box.min.z,box.max.z]:
                support_distance=max(support_distance,-dot(t.transform_location(unreal.Vector(x,y,z))-ship.get_actor_location(),up))
    return support_distance

def save(error=None):
    global _surface_pie_handle
    unreal.unregister_slate_post_tick_callback(_surface_pie_handle)
    ship.set_actor_tick_enabled(True)
    report.update(stage=stage,error=error,elapsed=time.monotonic()-started,aborted=aborted,min_hull_clearance_cm=min_clearance,max_cruise_speed_cm_s=max_cruise_speed,max_turn_per_tick_degrees=max_turn,landed=ship.is_landed(),failure=str(ship.get_last_landing_failure()),capture_height_above_envelope_cm=attempt_capture_height)
    check=support.validate_pose(planet,ship.get_actor_transform()) if ship.is_landed() else None
    report['final_support_valid']=bool(check and check.is_valid)
    report['visible_sole_errors_cm']=[]
    if ship.is_landed():
        meshes={c.get_name():c for c in ship.get_components_by_class(unreal.StaticMeshComponent)}
        for definition in support.feet:
            sole=meshes[str(definition.foot_component_name)].get_world_transform().transform_location(definition.sole_centre_local)
            contact=next(c for c in support.get_foot_contacts() if c.foot_component_name==definition.foot_component_name)
            report['visible_sole_errors_cm'].append((sole-contact.location).length())
    (root/'Docs/Validation/spacecraft_surface_pie.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('SURFACE_PIE_RESULT',json.dumps({k:val for k,val in report.items() if k!='samples'}))

def tick(delta):
    global stage,stage_time,landing_start,first_landing_position,attempt_capture_height,aborted,previous_rotation,max_turn,min_clearance,max_cruise_speed,last_sent
    now=time.monotonic()
    elapsed=now-stage_time
    try:
        if now-started>60: save('timed out'); return
        pos=ship.get_actor_location()
        # The replicated frame belongs to the start of the movement tick. Query the cached
        # field at the final position for accurate high-speed, low-frame-rate telemetry.
        f=envelope.evaluate(planet,pos) or envelope.get_frame()
        phase=str(ship.get_landing_assist_phase())
        if not report['phases'] or phase!=report['phases'][-1]: report['phases'].append(phase)
        height=dot(pos-f.location,f.radial_up) if f.valid else None
        report['samples'].append({'t':now-started,'stage':stage,'position':v(pos),'envelope_height':height,'speed':ship.get_current_speed(),'gear':presentation.get_gear_deploy_alpha(),'phase':phase,'following':envelope.is_following()})
        if stage in ['settle','cruise']:
            ground=planet.get_surface_frame_at(pos,ship.get_actor_forward_vector())
            if ground:
                up=planet.get_radial_up_vector(pos)
                min_clearance=min(min_clearance,dot(pos-ground.location,up)-hull_support(up))
        if stage=='settle':
            intent()
            if f.valid and elapsed>0.6:
                ship.set_actor_location(f.location+f.radial_up*30,False,True)
                stage='cruise'; stage_time=now
        elif stage=='cruise':
            intent(forward=1,pitch=-1,boost=True)
            report['cruise_following_every_sample'] &= envelope.is_following()
            max_cruise_speed=max(max_cruise_speed,ship.get_current_speed())
            if previous_rotation:
                q=ship.get_actor_transform().rotation
                cosine=abs(previous_rotation.x*q.x+previous_rotation.y*q.y+previous_rotation.z*q.z+previous_rotation.w*q.w)
                max_turn=max(max_turn,math.degrees(2*math.acos(min(1,cosine))))
            previous_rotation=ship.get_actor_transform().rotation
            if elapsed>3:
                report['cruise_distance_cm']=(pos-start_position).length()
                # Return to the measured footprint to exercise landing independent of flight destination.
                ship.set_actor_location_and_rotation(frame.location+frame.up*1200,frame.transform.rotation.rotator(),False,True)
                movement.stop_movement_immediately()
                intent()
                stage='descend'; stage_time=now
        elif stage=='descend':
            intent(lift=-1,held=True)
            if movement.is_assisted_landing():
                attempt_capture_height=height
                first_landing_position=pos
                landing_start=now
                stage='lock_and_abort';stage_time=now
        elif stage=='lock_and_abort':
            intent(forward=1,right=1,lift=-1,pitch=1,yaw=1,boost=True,held=True)
            report['locked_vertical_input']=movement.get_vertical_input()
            report['locked_boost']=movement.is_boosting()
            report['gear_deploy_started']=presentation.get_gear_deploy_alpha()>0
            report['descent_started_without_staging']=dot(pos-first_landing_position,frame.up)<-0.1
            if elapsed>0.25:
                ship.call_method('ServerRequestSurfaceTakeoff')
                aborted=not movement.is_assisted_landing() and not ship.is_landed()
                assert aborted,'Space RPC did not abort'
                stage='held_ctrl_after_abort';stage_time=now
        elif stage=='held_ctrl_after_abort':
            intent(lift=-1,held=True)
            assert not movement.is_assisted_landing(),'Held Ctrl recaptured after Space abort'
            if elapsed>0.5:
                stage='escape';stage_time=now
        elif stage=='escape':
            intent(lift=1)
            if f.valid and height>150:
                intent()
                stage='descend_again';stage_time=now
        elif stage=='descend_again':
            intent(lift=-1,held=True)
            if movement.is_assisted_landing():
                stage='touchdown';stage_time=now
        elif stage=='touchdown':
            intent(forward=-1,right=-1,lift=-1,pitch=-1,boost=True,held=True)
            if ship.is_landed():
                report['touchdown_seconds']=elapsed
                save();return
    except Exception as exc:
        save(str(exc))

_surface_pie_handle=unreal.register_slate_post_tick_callback(tick)
