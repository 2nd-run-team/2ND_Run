"""작성자 : 임진혁
2인 Ready PIE에서 실제 호스트 disconnect → 연결 중단 화면 → 새 로컬 호스트 로딩/Ready를 검사한다.
복구를 시작하는 버튼의 제품 함수를 호출하며 서버 종료를 흉내 낸 bool 대입은 하지 않는다.
"""
import unreal as u,json,time,traceback
from pathlib import Path
worlds=list(u.EditorLevelLibrary.get_pie_worlds(False));assert len(worlds)==2
sw=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority());cw=next(w for w in worlds if w!=sw)
gi=u.GameplayStatics.get_game_instance(cw)
session=next(s for s in u.ObjectIterator(u.SP1SessionSubsystem) if s.get_outer()==gi)
inputs=[u.GameplayStatics.get_player_pawn(w,0).get_component_by_class(u.SP1InteractionComponent) for w in [sw,cw]]
report={'passed':False,'checks':[],'observed_titles':[]}
def widgets():return [h for h in u.ObjectIterator(u.SP1HUDWidget) if h.is_in_viewport() and h.get_game_instance()==gi]
def title(h):return str(next(t for t in u.ObjectIterator(u.TextBlock) if t.get_name()=='MenuTitle' and t.get_path_name().startswith(h.get_path_name()+'.')).get_text())
def check(ok,label):report['checks'].append({'check':label,'passed':bool(ok)});assert ok,label
def steps():
 for i in inputs:u.SP1PIETestLibrary.next_tick(i,'ToggleReady')
 yield 1
 u.SP1PIETestLibrary.next_tick(inputs[0],'RequestStart');yield 1
 report['run_id']=session.last_state.run_id.export_text();check('RUNNING' in str(session.last_state.phase),'Active run remembered')
 u.SystemLibrary.execute_console_command(sw,'disconnect',u.GameplayStatics.get_player_controller(sw,0));yield 5
 check(session.aborted and session.last_state.final_value==0,'Connection failure has no settlement')
 check(len(widgets())==1 and '연결 중단' in title(widgets()[0]),'Exactly one connection-lost screen survives fallback map')
 report['aborted_screen']=title(widgets()[0]);yield 5
 session.return_to_prototype();yield 8
 check('열차 준비 중' in report['observed_titles'],'Actual loading screen rendered during retry')
 check(not session.aborted and not session.loading,'New local host clears failure/loading')
 check(len(widgets())==1 and '우주 열차 강탈' in title(widgets()[0]),'Retry returns to one Ready screen')
 round_component=u.GameplayStatics.get_game_state(session.get_world()).get_component_by_class(u.SP1RoundComponent)
 check(round_component is not None and round_component.get_owner().has_authority() and round_component.state.team_value==0 and not round_component.pings,'Fresh authoritative prototype round without old value/pings')
 report['passed']=True
sequence=steps();end=0
def finish():
 u.unregister_slate_post_tick_callback(handle)
 Path(u.Paths.project_saved_dir(),'Prototype01/p5-disconnect.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
def tick(delta):
 global end
 try:
  for h in widgets():
   t=title(h)
   if t not in report['observed_titles']:report['observed_titles'].append(t)
  if time.monotonic()>=end:end=time.monotonic()+next(sequence)
 except StopIteration:finish()
 except Exception:report['error']=traceback.format_exc();finish()
handle=u.register_slate_post_tick_callback(tick)
