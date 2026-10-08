"""Two-player PIE incident policy/replication/UI test. Does not save assets.

Uses server producer APIs, and explicitly tests client rejection. The separate
verify_incident_buttons_pie.py tests actual owning-client E input. Can create a
transient HUD director if the map has not yet been authored; reports that fact.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

def run_incident_checks():
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds) == 2
    server = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    # Isolate this earlier-stage fixture from newly placed laser hazards (PIE only).
    for laser in u.GameplayStatics.get_all_actors_of_class(server,u.SPLaserSecurityDevice):
        laser.set_editor_property("enabled",False);laser.refresh_device()
    client = next(w for w in worlds if w != server)
    def subsystem(w):
        return next(s for s in u.ObjectIterator(u.SPGuardAlertSubsystem) if s.get_outer() == w)
    s = subsystem(server)
    gs = lambda w: u.GameplayStatics.get_game_state(w)
    alarm = lambda w: gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state()
    players = lambda w: [p for p in u.GameplayStatics.get_all_actors_of_class(w,u.SPPlayerCharacter) if p.is_player_controlled()]
    a = next(p for p in players(server) if not p.is_locally_controlled()).get_editor_property('player_state')
    b = next(p for p in players(server) if p.is_locally_controlled()).get_editor_property('player_state')
    def ps_copy(w,ps):
        return next(p for p in gs(w).get_editor_property('player_array') if p.get_editor_property('player_id') == ps.get_editor_property('player_id'))
    identity = lambda w,ps: ps_copy(w,ps).get_component_by_class(u.SPStealthPlayerStateComponent)
    directors = u.GameplayStatics.get_all_actors_of_class(server,u.SPStealthTestDirector)
    transient = not directors
    if transient:
        u.SystemLibrary.execute_console_command(server,'summon SpacePirate.SPStealthTestDirector',u.GameplayStatics.get_player_controller(server,0))
        director = u.GameplayStatics.get_all_actors_of_class(server,u.SPStealthTestDirector)[0]
    else:
        director = directors[0]
    guards = sorted([g for g in u.GameplayStatics.get_all_actors_of_class(server,u.SPGuardCharacter) if str(g.get_editor_property("alert_group"))=="StealthTest_Baseline"],key=lambda g:g.get_name())
    for g in guards:
        g.set_editor_property('sight_distance',1)
        g.set_editor_property('hear_footsteps',False)
        g.set_editor_property('patrol_speed',170)
        g.set_editor_property('chase_speed',340)
    performance = u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    perf_before = O.get_properties(performance,['bThrottleCPUWhenNotForeground'])
    O.set_properties(performance,'{"bThrottleCPUWhenNotForeground":false}')
    report = {'checks':[], 'transient_director':transient, 'source':'Server incident APIs; real replicated PS/GS and local UMG'}
    handle = None
    kind = u.SPStealthIncident
    result = u.SPStealthResult
    scope = 'StageSecurity'
    group = str(guards[0].get_editor_property('alert_group'))
    def context(group_name=group):
        # The former (550,-400) point is inside low cover; complete-path policy rejects it.
        return s.make_incident_context(u.Vector(900,-650,0),group_name)
    def check(name,ok):
        report['checks'].append({'name':name,'passed':bool(ok)})
        u.log('STEALTH_INCIDENT_CHECK '+name+': '+str(bool(ok)))
    def states(expected_a,expected_b,expected_alarm):
        return all(identity(w,a).is_identified()==expected_a and identity(w,b).is_identified()==expected_b
                   and alarm(w).global_alarm==expected_alarm and identity(w,a).get_identity_state().stage_id.to_string() == alarm(w).stage_id.to_string()
                   and identity(w,b).get_identity_state().stage_id.to_string() == alarm(w).stage_id.to_string() for w in worlds)
    def effects():
        return (director.get_editor_property('first_identity_effects'),director.get_editor_property('first_alarm_effects'))
    def ui_matches():
        widgets = [w for w in u.ObjectIterator(u.SPStealthStatusWidget) if w.get_owning_player()
                   and w.get_owning_player().get_world() in worlds] if hasattr(u.PlayerController,'get_world') else [
            w for w in u.ObjectIterator(u.SPStealthStatusWidget) if w.get_owning_player()
            and any(w.get_owning_player() == p.get_controller() for world in worlds for p in players(world))]
        texts = [str(w.get_status_text()).replace(' [나]','') for w in widgets]
        report['last_ui'] = texts
        return len(texts)==2 and texts[0]==texts[1] and '동기화 중' not in texts[0]
    def steps():
        s.restart_stage()
        yield 1.5
        check('Both existing PlayerStates and GameStates have replicated components',states(False,False,False))
        check('Both local HUDs display identical authoritative state',ui_matches())
        start_positions = [g.get_actor_location() for g in guards]
        for k in [kind.LASER_CONTACT,kind.WORK_NOISE,kind.INDIRECT_REPORT]:
            r = s.submit_anonymous_incident(k,context())
            yield .8
            check(str(k)+' keeps both identities and global alarm unchanged',r.result==result.APPLIED and states(False,False,False))
            check(str(k)+' dispatches location without a target',all(g.get_editor_property('investigating_anonymous_incident') and not g.get_editor_property('target_player') for g in guards))
        check('Anonymous dispatched guards actually move using NavMesh',all((g.get_actor_location()-p).length()>20 for g,p in zip(guards,start_positions)))
        r = s.submit_anonymous_incident(kind.VICTIM_REPORT,context())
        yield .8
        check('Victim report raises replicated alarm with no identity',r.first_global_alarm and states(False,False,True))
        before = effects()
        r = s.submit_anonymous_incident(kind.ESCAPE_ACTIVATED,context())
        yield .8
        check('Escape can coexist with alarm; first effect not repeated',states(False,False,True) and effects()==before and not r.first_global_alarm)
        check('Alarm-only HUD matches on host and participant',ui_matches())
        s.restart_stage()
        yield .8
        r = s.submit_direct_incident(kind.IDENTIFIED_PLAYER_REDISCOVERED,a,scope,context())
        check('Rediscovery cannot expose an unknown player',r.result==result.UNKNOWN_IDENTITY and states(False,False,False))
        direct_context = context()
        before = effects()
        r = s.submit_direct_incident(kind.INSTANT_CRIME_WITNESSED,a,scope,direct_context)
        yield 1
        check('A identified, B hidden, global alarm replicated',r.first_identification and r.first_global_alarm and states(True,False,True))
        check('First effect callbacks ran exactly once',effects()==(before[0]+1,before[1]+1))
        r = s.submit_direct_incident(kind.INSTANT_CRIME_WITNESSED,a,scope,direct_context)
        yield .6
        check('Same event ID rejected without repeated first effects',r.result==result.DUPLICATE and effects()==(before[0]+1,before[1]+1))
        r = s.submit_direct_incident(kind.DIRECT_REPORT_COMPLETED,a,scope,context())
        r2 = s.submit_direct_incident(kind.SUSTAINED_CRIME_CONFIRMED,a,scope,context())
        yield .6
        check('Different direct causes do not repeat A first effects',r.result==result.APPLIED and r2.result==result.APPLIED and effects()==(before[0]+1,before[1]+1))
        check('Per-player exposed/hidden HUD matches on both displays',ui_matches())
        client_s = subsystem(client)
        history_before_client = len(s.get_recent_incidents())
        r = client_s.submit_direct_incident(kind.INSTANT_CRIME_WITNESSED,ps_copy(client,b),scope,context())
        r2 = client_s.submit_anonymous_incident(kind.ESCAPE_ACTIVATED,context())
        denied_stage = client_s.restart_stage()
        yield .8
        # Python's BlueprintAuthorityOnly dispatch can block the C++ body entirely,
        # returning the struct default InvalidRequest. Native C++ returns NotAuthority.
        report['client_rejection'] = [str(r.result),str(r2.result),denied_stage.to_string()]
        check('Client mutation calls rejected for direct, anonymous and stage reset',
              r.result in [result.NOT_AUTHORITY,result.INVALID_REQUEST]
              and r2.result in [result.NOT_AUTHORITY,result.INVALID_REQUEST]
              and denied_stage.to_string()=='0'*32 and len(s.get_recent_incidents())==history_before_client
              and states(True,False,True))
        guards[1].set_editor_property('alert_group','OtherDispatch')
        guards[1].set_editor_property('identity_scope',scope)
        s.restart_stage()
        yield .6
        r = s.submit_direct_incident(kind.SUSTAINED_CRIME_CONFIRMED,a,scope,context())
        yield .8
        check('Sharing identity does not dispatch the other group',s.is_identified(scope,a) and guards[0].get_editor_property('target_player') is not None and guards[1].get_editor_property('target_player') is None)
        r = s.submit_direct_incident(kind.IDENTIFIED_PLAYER_REDISCOVERED,a,scope,context('OtherDispatch'))
        yield .6
        check('Rediscovery dispatches the chosen group without first effects',r.result==result.APPLIED and not r.first_identification and not r.first_global_alarm and guards[1].get_editor_property('target_player') is not None)
        old = context()
        s.end_stage()
        yield .8
        check('End clears replicated identity/alarm and all guard responses',states(False,False,False) and all(not alarm(w).stage_active for w in worlds) and all(not g.get_editor_property('target_player') and not g.get_editor_property('investigating_anonymous_incident') for g in guards))
        r = s.submit_anonymous_incident(kind.VICTIM_REPORT,old)
        check('Ended stage rejects delayed completion',r.result==result.INACTIVE_STAGE)
        new_stage = s.restart_stage()
        yield .8
        r = s.submit_anonymous_incident(kind.VICTIM_REPORT,old)
        check('Restart rejects old generation and keeps clients clean',r.result==result.STALE_STAGE and new_stage.to_string()!=old.stage_id.to_string() and states(False,False,False))
        before = effects()
        r = s.submit_direct_incident(kind.DIRECT_REPORT_COMPLETED,b,scope,context())
        yield .8
        check('New stage permits B first exposure while A stays hidden',states(False,True,True) and effects()==(before[0]+1,before[1]+1))
        check('Final HUD matches host and participant',ui_matches())
        history = s.get_recent_incidents()
        check('Server audit has IDs, kinds, locations, times and results',len({x.kind for x in history})==9 and any(x.result==result.DUPLICATE for x in history) and any(x.result==result.STALE_STAGE for x in history) and all(x.server_time>=0 and x.context.incident_id.to_string()!='0'*32 for x in history))
        report['incident_count'] = len(history)
        guards[1].set_editor_property('alert_group',group)
    iterator = steps()
    remaining = next(iterator)
    last = u.GameplayStatics.get_time_seconds(server)
    def finish(error=None):
        nonlocal handle
        if handle is not None:
            u.unregister_slate_post_tick_callback(handle); handle=None
        O.set_properties(performance,perf_before)
        if error: report['error']=str(error)
        report['passed'] = not error and all(c['passed'] for c in report['checks'])
        path = Path(u.Paths.project_saved_dir(),'StealthEvents','incident-pie.json')
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(report,indent=2),encoding='utf-8')
        u.log('STEALTH_INCIDENT_PIE_COMPLETE: '+str(report['passed']))
    def tick(_):
        nonlocal remaining,last
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds): raise RuntimeError('PIE ended')
            now=u.GameplayStatics.get_time_seconds(server)
            remaining-=max(0,now-last);last=now
            if remaining<=0: remaining=next(iterator)
        except StopIteration: finish()
        except Exception as exc: finish(exc)
    handle=u.register_slate_post_tick_callback(tick)
    u.log('STEALTH_INCIDENT_PIE_STARTED')

run_incident_checks()

