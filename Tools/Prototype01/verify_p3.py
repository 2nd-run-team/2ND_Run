"""작성자 : 임진혁
P3 두 명/네 명의 실제 Enhanced Input, 소유 Pawn RPC, 장치와 중력 경계를 검사한다.
위치·시간·서버 피해만 테스트 fixture로 만들며 자산이나 점수를 직접 쓰지 않는다.
Listen Server + Client의 Ready에서 실행. 결과는 Saved/Prototype01/p3-pie.json 또는 p3-playthrough.json.
"""
import json, math, traceback
from pathlib import Path
import unreal as u

worlds=list(u.EditorLevelLibrary.get_pie_worlds(False))
worlds.sort(key=lambda w:not u.GameplayStatics.get_game_state(w).has_authority())
assert len(worlds) in [2,4]
N=len(worlds)
sw=worlds[0]
rounds=[u.GameplayStatics.get_game_state(w).get_component_by_class(u.SP1RoundComponent) for w in worlds]
subs=[next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world()==w) for w in worlds]
actions={'E':u.load_asset('/Game/Input/Actions/IA_Interact')}
actions.update({n:u.load_asset('/Game/SpacePirate/Player/Input/IA_SP'+n) for n in ['Move','Jump','Sprint','Look']})
output=Path(u.Paths.project_saved_dir(),'Prototype01/p3-pie.json')
report={'passed':False,'checks':[],'sprint_samples':[],'laser_samples':[],'safe_samples':[],'method':'Two PIE worlds; Enhanced Input + owned Pawn RPC; server impact/position fixtures'}
def prop(o,n): return o.get_editor_property(n)
def now(): return u.GameplayStatics.get_time_seconds(sw)
def pawns(): return [next(p for p in u.GameplayStatics.get_all_actors_of_class(w,u.Character) if p.is_locally_controlled()) for w in worlds]
def server(i): return next(p for p in u.GameplayStatics.get_all_actors_of_class(sw,u.Character) if p.player_state.player_id==pawns()[i].player_state.player_id)
def life(i,auth=True): return (server(i) if auth else pawns()[i]).get_component_by_class(u.SP1SurvivalComponent)
def hold(i,auth=False): return (server(i) if auth else pawns()[i]).get_component_by_class(u.SP1InteractionComponent)
def inventory(p): return p.get_component_by_class(u.SPInventoryComponent)
def phase(): return str(rounds[0].state.phase)
def cargo(kind): return next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1TransferCargo) if str(a.cargo_id).endswith(kind))
def lasers(): return [u.GameplayStatics.get_all_actors_of_class(w,u.SP1LaserHazard)[0] for w in worlds]
def command(i,name): u.SP1PIETestLibrary.next_tick(hold(i),name)
def wound(i,amount=20): u.SP1PIETestLibrary.wound_next_tick(server(i),amount)
def place(i,xyz):
    for p in set([server(i),pawns()[i]]):
        p.character_movement.stop_movement_immediately(); p.set_actor_location(u.Vector(*xyz),False,True)
def aim(i,point):
    d=u.Vector(*point)-pawns()[i].get_component_by_class(u.CameraComponent).get_world_location()
    pawns()[i].get_controller().set_control_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x)),roll=0))
def inject(i,*names):
    for name in names: subs[i].inject_input_vector_for_action(actions[name],u.Vector(0,1,0) if name=='Move' else u.Vector(1,0,0),[],[])
def check(ok,label,**data):
    report['checks'].append({'check':label,'passed':bool(ok),'time':now(),**data})
    assert ok,label+' '+str(data)
def wait(seconds,label,callback=None): return seconds,label,callback
def start():
    u.SP1PIETestLibrary.configure_timers(rounds[0],300,10)
    for i in [0,1]: command(i,'ToggleReady')
def safe_park():
    place(0,(-1800,-120,308.15)); place(1,(-2000,180,308.15))
def restart(): command(0,'RequestRestart')
def sprint(i):
    p=pawns()[i]; x=p.get_actor_location().x; yaw=p.get_controller().get_control_rotation().yaw
    if x>-1200: yaw=180
    if x<-1920: yaw=0
    p.get_controller().set_control_rotation(u.Rotator(pitch=0,yaw=yaw,roll=0))
    inject(i,'Move','Sprint')
    report['sprint_samples'].append({'role':i,'t':now(),'w':life(i).wounds,'s':life(i).get_stamina(),'local_s':life(i,False).get_stamina(),'exhausted':life(i).is_exhausted(),'speed':p.get_velocity().length(),'max_speed':p.character_movement.get_max_speed(),'position':p.get_actor_location().to_tuple()})
def sample_laser(i):
    pair=lasers()
    report['laser_samples'].append({'role':i,'t':now(),'w':life(i).wounds,'local_w':life(i,False).wounds,'phases':[str(a.get_phase()) for a in pair],'displayed':[str(a.displayed_phase) for a in pair],'active_beams':[sum(b.is_active() for b in a.beams) for a in pair],'epochs':[a.cycle_epoch for a in pair],'clocks':[u.GameplayStatics.get_game_state(w).get_server_world_time_seconds() for w in worlds]})
route=[]
def walk_route(i):
    p=pawns()[i]
    while route and math.hypot(route[0][0]-p.get_actor_location().x,route[0][1]-p.get_actor_location().y)<25: route.pop(0)
    if route:
        d=u.Vector(*route[0])-p.get_actor_location()
        p.get_controller().set_control_rotation(u.Rotator(pitch=0,yaw=math.degrees(math.atan2(d.y,d.x)),roll=0)); inject(i,'Move')
    report['safe_samples'].append({'role':i,'p':p.get_actor_location().to_tuple(),'w':life(i).wounds,'mode':str(p.character_movement.movement_mode)})

def gate(w=sw): return u.GameplayStatics.get_all_actors_of_class(w,u.SP1Bulkhead)[0]
def camera(w=sw): return u.GameplayStatics.get_all_actors_of_class(w,u.SP1SecurityCamera)[0]
def panel(kind): return next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1Panel) if kind in str(a.kind))
def park():
    for i in range(N): place(i,(-12300,-300+i*150,308.15))
def prepare_panel(i,kind):
    a=panel(kind); loc=a.get_actor_location(); place(i,(loc.x-150,loc.y,308.15)); aim(i,a.get_interaction_point().to_tuple())
def all_ready():
    for i in range(N): command(i,'ToggleReady')
def press(*indices):
    for i in indices: inject(i,'E')
def track_cargo(i,item):
    aim(i,item.get_interaction_point().to_tuple()); press(i)
def aim_cargo(i,item):
    pos=item.get_actor_location(); place(i,(pos.x-160,pos.y,308.15)); aim(i,item.get_interaction_point().to_tuple())
def end_run(): u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0)
def follow_camera(i):
    c=camera(); angle=math.radians(c.get_aim(rounds[0].now() if hasattr(rounds[0],'now') else u.GameplayStatics.get_game_state(sw).get_server_world_time_seconds()).yaw)
    # 위치는 테스트 fixture. 실제 노출·예고·피해 판정과 RPC는 제품 경로를 실행한다.
    for distance in [520,420,620]:
        loc=c.get_actor_location(); pos=(loc.x+math.cos(angle)*distance,loc.y+math.sin(angle)*distance,308.15)
        place(i,pos)
        if c.can_see(server(i),u.GameplayStatics.get_game_state(sw).get_server_world_time_seconds()): return
def camera_sample():
    report.setdefault('camera_samples',[]).append({'t':now(),'phase':[str(camera(w).state.phase) for w in worlds], 'deadline':[camera(w).state.deadline for w in worlds], 'w':[life(i).wounds for i in range(N)],'local_w':[life(i,False).wounds for i in range(N)]})
def await_warning(i):
    if 'SCANNING' in str(camera().state.phase): follow_camera(i)
    camera_sample()

def suite():
    report['players']=N
    if N==4:
        for w in worlds:
            for cmd in ['NetEmulation.PktLag 100','NetEmulation.PktLagVariance 20','NetEmulation.PktLoss 1']:
                u.SystemLibrary.execute_console_command(w,cmd,u.GameplayStatics.get_player_controller(w,0))
        report['all_devices_under_emulation']=True
    # 기존 입력과 충돌을 포함해, 먼저 시작한 패널을 3초 대기시킨 후 다른 패널을 합류시킨다.
    for role in [0,1]:
        check('READY' in phase(),'Ready before devices '+str(role))
        all_ready(); yield wait(.8,'ready'); command(0,'RequestStart'); yield wait(.8,'running'); park()
        prepare_panel(role,'LEFT'); prepare_panel(1-role,'BYPASS'); yield wait(.7,'aim')
        yield wait(3.1,'first panel waits without one-second deadline',lambda:press(role))
        check(hold(role,True).attempt.active and not gate().open and gate().progress_started_at<0,'Panel waits for partner '+str(role),reason=str(hold(role,True).attempt.reason))
        yield wait(.5,'conflicting bypass rejected',lambda:press(role,1-role))
        check(not hold(1-role,True).attempt.active and str(hold(1-role,True).attempt.reason)=='PanelsInUse','Coop route excludes bypass '+str(role))
        yield wait(.3,'release both'); prepare_panel(1-role,'RIGHT'); yield wait(.5,'opposite panel aim')
        yield wait(1.1,'together not yet complete',lambda:press(role,1-role))
        check(not gate().open,'No early gate completion '+str(role))
        yield wait(.6,'partner releases resets common progress',lambda:press(role))
        check(not gate().open and gate().progress_started_at<0,'Released partner resets joint progress '+str(role))
        yield wait(1.1,'rejoin starts a fresh two seconds',lambda:press(role,1-role))
        check(not gate().open,'Old progress not added to new pair '+str(role))
        yield wait(1.6,'two seconds simultaneous valid hold',lambda:press(role,1-role))
        check(all(gate(w).open for w in worlds),'Permanent gate open replicated '+str(role))
        check(not hold(role,True).attempt.active and not hold(1-role,True).attempt.active,'Both attempts finish once '+str(role))
        # S01 완료가 실제 기존 중력 영역 API를 변경하고 이웃 칸/연결 통로는 중력을 유지한다.
        s=cargo('S01'); aim_cargo(role,s); yield wait(.8,'S01 aim')
        yield wait(3.7,'S01 transfer',lambda:press(role)); yield wait(.5,'replication')
        check(s.transferred and rounds[0].state.team_value==350,'S01 transferred once for 350 '+str(role),reason=str(hold(role,True).attempt.reason))
        check(all('ZERO_GRAVITY' in str(u.GameplayStatics.get_all_actors_of_class(w,u.SP1GravityCargo)[0].gravity_zone.get_gravity_mode()) for w in worlds),'C04 zero gravity replicated '+str(role))
        for x,zero in [(-5600,True),(-6560,False),(-6850,False),(-4520,False),(-5560,True)]:
            place(role,(x,0,308.15)); yield wait(.8,'gravity boundary '+str(x))
            check(pawns()[role].character_movement.is_zero_gravity()==zero and server(role).character_movement.is_zero_gravity()==zero,'Gravity / reentry '+str(role)+' '+str(x))
        place(role,(-5400,300,400)); pawns()[role].get_controller().set_control_rotation(u.Rotator())
        yield wait(1.2,'zero-g body levels before vertical thrust'); z=pawns()[role].get_actor_location().z
        yield wait(.6,'Space rises in zero gravity',lambda:inject(role,'Jump'))
        check(pawns()[role].get_actor_location().z>z+30,'Actual zero-g upward input '+str(role))
        yield wait(2.5,'release thrust and settle at roof'); high=cargo('HighCore'); aim(role,high.get_interaction_point().to_tuple())
        yield wait(.7,'high cargo aim'); yield wait(3.2,'high shelf hold',lambda:track_cargo(role,high)); yield wait(.5,'high result')
        check(high.transferred,'High shelf reward reachable '+str(role),pos=pawns()[role].get_actor_location().to_tuple(),reason=str(hold(role,True).attempt.reason))
        park(); end_run(); yield wait(.6,'end run'); restart(); yield wait(1.2,'restart')
        check(not gate().open and not cargo('S01').transferred and all('GRAVITY:' in str(u.GameplayStatics.get_all_actors_of_class(w,u.SP1GravityCargo)[0].gravity_zone.get_gravity_mode()) for w in worlds),'New run resets devices '+str(role))
    # 마지막 생존자의 필수 우회 경로. 협동 경로가 시작되지 않으면 즉시 12초를 쓸 수 있다.
    all_ready(); yield wait(.7,'ready solo'); command(0,'RequestStart'); yield wait(.7,'running solo'); park()
    prepare_panel(0,'LEFT'); yield wait(.6,'panel before death'); yield wait(.7,'dying holder',lambda:press(0))
    for i in range(N-1): wound(i,100)
    yield wait(.7,'last survivor'); role=N-1
    prepare_panel(role,'BYPASS'); yield wait(.6,'bypass aim'); yield wait(11.3,'solo twelve second hold',lambda:press(role))
    check(not gate().open and rounds[0].state.living==1,'Last survivor bypass not early')
    yield wait(1.5,'solo completes',lambda:press(role))
    check(all(gate(w).open for w in worlds),'Last survivor permanent bypass')
    park(); end_run(); yield wait(.6,'solo end'); restart(); yield wait(1.2,'solo reset')
    # 지연 환경은 이 제품과 분리된 UE NetEmulation 기능으로 실제 연결에 적용한다.
    for w in worlds:
        for cmd in ['NetEmulation.PktLag 100','NetEmulation.PktLagVariance 20','NetEmulation.PktLoss 1']:
            u.SystemLibrary.execute_console_command(w,cmd,u.GameplayStatics.get_player_controller(w,0))
    report['emulation']={'outgoing_lag_ms':100,'variance_ms':20,'loss_percent':1}
    all_ready(); yield wait(1.2,'ready camera latency'); command(0,'RequestStart'); yield wait(1.2,'camera running'); park()
    for role in [0,1]:
        # 다음 칸의 참가자는 사거리 안이라도 피해 후보에서 제외된다.
        place(1-role,(-2320,0,308.15)); previous=life(role).wounds
        for _ in range(150):
            if 'WARNING' in str(camera().state.phase): break
            yield wait(.1,'seek camera warning',lambda:await_warning(role))
        check('WARNING' in str(camera().state.phase),'Server warning begins '+str(role))
        warn=now(); deadline=camera().state.deadline
        yield wait(.65,'one second warning before damage',camera_sample)
        check(life(role).wounds==previous,'Warning does not damage early '+str(role))
        yield wait(1,'camera pulse and delayed replication',camera_sample)
        check(life(role).wounds==previous+20 and life(role,False).wounds==previous+20,'Camera wound agrees after latency '+str(role))
        check(life(1-role).wounds==0 if role==0 else life(1-role).wounds==20,'Next car receives no camera damage '+str(role))
        check('COOLDOWN' in str(camera().state.phase),'Camera cooldown begins '+str(role))
        check(deadline-warn>.85,'Full server warning interval '+str(role),seconds=deadline-warn)
        park(); yield wait(6.2,'cooldown reset',camera_sample)
    # 발각된 뒤 엄폐하면 타격 순간의 가림 검사로 피해를 피한다.
    for _ in range(150):
        if 'WARNING' in str(camera().state.phase): break
        yield wait(.1,'warning for cover',lambda:await_warning(1))
    before=life(1).wounds; place(1,(-3320,340,308.15)); yield wait(1.8,'cover during warning',camera_sample)
    check(life(1).wounds==before,'Cover during warning avoids damage')
    park(); yield wait(6.2,'camera returns to scanning')
    c=camera(); clock=u.GameplayStatics.get_game_state(sw).get_server_world_time_seconds()
    # 편집값이 바뀌어 카메라가 출구 쪽을 보더라도 객차 경계가 피해를 차단해야 한다.
    original=c.get_actor_rotation(); c.set_actor_rotation(u.Rotator(yaw=original.yaw-c.get_aim(clock).yaw),True)
    place(0,(-2550,0,308.15)); place(1,(-2240,0,308.15))
    check(c.can_see(server(0),clock) and not c.can_see(server(1),clock),'Region excludes next car even when cone points toward it')
    c.set_actor_rotation(original,True)
    park(); end_run(); yield wait(.7,'finish camera run')
    report['passed']=True

sequence=suite(); stage=None; end=0
def save(): output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
    global stage,end
    try:
        if stage is None or now()>=end:
            stage=next(sequence); end=now()+stage[0]; report['current_stage']=stage[1]; save()
        if stage[2]: stage[2]()
    except StopIteration:
        u.unregister_slate_post_tick_callback(handle); save(); u.log('P3_PIE_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc(); u.unregister_slate_post_tick_callback(handle); save(); u.log_error('P3_PIE_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P3_PIE_STARTED')

