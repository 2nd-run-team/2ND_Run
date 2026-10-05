"""작성자 : 임진혁
Ready 상태의 2인 PIE에서 호스트 연결을 종료하고 클라이언트의 Aborted 상태/HUD를 검사한다.
"""
import json, time, traceback
from pathlib import Path
import unreal as u
worlds=list(u.EditorLevelLibrary.get_pie_worlds(False))
sw=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
cw=next(w for w in worlds if w!=sw)
lp=[next(p for p in u.GameplayStatics.get_all_actors_of_class(w,u.Character) if p.is_locally_controlled()) for w in [sw,cw]]
inputs=[p.get_component_by_class(u.SP1InteractionComponent) for p in lp]
client_gi=u.GameplayStatics.get_game_instance(cw)
session=next(s for s in u.ObjectIterator(u.SP1SessionSubsystem) if s.get_outer()==client_gi)
report={'passed':False,'checks':[]}
def check(ok,label):
    report['checks'].append({'check':label,'passed':bool(ok)})
    assert ok,label
def steps():
    for i in inputs: u.SP1PIETestLibrary.next_tick(i,'ToggleReady')
    yield 1
    u.SP1PIETestLibrary.next_tick(inputs[0],'RequestStart')
    yield 1
    check('RUNNING' in str(session.last_state.phase),'Client remembered active run')
    report['run_id']=session.last_state.run_id.export_text()
    u.SystemLibrary.execute_console_command(sw,'disconnect',lp[0].get_controller())
    yield 5
    report['client_state']=session.last_state.export_text()
    check(session.aborted,'Network failure distinguished as Aborted')
    check(session.last_state.final_value==0,'Host failure pays zero')
    widgets=[w for w in u.ObjectIterator(u.SP1HUDWidget) if w.is_in_viewport() and 'Aborted' in w.displayed_status]
    report['abort_hud_count']=len(widgets)
    check(len(widgets)==1,'Exactly one Aborted HUD survives fallback map')
    report['passed']=True
sequence=steps(); end=0
def finish():
    u.unregister_slate_post_tick_callback(handle)
    Path(u.Paths.project_saved_dir(),'Prototype01/p1-host-disconnect.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
def tick(delta):
    global end
    try:
        if time.monotonic()>=end: end=time.monotonic()+next(sequence)
    except StopIteration: finish()
    except Exception: report['error']=traceback.format_exc(); finish()
handle=u.register_slate_post_tick_callback(tick)
