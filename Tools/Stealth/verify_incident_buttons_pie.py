"""Saved test map, fresh two-player listen-server PIE. Real E input, no direct incident calls.

Moves test pawns and suppresses incidental guard sight in PIE only. Restores the
editor background throttle; stop PIE after completion. Evidence is written to
Saved/StealthEvents/buttons-pie.json.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

def run_button_checks():
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==2
    server=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    # Isolate this earlier-stage fixture from newly placed laser hazards (PIE only).
    for laser in u.GameplayStatics.get_all_actors_of_class(server,u.SPLaserSecurityDevice):
        laser.set_editor_property("enabled",False);laser.refresh_device()
    def players(w):
        return [p for p in u.GameplayStatics.get_all_actors_of_class(w,u.SPPlayerCharacter) if p.is_player_controlled()]
    a=next(p for p in players(server) if not p.is_locally_controlled())
    b=next(p for p in players(server) if p.is_locally_controlled())
    pid=lambda p:p.get_editor_property('player_state').get_editor_property('player_id')
    copies=lambda p:[c for w in worlds for c in players(w) if pid(c)==pid(p)]
    owner=lambda p:next(c for c in copies(p) if c.is_locally_controlled())
    buttons={c.get_actor_label():c for c in u.GameplayStatics.get_all_actors_of_class(server,u.SPStealthTestConsole)}
    assert len(buttons)==21, 'Author and save the map before this test.'
    director=u.GameplayStatics.get_all_actors_of_class(server,u.SPStealthTestDirector)[0]
    inputs={}
    for p in [a,b]:
        inputs[pid(p)]=next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
            if not s.get_name().startswith('Default__') and u.GameplayStatics.get_player_controller(s,0)==owner(p).get_controller())
    for g in u.GameplayStatics.get_all_actors_of_class(server,u.SPGuardCharacter):
        g.set_editor_property('sight_distance',1);g.set_editor_property('hear_footsteps',False)
    performance=u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    before=O.get_properties(performance,['bThrottleCPUWhenNotForeground'])
    O.set_properties(performance,'{"bThrottleCPUWhenNotForeground":false}')
    report={'checks':[],'source':'Saved map; actual host and owning participant Enhanced Input E'}
    held=False;active_player=a;handle=None
    def check(name,ok):
        report['checks'].append({'name':name,'passed':bool(ok)})
        u.log('STEALTH_BUTTON_CHECK '+name+': '+str(bool(ok)))
    def state(expected_a,expected_b,expected_alarm,active=True):
        for w in worlds:
            gs=u.GameplayStatics.get_game_state(w)
            alarm=gs.get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state()
            if alarm.global_alarm!=expected_alarm or alarm.stage_active!=active:return False
            for p,expected in [(a,expected_a),(b,expected_b)]:
                copy=next(c for c in players(w) if pid(c)==pid(p))
                c=copy.get_editor_property('player_state').get_component_by_class(u.SPStealthPlayerStateComponent)
                if not c or c.is_identified()!=expected or c.get_identity_state().stage_id.to_string()!=alarm.stage_id.to_string():return False
        return True
    def effects():
        return (director.get_editor_property('first_identity_effects'),director.get_editor_property('first_alarm_effects'))
    def press(name,p=None,seconds=None):
        nonlocal held,active_player
        held=False;active_player=p or a
        button=buttons[name];pos=button.get_actor_location()
        for c in copies(active_player):
            c.set_actor_location(u.Vector(pos.x,pos.y-185,98),False,True)
            c.character_movement.stop_movement_immediately()
            if c.get_controller():c.get_controller().set_control_rotation(u.Rotator(yaw=90,pitch=-12.8))
        yield .4 # Camera manager must update before Started is injected.
        held=True
        duration=button.get_editor_property('interactable').get_editor_property('hold_duration')
        yield seconds if seconds is not None else max(.2,duration+.3)
        held=False
        yield .7
    def steps():
        yield from press('Stage_End')
        check('E End clears and deactivates stage on both peers',state(False,False,False,False))
        yield from press('Stage_Start')
        check('E Start activates a clean stage',state(False,False,False))
        for name in ['Laser','WorkNoise','Indirect']:
            yield from press('Incident_'+name)
            check('Remote E '+name+' remains anonymous without alarm',state(False,False,False) and '처리 완료' in director.get_editor_property('last_result'))
        yield from press('Incident_Victim')
        check('E victim report alarms without exposing either player',state(False,False,True) and effects()==(0,1))
        yield from press('Incident_Escape')
        check('E escape adds no repeated alarm effect',state(False,False,True) and effects()==(0,1))
        yield from press('Stage_Replay')
        check('E replay uses the same ID and produces Duplicate',effects()==(0,1) and '중복 사건' in director.get_editor_property('last_result'))
        yield from press('Stage_Restart')
        check('E Restart clears identities/alarm and test counters',state(False,False,False) and effects()==(0,0))
        yield from press('Incident_Rediscover')
        check('E rediscover rejects unknown player',state(False,False,False) and '아직 미발각' in director.get_editor_property('last_result'))
        yield from press('Incident_DirectReport',seconds=.6)
        check('Cancelling E report before server completion has no effect',state(False,False,False) and effects()==(0,0))
        yield from press('Incident_Sustained')
        check('Continuous E confirmation identifies remote A only',state(True,False,True) and effects()==(1,1))
        for name in ['Instant','DirectReport','Rediscover']:
            yield from press('Incident_'+name)
            check('E '+name+' accepted without repeated first effects',state(True,False,True) and effects()==(1,1) and '처리 완료' in director.get_editor_property('last_result'))
        yield from press('Stage_Replay')
        check('Direct replay leaves first effects unchanged',state(True,False,True) and effects()==(1,1) and '중복 사건' in director.get_editor_property('last_result'))
        yield from press('Incident_Instant',p=b)
        check('Host E identifies B separately; global alarm remains once',state(True,True,True) and effects()==(2,1))
        texts=[]
        for widget in u.ObjectIterator(u.SPStealthStatusWidget):
            if widget.get_owning_player() in [owner(a).get_controller(),owner(b).get_controller()]:
                texts.append(str(widget.get_status_text()).replace(' [나]',''))
        check('Host and participant HUD state and effect counters agree',len(texts)==2 and texts[0]==texts[1])
        report['hud_text']=texts
        yield from press('Stage_Restart')
        check('Final restart resets both exposed players',state(False,False,False) and effects()==(0,0))
    iterator=steps();remaining=next(iterator);last=u.GameplayStatics.get_time_seconds(server)
    def finish(error=None):
        nonlocal handle,held
        held=False
        if handle is not None:u.unregister_slate_post_tick_callback(handle);handle=None
        O.set_properties(performance,before)
        if error:report['error']=str(error)
        report['passed']=not error and all(c['passed'] for c in report['checks'])
        path=Path(u.Paths.project_saved_dir(),'StealthEvents','buttons-pie.json')
        path.write_text(json.dumps(report,indent=2),encoding='utf-8')
        u.log('STEALTH_BUTTON_PIE_COMPLETE: '+str(report['passed']))
    def tick(_):
        nonlocal remaining,last
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds):raise RuntimeError('PIE ended')
            if held:inputs[pid(active_player)].inject_input_vector_for_action(owner(active_player).get_editor_property('interact_action'),u.Vector(1,0,0),[],[])
            now=u.GameplayStatics.get_time_seconds(server);remaining-=max(0,now-last);last=now
            if remaining<=0:remaining=next(iterator)
        except StopIteration:finish()
        except Exception as exc:finish(exc)
    handle=u.register_slate_post_tick_callback(tick)
    u.log('STEALTH_BUTTON_PIE_STARTED')

run_button_checks()
