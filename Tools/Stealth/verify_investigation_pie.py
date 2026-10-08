"""Fresh 2-player listen-server PIE. Real navigation/vision, replicated guard states.
Mutates only PIE actors. Tests anonymous lifecycle, dispatch, priorities, bounded
unreachable/stalled movement and actual E input on both new saved terminals.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

def run():
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==2
    server=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    # Isolate this earlier-stage fixture from newly placed laser hazards (PIE only).
    for laser in u.GameplayStatics.get_all_actors_of_class(server,u.SPLaserSecurityDevice):
        laser.set_editor_property("enabled",False);laser.refresh_device()
    client=next(w for w in worlds if w!=server)
    s=next(s for s in u.ObjectIterator(u.SPGuardAlertSubsystem) if s.get_outer()==server)
    actors=lambda w,t:u.GameplayStatics.get_all_actors_of_class(w,t)
    players=lambda w:[p for p in actors(w,u.SPPlayerCharacter) if p.is_player_controlled()]
    a=next(p for p in players(server) if not p.is_locally_controlled())
    b=next(p for p in players(server) if p.is_locally_controlled())
    pid=lambda p:p.get_editor_property('player_state').get_editor_property('player_id')
    copies=lambda p:[c for w in worlds for c in players(w) if pid(c)==pid(p)]
    local=next(p for p in copies(a) if p.is_locally_controlled())
    inp=next(x for x in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if not x.get_name().startswith('Default__') and u.GameplayStatics.get_player_controller(x,0)==local.get_controller())
    action=local.get_editor_property('interact_action')
    assert action
    guards=sorted([g for g in actors(server,u.SPGuardCharacter) if str(g.get_editor_property("alert_group"))=="StealthTest_Baseline"],key=lambda g:g.get_actor_label())
    g,other=guards
    remote=lambda guard:next(x for x in actors(client,u.SPGuardCharacter) if x.get_actor_label()==guard.get_actor_label())
    prop=lambda actor,name:actor.get_editor_property(name)
    state=lambda:prop(g,'guard_state')
    GS=u.SPGuardState;R=u.SPGuardInvestigationResult;K=u.SPStealthIncident
    group=str(prop(g,'alert_group'))
    labels={GS.PATROL:'순찰 중',GS.SUSPICIOUS:'범죄 확인 중',GS.PURSUING:'발각자 추격 중',GS.SEARCHING:'마지막 목격 위치 수색',GS.LISTENING:'소리 확인 중',GS.INVESTIGATING:'사건 위치로 이동',GS.SCENE_SEARCHING:'현장 수색 중',GS.MOVING_TO_LAST_SEEN:'마지막 목격 위치로 이동'}
    report={'checks':[],'observed_states':[],'source':'Saved map; actual server AI navigation and vision, 2-player replication, participant E input'}
    held=False;handle=None
    perf=u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    before=O.get_properties(perf,['bThrottleCPUWhenNotForeground']);O.set_properties(perf,'{"bThrottleCPUWhenNotForeground":false}')
    def check(name,ok):
        report['checks'].append({'name':name,'passed':bool(ok)})
        u.log('INVESTIGATION_CHECK '+name+': '+str(bool(ok)))
    def hidden(p):
        return all(not c.get_editor_property('player_state').get_component_by_class(u.SPStealthPlayerStateComponent).is_identified() for c in copies(p))
    def no_alarm():
        return all(not u.GameplayStatics.get_game_state(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm for w in worlds)
    def move(p,loc,yaw=0,pitch=0):
        for c in copies(p):
            c.set_actor_location(loc,False,True);c.character_movement.stop_movement_immediately()
            if c.get_controller():c.get_controller().set_control_rotation(u.Rotator(yaw=yaw,pitch=pitch))
    def snapshot(guard):
        target=prop(guard,'target_player')
        return [str(prop(guard,'guard_state')),prop(guard,'investigating_anonymous_incident'),
                prop(guard,'investigation_incident_id').to_string(),pid(target) if target else None,
                list(prop(guard,'investigation_location').to_tuple())]
    def synced(expected):
        c=remote(g)
        return state()==expected and snapshot(g)==snapshot(c) and str(prop(c,'alert_indicator').get_editor_property('text'))==labels[expected]
    def context(loc,radius=5000,cap=1,dispatch=group):
        c=s.make_incident_context(loc,dispatch);c.response_radius=radius;c.max_responders=cap;return c
    def send(loc):
        c=context(loc);return c,s.submit_anonymous_incident(K.WORK_NOISE,c)
    def receipt():return prop(g,'last_investigation')
    def reset():
        s.restart_stage()
        for guard in guards:
            guard.set_editor_property('hear_footsteps',False)
            guard.set_editor_property('sight_distance',1200)
            guard.set_editor_property('patrol_speed',170)
            guard.set_editor_property('chase_speed',340)
            guard.set_editor_property('search_time',2.5)
            guard.set_editor_property('move_timeout',30)
            guard.set_editor_property('stuck_timeout',3)
            guard.set_editor_property('max_move_retries',2)
            guard.set_editor_property('move_retry_interval',1)
            guard.character_movement.stop_movement_immediately()
        g.set_actor_location(u.Vector(-1250,400,98),False,True);g.set_actor_rotation(u.Rotator(yaw=0),True)
        other.set_actor_location(u.Vector(2250,800,98),False,True)
        move(a,u.Vector(-550,600,98),180);move(b,u.Vector(-2500,-500,98))
    def until(test,timeout):
        elapsed=0
        while not test() and elapsed<timeout:
            yield .1;elapsed+=.1
        return test()
    def press(name):
        nonlocal held
        btn=next(c for c in actors(server,u.SPStealthTestConsole) if c.get_actor_label()==name)
        pos=btn.get_actor_location();move(a,u.Vector(pos.x,pos.y-185,98),90,-12.8)
        yield .4
        held=True;yield .25;held=False;yield .5
    def steps():
        reset()
        c,r=send(u.Vector(-650,600,0))
        yield .7
        check('Nearest matching guard selected; cap is one',r.dispatched_guards==1 and prop(g,'investigating_anonymous_incident') and not prop(other,'investigating_anonymous_incident'))
        check('Investigation travel and overhead label replicate',synced(GS.INVESTIGATING))
        check('Anonymous job has no target on either peer',all(not prop(x,'target_player') for x in [g,remote(g)]))
        check('Nearby normal player is visible but stays hidden',g.can_see_player(a) and hidden(a) and hidden(b) and no_alarm())
        yield from until(lambda:state()==GS.SCENE_SEARCHING,10)
        yield .4
        check('Arrival changes to replicated scene search',synced(GS.SCENE_SEARCHING))
        check('Guard reaches snapshot with no Player target',(g.get_actor_location()-u.Vector(-650,600,98)).length()<115 and not prop(g,'target_player'))
        same=s.submit_anonymous_incident(K.WORK_NOISE,c)
        check('Repeated ID merges without another dispatch',same.result==u.SPStealthResult.DUPLICATE and same.dispatched_guards==0)
        yield from until(lambda:receipt().incident_id.to_string()==c.incident_id.to_string(),4)
        yield .4
        check('Timed search completes and both peers return to patrol',synced(GS.PATROL) and receipt().result==R.COMPLETED and prop(remote(g),'last_investigation').result==R.COMPLETED)
        check('Anonymous lifecycle never changes either identity/alarm',hidden(a) and hidden(b) and no_alarm())
        reset()
        first,_=send(u.Vector(-650,600,0));yield .3
        second,_=send(u.Vector(-700,-400,0));yield .5
        check('New event replaces snapshot and records previous job',receipt().incident_id.to_string()==first.incident_id.to_string() and receipt().result==R.SUPERSEDED and prop(g,'investigation_incident_id').to_string()==second.incident_id.to_string())
        reset()
        r=s.submit_anonymous_incident(K.WORK_NOISE,context(u.Vector(-650,600,0),radius=100));yield .3
        check('Radius prevents dispatch',r.dispatched_guards==0)
        r=s.submit_anonymous_incident(K.WORK_NOISE,context(u.Vector(-650,600,0),dispatch='OtherGroup'));yield .3
        check('Other dispatch group is excluded independently of identity scope',r.dispatched_guards==0)
        reset()
        g.set_actor_location(u.Vector(550,-800,98),False,True);g.set_actor_rotation(u.Rotator(yaw=0),True)
        move(a,u.Vector(950,-800,98),180)
        c,_=send(u.Vector(550,-400,0));yield .4
        check('Actual crime pauses travel for visual confirmation',synced(GS.SUSPICIOUS) and not prop(g,'target_player'))
        yield .9
        check('Continuous trespass switches investigation to confirmed pursuit',synced(GS.PURSUING) and prop(g,'target_player')==a and receipt().result==R.INTERRUPTED_BY_SIGHTING and not hidden(a) and hidden(b))
        saved=prop(g,'last_seen_location')
        r=s.submit_anonymous_incident(K.LASER_CONTACT,context(u.Vector(-650,600,0)));yield .3
        check('Anonymous call never steals the confirmed chase target',prop(g,'target_player')==a and not prop(g,'investigating_anonymous_incident'))
        # Ensure distance to the last snapshot, then hide the player. No actor-goal MoveTo.
        g.set_actor_location(u.Vector(400,-800,98),False,True)
        move(a,u.Vector(-2500,200,98))
        saved=prop(g,'last_seen_location');yield .4
        check('Hidden player leaves frozen last-known destination',synced(GS.MOVING_TO_LAST_SEEN) and (prop(g,'last_seen_location')-saved).length()<1)
        yield from until(lambda:state()==GS.SEARCHING,5);yield .4
        check('Last-seen search differs from anonymous scene search',synced(GS.SEARCHING) and not prop(g,'investigating_anonymous_incident'))
        yield from until(lambda:state()==GS.PATROL,5);yield .4
        check('Last-seen search ends without erasing known identity',synced(GS.PATROL) and not hidden(a) and hidden(b))
        reset()
        c,_=send(u.Vector(-650,600,900));yield .5
        check('Unreachable height is not treated as 2D arrival',state()==GS.INVESTIGATING and prop(g,'move_request_count')==1)
        yield from until(lambda:receipt().incident_id.to_string()==c.incident_id.to_string(),6);yield .4
        check('Unreachable destination fails after exactly three requests',receipt().result==R.FAILED and receipt().failure==u.SPGuardMoveFailure.UNREACHABLE and receipt().move_requests==3)
        check('Failure receipt and return to patrol replicate',synced(GS.PATROL) and prop(remote(g),'last_investigation').incident_id.to_string()==c.incident_id.to_string() and prop(remote(g),'last_investigation').failure==receipt().failure)
        yield 1
        check('Failed incident never relaunches itself',not prop(g,'investigating_anonymous_incident') and receipt().move_requests==3)
        reset()
        g.set_editor_property('patrol_speed',0);g.set_editor_property('stuck_timeout',.5)
        g.set_editor_property('max_move_retries',1);g.set_editor_property('move_retry_interval',.3)
        c,_=send(u.Vector(-650,600,0))
        yield from until(lambda:receipt().incident_id.to_string()==c.incident_id.to_string(),5);yield .4
        check('Real path with stationary movement detects stagnation',receipt().result==R.FAILED and receipt().failure==u.SPGuardMoveFailure.STALLED and receipt().move_requests==2 and synced(GS.PATROL))
        reset()
        g.set_editor_property('patrol_speed',20);g.set_editor_property('move_timeout',1)
        c,_=send(u.Vector(-650,600,0))
        yield from until(lambda:receipt().incident_id.to_string()==c.incident_id.to_string(),3);yield .4
        check('Slow but progressing travel has a finite overall deadline',receipt().result==R.FAILED and receipt().failure==u.SPGuardMoveFailure.TIMED_OUT and synced(GS.PATROL))
        reset()
        # Allow a short journey then search longer than travel budget.
        g.set_actor_location(u.Vector(-780,600,98),False,True);g.set_editor_property('move_timeout',1.5)
        c,_=send(u.Vector(-650,600,0));yield .9
        check('Arrival starts full search independently of travel deadline',synced(GS.SCENE_SEARCHING))
        yield 1
        check('Scene search outlives movement deadline',synced(GS.SCENE_SEARCHING))
        yield from until(lambda:receipt().incident_id.to_string()==c.incident_id.to_string(),3)
        check('Full scene search completes successfully',receipt().result==R.COMPLETED)
        reset()
        c,_=send(u.Vector(-650,600,0));yield .4
        s.restart_stage();yield .5
        check('Stage reset removes incident ID, response receipt and target on both peers',all(not prop(x,'investigating_anonymous_incident') and prop(x,'investigation_incident_id').to_string()=='0'*32 and prop(x,'last_investigation').result==R.NONE and not prop(x,'target_player') for x in [g,remote(g)]))
        reset()
        yield from press('Investigation_Public')
        last=s.get_recent_incidents()[-1]
        check('Participant E fires saved public-scene terminal with one responder',last.kind==K.WORK_NOISE and last.dispatched_guards==1 and (last.context.location-u.Vector(-650,600,0)).length()<1 and hidden(a) and hidden(b))
        yield from press('Investigation_Unreachable')
        last=s.get_recent_incidents()[-1]
        check('Participant E fires saved unreachable terminal with separate event ID',last.kind==K.WORK_NOISE and last.dispatched_guards==1 and (last.context.location-u.Vector(-650,600,900)).length()<1)
        yield from until(lambda:receipt().incident_id.to_string()==last.context.incident_id.to_string(),6);yield .4
        check('Saved unreachable button completes bounded failure on both peers',receipt().result==R.FAILED and prop(remote(g),'last_investigation').result==R.FAILED and hidden(a) and hidden(b) and no_alarm())
    iterator=steps();remaining=next(iterator);last_time=u.GameplayStatics.get_time_seconds(server)
    def finish(error=None):
        nonlocal handle
        if handle is not None:u.unregister_slate_post_tick_callback(handle);handle=None
        O.set_properties(perf,before)
        if error:report['error']=str(error)
        report['passed']=not error and all(c['passed'] for c in report['checks'])
        path=Path(u.Paths.project_saved_dir(),'StealthInvestigation','pie.json');path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(report,indent=2),encoding='utf-8');u.log('INVESTIGATION_PIE_COMPLETE '+str(report['passed']))
    def tick(_):
        nonlocal remaining,last_time
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds):raise RuntimeError('PIE ended')
            if held:inp.inject_input_vector_for_action(action,u.Vector(1,0,0),[],[])
            current=str(state())
            if current not in report['observed_states']:report['observed_states'].append(current)
            now=u.GameplayStatics.get_time_seconds(server);remaining-=max(0,now-last_time);last_time=now
            if remaining<=0:remaining=next(iterator)
        except StopIteration:finish()
        except Exception as exc:finish(exc)
    handle=u.register_slate_post_tick_callback(tick)
    u.log('INVESTIGATION_PIE_STARTED')
run()
