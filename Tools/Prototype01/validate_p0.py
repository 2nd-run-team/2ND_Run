"""작성자 : 임진혁

P0 BP·맵 연결을 읽고 컴파일한다. 에셋은 저장하지 않는다.
UnrealEditor-Cmd SpacePirate.uproject -run=pythonscript -script=<this file>
  -unattended -nullrhi -nosound -nop4
결과: Saved/Prototype01/asset-validation.json
"""
import json
from pathlib import Path
import unreal as u

ROOT = '/Game/SpacePirate/Prototype01/Blueprints'
MAP = '/Game/SpacePirate/Maps/Lvl_SPPrototype01'
TRAIN = '/Game/SpacePirate/Train'
PLANET = TRAIN + '/Environment/PlanetFlyby/Blueprints'
PARENTS = {
    'BP_SP1PlayerCharacter': '/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter',
    'BP_SP1PlayerController': PLANET + '/BP_SPPlanetTrainPlayerController',
    'BP_SP1GameState': PLANET + '/BP_SPPlanetTrainGameState',
    'BP_SP1GameMode': PLANET + '/BP_SPPlanetTrainGameMode',
}
report = {'engine': u.SystemLibrary.get_engine_version(), 'map': MAP, 'blueprints': [], 'errors': []}

def check(condition, message):
    if not condition:
        report['errors'].append(message)

assets = {}
for name, parent in PARENTS.items():
    bp = u.load_asset(ROOT + '/' + name)
    assert bp, name
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    actual_parent = u.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
    check(actual_parent == u.load_asset(parent).generated_class(), 'Parent mismatch: ' + name)
    check(bp.get_editor_property('status') == u.BlueprintStatus.BS_UP_TO_DATE, 'Compile failed: ' + name)
    check('작성자 : 임진혁' in bp.get_editor_property('blueprint_description'), 'Author missing: ' + name)
    assets[name] = bp
    report['blueprints'].append({'path': bp.get_path_name(), 'parent': actual_parent.get_path_name(), 'status': str(bp.get_editor_property('status'))})

gm = u.get_default_object(assets['BP_SP1GameMode'].generated_class())
for prop, name in [('default_pawn_class', 'BP_SP1PlayerCharacter'), ('player_controller_class', 'BP_SP1PlayerController'), ('game_state_class', 'BP_SP1GameState')]:
    check(gm.get_editor_property(prop) == assets[name].generated_class(), 'GameMode binding: ' + prop)
pawn = u.get_default_object(assets['BP_SP1PlayerCharacter'].generated_class())
team_pawn = u.get_default_object(u.load_asset(PARENTS['BP_SP1PlayerCharacter']).generated_class())
for prop in ['move_action', 'look_action', 'jump_action', 'sprint_action', 'interact_action', 'drop_action', 'select_slot_action', 'cycle_slot_action']:
    check(pawn.get_editor_property(prop) == team_pawn.get_editor_property(prop), 'Input changed: ' + prop)
pc = u.get_default_object(assets['BP_SP1PlayerController'].generated_class())
imc = u.load_asset('/Game/SpacePirate/Player/Input/IMC_SPPlayer')
check(imc in pc.get_editor_property('default_mapping_contexts'), 'Team IMC missing')
report['input_mappings'] = [{'action': m.get_editor_property('action').get_path_name(), 'key': str(m.get_editor_property('key').get_editor_property('key_name'))} for m in imc.get_editor_property('default_key_mappings').get_editor_property('mappings')]
check(any(m['key'] == 'E' and '/IA_Interact.' in m['action'] for m in report['input_mappings']), 'E mapping missing')
interact = pawn.get_editor_property('interact_action')
report['interact'] = {'asset': interact.get_path_name(), 'value_type': str(interact.get_editor_property('value_type')), 'triggers': [x.get_class().get_path_name() for x in interact.get_editor_property('triggers')]}
gs = u.get_default_object(assets['BP_SP1GameState'].generated_class())
report['sky_catalog'] = gs.get_editor_property('SkyCatalog').get_path_name()
check(report['sky_catalog'].startswith(TRAIN), 'Sky catalog changed')

# 현재 맵을 바꾸므로 별도 commandlet에서 실행한다. 원본도 읽기만 한다.
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert level.load_level(MAP)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
check(world.get_world_settings().get_editor_property('default_game_mode') == assets['BP_SP1GameMode'].generated_class(), 'World Settings GameMode mismatch')
actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
starts = [a for a in actors if isinstance(a, u.PlayerStart)]
cars = [a for a in actors if 'BP_SPFreightCar_' in a.get_class().get_name()]
check(len(starts) == 4, 'Expected four PlayerStarts')
check(len(cars) == 1, 'Expected one playable freight car')
consist = next(a for a in actors if a.get_class().get_name() == 'BP_SPTrainConsist_C')
check(consist.get_editor_property('MaxCars') == 1 and len(consist.get_editor_property('CarSequence')) == 1, 'Consist configuration mismatch')
check('CarId=P01_C01' in [str(t) for t in consist.get_editor_property('tags')], 'Car ID missing')
scene = next(a for a in actors if a.get_class().get_name() == 'BP_SPTrainSceneController_C')
report['scene_preset'] = scene.get_editor_property('EnvironmentPreset').get_path_name()
check(report['scene_preset'].startswith(TRAIN), 'Existing scene preset missing')
zone = next(a for a in actors if a.get_actor_label() == 'P01_C01_ExtractionPlaceholder')
report['exit_zone'] = {'center': zone.get_actor_location().to_tuple(), 'half_extent': zone.get_component_by_class(u.BoxComponent).get_unscaled_box_extent().to_tuple()}
report['starts'] = [a.get_actor_location().to_tuple() for a in starts]
report['car_count'] = len(cars)
report['map_actors'] = len(actors)
for name in ['P01_C01_Entry', 'P01_C01_Exit', 'P01_C01_ExitSign', 'P01_C01_EntrySign']:
    check(sum(a.get_actor_label() == name for a in actors) == 1, 'Marker missing: ' + name)
for label, position, yaw in [
    ('P01_C01_ExitSign', (-430, 0, 355), 180),
    ('P01_C01_EntrySign', (-2120, 0, 370), 0),
]:
    sign = next(a for a in actors if a.get_actor_label() == label)
    check(sign.get_actor_location().to_tuple() == position, 'Sign position: ' + label)
    rotation = sign.get_actor_rotation()
    check(abs(rotation.roll) < 0.1 and abs(rotation.pitch) < 0.1, 'Upside-down sign: ' + label)
    check(abs((rotation.yaw - yaw + 180) % 360 - 180) < 0.1, 'Backwards sign: ' + label)

# 새 맵과 자식 BP의 재귀 참조에서 누락된 패키지를 확인한다.
reg = u.AssetRegistryHelpers.get_asset_registry()
reg.search_all_assets(True)
options = u.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
pending, visited, missing = [MAP, *[ROOT + '/' + n for n in PARENTS]], set(), []
while pending:
    path = pending.pop()
    if path in visited:
        continue
    visited.add(path)
    for dep in reg.get_dependencies(path, options):
        dep = str(dep)
        if dep.startswith('/Game/'):
            if not reg.get_assets_by_package_name(dep):
                missing.append(dep)
            elif dep not in visited:
                pending.append(dep)
report['game_dependency_packages'] = len(visited)
report['missing_dependencies'] = missing
check(not missing, 'Missing game packages')
report['passed'] = not report['errors']
out = Path(u.Paths.project_saved_dir()) / 'Prototype01/asset-validation.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('P0_ASSET_VALIDATION ' + str(report['passed']))
assert report['passed'], report['errors']
