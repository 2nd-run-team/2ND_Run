"""작성자 : 임진혁
P5 2/4인 PIE 검증. 준비/핑/재시작은 소유 Pawn RPC, 확보는 Enhanced Input을 사용한다.
배치와 경계 시간만 P4의 명시적인 PIE fixture를 재사용한다. 사람의 청취/이해도 평가는 별도다.
"""
from pathlib import Path
import unreal as u
helpers=Path(u.Paths.project_dir(),'Tools/Prototype01/verify_p4.py').read_text(encoding='utf8').split('\ndef suite():')[0]
exec(compile(helpers,'verify_p4_helpers.py','exec'))
output=Path(u.Paths.project_saved_dir(),'Prototype01/p5-pie-%d.json'%N)
report={'passed':False,'players':N,'checks':[],'runs':[],'method':'4/2 local PIE worlds; owned RPC + Enhanced Input, position/time fixtures'}

def hud(i):
 widgets=[a for a in u.ObjectIterator(u.SP1HUDWidget) if a.get_world()==worlds[i] and a.is_in_viewport()]
 assert len(widgets)==1,[a.get_path_name() for a in widgets]
 return widgets[0]
def session(i):return next(s for s in u.ObjectIterator(u.SP1SessionSubsystem) if s.get_world()==worlds[i])
def text(i,field):
 name=''.join(x.capitalize() for x in field.split('_'))
 prefix=hud(i).get_path_name()+'.'
 return str(next(t for t in u.ObjectIterator(u.TextBlock) if t.get_path_name().startswith(prefix) and t.get_name()==name).get_text())
def hud_audio(i):
 sounds=set(hud(i).cues.values())
 return [a for a in u.ObjectIterator(u.AudioComponent) if a.get_world()==worlds[i] and a.sound in sounds and a.is_playing()]
def events(i):return [str(x) for x in hud(i).audio_events]
def ping_snapshot(r):return [(p.id,p.player_id,p.location.to_tuple(),p.expires_at,str(p.kind)) for p in r.pings]
def shoot(i,name):
 path=Path(u.Paths.project_saved_dir(),'Prototype01/p5-%dp-%s.png'%(N,name)).resolve().as_posix()
 u.SystemLibrary.execute_console_command(worlds[i],'Shot SHOWUI filename='+path+' -nosuffix',pcs[i])
def finish(label):
 u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],0);yield wait(.8,'finish '+label)
 check('FAILED' in phase() and all('시간이 끝났' in text(i,'menu_body') for i in range(N)),label+' failure reason on every screen',body=text(0,'menu_body'))
 if label=='repeat 0':shoot(0,'failure')
 yield wait(2.3,'result sound tail');command(0,'RequestRestart');yield wait(1.5,'restart '+label)
 check('READY' in phase() and all(not r.pings and r.state.team_value==0 for r in rounds),label+' replicated round reset')
 check(all(not events(i) and not hud_audio(i) for i in range(N)),label+' no old HUD audio / one HUD per world')

def suite():
 check(all('우주 열차 강탈' in text(i,'menu_title') for i in range(N)),'Ready screens in all worlds')
 yield from start_run();yield wait(.5,'team HUD')
 check(all('01번 칸' in hud(i).displayed_status and len(hud(i).displayed_team.splitlines())>=N for i in range(N)),'Four/two participant rows and current car')
 # 같은 조준 지점은 서버 첫 가림 표면으로 확정된다. 요청에는 좌표/대상 Actor가 없다.
 item=cargo(1,'Supply');p=item.get_actor_location()
 for i in range(N):place(i,(p.x-170,p.y+(-75 if i%2==0 else 75),308.15));aim(i,item.get_interaction_point().to_tuple())
 yield wait(.8,'ping aim settles')
 for i in range(N):command(i,'RequestPing')
 yield wait(.5,'ping replication')
 check(len(rounds[0].pings)==N and all(ping_snapshot(r)==ping_snapshot(rounds[0]) for r in rounds),'All owned pings match exact fixed points in every world')
 check(all('CARGO' in str(p.kind) for p in rounds[0].pings),'First visible cargo surface classified')
 first=ping_snapshot(rounds[0]);command(0,'RequestPing');yield wait(.25,'cooldown request')
 check(ping_snapshot(rounds[0])==first,'One second server cooldown rejects repeated request')
 yield wait(.5,'cooldown ends');command(0,'RequestPing');yield wait(1.2,'second ping');command(0,'RequestPing');yield wait(.4,'third ping')
 own=[p for p in rounds[0].pings if p.player_id==pcs[0].player_state.player_id]
 check(len(own)==2 and all(p.id!=first[0][0] for p in own),'Per-player cap replaces oldest ping')
 fixed=[p.location.to_tuple() for p in rounds[0].pings];place(0,(p.x-400,p.y,308.15));yield wait(.4,'move after ping')
 check([p.location.to_tuple() for p in rounds[0].pings]==fixed,'Ping remains at world point after requester movement')
 shoot(N-1,'pings');yield wait(8.3,'ping lifetime')
 check(all(not r.pings for r in rounds),'Eight second ping expiry on all worlds')
 # 보이는 벽 뒤의 화물을 직접 핑할 수 없다. 30m 이내 첫 벽만 남는다.
 place(0,(centers[1],0,308.15));aim(0,(centers[1],4000,350));yield wait(.7,'wall aim');command(0,'RequestPing');yield wait(.4,'wall ping')
 check(len(rounds[0].pings)==1 and 'LOCATION' in str(rounds[0].pings[0].kind) and abs(rounds[0].pings[0].location.y)<1000,'Occluding wall receives location ping')
 yield wait(.8,'wall cooldown');place(0,(centers[1],4000,4000));pcs[0].set_control_rotation(u.Rotator(pitch=80,yaw=90));yield wait(.7,'no surface within range')
 before=ping_snapshot(rounds[0]);command(0,'RequestPing');yield wait(.4,'range check')
 check(ping_snapshot(rounds[0])==before,'No ping when no surface exists within thirty metres');park()
 # 일반 완료와 S01 예고/취소/완료를 실제 확보 입력으로 관측한다.
 p=item.get_actor_location();place(0,(p.x-160,p.y,308.15));yield wait(.6,'cargo aim',lambda:aim(0,item.get_interaction_point().to_tuple()))
 yield wait(1.7,'normal cargo E hold',lambda:track(0,item));yield wait(.5,'normal transfer')
 check(all(r.state.team_value==40 for r in rounds),'Normal transfer value replicated to all HUDs')
 special=cargo(4,'S01');p=special.get_actor_location();place(1,(p.x-160,p.y,308.15));yield wait(.7,'S01 aim',lambda:aim(1,special.get_interaction_point().to_tuple()))
 yield wait(.4,'S01 preview',lambda:track(1,special));yield wait(.5,'S01 release')
 check('S01Warning' in events(1) and 'Cancel' in events(1) and not special.transferred,'Distinct S01 preview and cancel events')
 yield wait(3.8,'S01 completion',lambda:track(1,special));yield wait(.6,'S01 replication')
 check(all(r.state.team_value==390 for r in rounds),'S01 and normal total 390 in all worlds')
 check('무중력' in text(1,'mechanic_text'),'S01 result remains visible');shoot(1,'s01')
 park();yield wait(.6,'warning HUD')
 u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],59);yield wait(.4,'60s warning')
 check(all(events(i).count('Time60')==1 and '60초' in hud(i).displayed_alert for i in range(N)),'60 second visual and cue on every screen')
 for i in range(N):session(i).toggle_feedback_mute()
 yield wait(.2,'mute active audio');u.SP1PIETestLibrary.set_remaining_seconds(rounds[0],29);yield wait(.4,'silent 30s warning')
 check(all(events(i).count('Time30')==1 and '30초' in hud(i).displayed_alert and not hud_audio(i) for i in range(N)),'Muted warning stays readable without creating audio');shoot(0,'silent-warning')
 yield wait(.7,'warning stable');check(all(events(i).count('Time60')==1 and events(i).count('Time30')==1 for i in range(N)),'Time warnings do not repeat every tick')
 for i in range(N):session(i).toggle_feedback_mute()
 # 성공 화면과 마지막 5초. 화면 정산을 같은 서버 원장의 JSONL과 외부 도구에서 대조한다.
 for i in range(N):prepare_exit(i,False,-110+70*i)
 yield wait(.8,'exit aim');yield wait(1.5,'remote departure',lambda:press(1));yield wait(.4,'exit HUD')
 check('EXTRACTION_COUNTDOWN' in phase() and all('탑승' in hud(i).displayed_alert for i in range(N)),'Departure place / living / aboard HUD')
 shoot(0,'departure');yield wait(max(.1,state().extraction_deadline-now()+.7),'ten second departure')
 check('SUCCEEDED' in phase() and state().final_value==390 and state().escaped==N,'Successful settlement and every player aboard')
 check(all(events(i).count('Departure')==1 and events(i).count('Countdown')==5 and events(i).count('Success')==1 for i in range(N)),'Departure / final five ticks / result cue exactly once')
 check(all('탈출 성공' in text(i,'menu_title') and '390' in text(i,'menu_body') for i in range(N)),'Success result screen matches server');shoot(N-1,'success')
 check(all(not r.pings for r in rounds),'Terminal result clears pings')
 command(0,'RequestRestart');yield wait(1.5,'first restart');check(all(not events(i) for i in range(N)),'First restart clears UI sound history')
 # 다섯 번 이상 새 판을 반복한다. 마지막 판은 사망한 관전자의 핑을 거부한다.
 for n in range(5):
  yield from start_run();yield wait(.5,'restart current car')
  if n==4:
   wound(1,100);yield wait(.5,'spectator');before=ping_snapshot(rounds[0]);command(1,'RequestPing');yield wait(.3,'dead ping')
   check(ping_snapshot(rounds[0])==before and '관전' in text(1,'target'),'Spectator input blocked with explicit message')
  yield from finish('repeat '+str(n))
 check(len(set(report['runs']))==6,'Six unique completed RunIds')
 report['passed']=True

sequence=suite();stage=None;end=0
def save():output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
 global stage,end
 try:
  if stage is None or now()>=end:stage=next(sequence);end=now()+stage[0];report['current_stage']=stage[1];save()
  if stage[2]:stage[2]()
 except StopIteration:u.unregister_slate_post_tick_callback(handle);save();u.log('P5_PIE_RESULT '+str(report['passed']))
 except Exception:
  report['error']=traceback.format_exc();u.unregister_slate_post_tick_callback(handle);save();u.log_error('P5_PIE_FAILED '+report['error'])
handle=u.register_slate_post_tick_callback(tick)
u.log('P5_PIE_STARTED')
