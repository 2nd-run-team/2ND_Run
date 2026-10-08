"""Read saved SP naming redirects and train/status parents after an editor restart.

No saves or compilation. Run outside PIE. Writes Saved/StealthMerge/assets.json.
"""
import json
from pathlib import Path
import unreal as u

assert not u.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before asset validation.'
report = {'checks': []}


def check(name, value):
    report['checks'].append({'name': name, 'passed': bool(value)})


registry = u.AssetRegistryHelpers.get_asset_registry()
for folder, old, new in [
        ('UI', 'WBP_DebugHelp', 'WBP_SPDebugHelp'),
        ('Cargo', 'ABP_CargoCarry', 'ABP_SPCargoCarry'),
        ('Cargo', 'AS_CargoCarryPose', 'AS_SPCargoCarryPose')]:
    base = '/Game/SpacePirate/' + folder + '/'
    asset = u.load_asset(base + new)
    check(new + ' exists with correct object name', asset is not None and asset.get_name() == new)
    data = registry.get_asset_by_object_path(base + old + '.' + old)
    check(old + ' is an intentional compatibility redirector', data.is_valid() and data.is_redirector())
    check(old + ' resolves to new asset', u.load_asset(base + old) == asset)

player_bp = u.load_asset('/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter')
player = u.get_default_object(player_bp.generated_class())
check('Existing player loads renamed help widget', player.get_editor_property('debug_help_widget_class').get_path_name().endswith('WBP_SPDebugHelp_C'))
check('Existing player loads renamed cargo animation BP', player.get_editor_property('mesh').get_editor_property('anim_class').get_path_name().endswith('ABP_SPCargoCarry_C'))
help_bp = u.load_asset('/Game/SpacePirate/UI/WBP_SPDebugHelp')
check('Saved BP help explains full reset', any('발각' in str(e.description) and '경보' in str(e.description)
      for e in u.get_default_object(help_bp.generated_class()).get_editor_property('help_entries')))
for path in ['/Game/SpacePirate/Train/Framework/GameStates/BP_SPTrainGameState',
             '/Game/SpacePirate/Train/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameState']:
    bp = u.load_asset(path)
    check(path.rsplit('/', 1)[-1] + ' inherits shared status GameState', bp is not None and
          isinstance(u.get_default_object(bp.generated_class()), u.SPGameState))
report['passed'] = all(x['passed'] for x in report['checks'])
output = Path(u.Paths.project_saved_dir(), 'StealthMerge', 'assets.json')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('STEALTH_INTEGRATION_ASSETS: ' + str(report['passed']))
assert report['passed'], [x for x in report['checks'] if not x['passed']]
