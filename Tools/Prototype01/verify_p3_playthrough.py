"""작성자 : 임진혁
2인 또는 4인 Ready PIE에서 실제 입력으로 여섯 객차를 통과하고 정산한다.
스폰 후 위치/시간/상처/점수를 바꾸지 않는 자동 플레이. 사람의 재미 평가는 별도로 수행한다.
결과: Saved/Prototype01/p3-playthrough.json.
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
output=Path(u.Paths.project_saved_dir(),'Prototype01/p3-playthrough.json')
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

# 판정 fixture 검사와 달리, 이 한 판에서는 스폰 뒤 위치/시간/상처를 조작하지 않는다.
# 경로와 화물 선택은 명시한 자동 플레이 정책이며 사람의 재미 평가를 대신하지 않는다.
jobs={}
def walk_party():
    for i,points in jobs.items():
        p=pawns()[i]; loc=p.get_actor_location()
        while points and math.hypot(points[0][0]-loc.x,points[0][1]-loc.y)<40: points.pop(0)
        if points:
            d=u.Vector(points[0][0],points[0][1],loc.z)-loc
            if p.character_movement.is_zero_gravity():
                # 기존 항력 0에서는 목표를 향한 계속 가속만으로 정지할 수 없다.
                # 실제 이동 입력의 역추진으로 감속한다. 속도/위치를 fixture로 덮어쓰지 않는다.
                p.get_controller().set_control_rotation(u.Rotator())
                distance=math.hypot(d.x,d.y); speed=min(280,distance*1.5)
                velocity=p.get_velocity(); error=u.Vector(d.x/distance*speed-velocity.x,d.y/distance*speed-velocity.y,0)*2
                forward=p.get_actor_forward_vector(); right=p.get_actor_right_vector()
                thrust_forward=max(-1,min(1,(error.x*forward.x+error.y*forward.y)/500))
                thrust_right=max(-1,min(1,(error.x*right.x+error.y*right.y)/500))
                subs[i].inject_input_vector_for_action(actions['Move'],u.Vector(thrust_right,thrust_forward,0),[],[])
            else:
                p.get_controller().set_control_rotation(u.Rotator(pitch=0,yaw=math.degrees(math.atan2(d.y,d.x))))
                inject(i,'Move')
    report.setdefault('play_samples',[]).append({'t':now(),'p':[p.get_actor_location().to_tuple() for p in pawns()],'w':[life(i).wounds for i in range(N)],'value':rounds[0].state.team_value})
def travel(routes,label,limit=60):
    jobs.clear(); jobs.update({i:list(points) for i,points in routes.items()})
    began=now()
    while any(jobs.values()) and now()-began<limit: yield wait(.1,label,walk_party)
    check(not any(jobs.values()),label,remaining=jobs,positions=[p.get_actor_location().to_tuple() for p in pawns()])
    report.setdefault('travel_times',[]).append({'stage':label,'seconds':now()-began})
    yield wait(.25,'release movement')
def grab(i,item):
    aim(i,item.get_interaction_point().to_tuple()); yield wait(.8,'aim cargo')
    yield wait(item.definition.hold_seconds+1,'hold cargo',lambda:track_cargo(i,item))
    check(item.transferred,'Play transfer '+str(item.cargo_id),reason=str(hold(i,True).local_reason))
    yield wait(.3,'release cargo key')
def suite():
    report['players']=N
    report['method']='Actual PIE round driven by continuous Enhanced Input after spawn. No teleport, timer, damage or score fixtures. Automated route and cargo choices.'
    if N==4:
        for w in worlds:
            for cmd in ['NetEmulation.PktLag 100','NetEmulation.PktLagVariance 20','NetEmulation.PktLoss 1']:
                u.SystemLibrary.execute_console_command(w,cmd,u.GameplayStatics.get_player_controller(w,0))
        report['emulation']={'outgoing_lag_ms':100,'variance_ms':20,'loss_percent':1}
    check('READY' in phase(),'Playthrough starts Ready')
    all_ready(); yield wait(1.2,'ready'); command(0,'RequestStart'); yield wait(1.2,'start')
    started=now()
    # 첫 객차 보급품을 확보하고 레이저는 걷기 우회한다.
    waiting=[(-12100,300),(-12250,-100),(-12400,100)]
    yield from travel({0:[(-11910,-300)],**{i:[waiting[i-1]] for i in range(1,N)}},'Entry supply approach')
    yield from grab(0,cargo('Supply'))
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90; safe=-380 if i%2==0 else -280
        routes[i]=[(-11500,lane),(-11000,lane),(-10800,lane),(-10500,safe),(-9350,safe),(-9150,lane),(-8750,lane),(-8400-160*(i//2),lane)]
    yield from travel(routes,'C01 to C03 through safe laser bypass')
    check(all(life(i).wounds==0 for i in range(N)),'Entire party walks around laser')
    # 다른 두 참가자가 패널을 함께 유지한다. 후발 참가자는 문 뒤에 막히지 않는다.
    routes={0:[(-7350,-175),(-7140,-175)],1:[(-7350,175),(-7140,175)]}
    routes.update({i:[(-7500-150*(i//2),-90 if i%2==0 else 90)] for i in range(2,N)})
    yield from travel(routes,'Approach cooperative panels')
    for i,kind in [(0,'LEFT'),(1,'RIGHT')]: aim(i,panel(kind).get_interaction_point().to_tuple())
    yield wait(.8,'panel aim'); began=now()
    yield wait(3,'cooperative gate',lambda:press(0,1)); check(gate().open,'Play gate opens'); report['gate_hold_seconds']=now()-began
    yield wait(.3,'release panels')
    # 바닥 합금을 먼저 챙긴 뒤 S01을 전송한다. 높은 선반은 남기고 빠른 탈출을 선택한다.
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90
        routes[i]=[(-6650,lane),(-6450,lane),(-6200,lane),(-6125,-300)] if i==0 else [(-6650,lane),(-6450,lane),(-6200-150*(i//2),lane)]
    yield from travel(routes,'Pass gate and enter S01 car')
    yield from grab(0,cargo('Alloy'))
    yield from travel({0:[(-5980,0),(-5830,0)]},'Approach S01')
    yield from grab(0,cargo('S01'))
    report['cargo_left_reasons']={'P01_C02_Parts':'보급 확보 뒤 걷기 우회를 유지하며 전진하는 정책을 선택했다.','P01_C04_HighCore':'바닥 합금을 먼저 확보했다. 상승·관성 제어와 선반 확보에 시간을 추가하지 않고 S01 전송 가치로 탈출하기로 선택했다.','P01_C05_Core':'감시 구역 안에서 멈춰 노출을 쌓는 추가 확보 대신 엄폐 사이로 통과하기로 선택했다.'}
    for p in pawns(): p.get_controller().set_control_rotation(u.Rotator())
    yield wait(1.2,'level body after S01')
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90
        routes[i]=[(-5000,lane),(-4700,lane),(-4480,lane),(-4200,lane),(-3950,-420),(-3530,-420),(-3360,-100),(-3010,-100),(-2690,lane),(-2490,lane),(-2210,lane),(-2000-150*(i//2),lane)]
    yield from travel(routes,'Zero-g boundary then camera cover route',90)
    check(all(not life(i).is_dead() for i in range(N)),'Party survives camera route')
    # 먼저 도착한 플레이어를 앞쪽에 세워 뒤 참가자의 목적지를 가로막지 않는다.
    targets=[(-560,-80),(-570,80),(-720,-80),(-730,80)]
    yield from travel({i:[targets[i]] for i in range(N)},'Enter safe extraction')
    exit_actor=u.GameplayStatics.get_all_actors_of_class(sw,u.SP1ExtractionZone)[0]
    aim(0,exit_actor.get_interaction_point().to_tuple()); yield wait(.8,'exit aim'); yield wait(1.8,'departure terminal',lambda:press(0))
    check('EXTRACTION_COUNTDOWN' in phase(),'Departure countdown begins')
    began=now(); yield wait(10.5,'ten second extraction')
    check('SUCCEEDED' in phase() and rounds[0].state.escaped==N,'Entire party completes actual round',value=rounds[0].state.final_value,escaped=rounds[0].state.escaped)
    check(rounds[0].state.final_value==550,'Selected cargo settled once')
    report['elapsed_seconds']=now()-started
    report['waiting_observation']='필수 장치 대기는 격벽 2초와 탈출 10초. 경로 이동 완료 뒤 입력 안정화 0.25~1.2초와 RPC 여유는 자동 플레이 도구의 대기이며 게임의 강제 대기가 아니다. 두 패널의 시작 시각을 맞추기 위한 별도 재시도는 없었다.'
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
        u.unregister_slate_post_tick_callback(handle); save(); u.log('P3_PLAY_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc(); u.unregister_slate_post_tick_callback(handle); save(); u.log_error('P3_PLAY_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P3_PLAY_STARTED')

