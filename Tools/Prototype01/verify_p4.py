"""작성자 : 임진혁
P4 Listen Server + 1/3 clients. 실제 Enhanced Input/소유 Pawn RPC를 사용한다.
위치·피해·경계 시각만 명시적인 PIE fixture다. 점수/완료/부활은 제품 서버 경로로 판정한다.
"""
import json,math,traceback
from pathlib import Path
import unreal as u
worlds=list(u.EditorLevelLibrary.get_pie_worlds(False));worlds.sort(key=lambda w:not u.GameplayStatics.get_game_state(w).has_authority())
N=len(worlds);assert N in [2,4];sw=worlds[0]
rounds=[u.GameplayStatics.get_game_state(w).get_component_by_class(u.SP1RoundComponent) for w in worlds]
pcs=[u.GameplayStatics.get_player_controller(w,0) for w in worlds]
subs=[next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world()==w) for w in worlds]
actions={'E':u.load_asset('/Game/Input/Actions/IA_Interact')}
actions.update({n:u.load_asset('/Game/SpacePirate/Player/Input/IA_SP'+n) for n in ['Move','Jump','Sprint']})
report={'passed':False,'players':N,'checks':[],'runs':[],'method':'Enhanced Input + owned Pawn RPC; explicit position, damage and boundary-time fixtures'}
output=Path(u.Paths.project_saved_dir(),'Prototype01/p4-pie-%d.json'%N)
centers={i:-1250-(10-i)*2160 for i in range(1,11)}
def now():return u.GameplayStatics.get_game_state(sw).get_server_world_time_seconds()
def pawn(i):return u.GameplayStatics.get_player_pawn(worlds[i],0)
def server(i):return next(p for p in u.GameplayStatics.get_all_actors_of_class(sw,u.Character) if p.player_state and p.player_state.player_id==pcs[i].player_state.player_id and p.get_controller())
def life(i,auth=True):return (server(i) if auth else pawn(i)).get_component_by_class(u.SP1SurvivalComponent)
def hold(i,auth=False):return (server(i) if auth else pawn(i)).get_component_by_class(u.SP1InteractionComponent)
def participant(i):return next(p for p in rounds[0].participants if p.player_id==pcs[i].player_state.player_id)
def state():return rounds[0].state
def phase():return str(state().phase)
def maintenance():return str(state().maintenance.phase)
def allof(cls,w=sw):return u.GameplayStatics.get_all_actors_of_class(w,cls)
def station(w=sw):return allof(u.SP1MaintenanceStation,w)[0]
def exit_zone(mid,w=sw):return next(a for a in allof(u.SP1ExtractionZone,w) if a.intermediate==mid)
def cargo(car,role,index=1):return next(a for a in allof(u.SP1TransferCargo) if str(a.cargo_id)=='P01_C%02d_%s_%02d'%(car,role,index))
def command(i,name):u.SP1PIETestLibrary.next_tick(hold(i),name)
def wound(i,amount):u.SP1PIETestLibrary.wound_next_tick(server(i),amount)
def place(i,xyz):
    for p in set([server(i),pawn(i)]):p.character_movement.stop_movement_immediately();p.set_actor_location(u.Vector(*xyz),False,True)
def aim(i,point):
    d=u.Vector(*point)-pawn(i).get_component_by_class(u.CameraComponent).get_world_location()
    pcs[i].set_control_rotation(u.Rotator(pitch=math.degrees(math.atan2(d.z,math.hypot(d.x,d.y))),yaw=math.degrees(math.atan2(d.y,d.x))))
def inject(i,name):subs[i].inject_input_vector_for_action(actions[name],u.Vector(0,1,0) if name=='Move' else u.Vector(1,0,0),[],[])
def press(*ids):
    for i in ids:inject(i,'E')
def track(i,target):aim(i,target.get_interaction_point().to_tuple());press(i)
def check(ok,label,**data):
    report['checks'].append({'check':label,'passed':bool(ok),'t':now(),**data});assert ok,label+' '+str(data)
def wait(seconds,label,callback=None):return seconds,label,callback
def remaining():return state().maintenance.frozen_mission_remaining if 'ACTIVE' in maintenance() else max(0,state().mission_deadline-now())
def park():
    for i in range(N):
        if not life(i).is_dead():place(i,(centers[1]-300,-300+i*180,308.15))
def prepare_station(i,side=0):
    p=station().get_actor_location();place(i,(p.x-150,p.y+(-80 if side==0 else 80),308.15));aim(i,station().get_interaction_point().to_tuple())
def prepare_exit(i,mid,offset=0):
    z=exit_zone(mid);p=z.get_actor_location();place(i,(p.x+50,p.y+offset,308.15));aim(i,z.get_interaction_point().to_tuple())
def reset_checks(label):
    check('READY' in phase() and 'UNUSED' in maintenance(),label+' ready and unused')
    check(state().team_value==0 and str(state().departure_id)=='None' and rounds[0].extraction is None,label+' value / departure cleared')
    check(all(not a.transferred for a in allof(u.SP1TransferCargo)) and len(allof(u.SP1TransferCargo))==42,label+' all 42 cargo reset')
    check(all('GRAVITY' in str(a.gravity_zone.get_gravity_mode()) and 'ZERO' not in str(a.gravity_zone.get_gravity_mode()) for a in allof(u.SP1GravityCargo)),label+' both gravity zones reset')
    check(not allof(u.SP1Bulkhead)[0].open and all(life(i).wounds==0 for i in range(N)),label+' gate and wounds reset')
    check(all(not p.maintenance_revived for p in rounds[0].participants),label+' revive allowances cleared')
def start_run():
    for i in range(N):command(i,'ToggleReady')
    yield wait(1,'ready');command(0,'RequestStart');yield wait(1,'running')
    check('RUNNING' in phase(),'Valid scenario can start',reason=str(state().reason))
    report['runs'].append(state().run_id.export_text());park()
def finish_restart(label):
    # 활성 정비 중에는 먼저 C06 진입으로 시계를 재개한다.
    if 'ACTIVE' in maintenance():
        i=next(i for i in range(N) if not life(i).is_dead());place(i,(centers[6]-750,350,308.15));yield wait(.5,'close before fixture expiry')
    u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0);yield wait(.8,'finish')
    command(0,'RequestRestart');yield wait(1.4,'restart');reset_checks(label)

def suite():
    for w in worlds:u.SystemLibrary.execute_console_command(w,'NetEmulation.Off',u.GameplayStatics.get_player_controller(w,0))
    check(all(len(allof(u.Character,w))==N for w in worlds),'Each world sees every player')
    check(state().mission_limit==(600 if N==2 else 420),'Automatic separated party preset',settings=str(state().settings_id),seconds=state().mission_limit)
    report['spawn_capsules']=[{'p':pawn(i).get_actor_location().to_tuple(),'radius':pawn(i).capsule_component.get_scaled_capsule_radius(),'half_height':pawn(i).capsule_component.get_scaled_capsule_half_height()} for i in range(N)]
    check(all((pawn(i).get_actor_location()-pawn(j).get_actor_location()).length()>68 for i in range(N) for j in range(i)),'Spawn capsules do not overlap')
    # 호스트와 원격 클라이언트가 각각 조작자/부활 대상이 되도록 교환한다.
    for role in [0,1]:
        yield from start_run();victim=1-role
        fixture=u.SP1PIETestLibrary.spawn_carry_fixture(server(victim),server(victim).get_actor_location()+u.Vector(80,0,0));yield wait(.4,'carry fixture')
        local_fixture=next(a for a in allof(u.SPCargo,worlds[victim]) if a.get_name()==fixture.get_name())
        u.SP1PIETestLibrary.pickup_next_tick(pawn(victim),local_fixture);yield wait(.8,'owned inventory pickup')
        check(fixture.get_attach_parent_actor() is not None,'Carry fixture picked up '+str(role))
        wound(victim,100)
        if N==4:wound(3,100)
        yield wait(.8,'death and spectator')
        check(life(victim).is_dead() and life(victim).drop_batches==1 and fixture.get_attach_parent_actor() is None,'Death drops once '+str(role))
        # 일반 운반 화물은 드롭 뒤 물리 낙하한다. 바닥에 안정된 위치를 부활 전 기준으로 기록한다.
        yield wait(1.5,'dropped cargo settles before revive')
        dropped=fixture.get_actor_location();oldpath=server(victim).get_path_name()
        prepare_station(role)
        if N==4:prepare_station(2,1)
        yield wait(.8,'simultaneous maintenance entry')
        check('ACTIVE' in maintenance(),'First living entry activates once '+str(role))
        deadline=state().maintenance.deadline;frozen=remaining()
        check(abs(deadline-state().maintenance.started_at-30)<.01,'Thirty second real-time window '+str(role))
        holders=[role,2] if N==4 else [role]
        yield wait(3.8,'hold revive panel',lambda:press(*holders));yield wait(.8,'new pawn possession')
        check(not life(victim).is_dead() and life(victim).wounds==50 and abs(life(victim).get_stamina()-50)<.1,'Revive W50 S50 '+str(role))
        check(life(victim,False).wounds==50 and server(victim).get_path_name()!=oldpath,'Replicated new pawn '+str(role))
        check(participant(victim).maintenance_revived and fixture.get_attach_parent_actor() is None and (fixture.get_actor_location()-dropped).length()<100,'Once allowance and dropped inventory stays '+str(role),before=dropped.to_tuple(),after=fixture.get_actor_location().to_tuple())
        if N==4:check(life(3).wounds==50 and participant(3).maintenance_revived,'All connected dead revived once by concurrent holders '+str(role))
        before=pawn(victim).get_actor_location();pcs[victim].set_control_rotation(u.Rotator())
        yield wait(.5,'revived player movement',lambda:inject(victim,'Move'))
        check((pawn(victim).get_actor_location()-before).length()>50 and pcs[victim].get_view_target()==pawn(victim),'Revived movement and camera restored '+str(role))
        # 후방 화물은 정비 정지 중에도 기존 확보 시간으로 완료된다.
        item=cargo(1,'Supply');p=item.get_actor_location();place(victim,(p.x-150,p.y,308.15));aim(victim,item.get_interaction_point().to_tuple())
        yield wait(.6,'rear cargo aim');yield wait(1.6,'loot rear during maintenance',lambda:track(victim,item));yield wait(.3,'rear transfer')
        check(item.transferred and state().team_value==40 and abs(remaining()-frozen)<.01,'Rear looting continues while mission alone freezes '+str(role))
        # 같은 사망자를 다시 죽여도 부활 한도는 새 Pawn으로 초기화되지 않는다.
        wound(victim,100);yield wait(.7,'second death');prepare_station(role);yield wait(.5,'retry panel aim')
        yield wait(3.8,'second revive denied',lambda:press(role));yield wait(.4,'allowance result')
        check(life(victim).is_dead() and participant(victim).maintenance_revived,'Same participant cannot revive twice '+str(role))
        check(state().maintenance.deadline==deadline,'Reentry cannot extend window '+str(role))
        # 서버 레이저 주기가 실제 시간을 따르며 정비 중에도 피해를 준다.
        laser=next(a for a in allof(u.SP1LaserHazard) if str(a.hazard_id)=='P01_C02_Laser01');pos=laser.get_actor_location();place(role,(pos.x,pos.y,308.15));w=life(role).wounds
        for _ in range(35):
            if life(role).wounds>w:break
            yield wait(.1,'laser continues during maintenance')
        check(life(role).wounds>w and 'ACTIVE' in maintenance(),'Hazards continue while mission paused '+str(role))
        prepare_station(role);yield wait(.5,'safe from laser')
        yield wait(max(.1,deadline-now()+.4),'natural maintenance expiration')
        check('CLOSED' in maintenance() and str(state().maintenance.close_reason)=='WindowExpired','Natural expiration reason replicated '+str(role))
        check(all(str(r.state.maintenance.close_reason)=='WindowExpired' for r in rounds),'All players receive maintenance close reason '+str(role))
        r=remaining();prepare_station(role);yield wait(1,'reenter closed')
        check('CLOSED' in maintenance() and remaining()<r-.5,'Reentry does not reactivate '+str(role))
        yield from finish_restart('revive reset '+str(role))
    # 부활 완료 시각과 정비 종료 시각이 같으면 종료가 먼저다.
    yield from start_run();wound(1,100);yield wait(.7,'boundary victim');prepare_station(0);yield wait(.7,'boundary active')
    yield wait(.5,'start boundary hold',lambda:press(0));check(hold(0,True).attempt.active,'Boundary revive hold started')
    u.SP1PIETestLibrary.align_maintenance_deadline_to_attempt(hold(0,True))
    yield wait(3.2,'revive deadline tie',lambda:press(0));yield wait(.5,'boundary replication')
    check(life(1).is_dead() and not participant(1).maintenance_revived and 'CLOSED' in maintenance(),'Maintenance end wins exact revive tie')
    check(str(hold(0,True).attempt.reason)=='WindowExpired','Boundary hold canceled with close reason')
    yield from finish_restart('boundary reset')
    # 먼저 전진한 생존자를 후발 팀원의 정비 진입이 되돌리지 않는다.
    yield from start_run();place(1,(centers[6]-700,350,308.15));yield wait(.5,'leader already advanced');prepare_station(0);yield wait(.6,'late maintenance entry')
    check('CLOSED' in maintenance() and str(state().maintenance.close_reason)=='SurvivorAdvanced','Already advanced teammate closes new maintenance immediately')
    yield from finish_restart('advanced reset')
    # 중간 탈출 시작은 정비를 닫고 임무 시간을 다시 소비한다. 탑승 여부는 출발 순간에 확정한다.
    yield from start_run()
    for i in range(N):place(i,(centers[5]-500,-300+i*180,308.15))
    yield wait(.6,'mid window');prepare_exit(1,True);yield wait(.6,'mid aim');frozen=remaining()
    yield wait(1.7,'client starts mid extraction',lambda:press(1));yield wait(.4,'departure replication')
    check('EXTRACTION_COUNTDOWN' in phase() and str(state().departure_id)=='P01_C05_Mid','Remote client starts one mid departure')
    check('CLOSED' in maintenance() and str(state().maintenance.close_reason)=='DepartureStarted' and remaining()<frozen,'Departure resumes mission clock')
    deadline=state().extraction_deadline
    # 출발 중 합류 / 이탈을 반복하고 마지막에 전원을 안전한 네 위치에 둔다.
    for i in range(N):place(i,(centers[5]-500,-300+i*180,308.15))
    yield wait(.5,'leave exit');check(state().aboard==0,'Countdown allows leaving')
    z=exit_zone(True).get_actor_location()
    for i,(dx,y) in enumerate([(-100,-100),(-100,100),(100,-100),(100,100)][:N]):place(i,(z.x+dx,y,308.15))
    yield wait(.6,'all board');check(state().aboard==N,'Four capsules / late boarding accepted')
    report['mid_capsules']=[pawn(i).get_actor_location().to_tuple() for i in range(N)]
    yield wait(max(.1,deadline-now()+.8),'mid departure result')
    check('SUCCEEDED' in phase() and state().escaped==N and state().left_behind==0,'Mid result includes place and participant counts')
    command(0,'RequestRestart');yield wait(1.4,'mid restart');reset_checks('mid reset')
    # 중간 조작이 끝나는 프레임에 앞칸 생존자와 최종 요청이 오면 정비 종료가 먼저다.
    yield from start_run();prepare_exit(0,True);yield wait(.5,'mid simultaneous aim')
    yield wait(.4,'mid attempt starts',lambda:press(0));prepare_exit(1,False);yield wait(.5,'final arrival closes maintenance',lambda:press(0))
    yield wait(1.4,'simultaneous departure requests',lambda:press(0,1));yield wait(.5,'single departure')
    check(str(state().departure_id)=='P01_C10_Final' and 'EXTRACTION_COUNTDOWN' in phase(),'Two departure sites leave one accepted final departure')
    fixed=state().extraction_deadline;yield wait(.6,'repeated starts rejected',lambda:press(0,1))
    check(state().extraction_deadline==fixed,'Duplicate request cannot extend countdown')
    # 마지막 2m와 구역은 직접 피해도 제외한다. 카메라가 그쪽으로 돌아도 감지할 수 없다.
    c=next(a for a in allof(u.SP1SecurityCamera) if str(a.region.car_id)=='P01_C10')
    place(1,(-1150,0,308.15));w=life(1).wounds;wound(1,100);yield wait(.5,'safe approach damage rejected')
    check(life(1).wounds==w and not c.can_see(server(1),now()),'Final two metre approach excludes all queued damage / camera')
    z=exit_zone(False).get_actor_location()
    for i,(dx,y) in enumerate([(-100,-100),(-100,100),(100,-100),(100,100)][:N]):place(i,(z.x+dx,y,308.15))
    yield wait(.6,'four final capsules');report['final_capsules']=[pawn(i).get_actor_location().to_tuple() for i in range(N)]
    check(state().aboard==N and all(exit_zone(False).contains_pawn(server(i)) for i in range(N)),'Final capsule fit / all aboard')
    for i in range(N):wound(i,100)
    yield wait(.5,'final damage protection');check(all(life(i).wounds==0 for i in range(N)),'Final zone immune to every damage request')
    # 같은 임무/출발 마감은 탑승자가 있어도 실패한다.
    u.SP1PIETestLibrary.align_mission_deadline_to_departure(rounds[0]);yield wait(max(.1,fixed-now()+.8),'mission departure tie')
    check('FAILED' in phase() and str(state().reason)=='MissionExpired' and state().final_value==0,'Mission expiration wins exact departure tie')
    command(0,'RequestRestart');yield wait(1.4,'final reset');reset_checks('final reset')
    # 중간 출발 조작과 정비 종료의 정확한 경계, 빈 출발, 실제 10초 카운트다운.
    yield from start_run();prepare_exit(0,True);yield wait(.5,'mid boundary aim');yield wait(.35,'mid boundary start',lambda:press(0))
    check(hold(0,True).attempt.active,'Mid boundary attempt started');u.SP1PIETestLibrary.align_maintenance_deadline_to_attempt(hold(0,True))
    yield wait(1.3,'mid boundary tie',lambda:press(0))
    check(str(state().departure_id)=='None' and 'CLOSED' in maintenance(),'Maintenance end wins exact mid departure tie')
    yield from finish_restart('mid boundary reset')
    yield from start_run();prepare_exit(0,False);yield wait(.5,'empty exit aim');yield wait(1.6,'start empty departure',lambda:press(0));yield wait(.3,'exit started')
    check('EXTRACTION_COUNTDOWN' in phase(),'Empty departure first starts normally')
    deadline=state().extraction_deadline;park();yield wait(max(.1,deadline-now()+.6),'empty departure result')
    check('FAILED' in phase() and str(state().reason)=='EmptyExtraction' and state().escaped==0 and state().left_behind==N,'Nobody aboard fails and reports all left behind')
    command(0,'RequestRestart');yield wait(1.4,'last restart');reset_checks('all systems reset')
    # 두 S01을 실제로 전송한 뒤 새 판에서 두 영역/화물/가치를 함께 초기화한다.
    yield from start_run()
    specials=[cargo(4,'S01'),cargo(8,'S01')]
    for i,item in enumerate(specials):
        p=item.get_actor_location();place(i,(p.x-160,p.y,308.15));aim(i,item.get_interaction_point().to_tuple())
    yield wait(.7,'two S01 aim');yield wait(3.9,'two independent S01 holds',lambda:[track(i,a) for i,a in enumerate(specials)]);yield wait(.7,'two gravity replications')
    check(state().team_value==700 and all(a.transferred for a in specials),'C04 and C08 S01 transfer independently for 700')
    check(all(all('ZERO_GRAVITY' in str(a.gravity_zone.get_gravity_mode()) for a in allof(u.SP1GravityCargo,w)) for w in worlds),'Both gravity states replicate to every player')
    for i,x in [(0,centers[4]+1050),(1,centers[8]+1050)]:place(i,(x,0,308.15))
    yield wait(.8,'independent gravity boundaries')
    check(all(not pawn(i).character_movement.is_zero_gravity() for i in [0,1]),'Both connecting passages retain gravity')
    yield from finish_restart('transferred S01 reset')
    check(len(set(report['runs']))==len(report['runs']) and len(report['runs'])>=5,'At least five distinct RunIds / restarts')
    report['passed']=True

sequence=suite();stage=None;end=0
def save():output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
    global stage,end
    try:
        if stage is None or now()>=end:stage=next(sequence);end=now()+stage[0];report['current_stage']=stage[1];save()
        if stage[2]:stage[2]()
    except StopIteration:u.unregister_slate_post_tick_callback(handle);save();u.log('P4_PIE_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc();u.unregister_slate_post_tick_callback(handle);save();u.log_error('P4_PIE_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P4_PIE_STARTED')
