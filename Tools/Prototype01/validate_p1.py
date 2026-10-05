"""작성자 : 임진혁
읽기 전용 P1 자산/상속/입력/배치/의존 검사. commandlet로 실행하며 저장하지 않는다.
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
check(sum('BP_SPFreightCar_' in a.get_class().get_name() for a in actors) == 1, 'Freight car count')
cargo_cls = u.load_asset(ROOT+'/Blueprints/BP_SP1TransferCargo').generated_class()
exit_cls = u.load_asset(ROOT+'/Blueprints/BP_SP1ExtractionZone').generated_class()
cargo = [a for a in actors if a.get_class() == cargo_cls]
zones = [a for a in actors if a.get_class() == exit_cls]
check(len(cargo) == 4 and len(zones) == 1, 'P1 gameplay actor count')
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
report['passed'] = not report['errors']
Path(u.Paths.project_saved_dir(),'Prototype01/p1-assets-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
u.log('P1_ASSET_VALIDATION '+str(report['passed']))
assert report['passed'], report['errors']
