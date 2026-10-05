"""작성자 : 임진혁
읽기 전용 P4 자산/상속/입력/배치/의존 검사. commandlet로 실행하며 저장하지 않는다.
"""
import json
from pathlib import Path
import unreal as u

ROOT = '/Game/SpacePirate/Prototype01'
MAP = '/Game/SpacePirate/Maps/Lvl_SPPrototype01'
report = {'engine':u.SystemLibrary.get_engine_version(),'blueprints':[],'errors':[],'cargo':[]}
def check(ok, message):
    if not ok: report['errors'].append(message)
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
paths = [str(a.package_name) for a in registry.get_assets_by_path(ROOT, True)]
for path in paths:
    asset = u.load_asset(path)
    check(bool(asset), 'Load failed: ' + path)
    if isinstance(asset,u.Blueprint):
        u.BlueprintEditorLibrary.compile_blueprint(asset)
        check(asset.get_editor_property('status') == u.BlueprintStatus.BS_UP_TO_DATE, 'Compile: ' + path)
        report['blueprints'].append({'path':path,'parent':u.BlueprintEditorLibrary.get_blueprint_parent_class(asset).get_path_name(),'status':str(asset.get_editor_property('status'))})
        check('작성자 : 임진혁' in asset.get_editor_property('blueprint_description'), 'Author: ' + path)
for name, parent in [('BP_SP1PlayerCharacter','/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter'),('BP_SP1GameState','/Game/SpacePirate/Train/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameState')]:
    bp = u.load_asset(ROOT+'/Blueprints/'+name)
    check(u.BlueprintEditorLibrary.get_blueprint_parent_class(bp) == u.load_asset(parent).generated_class(), 'Changed parent: '+name)
sub = u.get_engine_subsystem(u.SubobjectDataSubsystem)
lib = u.SubobjectDataBlueprintFunctionLibrary
for bp_name, component_name in [('BP_SP1PlayerCharacter','SP1InteractionComponent'),('BP_SP1GameState','SP1RoundComponent')]:
    bp = u.load_asset(ROOT+'/Blueprints/'+bp_name)
    objects = [lib.get_object_for_blueprint(lib.get_data(h),bp) for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
    matches = [o for o in objects if o and o.get_class().get_name() == component_name]
    check(len(matches) == 1, 'Expected one '+component_name)
    if matches:
        check(matches[0].get_editor_property('replicates'), 'Replication disabled: '+component_name)
        if component_name == 'SP1InteractionComponent':
            check(bool(matches[0].get_editor_property('hud_class')), 'HUD missing')
            report['hold_tuning'] = {p:matches[0].get_editor_property(p) for p in ['start_distance','maintain_distance','aim_angle_degrees','aim_grace_seconds','heartbeat_interval','heartbeat_timeout']}
        else:
            report['round_tuning'] = {p:matches[0].get_editor_property(p) for p in ['mission_seconds','departure_seconds']}
imc = u.load_asset('/Game/SpacePirate/Player/Input/IMC_SPPlayer')
mappings = imc.get_editor_property('default_key_mappings').get_editor_property('mappings')
check(sum(str(m.get_editor_property('key').get_editor_property('key_name')) == 'E' for m in mappings) == 1,'E mapped more than once')
check(len(u.load_asset('/Game/Input/Actions/IA_Interact').get_editor_property('triggers')) == 0,'Common E trigger changed')
assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(MAP)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
check(world.get_world_settings().get_editor_property('default_game_mode') == u.load_asset(ROOT+'/Blueprints/BP_SP1GameMode').generated_class(),'World Settings GameMode')
actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
check(sum(isinstance(a,u.PlayerStart) for a in actors) == 4, 'PlayerStart count')
check(sum('BP_SPFreightCar_' in a.get_class().get_name() for a in actors) == 10, 'Freight car count')
cargo_cls = u.load_asset(ROOT+'/Blueprints/BP_SP1TransferCargo').generated_class()
exit_cls = u.load_asset(ROOT+'/Blueprints/BP_SP1ExtractionZone').generated_class()
cargo = [a for a in actors if isinstance(a,u.SP1TransferCargo)]
zones = [a for a in actors if a.get_class() == exit_cls]
check(len(cargo) == 42 and len(zones) == 2, 'P1 gameplay actor count')
ids = [str(a.get_editor_property('cargo_id')) for a in cargo]
check(len(set(ids)) == len(ids) and 'None' not in ids,'Cargo IDs')
for actor in cargo:
    data = actor.get_editor_property('definition')
    check(bool(data),'Definition missing')
    mesh = actor.get_editor_property('mesh')
    check(not mesh.is_simulating_physics(),'Cargo physics enabled')
    report['cargo'].append({'id':str(actor.get_editor_property('cargo_id')),'definition':data.get_path_name(),'mesh':data.get_editor_property('mesh').get_path_name(),'value':data.get_editor_property('value'),'seconds':data.get_editor_property('hold_seconds'),'position':actor.get_actor_location().to_tuple(),'point':actor.get_interaction_point().to_tuple()})
pending, visited, missing = [MAP,*paths], set(), set()
options = u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
while pending:
    path = pending.pop()
    if path in visited: continue
    visited.add(path)
    for dep in registry.get_dependencies(path,options):
        dep = str(dep)
        if dep.startswith('/Game/'):
            if not registry.get_assets_by_package_name(dep): missing.add(dep)
            elif dep not in visited: pending.append(dep)
report['dependency_packages'] = len(visited)
report['missing_dependencies'] = sorted(missing)
check(not missing,'Missing dependencies')
# P4의 전용 설정만 변경되었는지 공용 부모와 함께 검사한다.
pawn=u.load_asset(ROOT+'/Blueprints/BP_SP1PlayerCharacter')
objects=[lib.get_object_for_blueprint(lib.get_data(h),pawn) for h in sub.k2_gather_subobject_data_for_blueprint(pawn)]
lives=[o for o in objects if isinstance(o,u.SP1SurvivalComponent)]
check(len(lives)==1,'Expected one replicated survival component')
if lives:
    report['survival']={name:lives[0].get_editor_property(name) for name in ['replicates','sprint_drain','recovery_delay','recovery_rate']}
    check(report['survival']==dict(replicates=True,sprint_drain=20,recovery_delay=.75,recovery_rate=25),'Survival tuning')
parent=u.get_default_object(u.load_asset('/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter').generated_class())
default=u.get_default_object(pawn.generated_class())
report['walking']={'common':parent.character_movement.max_walk_speed,'prototype':default.character_movement.max_walk_speed,'sprint':default.character_movement.sprint_speed}
check(report['walking']==dict(common=400,prototype=450,sprint=700),'Common / dedicated movement tuning')
lasers=[a for a in actors if isinstance(a,u.SP1LaserHazard)]
check(len(lasers)==2,'Two periodic lasers')
if lasers:
    laser=next(a for a in lasers if str(a.hazard_id)=='P01_C02_Laser01')
    report['laser']={name:laser.get_editor_property(name) for name in ['on_seconds','off_seconds','warning_seconds','wound_amount','damage_interval']}
    report['laser']['id']=str(laser.hazard_id)
    report['laser']['position']=laser.get_actor_location().to_tuple()
    report['laser']['beam']=laser.beam_system.get_path_name()
    report['laser']['warning_sound']=laser.warning_sound.get_path_name()
    report['laser']['bounds']=laser.damage_bounds.get_unscaled_box_extent().to_tuple()
    check(laser.hazard_id=='P01_C02_Laser01','Hazard ID')

regions=[a for a in actors if isinstance(a,u.SP1CarRegion)]
check(len(regions)==10,'Ten explicit car regions')
check(len(set(str(a.car_id) for a in regions))==10,'Unique car region IDs')
cars=[a for a in actors if 'BP_SPFreightCar_Long' in a.get_class().get_name()]
check(sorted(round(a.get_actor_location().x) for a in cars)==sorted(round(a.get_actor_location().x) for a in regions),'All ten floors align with gameplay regions')
report['car_centers']=[a.get_actor_location().to_tuple() for a in sorted(cars,key=lambda a:a.get_actor_location().x)]
gates=[a for a in actors if isinstance(a,u.SP1Bulkhead)]
panels=[a for a in actors if isinstance(a,u.SP1Panel)]
check(len(gates)==1 and len(panels)==3,'One gate and three panels')
check(all(a.bulkhead==gates[0] for a in panels),'Panel links')
check(len(set(str(a.kind) for a in panels))==3,'Distinct panel routes')
special=[a for a in cargo if isinstance(a,u.SP1GravityCargo)]
check(len(special)==2 and special[0].definition.value==350 and special[0].definition.hold_seconds==3,'S01 350 / 3s')
check(bool(special[0].gravity_zone),'S01 gravity link')
check(special[0].gravity_zone.get_component_by_class(u.BoxComponent).get_unscaled_box_extent().x==900,'S01 excludes connectors')
cameras=[a for a in actors if isinstance(a,u.SP1SecurityCamera)]
check(len(cameras)==2 and {str(a.region.car_id) for a in cameras}=={'P01_C06','P01_C10'},'Camera car scope')
report['camera']={n:cameras[0].get_editor_property(n) for n in ['range','full_angle','exposure_seconds','decay_per_second','warning_seconds','cooldown_seconds','wound_amount']}
report['bulkhead']={'together':gates[0].together_seconds,'bypass':gates[0].bypass_seconds}
report['mesh_collision_audit']=[]
for path in ['/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Door_half_01','/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01','/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Sci_fi_wall_straight_01']:
    mesh=u.load_asset(path); body=mesh.get_editor_property('body_setup')
    report['mesh_collision_audit'].append({'path':path,'geometry':body.get_editor_property('agg_geom').export_text() if body else None})

report['actor_count']=len(actors)
# Data budget and all ten spatial assignments are checked independently of the setup script.
from collections import Counter
actual=Counter((str(a.car_id),a.definition.get_path_name(),isinstance(a,u.SP1GravityCargo)) for a in cargo)
scenario=u.load_asset(ROOT+'/Data/DA_SP1Plan04')
expected=Counter({(str(r.car_id),r.definition.get_path_name(),r.gravity_cargo):r.count for r in scenario.cargo_budget})
check(actual==expected,'Per-car cargo budget')
check(len(cargo)-len(special)==40 and sum(a.definition.value for a in cargo)==7060,'40 normal + 2 S01 = 7060')
check(len(set(str(s.gravity_zone.get_path_name()) for s in special))==2,'Independent gravity zones')
for a in cargo:
    containing=[r for r in regions if r.contains_point(a.get_actor_location())]
    check(len(containing)==1 and containing[0].car_id==a.car_id,'Cargo spatial CarId '+str(a.cargo_id))
report['budget']={'normal':40,'special':len(special),'value':sum(a.definition.value for a in cargo),'rows':[{'car':k[0],'definition':k[1],'special':k[2],'count':v} for k,v in actual.items()]}
report['settings']=[]
for name,seconds,players in [('FourPlayer420',420,4),('TwoPlayer600',600,2),('Original600',600,4)]:
    setting=u.load_asset(ROOT+'/Data/DA_SP1'+name)
    check(setting.mission_seconds==seconds and setting.intended_players==players,'Timing preset '+name)
    report['settings'].append({'id':str(setting.settings_id),'seconds':setting.mission_seconds,'players':setting.intended_players})
stations=[a for a in actors if isinstance(a,u.SP1MaintenanceStation)]
check(len(stations)==1 and str(stations[0].region.car_id)=='P01_C05','One maintenance car within 10')
check(stations[0].hold_seconds==3 and stations[0].window_seconds==30 and len(stations[0].revive_offsets)==4,'Revive tuning / four spawn positions')
check(len(set(str(z.departure_id) for z in zones))==2 and sum(z.intermediate for z in zones)==1,'Exclusive departure IDs / types')
report['exits']=[]
for z in zones:
    check(bool(z.terminal.static_mesh),'Departure terminal mesh / interaction collision '+str(z.departure_id))
    e=z.zone.get_unscaled_box_extent();check(e.x==200 and e.y==200,'4m by 4m exit')
    if not z.intermediate:check(z.protect_from_hazards and z.safe_approach_centimeters>=200,'Final exit + two metre protected approach')
    report['exits'].append({'id':str(z.departure_id),'label':str(z.departure_label),'position':z.get_actor_location().to_tuple(),'extent':e.to_tuple(),'protected':z.protect_from_hazards,'approach_cm':z.safe_approach_centimeters})
report['passed'] = not report['errors']
Path(u.Paths.project_saved_dir(),'Prototype01/p4-assets-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('P4_ASSET_VALIDATION '+str(report['passed']))
assert report['passed'], report['errors']
