"""작성자 : 임진혁

P0 전용 BP/맵에 P1 컴포넌트, 화물 데이터와 표현을 연결한다.
UE 5.8.2의 지원되는 에디터 API만 사용한다. 수정 대상의 LFS 락을 먼저 확보한다.
기존 Train/Player/외부 팩 원본은 읽기만 하고, P1 이름으로 만든 액터만 갱신한다.
UnrealEditor-Cmd <project> -run=pythonscript -script=<this file> -unattended -nullrhi -nosound -nop4
"""
import json
from pathlib import Path
import unreal as u

ROOT = '/Game/SpacePirate/Prototype01'
MAP = '/Game/SpacePirate/Maps/Lvl_SPPrototype01'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
tools = u.AssetToolsHelpers.get_asset_tools()

def native(name):
    cls = u.load_class(None, '/Script/SpacePirate.' + name)
    assert cls, 'Build P1 C++ first: ' + name
    return cls

def blueprint(name, folder, parent, note, widget=False):
    path = ROOT + '/' + folder + '/' + name
    bp = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if not bp:
        factory = u.WidgetBlueprintFactory() if widget else u.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        bp = tools.create_asset(name, ROOT + '/' + folder, u.WidgetBlueprint if widget else u.Blueprint, factory)
    assert u.BlueprintEditorLibrary.get_blueprint_parent_class(bp) == parent
    bp.set_editor_property('blueprint_description', '작성자 : 임진혁\n' + note)
    u.EditorAssetLibrary.set_metadata_tag(bp, 'Author', '임진혁')
    if not widget:
        u.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp

hud = blueprint('WBP_SP1HUD', 'UI', native('SP1HUDWidget'), 'P1 상태/진행/결과 UI. Designer에서 배치/색/글꼴을, 기본값에서 음원을 조절한다. 명명된 데이터 위젯의 이름은 유지한다.', True)
assert u.SP1EditorLibrary.build_hud_template(hud)
u.BlueprintEditorLibrary.compile_blueprint(hud)
hud_default = u.get_default_object(hud.generated_class())
audio_root = '/Game/Assets/Sound/InterfaceAndItemSounds/Cues/'
for field, name in [('start_sound','Futuristic_Click_01'),('cancel_sound','Back_Click_01'),('success_sound','Special_Musical_01'),('failure_sound','Error_Buzz_01')]:
    sound = u.load_asset(audio_root + name + '_wav_Cue')
    assert sound
    hud_default.set_editor_property(field,sound)
atten_path = ROOT + '/Audio/ATT_SP1Cargo'
atten = u.load_asset(atten_path) if u.EditorAssetLibrary.does_asset_exist(atten_path) else tools.create_asset('ATT_SP1Cargo',ROOT+'/Audio',u.SoundAttenuation,u.SoundAttenuationFactory())
settings = atten.get_editor_property('attenuation')
settings.set_editor_property('attenuate', True)
settings.set_editor_property('spatialize', True)
settings.set_editor_property('attenuation_shape_extents', u.Vector(100,0,0))
settings.set_editor_property('falloff_distance', 1000.0)
atten.set_editor_property('attenuation',settings)
u.EditorAssetLibrary.set_metadata_tag(atten,'Author','임진혁')
assert u.EditorAssetLibrary.save_loaded_asset(atten,False)
cargo_bp = blueprint('BP_SP1TransferCargo', 'Blueprints', native('SP1TransferCargo'), '서버 판정과 독립된 전송 화물 외형. DataAsset으로 메시/가치/시간을 지정한다.')
exit_bp = blueprint('BP_SP1ExtractionZone', 'Blueprints', native('SP1ExtractionZone'), '캡슐 중심으로 탈출 구역을 판정한다. 보유 통신 단말 메시를 출발 패널로 재사용한다.')
sub = u.get_engine_subsystem(u.SubobjectDataSubsystem)
lib = u.SubobjectDataBlueprintFunctionLibrary

def component(bp, cls):
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        obj = lib.get_object_for_blueprint(lib.get_data(handle), bp)
        if obj and obj.get_class() == cls:
            return obj
    handle, fail = sub.add_new_subobject(u.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=bp))
    obj = lib.get_object_for_blueprint(lib.get_data(handle), bp)
    assert obj, str(fail)
    return obj

# 기존 계층을 유지하고 전용 자식의 SCS에만 컴포넌트를 붙인다.
pawn_bp = u.load_asset(ROOT + '/Blueprints/BP_SP1PlayerCharacter')
gs_bp = u.load_asset(ROOT + '/Blueprints/BP_SP1GameState')
hold = component(pawn_bp, native('SP1InteractionComponent'))
hold.set_editor_property('hud_class', hud.generated_class())
round_component = component(gs_bp, native('SP1RoundComponent'))
round_component.set_editor_property('mission_seconds', 120.0)
round_component.set_editor_property('departure_seconds', 10.0)
pawn_bp.set_editor_property('blueprint_description', '작성자 : 임진혁\n팀 Pawn 상속. SP1Interaction 컴포넌트가 있을 때만 E 홀드 경로를 사용한다.')
gs_bp.set_editor_property('blueprint_description', '작성자 : 임진혁\nPlanetTrain GameState를 상속하고 SP1Round 컴포넌트로 P1 판정을 연결한다.')

# 원본 메시 9개를 실제 로드/경계/충돌 검사한 뒤 실루엣이 다른 네 가지를 선택했다.
specs = [
    ('Supply','보급 상자',4,40,0.8,0.9,(-1500,-300,212)),
    ('Parts','부품 케이스',3,100,1.2,0.85,(-1200,-300,212)),
    ('Alloy','합금 상자',1,160,2.5,0.65,(-1500,300,212)),
    ('Core','데이터 코어',8,300,2.0,1.0,(-1050,300,212)),
]
definitions = {}
for role, title, box, value, seconds, scale, position in specs:
    name = 'DA_SP1' + role
    path = ROOT + '/Data/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        data = u.load_asset(path)
    else:
        factory = u.DataAssetFactory()
        factory.set_editor_property('data_asset_class', native('SP1CargoDefinition'))
        data = tools.create_asset(name, ROOT + '/Data', u.DataAsset, factory)
    mesh = u.load_asset('/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_' + str(box))
    assert mesh
    data.set_editor_property('display_name', title)
    data.set_editor_property('value', value)
    data.set_editor_property('hold_seconds', seconds)
    data.set_editor_property('mesh', mesh)
    data.set_editor_property('complete_sound',u.load_asset(audio_root+'Item_Sell_Purchase_01_wav_Cue'))
    data.set_editor_property('complete_attenuation',atten)
    data.set_editor_property('mesh_scale', u.Vector(scale,scale,scale))
    center = mesh.get_bounds().origin
    data.set_editor_property('interaction_offset', center * scale)
    u.EditorAssetLibrary.set_metadata_tag(data, 'Author', '임진혁')
    assert u.EditorAssetLibrary.save_loaded_asset(data, False)
    definitions[role] = data

for bp in [hud,cargo_bp,exit_bp,pawn_bp,gs_bp]:
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert bp.get_editor_property('status') == u.BlueprintStatus.BS_UP_TO_DATE, bp.get_path_name()
    assert u.EditorAssetLibrary.save_loaded_asset(bp, False)

levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(MAP)
existing = {a.get_actor_label():a for a in actors.get_all_level_actors()}

def place(cls, label, position):
    actor = existing.get(label)
    if actor:
        assert actor.get_class() == cls, 'Unexpected user actor: ' + label
    else:
        actor = actors.spawn_actor_from_class(cls, u.Vector(*position))
    actor.set_actor_label(label)
    actor.set_actor_location_and_rotation(u.Vector(*position), u.Rotator(), False, True)
    actor.set_editor_property('tags',['Prototype01','CarId=P01_C01','Author=임진혁'])
    actor.set_folder_path('Prototype01/C01')
    return actor

for role, title, box, value, seconds, scale, position in specs:
    actor = place(cargo_bp.generated_class(), 'P1_C01_Cargo_' + role, position)
    actor.set_editor_property('cargo_id', 'P01_C01_' + role)
    actor.set_editor_property('definition', definitions[role])
    # Definition 변경을 에디터에서 바로 시각화한다.
    mesh = actor.get_component_by_class(u.StaticMeshComponent)
    mesh.set_static_mesh(definitions[role].get_editor_property('mesh'))
    mesh.set_relative_scale3d(u.Vector(scale,scale,scale))

zone = place(exit_bp.generated_class(), 'P1_C01_Extraction', (-650,0,330))
terminal = zone.get_editor_property('terminal')
terminal.set_static_mesh(u.load_asset('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01'))
terminal.set_relative_location(u.Vector(180,0,-20), False, True)
terminal.set_relative_rotation(u.Rotator(pitch=0,yaw=90,roll=0), False, True)
terminal.set_relative_scale3d(u.Vector(0.6,0.6,0.6))
zone.set_editor_property('interaction_offset', u.Vector(180,0,20))
placeholder = existing.get('P01_C01_ExtractionPlaceholder')
if placeholder:
    assert 'P0MarkerOnly' in [str(t) for t in placeholder.get_editor_property('tags')]
    actors.destroy_actor(placeholder)
sign = existing['P01_C01_ExitSign'].get_component_by_class(u.TextRenderComponent)
sign.set_text('EXIT / C01\nHOLD E 1s\nDEPARTURE 10s')
existing['P01_C01_ExitSign'].set_actor_location(u.Vector(-430,0,510),False,True)
assert levels.save_current_level()
out = Path(u.Paths.project_saved_dir()) / 'Prototype01/p1-setup.json'
out.write_text(json.dumps({'map':MAP,'blueprints':[bp.get_path_name() for bp in [hud,cargo_bp,exit_bp,pawn_bp,gs_bp]],'cargo_definitions':[d.get_path_name() for d in definitions.values()]},indent=2),encoding='utf-8')
u.log('P1_SETUP_COMPLETE')
