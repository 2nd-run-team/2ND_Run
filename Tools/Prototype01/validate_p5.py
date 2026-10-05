"""작성자 : 임진혁
P4의 배치/화물/상속 검사를 재사용하고 P5 표현 참조와 설정 버전을 읽기 전용으로 검사한다.
기존 단계의 검증 결과 파일은 덮지 않는다. BP 컴파일 결과도 저장하지 않는다.
"""
from pathlib import Path
import json
import unreal as u
source=Path(u.Paths.project_dir(),'Tools/Prototype01/validate_p4.py').read_text(encoding='utf8')
exec(compile(source.replace('p4-assets-validation.json','p5-assets-validation.json').replace('P4_ASSET_VALIDATION','P5_BASE_VALIDATION'), 'validate_p4.py','exec'))
definitions=[u.load_asset(ROOT+'/Data/DA_SP1'+name) for name in ['Supply','Parts','Alloy','Core','S01']]
check(len({a.mesh.get_path_name() for a in definitions})==5,'Five cargo silhouettes')
check(len({a.icon.get_path_name() for a in definitions if a.icon})==5,'Five cargo icons')
check(all(str(a.badge_label) for a in definitions),'Cargo text labels independent of materials')
hud=u.load_asset(ROOT+'/UI/WBP_SP1HUD'); cdo=u.get_default_object(hud.generated_class())
check(len(cdo.cues)==9 and len({s.get_path_name() for s in cdo.cues.values()})==9,'Nine distinct HUD sound cues')
check(all(cdo.get_editor_property(p) for p in ['location_ping_icon','cargo_ping_icon','danger_ping_icon']),'Ping icon references')
gs=u.load_asset(ROOT+'/Blueprints/BP_SP1GameState')
components=[lib.get_object_for_blueprint(lib.get_data(h),gs) for h in sub.k2_gather_subobject_data_for_blueprint(gs)]
round_component=next(o for o in components if isinstance(o,u.SP1RoundComponent))
check(round_component.settings_version=='Plan01-P05.1' and '+P5-' in round_component.build_identifier,'P5 log version metadata')
report['p5']={'build_id':round_component.build_identifier,'settings_version':str(round_component.settings_version),'icons':[a.icon.get_path_name() for a in definitions],'cues':{str(k):v.get_path_name() for k,v in cdo.cues.items()}}
report['passed']=not report['errors']
Path(u.Paths.project_saved_dir(),'Prototype01/p5-assets-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
u.log('P5_ASSET_VALIDATION '+str(report['passed']))
assert report['passed'],report['errors']

