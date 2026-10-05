"""작성자 : 임진혁
2인 또는 4인 Ready PIE에서 실제 입력으로 열 객차를 통과하고 정산한다.
스폰 후 위치/시간/상처/점수를 바꾸지 않는 자동 플레이. 사람의 재미 평가는 별도로 수행한다.
결과: Saved/Prototype01/p4-playthrough.json.
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
output=Path(u.Paths.project_saved_dir(),'Prototype01/p4-playthrough.json')
report={'passed':False,'checks':[],'players':N,'method':'Continuous Enhanced Input after spawn; no position/time/damage/score fixtures'}
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
def aim(i,point):
    d=u.Vector(*point)-pawns()[i].get_component_by_class(u.CameraComponent).get_world_location()
    pawns()[i].get_controller().set_control_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x)),roll=0))
def inject(i,*names):
    for name in names: subs[i].inject_input_vector_for_action(actions[name],u.Vector(0,1,0) if name=='Move' else u.Vector(1,0,0),[],[])
def check(ok,label,**data):
    report['checks'].append({'check':label,'passed':bool(ok),'time':now(),**data})
    assert ok,label+' '+str(data)
def wait(seconds,label,callback=None): return seconds,label,callback
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
    check(not any(jobs.values()),label,remaining={i:list(points) for i,points in jobs.items()},positions=[p.get_actor_location().to_tuple() for p in pawns()])
    report.setdefault('travel_times',[]).append({'stage':label,'seconds':now()-began})
    yield wait(.25,'release movement')
def grab(i,item):
    aim(i,item.get_interaction_point().to_tuple()); yield wait(.8,'aim cargo')
    yield wait(item.definition.hold_seconds+1,'hold cargo',lambda:track_cargo(i,item))
    check(item.transferred,'Play transfer '+str(item.cargo_id),reason=str(hold(i,True).local_reason))
    yield wait(.3,'release cargo key')

def track_cargo(i,item):
    aim(i,item.get_interaction_point().to_tuple());inject(i,'E')

def suite():
    check('READY' in phase(),'Start Ready')
    for w in worlds:u.SystemLibrary.execute_console_command(w,'NetEmulation.Off',u.GameplayStatics.get_player_controller(w,0))
    for i in range(N):command(i,'ToggleReady')
    yield wait(1.2,'ready');command(0,'RequestStart');yield wait(1.2,'start');started=now()
    def cargo_id(car,role):return next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1TransferCargo) if str(a.cargo_id)=='P01_C%02d_%s_01'%(car,role))
    centers={i:-1250-(10-i)*2160 for i in range(1,11)}
    routes={0:[(-21140,-300)]}
    routes.update({i:[(-21300-(i//2)*150,120 if i%2 else -100)] for i in range(1,N)})
    yield from travel(routes,'C01 supply approach');yield from grab(0,cargo_id(1,'Supply'))
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90;safe=-380 if i%2==0 else -280
        routes[i]=[(-20140,lane),(-19640,lane),(-19440,lane),(-19140,safe),(-17990,safe),(-17790,lane),(-17390,lane),(-17040-160*(i//2),lane)]
    yield from travel(routes,'C01 to C03 walking laser bypass')
    check(all(life(i).wounds==0 for i in range(N)),'C02 safe path avoids laser')
    routes={0:[(-15990,-175),(-15780,-175)],1:[(-15990,175),(-15780,175)]}
    routes.update({i:[(-16290-150*(i//2),-90 if i%2==0 else 90)] for i in range(2,N)})
    yield from travel(routes,'Cooperative panel approach')
    panels=u.GameplayStatics.get_all_actors_of_class(sw,u.SP1Panel)
    for i,kind in [(0,'LEFT'),(1,'RIGHT')]:aim(i,next(a for a in panels if kind in str(a.kind)).get_interaction_point().to_tuple())
    yield wait(.8,'panel aim');yield wait(3,'two player hold',lambda:[inject(i,'E') for i in [0,1]])
    check(u.GameplayStatics.get_all_actors_of_class(sw,u.SP1Bulkhead)[0].open,'Cooperative gate opens');yield wait(.3,'release E')
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90;wide=-180 if i%2==0 else 180
        routes[i]=[(-15290,lane),(-15090,lane),(-14850,wide),(-13700,wide),(-13320,lane),(-12890,lane),(-12550-150*(i//2),lane)]
    yield from travel(routes,'C04 ground route to C05 maintenance')
    check('ACTIVE' in str(rounds[0].state.maintenance.phase),'Actual first entry starts maintenance')
    frozen=rounds[0].state.maintenance.frozen_mission_remaining
    yield wait(2,'Read forward reward sign while mission paused')
    check(abs(rounds[0].state.maintenance.frozen_mission_remaining-frozen)<.01,'Mission clock paused during regroup')
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90
        routes[i]=[(-11650,lane),(-11200,lane),(-10850,lane),(-10430,-420),(-10010,-420),(-9840,-100),(-9490,-100),(-9170,lane),(-8970,lane),(-8690,lane),(-8450-150*(i//2),lane)]
    yield from travel(routes,'C06 camera cover path closes maintenance',90)
    check('CLOSED' in str(rounds[0].state.maintenance.phase) and str(rounds[0].state.maintenance.close_reason)=='SurvivorAdvanced','First advance closes maintenance for entire party')
    routes={}
    for i in range(N):
        lane=-90 if i%2==0 else 90;wide=-180 if i%2==0 else 180;safe=380 if i%2==0 else 280
        # 네 캡슐을 한 점에 세우면 선두가 후발의 도착을 막는다. 대기점만 서로 떨어뜨린다.
        routes[i]=[(-8250,lane),(-7190,lane),(-6760,lane),(-6460,lane),(-6300,wide),(-5070,wide),(-4690,lane),(-4380,lane),(-4030,safe),(-2810,safe),(-2570,lane),(-2320,lane),(-2060,90),(-1660-140*(i//2),90+(100*(i%2) if N==4 else 0))]
    yield from travel(routes,'C07 cargo / C08 ground / C09 opposite safe path / engine entry',120)
    check(all(not life(i).is_dead() for i in range(N)),'All players survive ten-car route')
    targets=[(-760,-100),(-760,100),(-950,-100),(-950,100)]
    yield from travel({i:[targets[i]] for i in range(N)},'Four capsule positions in final exit')
    exit_actor=next(a for a in u.GameplayStatics.get_all_actors_of_class(sw,u.SP1ExtractionZone) if not a.intermediate)
    aim(0,exit_actor.get_interaction_point().to_tuple());yield wait(.8,'final aim');yield wait(1.8,'final departure',lambda:inject(0,'E'))
    check('EXTRACTION_COUNTDOWN' in phase(),'Final extraction starts')
    check(rounds[0].state.aboard==N,'All capsule centres aboard')
    yield wait(10.8,'final result')
    check('SUCCEEDED' in phase() and rounds[0].state.escaped==N and rounds[0].state.final_value==40,'Ten-car final extraction settles transmitted supply')
    report['elapsed_seconds']=now()-started;report['run_id']=rounds[0].state.run_id.export_text()
    report['mission_consumed_seconds']=rounds[0].state.mission_limit-(rounds[0].state.mission_deadline-now())
    report['wounds']=[life(i).wounds for i in range(N)]
    report['cargo_left_reason']='통과성 검사 정책: 첫 보급 40만 확보하고 두 S01의 무중력/높은 선반과 나머지 화물은 남겼다. 정비에서 2초 합류 후 최종 기관실까지 전진했다. 사람의 선택/재미 평가가 아니다.'
    report['passed']=True

sequence=suite();stage=None;end=0
def save():output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
    global stage,end
    try:
        if stage is None or now()>=end:stage=next(sequence);end=now()+stage[0];report['current_stage']=stage[1];save()
        if stage[2]:stage[2]()
    except StopIteration:u.unregister_slate_post_tick_callback(handle);save();u.log('P4_PLAY_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc();u.unregister_slate_post_tick_callback(handle);save();u.log_error('P4_PLAY_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
