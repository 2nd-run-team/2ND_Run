"""작성자 : 임진혁
두 PIE 월드의 실제 Enhanced Input/소유 Pawn RPC/이동 예측을 검사한다.
위치·시간·서버 피해만 테스트 fixture로 만들며 자산이나 점수를 직접 쓰지 않는다.
Listen Server + Client의 Ready에서 실행. 결과는 Saved/Prototype01/p2-pie.json.
"""
import json, math, traceback
from pathlib import Path
import unreal as u

worlds=list(u.EditorLevelLibrary.get_pie_worlds(False))
worlds.sort(key=lambda w:not u.GameplayStatics.get_game_state(w).has_authority())
assert len(worlds)==2
sw=worlds[0]
rounds=[u.GameplayStatics.get_game_state(w).get_component_by_class(u.SP1RoundComponent) for w in worlds]
subs=[next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem) if s.get_world()==w) for w in worlds]
actions={'E':u.load_asset('/Game/Input/Actions/IA_Interact')}
actions.update({n:u.load_asset('/Game/SpacePirate/Player/Input/IA_SP'+n) for n in ['Move','Jump','Sprint','Look']})
output=Path(u.Paths.project_saved_dir(),'Prototype01/p2-pie.json')
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

def suite():
    check('READY' in phase(),'Initial Ready')
    for role in [0,1]:
        start(); yield wait(.7,'ready'); command(0,'RequestStart'); yield wait(.7,'running'); safe_park()
        place(role,(-1800,0,308.15)); place(1-role,(-2000,-300,308.15))
        yield wait(.5,'settle')
        check(life(role).wounds==0 and life(role).get_stamina()==100,f'{role} fresh W0 S100')
        yield wait(5.3,f'{role} sprint to exhaustion',lambda:sprint(role))
        rows=[r for r in report['sprint_samples'] if r['role']==role]
        check(min(r['s'] for r in rows)<.1 and life(role).is_exhausted(),f'{role} exhausts without death',w=life(role).wounds)
        check(life(role).wounds==0 and not life(role).is_dead(),f'{role} sprint never wounds')
        check(max(r['speed'] for r in rows)>680 and max(r['max_speed'] for r in rows)==700,f'{role} actual sprint 700')
        check(max(abs(r['s']-r['local_s']) for r in rows)<4,f'{role} server / predicted stamina agree',max_error=max(abs(r['s']-r['local_s']) for r in rows))
        yield wait(1.2,'held exhausted key regenerates without resprint',lambda:sprint(role))
        check(life(role).get_stamina()>10 and pawns()[role].character_movement.get_max_speed()==450,f'{role} exhausted key stays walking during regeneration')
        yield wait(.3,'release Sprint')
        check(not life(role).is_exhausted(),f'{role} release clears exhaustion latch')
        yield wait(4,'fully regenerate')
        check(life(role).get_stamina()==100,f'{role} recovers to full')
        yield wait(.6,'short sprint',lambda:sprint(role)); before=life(role).get_stamina()
        yield wait(.45,'recovery delay')
        check(life(role).get_stamina()<=before+1,f'{role} no recovery before .75s',before=before,after=life(role).get_stamina())
        yield wait(.7,'recover after delay')
        check(life(role).get_stamina()>before+3,f'{role} recovers after delay')
        # 비치명 피해는 실제 확보를 유지하고, 최대 용량만 줄인다.
        place(role,(-1660,300,308.15)); aim(role,cargo('Alloy').get_interaction_point().to_tuple())
        yield wait(.6,'aim'); yield wait(.35,'hold',lambda:inject(role,'E'))
        check(hold(role,True).attempt.active,f'{role} acquisition active before wound')
        wound(role); yield wait(.3,'nonfatal impact',lambda:inject(role,'E'))
        check(life(role).wounds==20 and life(role).get_maximum()==80 and life(role).get_stamina()<=80,f'{role} W20 caps M80')
        check(hold(role,True).attempt.active,f'{role} nonfatal hit keeps acquisition')
        yield wait(2.4,'finish wounded acquisition',lambda:inject(role,'E'))
        check(cargo('Alloy').transferred and rounds[0].state.team_value==160,f'{role} wounded acquisition completes once')
        yield wait(.3,'release E')
        # 기존 운반 화물을 두 칸에 넣어 숨은 슬롯까지 사망 드롭을 검사한다.
        fixtures=[]
        for n in [0,1]:
            fixture=u.SP1PIETestLibrary.spawn_carry_fixture(server(role),u.Vector(-1630+n*20,260,250)); fixtures.append(fixture)
            yield wait(.5,'replicate carry fixture')
            local_fixture=next(a for a in u.GameplayStatics.get_all_actors_of_class(worlds[role],u.SPCargo) if a.get_name()==fixture.get_name())
            u.SP1PIETestLibrary.pickup_next_tick(pawns()[role],local_fixture)
            yield wait(.5,'owned inventory pickup')
        check(all(a.get_attach_parent_actor()==server(role) for a in fixtures),f'{role} two inventory slots occupied')
        check(any(prop(a,'hidden') for a in fixtures),f'{role} inactive slot hidden before death')
        for expected in [40,60,80]:
            wound(role); yield wait(.25,'next ordinary wound')
            check(life(role).wounds==expected and not life(role).is_dead(),f'{role} wound {expected} alive')
        place(role,(-1370,-300,308.15)); aim(role,cargo('Parts').get_interaction_point().to_tuple())
        yield wait(.5,'last hold aim'); yield wait(.35,'last hold start',lambda:inject(role,'E'))
        check(hold(role,True).attempt.active,f'{role} hold active before fatal hit')
        wound(role); yield wait(.7,'fifth wound kills',lambda:inject(role,'E'))
        check(all(life(role,a).is_dead() and life(role,a).wounds==100 for a in [False,True]),f'{role} fifth wound death replicated')
        check(not hold(role,True).attempt.active and str(hold(role,True).attempt.reason)=='Dead',f'{role} death cancels acquisition')
        check(not cargo('Parts').transferred and rounds[0].state.team_value==160,f'{role} death no extra transfer')
        check(life(role).death_count==1 and life(role).drop_batches==1 and all(a.get_attach_parent_actor() is None for a in fixtures),f'{role} one death / one drop batch')
        check(all(not prop(a,'hidden') and a.get_attach_parent_actor() is None for a in fixtures),f'{role} all slots visible and detached')
        check(pawns()[role].get_controller().get_view_target()==life(role,False).spectate_target and life(role,False).spectate_target is not None,f'{role} spectates living teammate')
        position=pawns()[role].get_actor_location(); wound(role,100)
        yield wait(.8,'dead movement/actions rejected',lambda:inject(role,'Move','Sprint','Jump','Look','E'))
        check((pawns()[role].get_actor_location()-position).length()<1 and 'NONE' in str(pawns()[role].character_movement.movement_mode),f'{role} spectator cannot move or jump')
        check(not hold(role,True).attempt.active and life(role).death_count==1 and life(role).drop_batches==1,f'{role} repeated damage and E cannot repeat death / drop')
        local_fixture=next(a for a in u.GameplayStatics.get_all_actors_of_class(worlds[role],u.SPCargo) if a.get_name()==fixtures[0].get_name())
        u.SP1PIETestLibrary.pickup_next_tick(pawns()[role],local_fixture)
        yield wait(.4,'dead pickup request rejected')
        check(all(a.get_attach_parent_actor() is None for a in fixtures),f'{role} spectator inventory RPC rejected')
        check(pawns()[role].get_controller().is_look_input_ignored() and pawns()[role].get_controller().is_move_input_ignored(),f'{role} spectator input disabled')
        wound(1-role,100); yield wait(.6,'all dead')
        check('FAILED' in phase() and rounds[0].state.reason=='NoConnectedSurvivors',f'{role} all dead fails',reason=rounds[0].state.reason)
        restart(); yield wait(1.2,'new round respawns')
        check('READY' in phase() and all(not life(i).is_dead() and life(i).wounds==0 for i in [0,1]),f'{role} dead host can restart / fresh life')
        for fixture in fixtures: fixture.destroy_actor()
        # 동일 레이저 접촉을 두 역할에서 반복하고 서버 기준 경고 시각을 비교한다.
        start(); yield wait(.6,'ready laser run'); command(0,'RequestStart'); yield wait(.6,'running laser'); safe_park()
        place(role,(-950,100,308.15)); yield wait(11,'continuous laser contact',lambda:sample_laser(role))
        samples=[s for s in report['laser_samples'] if s['role']==role]
        hits=[]; previous=0
        for row in samples:
            if row['w']>previous: hits.append((row['t'],row['w'])); previous=row['w']
        check([w for _,w in hits]==[20,40,60,80,100],f'{role} five laser hits kill',hits=hits)
        check(all(b[0]-a[0]>=.97 for a,b in zip(hits,hits[1:])),f'{role} same hazard damage spacing >=1s',intervals=[b[0]-a[0] for a,b in zip(hits,hits[1:])])
        stable=[s for s in samples if min(abs(((s['clocks'][0]-s['epochs'][0])%4)-edge) for edge in [0,1.5,2,4])>.15]
        check(stable and all(s['phases'][0]==s['phases'][1] and s['epochs'][0]==s['epochs'][1] for s in stable),f'{role} shared epoch / warning and ON phases agree away from boundaries')
        check(all(s['displayed']==s['phases'] and all(n==(4 if 'ON:' in s['phases'][0] else 0) for n in s['active_beams']) for s in stable),f'{role} replicated phase drives four beams on / off')
        wound(1-role,100); yield wait(.5,'end laser run'); restart(); yield wait(1,'reset laser run')
        start(); yield wait(.5,'ready safe route'); command(0,'RequestStart'); yield wait(.6,'running safe route'); safe_park()
        place(role,(-1150,-160,308.15)); route[:]=[(-1100,-275,308.15),(-775,-275,308.15),(-650,0,308.15)]
        yield wait(.5,'safe route settle'); yield wait(5,'walk bypass only',lambda:walk_route(role))
        check(not route and life(role).wounds==0,f'{role} walking bypass arrives without wounds',remaining=route,position=pawns()[role].get_actor_location().to_tuple())
        check(all('WALKING' in s['mode'] for s in report['safe_samples'] if s['role']==role),f'{role} bypass uses no jump/crouch')
        u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0); yield wait(.4,'end route run'); restart(); yield wait(1,'reset route run')
    report['passed']=True

sequence=suite(); stage=None; end=0
def save(): output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
def tick(delta):
    global stage,end
    try:
        if stage is None or now()>=end:
            stage=next(sequence); end=now()+stage[0]; report['current_stage']=stage[1]; save()
        if stage[2]: stage[2]()
    except StopIteration:
        u.unregister_slate_post_tick_callback(handle); save(); u.log('P2_PIE_RESULT '+str(report['passed']))
    except Exception:
        report['error']=traceback.format_exc(); u.unregister_slate_post_tick_callback(handle); save(); u.log_error('P2_PIE_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P2_PIE_STARTED')





