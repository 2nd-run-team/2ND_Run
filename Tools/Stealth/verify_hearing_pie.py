"""Fresh two-player Listen Server PIE: real remote input, hearing and animation.

Changes PIE actors only. Run separately from verify_pie.py, then stop PIE.
"""
import unreal as u
import json
from pathlib import Path
from editor_toolset.toolsets.object import ObjectTools

# Share the actor/input fixture, without starting its direct-sight test callback.
exec(Path(u.Paths.project_dir(),'Tools','Stealth','verify_pie.py').read_text(encoding='utf-8').split('stage=0')[0])
report={'checks':[], 'animation_samples':{}, 'timeline':[], 'input':'Remote owning client Enhanced Input'}
all_guards=[g for w in worlds for g in u.GameplayStatics.get_all_actors_of_class(w,u.SPGuardCharacter)]
for g in all_guards:
    g.get_editor_property('mesh').set_editor_property('visibility_based_anim_tick_option',
        u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    assert isinstance(g.get_editor_property('mesh').get_anim_instance(),u.SPGuardAnimInstance)

freeze(guards[0],(-4245,-630,308),180)
freeze(guards[1],(-4945,630,308),0)
for g in guards:
    g.set_editor_property('patrol_speed',170)
    g.set_editor_property('chase_speed',340)
    g.get_editor_property('character_movement').set_editor_property('orient_rotation_to_movement',True)
place(a,(-3800,-120,308))
place(b,(-3675,120,308))

# Input injection needs normal game ticks even when the editor is in the background.
performance_settings=u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
performance_before=ObjectTools.get_properties(performance_settings,['bThrottleCPUWhenNotForeground'])
assert ObjectTools.set_properties(performance_settings,'{"bThrottleCPUWhenNotForeground":false}')

stage=0
elapsed=0
last_time=u.GameplayStatics.get_time_seconds(server)
inject_move=False
inject_crouch=False
saw_listening=False
samples={'walk':{},'run':{}}
handle=None

def collect(gait):
    for g in all_guards:
        anim=g.get_editor_property('mesh').get_anim_instance()
        speed=anim.get_editor_property('ground_speed')
        blend=anim.get_editor_property('blend_speed')
        valid=(100<speed<200 and 270<blend<330) if gait=='walk' else (300<speed<355 and blend>540)
        if valid:
            point=g.get_editor_property('mesh').get_socket_transform('foot_l',u.RelativeTransformSpace.RTS_COMPONENT).translation
            samples[gait].setdefault(g.get_path_name(),[]).append(point.to_tuple())

def verify_animation(gait):
    results=[]
    for g in all_guards:
        poses=samples[gait].get(g.get_path_name(),[])
        changed=len(poses)>2 and any((u.Vector(*p)-u.Vector(*poses[0])).length()>5 for p in poses[1:])
        results.append(changed)
        report['animation_samples'][gait+' '+g.get_path_name()]={'samples':len(poses),'foot_pose_changes':changed}
    check(gait+' animation advances on both guards in both worlds',all(results))

def tick_hearing(_delta):
    global stage,elapsed,last_time,inject_move,inject_crouch,saw_listening
    try:
        now=u.GameplayStatics.get_time_seconds(server)
        elapsed+=max(0,now-last_time)
        last_time=now
        if inject_crouch:
            input_subsystem.inject_input_vector_for_action(crouch_action,u.Vector(1,0,0),[],[])
        if inject_move:
            input_subsystem.inject_input_vector_for_action(move_action,u.Vector(0,1,0),[],[])
        if stage==0: collect('walk')
        if stage==4: collect('run')
        if stage in (2,4):
            report['timeline'].append({'stage':stage,'time':elapsed,'player':a.get_actor_location().to_tuple(),
                'speed':a.get_velocity().length(),'crouched':a.get_editor_property('is_crouched'),
                'grounded':a.get_editor_property('character_movement').is_moving_on_ground(),
                'guards':[(str(g.get_editor_property('guard_state')),g.get_actor_rotation().yaw,
                           g.get_velocity().length(),g.get_editor_property('mesh').get_anim_instance().get_editor_property('blend_speed')) for g in guards]})
        if stage==2 and guards[0].get_editor_property('guard_state')==u.SPGuardState.LISTENING:
            saw_listening=True
        if elapsed<[2.5,1.1,.6,1.6,1.0][stage]: return
        elapsed=0
        if stage==0:
            verify_animation('walk')
            freeze(guards[0],(-4200,-630,308),0)
            freeze(guards[1],(-4945,630,308),90)
            place(a,(-4780,-630,308),0)
            inject_move=True
            inject_crouch=True
        elif stage==1:
            check('Crouch walk behind guard stays inaudible',not guards[0].can_hear_player(a)
                  and guards[0].get_editor_property('guard_state')==u.SPGuardState.PATROL)
            check('Crouched player is not identified',all(not g.get_editor_property('target_player') for g in all_guards))
            inject_crouch=False
        elif stage==2:
            check('Normal footsteps start listening and turn the guard',saw_listening
                  and abs(guards[0].get_actor_rotation().yaw)>10)
            check('Hearing alone does not broadcast identification',all(not g.get_editor_property('target_player') for g in all_guards))
            inject_move=False
        elif stage==3:
            check('Visual confirmation after turning identifies only A',all_target_a())
            check('Silent B remains uninvolved',all(g.get_editor_property('target_player')!=b for g in guards))
            place(a,(-4900,-630,308))
            for g in all_guards:
                g.set_editor_property('patrol_speed',170)
                g.set_editor_property('chase_speed',340)
            for g in guards:
                g.get_editor_property('character_movement').set_editor_property('orient_rotation_to_movement',True)
        elif stage==4:
            verify_animation('run')
            finish_hearing()
            return
        stage+=1
    except Exception as exc:
        report['error']=repr(exc)
        finish_hearing()

def finish_hearing():
    report['passed']=not report.get('error') and len(report['checks'])==8 and all(c['passed'] for c in report['checks'])
    Path(u.Paths.project_saved_dir(),'Stealth','hearing-pie-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    u.unregister_slate_post_tick_callback(handle)
    ObjectTools.set_properties(performance_settings,performance_before)
    u.log('STEALTH_HEARING_PIE_COMPLETE: '+str(report['passed']))

handle=u.register_slate_post_tick_callback(tick_hearing)
