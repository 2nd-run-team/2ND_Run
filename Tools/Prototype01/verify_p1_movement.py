"""작성자 : 임진혁
P1 홀드의 실제 입력/이동 예측 검사. 두 PIE 월드의 Ready에서 실행. 에셋을 저장하지 않는다.
"""
import json, math, traceback
from pathlib import Path
import unreal as u
worlds=list(u.EditorLevelLibrary.get_pie_worlds(False))
worlds.sort(key=lambda w:not u.GameplayStatics.get_game_state(w).has_authority())
assert len(worlds)==2
sw=worlds[0]
rounds=[u.GameplayStatics.get_game_state(w).get_component_by_class(u.SP1RoundComponent) for w in worlds]
def pawns(): return [next(p for p in u.GameplayStatics.get_all_actors_of_class(w,u.Character) if p.is_locally_controlled()) for w in worlds]
def comp(i): return pawns()[i].get_component_by_class(u.SP1InteractionComponent)
def authoritative(i):
    ident=pawns()[i].player_state.player_id
    return next(p for p in u.GameplayStatics.get_all_actors_of_class(sw,u.Character) if p.player_state.player_id==ident)
def now(): return u.GameplayStatics.get_time_seconds(sw)
subs=[next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world()==w) for w in worlds]
actions={'E':u.load_asset('/Game/Input/Actions/IA_Interact')}
actions.update({n:u.load_asset('/Game/SpacePirate/Player/Input/IA_SP'+n) for n in ['Move','Jump','Sprint']})
report={'passed':False,'checks':[],'samples':[]}
def check(ok,label):
    report['checks'].append({'check':label,'passed':bool(ok)})
    assert ok,label
def inject(i,move=False):
    for name in (['E','Move','Jump','Sprint'] if move else ['E']):
        subs[i].inject_input_vector_for_action(actions[name],u.Vector(0,1,0) if name=='Move' else u.Vector(1,0,0),[],[])
def arrange(i):
    for role,xyz in [(1-i,(-2000,-300,308.15)),(i,(-1700,300,308.15))]:
        for p in set([pawns()[role],authoritative(role)]):
            p.character_movement.stop_movement_immediately(); p.set_actor_location(u.Vector(*xyz),False,True)
    p=pawns()[i]; target=next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1TransferCargo) if str(a.cargo_id).endswith('Alloy'))
    d=target.get_interaction_point()-p.get_component_by_class(u.CameraComponent).get_world_location()
    p.get_controller().set_control_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x)),roll=0))
def sample(i):
    inject(i,True)
    report['samples'].append({'role':i,'time':now(),'players':[{'authority':p.has_authority(),'max_speed':p.character_movement.get_max_speed(),'speed':p.get_velocity().length(),'z':p.get_actor_location().z,'mode':str(p.character_movement.movement_mode)} for p in [pawns()[i],authoritative(i)]]})
def steps():
    check('READY' in str(rounds[0].state.phase),'Ready before movement test')
    u.SP1PIETestLibrary.configure_timers(rounds[0],60,10)
    for i in [0,1]: u.SP1PIETestLibrary.next_tick(comp(i),'ToggleReady')
    yield .6,None
    u.SP1PIETestLibrary.next_tick(comp(0),'RequestStart')
    yield .6,None
    for i in [0,1]:
        arrange(i); yield .6,None
        yield .4,lambda:inject(i)
        check(authoritative(i).get_component_by_class(u.SP1InteractionComponent).attempt.active,f'{i} holding')
        yield .3,lambda:sample(i)
        check(authoritative(i).get_component_by_class(u.SP1InteractionComponent).attempt.active,f'{i} sprint/jump did not cancel hold')
        walk=pawns()[i].character_movement.max_walk_speed
        samples=[p for row in report['samples'] if row['role']==i for p in row['players']]
        check(all(abs(p['max_speed']-walk*.5)<.1 for p in samples),f'{i} local/server speed limit 50 percent')
        check(all(p['speed']<=walk*.5+5 and abs(p['z']-308.15)<1 and 'WALKING' in p['mode'] for p in samples),f'{i} sprint blocked and no jump')
        yield .5,None
        check(all(abs(p.character_movement.get_max_speed()-walk)<.1 for p in [pawns()[i],authoritative(i)]),f'{i} release restores walking')
    u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0)
    yield .6,None
    subs[1].inject_input_vector_for_action(actions['Jump'],u.Vector(1,0,0),[],[])
    yield .4,None
    check(all('NONE' in str(p.character_movement.movement_mode) for p in pawns()),'Terminal disables movement on both owners')
    u.SP1PIETestLibrary.next_tick(comp(0),'RequestRestart')
    yield 1,None
    check(all(comp(i).is_alive() for i in [0,1]),'Restart restores both movement components')
    report['passed']=True
sequence=steps(); step=None; end=0
def finish():
    u.unregister_slate_post_tick_callback(handle)
    Path(u.Paths.project_saved_dir(),'Prototype01/p1-movement.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def tick(delta):
    global step,end
    try:
        if step is None or now()>=end: step=next(sequence); end=now()+step[0]
        if step[1]: step[1]()
    except StopIteration: finish()
    except Exception: report['error']=traceback.format_exc(); finish()
handle=u.register_slate_post_tick_callback(tick)
