"""작성자 : 임진혁
이미 Running인 2/4인 PIE에서 재질 교체/장치 UI/실제 AudioComponent/빈 출발 표시를 확인한다.
재질 교체는 PIE 객체에만 적용하며 에디터 자산을 저장하지 않는다.
"""
from pathlib import Path
import unreal as u
helpers=Path(u.Paths.project_dir(),'Tools/Prototype01/verify_p5.py').read_text(encoding='utf8').split('\ndef suite():')[0]
exec(compile(helpers,'p5_helpers','exec'))
output=Path(u.Paths.project_saved_dir(),'Prototype01/p5-presentation.json')
report={'passed':False,'checks':[],'run_id':state().run_id.export_text()}
assert 'RUNNING' in phase()
def camera(w=sw):return next(a for a in allof(u.SP1SecurityCamera,w) if str(a.region.car_id)=='P01_C06')
def audio_at(i,sound,point):return [a for a in u.ObjectIterator(u.AudioComponent) if a.get_world()==worlds[i] and a.sound==sound and a.is_playing() and (a.get_world_location()-point).length()<10]
def follow_camera(i):
 c=camera();angle=math.radians(c.get_aim(now()).yaw)
 for distance in [520,420,620]:
  loc=c.get_actor_location();place(i,(loc.x+math.cos(angle)*distance,loc.y+math.sin(angle)*distance,308.15))
  if c.can_see(server(i),now()):break
 aim(i,c.get_actor_location().to_tuple())
def suite():
 park();material=u.load_asset('/Engine/EngineMaterials/DefaultMaterial');assert material
 for w in worlds:
  for a in allof(u.SP1TransferCargo,w):
   for mesh in a.get_components_by_class(u.StaticMeshComponent):
    for index in range(mesh.get_num_materials()):mesh.set_material(index,material)
 item=cargo(4,'S01');p=item.get_actor_location();place(1,(p.x-160,p.y,308.15));yield wait(.7,'material swapped target',lambda:aim(1,item.get_interaction_point().to_tuple()))
 check('무중력' in text(1,'target') and '+350' in text(1,'target'),'Material replacement preserves S01 danger / value / duration text')
 icon=next(t for t in u.ObjectIterator(u.Image) if t.get_name()=='TargetIcon' and t.get_path_name().startswith(hud(1).get_path_name()+'.'))
 check(icon.get_editor_property('brush').get_editor_property('resource_object')==item.definition.icon,'Material replacement preserves target icon')
 yield wait(6,'material review pause')
 yield wait(3.3,'S01 real hold',lambda:track(1,item));yield wait(.2,'S01 completion audio')
 check(item.transferred and all(r.state.team_value==350 for r in rounds),'Swapped-material S01 transfer still authoritative')
 report['transfer_audio_components']=[len(audio_at(i,item.definition.complete_sound,item.get_actor_location())) for i in range(N)]
 check(report['transfer_audio_components']==[int(i==1) for i in range(N)],'One nearby transfer AudioComponent; other cars attenuated, no duplicates')
 yield wait(3,'completed material review');park();wound(1,100);yield wait(.5,'dead teammate');prepare_station(0);yield wait(.6,'maintenance HUD')
 check('정비' in text(0,'mechanic_text') and '1명' in text(0,'target') and '사망' in hud(0).displayed_team,'Maintenance eligible count and teammate death visible')
 yield wait(3.5,'revive E hold',lambda:press(0));yield wait(.5,'revive HUD')
 check(not life(1).is_dead() and '부활 사용' in hud(0).displayed_team,'Revive result updates team display')
 park()
 for _ in range(150):
  if 'WARNING' in str(camera().state.phase):break
  yield wait(.1,'seek camera warning',lambda:follow_camera(1))
 check('WARNING' in str(camera().state.phase),'Camera entered product warning')
 yield wait(.25,'warning replication')
 report['camera_audio_components']=[len(audio_at(i,camera().warning_sound,camera().get_actor_location())) for i in range(N)]
 check(report['camera_audio_components']==[int(i==1) for i in range(N)] and '지금 엄폐' in text(1,'mechanic_text'),'One nearby camera warning sound, distant cars attenuated, explicit visual warning')
 for i in range(N):session(i).toggle_feedback_mute()
 yield wait(.15,'mute camera in flight')
 check(all(not audio_at(i,camera().warning_sound,camera().get_actor_location()) for i in range(N)),'Mute immediately stops active world warning audio')
 check('지금 엄폐' in text(1,'mechanic_text'),'Camera warning remains visible while muted')
 yield wait(1,'warning ends');park()
 for i in range(N):session(i).toggle_feedback_mute()
 prepare_exit(0,False);yield wait(.6,'empty exit aim');yield wait(1.5,'start empty exit',lambda:press(0));yield wait(.3,'countdown');deadline=state().extraction_deadline;park()
 yield wait(max(.1,deadline-now()+.5),'empty departure result')
 check('FAILED' in phase() and state().final_value==0 and all('생존자가 없었' in text(i,'menu_body') for i in range(N)),'Empty departure failure explains cause and pays zero')
 check(all(not r.pings for r in rounds),'Terminal world pings cleared')
 yield wait(2.2,'result cue finishes');command(0,'RequestRestart');yield wait(1.5,'presentation reset')
 active=[a.get_path_name() for a in u.ObjectIterator(u.AudioComponent) if a.get_world() in worlds and a.sound and a.sound.get_path_name().startswith('/Game/Assets/Sound/') and a.is_playing()]
 check(not active and all(not events(i) for i in range(N)),'Restart leaves no prototype world/UI sound playing',active=active)
 report['passed']=True
sequence=suite();stage=None;end=0
def save():output.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
 global stage,end
 try:
  if stage is None or now()>=end:stage=next(sequence);end=now()+stage[0];report['current_stage']=stage[1];save()
  if stage[2]:stage[2]()
 except StopIteration:u.unregister_slate_post_tick_callback(handle);save()
 except Exception:report['error']=traceback.format_exc();u.unregister_slate_post_tick_callback(handle);save();u.log_error(report['error'])
handle=u.register_slate_post_tick_callback(tick)
