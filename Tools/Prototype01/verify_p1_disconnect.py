"""작성자 : 임진혁
두 PIE 월드 Ready에서 클라이언트의 실제 disconnect를 발생시킨다. 마지막에 PIE를 다시 시작해야 한다.
"""
import json, math, traceback
from pathlib import Path
import unreal as u
worlds=list(u.EditorLevelLibrary.get_pie_worlds(False))
sw=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
cw=next(w for w in worlds if w!=sw)
round=u.GameplayStatics.get_game_state(sw).get_component_by_class(u.SP1RoundComponent)
lp=[next(p for p in u.GameplayStatics.get_all_actors_of_class(w,u.Character) if p.is_locally_controlled()) for w in [sw,cw]]
inputs=[p.get_component_by_class(u.SP1InteractionComponent) for p in lp]
client_id=lp[1].player_state.player_id
server_client=next(p for p in u.GameplayStatics.get_all_actors_of_class(sw,u.Character) if p.player_state.player_id==client_id)
remote_hold=server_client.get_component_by_class(u.SP1InteractionComponent)
sub=next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world()==cw)
action=u.load_asset('/Game/Input/Actions/IA_Interact')
cargo=next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1TransferCargo) if str(a.cargo_id).endswith('Alloy'))
report={'passed':False,'checks':[]}
def check(ok,label):
    report['checks'].append({'check':label,'passed':bool(ok)})
    assert ok,label
def now(): return u.GameplayStatics.get_time_seconds(sw)
def steps():
    check('READY' in str(round.state.phase),'Ready before disconnect test')
    u.SP1PIETestLibrary.configure_timers(round,60,10)
    for i in inputs: u.SP1PIETestLibrary.next_tick(i,'ToggleReady')
    yield .6,None
    u.SP1PIETestLibrary.next_tick(inputs[0],'RequestStart')
    yield .6,None
    lp[0].set_actor_location(u.Vector(-2000,-300,308.15),False,True)
    for p in [server_client,lp[1]]: p.set_actor_location(u.Vector(-1660,300,308.15),False,True)
    d=cargo.get_interaction_point()-lp[1].get_component_by_class(u.CameraComponent).get_world_location()
    lp[1].get_controller().set_control_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x)),roll=0))
    yield .6,None
    yield .4,lambda:sub.inject_input_vector_for_action(action,u.Vector(1,0,0),[],[])
    check(remote_hold.attempt.active,'Server accepted remote hold before disconnect')
    report['run_id']=round.state.run_id.export_text()
    report['attempt']=remote_hold.attempt.export_text()
    u.SystemLibrary.execute_console_command(cw,'disconnect',lp[1].get_controller())
    yield 4,None
    check(round.state.team_value==0 and not cargo.transferred,'Disconnect does not leave automatic completion')
    row=next(p for p in round.participants if p.player_id==client_id)
    check(not row.connected and not row.alive,'Server marks disconnected player absent')
    check(round.state.living==1,'Host remains alive')
    report['state']=round.state.export_text()
    report['passed']=True
sequence=steps(); step=None; end=0
def finish():
    u.unregister_slate_post_tick_callback(handle)
    Path(u.Paths.project_saved_dir(),'Prototype01/p1-disconnect.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def tick(delta):
    global step,end
    try:
        if step is None or now()>=end: step=next(sequence); end=now()+step[0]
        if step[1]: step[1]()
    except StopIteration: finish()
    except Exception: report['error']=traceback.format_exc(); finish()
handle=u.register_slate_post_tick_callback(tick)
