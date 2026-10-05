"""작성자 : 임진혁
Listen Server + Client, Run Under One Process PIE에서 실행한다.
Cmd: py "E:/GitHub/2ND_Run/Tools/Prototype01/verify_p1.py"
배치/시간은 테스트 fixture, E는 Enhanced Input 주입이다. 에셋은 저장하지 않는다.
실제 키보드, 별도 프로세스 종료, 패킷 손실 검사는 별도다.
"""
import json
import math
import traceback
from pathlib import Path
import unreal as u

worlds = list(u.EditorLevelLibrary.get_pie_worlds(False))
assert len(worlds) == 2
sw = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
cw = next(w for w in worlds if w != sw)
worlds = [sw, cw]
rounds = [u.GameplayStatics.get_game_state(w).get_component_by_class(u.SP1RoundComponent) for w in worlds]
assert all(rounds)
subsystems = [next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world() == w) for w in worlds]
action = u.load_asset('/Game/Input/Actions/IA_Interact')
jump = u.load_asset('/Game/SpacePirate/Player/Input/IA_SPJump')
output = Path(u.Paths.project_saved_dir(), 'Prototype01/p1-pie.json')
report = {'method':'Two PIE worlds, actual Enhanced Input E; server/local position and aim fixtures; no direct score or completion writes', 'checks':[], 'run_ids':[], 'passed':False}

def prop(o, name): return o.get_editor_property(name)
def run(i=0): return prop(rounds[i], 'state')
def value(name, i=0): return prop(run(i), name)
def phase(i=0): return str(value('phase', i))
def now(): return u.GameplayStatics.get_time_seconds(sw)
def locals(): return [next(p for p in u.GameplayStatics.get_all_actors_of_class(w,u.Character) if p.is_locally_controlled()) for w in worlds]
def inter(role): return locals()[role].get_component_by_class(u.SP1InteractionComponent)
def server_pawn(role):
    ident=prop(prop(locals()[role],'player_state'),'player_id')
    return next(p for p in u.GameplayStatics.get_all_actors_of_class(sw,u.Character) if prop(prop(p,'player_state'),'player_id')==ident)
def attempt(role): return prop(server_pawn(role).get_component_by_class(u.SP1InteractionComponent),'attempt')
def cargo(kind, world=sw): return next(a for a in u.GameplayStatics.get_all_actors_of_class(world,u.SP1TransferCargo) if str(prop(a,'cargo_id')).endswith(kind))
def zone(world=sw): return u.GameplayStatics.get_all_actors_of_class(world,u.SP1ExtractionZone)[0]
def place(role, xyz):
    for p in set([server_pawn(role), locals()[role]]):
        prop(p,'character_movement').stop_movement_immediately()
        p.set_actor_location(u.Vector(*xyz), False, True)
def aim(role, xyz):
    p=locals()[role]
    eye=p.get_component_by_class(u.CameraComponent).get_world_location()
    d=u.Vector(*xyz)-eye
    rot=u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))), yaw=math.degrees(math.atan2(d.y,d.x)),roll=0)
    p.get_controller().set_control_rotation(rot)
def aim_at(role, target): aim(role,target.get_interaction_point().to_tuple())
def inject(roles):
    for role in roles: subsystems[role].inject_input_vector_for_action(action,u.Vector(1,0,0),[],[])
def wait(seconds, label, during=None): return (seconds,label,during)
def check(ok, label, **details):
    report['checks'].append({'check':label,'passed':bool(ok),'server_time':now(),**details})
    assert ok, label + ' ' + str(details)
def snapshot():
    return {'states':[run(i).export_text() for i in [0,1]],'participants':[[p.export_text() for p in prop(r,'participants')] for r in rounds], 'attempts':[attempt(i).export_text() for i in [0,1]],'local_reasons':[str(prop(inter(i),'local_reason')) for i in [0,1]]}
def arrange(role):
    place(1-role,(-2000,-300,308.15))
    place(role,(-1660,300,308.15))
    aim_at(role,cargo('Alloy'))
def start(seconds=120):
    u.SP1PIETestLibrary.configure_timers(rounds[0],float(seconds),10.0)
    for role in [0,1]:
        ident=prop(prop(locals()[role],'player_state'),'player_id')
        row=next(p for p in prop(rounds[role],'participants') if prop(p,'player_id')==ident)
        if not prop(row,'ready'): u.SP1PIETestLibrary.next_tick(inter(role),'ToggleReady')
def move_cargo(kind, xyz):
    for w in worlds: cargo(kind,w).set_actor_location(u.Vector(*xyz),False,True)
wall_fixtures=[]
def wall(enabled):
    # 저장하지 않는 PIE 전용 가림 fixture. 기존 큐브 표식 하나를 잠시 얇은 벽으로 사용한다.
    if enabled:
        for w in worlds:
            a=next(a for a in u.GameplayStatics.get_all_actors_of_class(w,u.StaticMeshActor) if a.get_actor_label()=='P01_C01_ExitBorder_0')
            mesh=a.get_component_by_class(u.StaticMeshComponent)
            wall_fixtures.append((a,a.get_actor_transform(),mesh.get_collision_profile_name(),prop(mesh,'mobility')))
            mesh.set_mobility(u.ComponentMobility.MOVABLE)
            mesh.set_collision_profile_name('BlockAll')
            a.set_actor_location(u.Vector(-1580,300,315),False,True)
            a.set_actor_scale3d(u.Vector(.08,2,2))
    else:
        for a,transform,profile,mobility in wall_fixtures:
            a.set_actor_transform(transform,False,True)
            mesh=a.get_component_by_class(u.StaticMeshComponent)
            mesh.set_collision_profile_name(profile); mesh.set_mobility(mobility)
        wall_fixtures.clear()

def suite():
    if 'READY' not in phase():
        u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0)
        yield wait(.4,'reset previous test session')
        u.SP1PIETestLibrary.next_tick(inter(0),'RequestRestart')
        yield wait(1.2,'fresh test pawns')
    move_cargo('Alloy',(-1500,300,212)); move_cargo('Core',(-1050,300,212))
    check('READY' in phase(),'Initial Ready')
    report['run_ids'].append(value('run_id').export_text())
    start()
    yield wait(1.2,'Ready replication')
    report['ready_snapshot']=snapshot()
    u.SP1PIETestLibrary.next_tick(inter(0),'RequestStart')
    yield wait(.6,'Start replication')
    check(all('RUNNING' in phase(i) for i in [0,1]),'Both Running')
    # 각 역할이 동일한 입력/서버 검사를 통과해야 한다.
    for role in [0,1]:
        arrange(role)
        yield wait(.6,f'{role} aim settle')
        yield wait(.4,f'{role} E held',lambda:inject([role]))
        check(prop(attempt(role),'active'),f'{role} E starts hold',attempt=attempt(role).export_text())
        yield wait(.4,f'{role} E release')
        check(not prop(attempt(role),'active') and str(prop(attempt(role),'reason'))=='CanceledByInput',f'{role} release cancels')
        check(value('team_value')==0,f'{role} release no score')
        u.SP1PIETestLibrary.next_tick(inter(role),'BeginHold')
        yield wait(1.1,f'{role} heartbeat starvation')
        check(str(prop(attempt(role),'reason'))=='HeartbeatTimeout',f'{role} missing heartbeat cancels',attempt=attempt(role).export_text())
        yield wait(.4,f'{role} begin second input',lambda:inject([role]))
        aim(role,(-1660,-100,370))
        yield wait(.5,f'{role} look away',lambda:inject([role]))
        check(not prop(attempt(role),'active') and str(prop(attempt(role),'reason'))=='LookAway',f'{role} aim grace expires',attempt=attempt(role).export_text())
        yield wait(.25,'release')
        arrange(role)
        yield wait(.5,'aim settle')
        yield wait(.4,'start before wall',lambda:inject([role]))
        wall(True)
        yield wait(.4,'wall occlusion',lambda:inject([role]))
        check(not prop(attempt(role),'active') and str(prop(attempt(role),'reason'))=='Occluded',f'{role} wall cancels',attempt=attempt(role).export_text())
        wall(False)
        yield wait(.3,'release')
        place(role,(-1325,240,308.15))
        aim_at(role,cargo('Alloy'))
        yield wait(.5,'aim settle')
        yield wait(.4,'start before switching',lambda:inject([role]))
        # 다른 화물을 250 cm 안에 배치해 새 대상 조준을 만든다.
        move_cargo('Core',(-1200,300,212))
        aim_at(role,cargo('Core'))
        yield wait(.4,'switch target',lambda:inject([role]))
        check(not prop(attempt(role),'active'),f'{role} different target cancels',attempt=attempt(role).export_text(),local_reason=str(prop(inter(role),'local_reason')))
        move_cargo('Core',(-1050,300,212))
        yield wait(.3,'release')
    place(0,(-1645,230,308.15)); place(1,(-1360,230,308.15))
    for role in [0,1]: aim_at(role,cargo('Alloy'))
    yield wait(.7,'simultaneous aim settle')
    yield wait(1.4,'simultaneous independent progress',lambda:inject([0,1]))
    check(all(prop(attempt(i),'active') for i in [0,1]),'Two active attempts')
    check(value('team_value')==0 and not prop(cargo('Alloy'),'transferred'),'Progress is not summed')
    yield wait(1.5,'simultaneous finish',lambda:inject([0,1]))
    yield wait(.5,'transfer replication')
    check(all(value('team_value',i)==160 for i in [0,1]),'One credit replicated',states=[run(i).export_text() for i in [0,1]])
    check(all(prop(cargo('Alloy',w),'transferred') for w in worlds),'Transfer disappearance replicated')
    # 출발 입력 역할을 교대한다. 첫 판은 호스트가 나가고 클라이언트만 남는다.
    for iteration in range(5):
        if iteration:
            start(10 if iteration==3 else 40)
            yield wait(.5,'ready new round')
            u.SP1PIETestLibrary.next_tick(inter(0),'RequestStart')
            yield wait(.5,'running new round')
            place(0,(-1640,-300,308.15)); place(1,(-2000,300,308.15))
            aim_at(0,cargo('Supply'))
            yield wait(.5,'supply aim')
            yield wait(1.2,'supply hold',lambda:inject([0]))
            yield wait(.4,'supply replicated')
            check(value('team_value')==40,f'run {iteration} supply credited')
        starter=iteration%2
        place(starter,(-640,-70,308.15)); place(1-starter,(-1150,70,308.15))
        aim_at(starter,zone())
        yield wait(.6,'exit aim settle')
        if iteration==3:
            # 1초 홀드 완료 시 약 9초가 남는다. 출발 자체는 실제 10초다.
            u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],10)
        yield wait(1.5,'exit hold',lambda:inject([starter]))
        check('EXTRACTION_COUNTDOWN' in phase(),f'run {iteration} countdown started',state=run().export_text(),attempt=attempt(starter).export_text())
        place(1-starter,(-670,70,308.15))
        yield wait(.5,'other player enters exit')
        check(value('aboard')==2,f'run {iteration} joins during countdown')
        place(starter,(-1150,-70,308.15))
        if iteration==2: place(1-starter,(-1150,70,308.15))
        yield wait(.5,'leave exit')
        check(value('aboard')==(0 if iteration==2 else 1),f'run {iteration} leaves during countdown')
        yield wait(10,'departure or expiry')
        yield wait(.6,'result replication')
        expected='FAILED' if iteration in [2,3] else 'SUCCEEDED'
        check(all(expected in phase(i) for i in [0,1]),f'run {iteration} result {expected}',states=[run(i).export_text() for i in [0,1]])
        if iteration==2: check(value('reason')=='EmptyExtraction','Empty extraction fails')
        if iteration==3: check(value('reason')=='MissionExpired','9 seconds loses to 10 second departure')
        expected_value=0 if iteration in [2,3] else (160 if iteration==0 else 40)
        check(value('final_value')==expected_value,f'run {iteration} final value')
        old=run().export_text()
        for _ in range(3): u.SP1PIETestLibrary.next_tick(inter(0),'RequestStart'); u.SP1PIETestLibrary.next_tick(inter(1),'RequestStart'); u.SP1PIETestLibrary.next_tick(inter(1),'RequestRestart')
        yield wait(.4,'duplicate invalid result commands')
        check(run().export_text()==old,f'run {iteration} terminal immutable / remote restart denied')
        u.SP1PIETestLibrary.next_tick(inter(0),'RequestRestart'); u.SP1PIETestLibrary.next_tick(inter(0),'RequestRestart')
        yield wait(1.3,'restart respawn and replication')
        check(all('READY' in phase(i) for i in [0,1]),f'restart {iteration+1} Ready')
        ident=value('run_id').export_text()
        check(ident not in report['run_ids'],f'restart {iteration+1} unique RunId')
        report['run_ids'].append(ident)
        check(all(value('team_value',i)==0 and value('final_value',i)==0 for i in [0,1]),f'restart {iteration+1} zero value')
        check(all(not prop(a,'transferred') for w in worlds for a in u.GameplayStatics.get_all_actors_of_class(w,u.SP1TransferCargo)),f'restart {iteration+1} cargo reset')
        check(all(inter(i).is_alive() for i in [0,1]),f'restart {iteration+1} owned pawns alive')
    report['passed']=True

sequence=suite()
stage=None
stage_end=0
def save(): output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
def tick(delta):
    global stage,stage_end
    try:
        if stage is None or now()>=stage_end:
            stage=next(sequence)
            stage_end=now()+stage[0]
            report['current_stage']=stage[1]
            save()
        if stage[2]: stage[2]()
    except StopIteration:
        u.unregister_slate_post_tick_callback(handle)
        save(); u.log('P1_PIE_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc()
        try: report['failure_snapshot']=snapshot()
        except Exception: pass
        if wall_fixtures: wall(False)
        u.unregister_slate_post_tick_callback(handle)
        save(); u.log_error('P1_PIE_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P1_PIE_STARTED')
