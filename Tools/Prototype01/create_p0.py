"""작성자 : 임진혁

UE 5.8.2 에디터 Python으로 P0 전용 BP와 맵을 최초 생성한다.
실행 전 아래 신규 .uasset 4개와 맵의 LFS 락을 확보한다.
기존 목적지가 있으면 중단하며 원본 열차 에셋은 저장하지 않는다.
UnrealEditor-Cmd SpacePirate.uproject -run=pythonscript -script=<this file>
  -unattended -nullrhi -nosound -nop4
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools

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
NOTES = {
    'BP_SP1PlayerCharacter': '기존 이동·카메라·직접 집기를 상속한다. P1에서만 홀드 입력 opt-in을 연결한다.',
    'BP_SP1PlayerController': '기존 팀 IMC와 열차·하늘 RPC 경로를 상속한다. P0에서는 입력을 추가하지 않는다.',
    'BP_SP1GameState': '열차 MotionState·하늘 상태를 상속한다. P1 라운드 C++ 컴포넌트를 부착할 전용 자식이다.',
    'BP_SP1GameMode': '전용 맵에서만 사용하는 Pawn·Controller·GameState 연결. P0에는 라운드 판정이 없다.',
}

assert u.SystemLibrary.get_engine_version().startswith('5.8.2-'), 'UE 5.8.2 required'
for path in [MAP] + [ROOT + '/' + name for name in PARENTS]:
    assert not u.EditorAssetLibrary.does_asset_exist(path), 'Existing destination: ' + path
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save current user map before running'
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Save current user assets before running'

# 부모를 바꾸지 않고 자식의 기본값만 설정하여 기존 Train 캐스트와 이벤트를 유지한다.
assets = {}
asset_tools = u.AssetToolsHelpers.get_asset_tools()
for name, parent in PARENTS.items():
    factory = u.BlueprintFactory()
    factory.set_editor_property('parent_class', u.load_asset(parent).generated_class())
    bp = asset_tools.create_asset(name, ROOT, u.Blueprint, factory)
    assert bp, name
    bp.set_editor_property('blueprint_description', '작성자 : 임진혁\n' + NOTES[name])
    u.EditorAssetLibrary.set_metadata_tag(bp, 'Author', '임진혁')
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assets[name] = bp
gm = u.get_default_object(assets['BP_SP1GameMode'].generated_class())
gm.set_editor_property('default_pawn_class', assets['BP_SP1PlayerCharacter'].generated_class())
gm.set_editor_property('player_controller_class', assets['BP_SP1PlayerController'].generated_class())
gm.set_editor_property('game_state_class', assets['BP_SP1GameState'].generated_class())
for bp in assets.values():
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == u.BlueprintStatus.BS_UP_TO_DATE
    assert u.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)

# 지원되는 template API는 원본을 무명 패키지로 로드한 뒤 새 경로에 저장한다.
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert level.new_level_from_template(MAP, '/Game/SpacePirate/Maps/Lvl_SPTrainFreight')
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', assets['BP_SP1GameMode'].generated_class())
consist = next(a for a in actors.get_all_level_actors() if a.get_class().get_name() == 'BP_SPTrainConsist_C')
assert ObjectTools.set_properties(consist, json.dumps({
    'CarSequence': [{'refPath': TRAIN + '/Freight/Data/DA_SPCar_Long.DA_SPCar_Long'}], 'MaxCars': 1}))
consist.set_actor_label('P01_C01_TrainConsist')
consist.set_editor_property('tags', ['Prototype01', 'CarId=P01_C01', 'Author=임진혁'])

# 네 캡슐을 객차 후방에 배치하고 전방의 안전한 표시 구역을 향하게 한다.
starts = sorted((a for a in actors.get_all_level_actors() if isinstance(a, u.PlayerStart)), key=lambda a: a.get_actor_label())
assert len(starts) == 4
for index, (start, position) in enumerate(zip(starts, [(-1900, -90, 320), (-1900, 90, 320), (-1600, -90, 320), (-1600, 90, 320)]), 1):
    start.set_actor_location_and_rotation(u.Vector(*position), u.Rotator(), False, True)
    start.set_actor_label('P01_C01_PlayerStart_' + str(index))
    start.set_editor_property('player_start_tag', 'P01_C01')

def mark(actor, label, extra_tags=()):
    actor.set_actor_label(label)
    actor.set_folder_path('Prototype01/C01')
    actor.set_editor_property('tags', ['Prototype01', 'CarId=P01_C01', 'Author=임진혁', *extra_tags])
    return actor

for label, position in [('P01_C01_Entry', (-2130, 0, 320)), ('P01_C01_Exit', (-450, 0, 320))]:
    mark(actors.spawn_actor_from_class(u.TargetPoint, u.Vector(*position)), label, [label])
zone = mark(actors.spawn_actor_from_class(u.TriggerBox, u.Vector(-650, 0, 330)), 'P01_C01_ExtractionPlaceholder', ['P0MarkerOnly'])
zone.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(200, 150, 120), False)

# 표시 구역은 아직 탈출 규칙이 없는 배치 기준이다. 경계는 비충돌 메시로 보여 준다.
cube = u.load_asset('/Engine/BasicShapes/Cube')
trim = u.load_asset(TRAIN + '/Freight/Materials/M_SPSafetyTrim')
for index, (position, scale) in enumerate([
    ((-850, 0, 214), (0.04, 3.0, 0.02)), ((-450, 0, 214), (0.04, 3.0, 0.02)),
    ((-650, -150, 214), (4.0, 0.04, 0.02)), ((-650, 150, 214), (4.0, 0.04, 0.02)),
]):
    border = mark(actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(*position)), 'P01_C01_ExitBorder_' + str(index))
    mesh = border.get_component_by_class(u.StaticMeshComponent)
    mesh.set_static_mesh(cube)
    mesh.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    if trim:
        mesh.set_material(0, trim)
    border.set_actor_scale3d(u.Vector(*scale))
for label, text, position in [
    ('P01_C01_ExitSign', 'EXIT / P0\nMARKER ONLY', (-430, 0, 355)),
    ('P01_C01_EntrySign', 'C01 / ENTRY', (-2120, 0, 370)),
]:
    # 두 표지를 객차 안쪽에서 읽도록 앞면을 회전한다. Rotator는 이름 인자를 쓴다.
    yaw = 180 if label.endswith('ExitSign') else 0
    sign = mark(actors.spawn_actor_from_class(u.TextRenderActor, u.Vector(*position), u.Rotator(pitch=0, yaw=yaw, roll=0)), label)
    text_component = sign.get_component_by_class(u.TextRenderComponent)
    text_component.set_text(text)
    text_component.set_world_size(28)
    text_component.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    text_component.set_text_render_color(u.Color(80, 220, 255, 255))

assert level.save_current_level()
report = {'map': MAP, 'assets': [bp.get_path_name() for bp in assets.values()], 'actors': []}
for actor in actors.get_all_level_actors():
    row = {'label': actor.get_actor_label(), 'class': actor.get_class().get_path_name(), 'position': actor.get_actor_location().to_tuple()}
    if 'FreightCar_' in actor.get_class().get_name():
        row['meshes'] = [{'name': c.get_name(), 'bounds': str(c.get_local_bounds()), 'transform': str(c.get_world_transform())} for c in actor.get_components_by_class(u.StaticMeshComponent)]
    report['actors'].append(row)
output = Path(u.Paths.project_saved_dir()) / 'Prototype01'
output.mkdir(parents=True, exist_ok=True)
(output / 'creation.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
u.log('P0_CREATED ' + MAP)
