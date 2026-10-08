"""Localize the existing lab in place. Run once after the Korean C++ build.

Requires owned LFS locks; refuses dirty packages/PIE. Backs up edited binaries.
Actor labels/IDs, geometry, crime rules and dispatch settings remain intact.
The generated font embeds only needed glyphs from the installed Malgun Gothic.
"""
import json, shutil, subprocess
from datetime import datetime
from pathlib import Path
import unreal as u

SIGNS = {
    'Welcome': '잠입 시험장\n먼저 01 → 03 → 04 → 02 → 05\n매 검사 전 08번에서 보안 초기화\n물건 복원은 플레이 종료 후 다시 시작',
    'Public_Sign': '01 일반 구역\n걷기 · 달리기 · 점프 · 가방 운반\n미발각 상태에서는 경비가 무시합니다',
    'Restricted_Sign': '02 제한 구역 · 실제 시야 시험\n침입한 모습을 1초 동안 보면 발각\n벽 뒤에 숨으면 확인이 끊깁니다',
    'Interaction_Sign': '03 줍기 · 운반 · 실제 포장\nE 줍기 / E 2초 유지: 포장\nG 내려놓기 / 1~4 물건 선택\n포장은 목격될 때만 범죄로 발각',
    'Reserved_Sign_40': '06 익명 사건 강제 시험\n상자 조준 + E → 지정 위치에 경비 호출\n신원 발각 없음 · 전체 경보 없음',
    'Reserved_Sign_50': '07 발각 · 경보 강제 시험\n실제 시야 없이 결과를 만드는 개발 버튼\n실제 목격 시험은 02번과 04번',
    'Reserved_Sign_60': '08 시험 제어\n새 시험 시작 / 종료 / 보안 초기화\n물건과 플레이어 위치는 복원하지 않음',
    'Investigation_Sign_Public': '09 현장 수색 목적지\n06번의 현장 조사 버튼으로 호출\n경비 도착 → 5초 수색 → 순찰',
    'Investigation_Sign_Unreachable': '09 도달 불가 목적지\n공중 위치 · 실제 발판 아님\n경로 실패 후 제한 재시도하고 복귀',
    'Crime_PublicNormal_Sign': '01 정상 행동 확인\n이동 · 점프 · 가방 운반은 범죄 아님',
    'Crime_PublicWork_Sign': '04 실제 범죄 목격 시험\n경비가 행동을 봐야 발각됩니다\n지속 행동 1초 / 순간 행동 즉시',
    'Crime_NoiseWall_Sign': '04 벽 뒤 작업 소리\nE로 벽 뒤 위치에 경비 1명 호출\n신원 · 전체 경보는 그대로 유지',
    'Laser_Crouch_Sign': '05-1 앉아서 통과 · 높이 145센티미터\n안전 공간에서 Ctrl을 먼저 누르세요\n끝까지 앉으면 무접촉 / 서면 감지',
    'Laser_Jump_Sign': '05-2 점프로 통과 · 높이 40센티미터\n걷다가 빔 앞에서 Space\n발이 빔을 넘으면 무접촉 / 걸으면 감지',
    'Laser_Wait_Sign': '05-3 꺼짐 대기 · 높이 95센티미터\n켜짐 4초 / 꺼짐 4초 / 예고 1초\n꺼짐 · 통과 가능 표시에서 이동',
    'Laser_Crouch_SafeSign': '안전 대기 공간\nCtrl 유지 후 앞으로 이동',
    'Laser_Jump_SafeSign': '안전 대기 공간\n걷기 시작 후 빔 앞에서 점프',
    'Laser_Wait_SafeSign': '안전 대기 공간\n파란 빔과 꺼짐 표시를 기다리세요',
}
BUTTONS = {
    'Incident_Laser': '06-1 레이저 사건 강제 발생\nE: 지정 위치 수색\n실제 빔 시험은 05번',
    'Incident_WorkNoise': '06-2 작업 소리 사건\nE: 지정 위치 수색\n신원 · 경보 변화 없음',
    'Incident_Indirect': '06-3 간접 신고 사건\nE: 지정 위치 수색\n신원 · 경보 변화 없음',
    'Incident_Sustained': '07-1 지속 범죄 강제 목격\nE 1초 유지\n본인 발각 + 전체 경보',
    'Incident_Instant': '07-2 순간 범죄 강제 목격\nE 한 번\n본인 발각 + 전체 경보',
    'Incident_DirectReport': '07-3 직접 목격 신고 완료\nE 2초 유지\n본인 발각 + 전체 경보',
    'Incident_Victim': '07-4 피해자 신고\nE: 전체 경보만 발생\n신원 정보 불필요',
    'Incident_Rediscover': '07-5 발각자 재발견\nE: 이미 발각된 본인만 처리\n미발각이면 거부',
    'Incident_Escape': '07-6 탈출 작동 사건\nE: 전체 경보만 발생\n실제 탈출 완료 기능 아님',
    'Stage_Start': '08-1 새 시험 시작\nE: 보안 상태 초기화\n새 회차로 시작',
    'Stage_End': '08-2 시험 종료\nE: 보안 감지 중지\n기존 발각 · 경보 지우기',
    'Stage_Restart': '08-3 보안 초기화\nE: 발각 · 경보 · 수색 초기화\n물건 · 위치는 유지',
    'Stage_Replay': '08-4 마지막 버튼 사건 반복\nE: 같은 사건 다시 제출\n중복이면 추가 효과 없음',
    'Investigation_Public': '06-4 현장 조사 호출\nE: 09번 바닥으로 경비 1명\n도착 후 5초 수색',
    'Investigation_Unreachable': '06-5 도달 불가 호출\nE: 09번 공중으로 경비 1명\n실패 후 무한 정체 없이 복귀',
    'Crime_Pickpocket': '04-1 소매치기 행동 시험\nE 한 번 / 그 순간 봐야 발각\n실제 물건 탈취는 미구현',
    'Crime_Terminal': '04-2 보안 단말 행동 시험\nE 유지 / 1초 목격 시 발각\n3초 완료 · 단말 효과 미구현',
    'Crime_Vault': '04-3 금고 작업 행동 시험\nE 유지 / 1초 목격 시 발각\n3초 완료 · 금고 개방 미구현',
    'Crime_Packing': '04-4 포장 행동 시험\nE 유지 / 1초 목격 시 발각\n실제 가방 생성은 옆 포장 묶음',
    'Crime_Tool': '04-5 제압 도구 행동 시험\nE 한 번 / 그 순간 봐야 발각\n실제 기절 효과는 미구현',
    'Crime_WorkNoise': '04-6 벽 뒤 작업 소리\nE: 경비 1명만 조사\n신원 · 경보 변화 없음',
}

def style_blueprint(bp, font, material):
    """Persist inherited native component defaults, not just the live CDO.

    Runtime setters alone do not dirty a Blueprint package; save_loaded_asset
    can then skip it. Editor property notifications also update placed copies.
    """
    bp.modify()
    component = u.get_default_object(bp.generated_class()).get_component_by_class(u.TextRenderComponent)
    assert component
    component.modify()
    component.set_editor_property('font', font)
    component.set_editor_property('text_material', material)
    if bp.get_name() == 'BP_SPGuardCharacter':
        component.set_editor_property('text', '순찰 중')
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)


def run():
    project=Path(u.Paths.project_dir()).resolve()
    root='/Game/SpacePirate/Stealth'
    font_path=root+'/Prototype/UI/F_SPStealthKorean'
    material_path=root+'/Prototype/Materials/M_SPStealthKoreanText'
    required=['Content/SpacePirate/Maps/Lvl_SPStealthTest.umap']+[
        'Content/SpacePirate/Stealth/Blueprints/'+name+'.uasset' for name in ['BP_SPLaserSecurityDevice','BP_SPStealthTestConsole','BP_SPGuardCharacter']]+[
        'Content/SpacePirate/Stealth/Prototype/UI/F_SPStealthKorean.uasset',
        'Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthKoreanText.uasset']
    locks=json.loads(subprocess.check_output(['git','lfs','locks','--verify','--json'],cwd=project,
        creationflags=subprocess.CREATE_NO_WINDOW,text=True,encoding='utf-8'))
    assert set(required)<={r['path'] for r in locks['ours']},'Acquire owned LFS locks first.'
    assert not u.EditorLevelLibrary.get_pie_worlds(False)
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().split('.')[0]=='/Game/SpacePirate/Maps/Lvl_SPStealthTest'
    assert not u.EditorAssetLibrary.does_asset_exist(font_path),'Already localized; edit saved font/map instead.'
    backup=project/'Saved/StealthKorean'/('before-assets-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    for path in required:
        if (project/path).exists():shutil.copy2(project/path,backup/Path(path).name)

    text=''.join(SIGNS.values())+''.join(BUTTONS.values())+'순찰 중 화물 보안 구역'
    for name in ['SPGuardCharacter.cpp','SPLaserSecurityDevice.cpp','SPStealthStatusWidget.cpp','SPStealthTestConsole.cpp']:
        text+=(project/'Source/SpacePirate'/name).read_text(encoding='utf-8-sig')
    # Explicit Chars filtering also applies to ASCII in this UE importer.
    # Include digits, key names and spaces explicitly, not just the range flag.
    chars=''.join(chr(i) for i in range(32,127))+''.join(sorted({c for c in text if ord(c)>126 and c.isprintable()}))
    factory=u.TrueTypeFontFactory()
    options=factory.get_editor_property('import_options');data=options.get_editor_property('data')
    for key,value in [('font_name','Malgun Gothic'),('height',32),('chars',chars),('include_ascii_range',True),
        ('texture_page_width',2048),('texture_page_max_height',2048),('enable_antialiasing',True),('use_distance_field_alpha',False)]:
        data.set_editor_property(key,value)
    options.set_editor_property('data',data)
    font=u.AssetToolsHelpers.get_asset_tools().create_asset('F_SPStealthKorean',root+'/Prototype/UI',u.Font,factory)
    assert font
    # This importer reserves 256 ASCII/Latin-1 slots even when filtered out.
    # Nonzero glyph metrics, rather than the slot count, prove keys are usable.
    glyphs=font.get_editor_property('characters')
    assert all(glyphs[i].get_editor_property('u_size')>0 for i in [32,48,49,69,71,87,183])
    assert u.EditorAssetLibrary.save_loaded_asset(font)
    mat=u.AssetToolsHelpers.get_asset_tools().create_asset('M_SPStealthKoreanText',root+'/Prototype/Materials',u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided',True)
    sample=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionFontSampleParameter,-300,100)
    sample.set_editor_property('parameter_name','Font');sample.set_editor_property('font',font)
    sample.set_editor_property('font_texture_page',0)
    color=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionVertexColor,-300,-100)
    assert u.MaterialEditingLibrary.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert u.MaterialEditingLibrary.connect_material_property(sample,'A',u.MaterialProperty.MP_OPACITY_MASK)
    u.MaterialEditingLibrary.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat)

    def style(component,size=None):
        component.set_font(font);component.set_text_material(mat)
        if size is not None:component.set_world_size(size)
    for bpname in ['BP_SPLaserSecurityDevice','BP_SPStealthTestConsole','BP_SPGuardCharacter']:
        bp=u.load_asset(root+'/Blueprints/'+bpname)
        style_blueprint(bp,font,mat)
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    by_name={a.get_actor_label():a for a in actors}
    assert set(SIGNS)|set(BUTTONS)<=set(by_name)
    for label,value in SIGNS.items():
        a=by_name[label];c=a.get_component_by_class(u.TextRenderComponent)
        c.set_text(value);style(c,20 if label.startswith('Laser_') else 22 if label.startswith(('Crime_','Investigation_')) else 32)
    for label,value in BUTTONS.items():
        a=by_name[label];a.set_editor_property('button_label',value)
        a.get_editor_property('label').set_text(value);style(a.get_editor_property('label'),15 if label.startswith('Crime_') else 19)
        a.get_editor_property('interactable').set_editor_property('prompt',value.split('\n')[0])
    for a in actors:
        for c in a.get_components_by_class(u.TextRenderComponent):
            style(c)
            if isinstance(a,u.SPGuardCharacter):c.set_text('순찰 중')
        if isinstance(a,u.SPRestrictedArea):a.set_editor_property('area_name','화물 보안 구역')
    # The start notice faces the safe spawn area. Only this decorative sign moves.
    by_name['Welcome'].set_actor_location(u.Vector(-1850,-100,220),False,True)
    by_name['Welcome'].set_actor_rotation(u.Rotator(yaw=180),False)
    by_name['Welcome'].get_component_by_class(u.TextRenderComponent).set_world_size(30)
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    manifest={'signs':SIGNS,'buttons':BUTTONS,'font_characters':chars,'actor_count':len(actors),
        'font':font_path,'material':material_path,'backup':str(backup)}
    (project/'Saved/StealthKorean/authoring.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    u.log('STEALTH_KOREAN_SAVED')

if __name__=='__main__':run()
