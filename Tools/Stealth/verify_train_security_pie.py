"""Read/check existing train GameState compatibility in two-player PIE.

Calls existing server speed/sky APIs and security APIs in PIE only; never saves
or reparents assets. Stop PIE afterwards.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

def run_train_security_checks():
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==2 and all('Lvl_SPTrainFreight' in w.get_path_name() for w in worlds)
    gs=lambda w:u.GameplayStatics.get_game_state(w)
    server=next(w for w in worlds if gs(w).has_authority())
    server_gs=gs(server)
    s=next(x for x in u.ObjectIterator(u.SPGuardAlertSubsystem) if x.get_outer()==server)
    report={'checks':[],'game_states':[gs(w).get_class().get_path_name() for w in worlds]}
    performance=u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    before=O.get_properties(performance,['bThrottleCPUWhenNotForeground'])
    O.set_properties(performance,'{"bThrottleCPUWhenNotForeground":false}')
    snapshot=lambda w:json.loads(O.get_properties(gs(w),['MotionState','SkyTransition']))
    def check(name,ok):
        report['checks'].append({'name':name,'passed':bool(ok)})
        u.log('STEALTH_TRAIN_CHECK '+name+': '+str(bool(ok)))
    def steps():
        check('Existing planet/train GameState class preserved',all(p.endswith('BP_SPPlanetTrainGameState_C') for p in report['game_states']))
        check('Train hierarchy inherits shared SPGameState',all(isinstance(gs(w),u.SPGameState) for w in worlds))
        check('Train has one independent replicated restricted area',all(len(u.GameplayStatics.get_all_actors_of_class(w,u.SPRestrictedArea))==1 and
            not u.GameplayStatics.get_all_actors_of_class(w,u.SPRestrictedArea)[0].get_attach_parent_actor() for w in worlds))
        check('Saved train guards use MVP silent movement and one-second sight',all(not g.get_editor_property('hear_footsteps') and
            abs(g.get_editor_property('confirm_sight_time')-1)<.01 for w in worlds for g in u.GameplayStatics.get_all_actors_of_class(w,u.SPGuardCharacter)))
        check('One security component per existing GS and PS',all(len(gs(w).get_components_by_class(u.SPStealthGameStateComponent))==1
              and len(gs(w).get_editor_property('player_array'))==2
              and all(len(p.get_components_by_class(u.SPStealthPlayerStateComponent))==1 for p in gs(w).get_editor_property('player_array')) for w in worlds))
        for speed in [0.0,1500.0,3000.0]:
            server_gs.call_method('SetTrainSpeed',args=(speed,True))
            yield 1
            states=[snapshot(w)['MotionState'] for w in worlds]
            check('Existing speed API replicates '+str(speed),states[0]==states[1] and states[0]['targetSpeed']==speed
                  and all(abs(gs(w).call_method('GetTravelState')[1]-speed)<1 for w in worlds))
        sky_before=snapshot(server)['SkyTransition']
        server_gs.call_method('RequestNextSky')
        yield 1
        sky_after=snapshot(server)['SkyTransition']
        check('Existing sky transition API still replicates',sky_before!=sky_after and all(snapshot(w)['SkyTransition']==sky_after for w in worlds))
        stable=snapshot(server)
        s.submit_anonymous_incident(u.SPStealthIncident.VICTIM_REPORT,s.make_incident_context(u.Vector(0,0,0),'None'))
        yield .8
        check('Alarm replicated on original train GameState',all(gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm for w in worlds))
        s.end_stage()
        yield .8
        check('Security end leaves train motion and sky untouched',all(snapshot(w)==stable and not gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().stage_active for w in worlds))
        s.start_stage()
        yield .8
        check('Security start leaves existing train/sky functions running',all(snapshot(w)==stable and gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().stage_active
              and not gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm
              and gs(w).call_method('GetTravelState')[1]==3000 for w in worlds))
        players=[p for p in u.GameplayStatics.get_all_actors_of_class(server,u.SPPlayerCharacter) if p.is_player_controlled()]
        check('Train counts both active players',len(players)==2 and all(gs(w).get_active_player_count()==2 for w in worlds))
        for player in players:player.get_status_component().apply_damage(1000)
        yield .8
        check('All-down failure replicates without replacing train state',all(gs(w).is_operation_failed() and gs(w).get_active_player_count()==0 and snapshot(w)==stable for w in worlds))
        s.submit_anonymous_incident(u.SPStealthIncident.VICTIM_REPORT,s.make_incident_context(u.Vector(0,0,0),'None'))
        stage_before=gs(server).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().stage_id.to_string()
        u.GameplayStatics.get_game_mode(server).reset_for_stage()
        yield .8
        check('Full reset clears train failure and restores crew',all(not gs(w).is_operation_failed() and gs(w).get_active_player_count()==2 for w in worlds)
              and all(p.get_status_component().get_health()==100 for p in players))
        check('Full reset refreshes security and preserves train motion/sky',all(snapshot(w)==stable and
              gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().stage_active and
              not gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm and
              gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().stage_id.to_string()!=stage_before for w in worlds))
        report['final_train_state']=stable
    iterator=steps();remaining=next(iterator);last=u.GameplayStatics.get_time_seconds(server);handle=None
    def finish(error=None):
        nonlocal handle
        if handle is not None:u.unregister_slate_post_tick_callback(handle);handle=None
        O.set_properties(performance,before)
        if error:report['error']=str(error)
        report['passed']=not error and all(c['passed'] for c in report['checks'])
        Path(u.Paths.project_saved_dir(),'StealthEvents','train-compatibility.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        u.log('STEALTH_TRAIN_COMPLETE: '+str(report['passed']))
    def tick(_):
        nonlocal remaining,last
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds):raise RuntimeError('PIE ended')
            now=u.GameplayStatics.get_time_seconds(server);remaining-=max(0,now-last);last=now
            if remaining<=0:remaining=next(iterator)
        except StopIteration:finish()
        except Exception as exc:finish(exc)
    handle=u.register_slate_post_tick_callback(tick)

run_train_security_checks()
