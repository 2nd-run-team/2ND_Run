"""작성자 : 임진혁
UE 5.8.2 지원 에디터 API로 P2 전용 생존·HUD·레이저와 걷기 우회길을 연결한다.
기존 전용 맵/Pawn/HUD 및 신규 레이저 BP의 LFS 락을 먼저 확인하고 실행한다.
P1 생성 스크립트를 재실행하지 않으며 공용 부모와 원본 팩은 저장하지 않는다.
"""
import json
from pathlib import Path
import unreal as u

ROOT='/Game/SpacePirate/Prototype01'
MAP='/Game/SpacePirate/Maps/Lvl_SPPrototype01'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
tools=u.AssetToolsHelpers.get_asset_tools()
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem)
lib=u.SubobjectDataBlueprintFunctionLibrary
pawn=u.load_asset(ROOT+'/Blueprints/BP_SP1PlayerCharacter')
handles=sub.k2_gather_subobject_data_for_blueprint(pawn)
life_cls=u.load_class(None,'/Script/SpacePirate.SP1SurvivalComponent')
life=next((lib.get_object_for_blueprint(lib.get_data(h),pawn) for h in handles if lib.get_object_for_blueprint(lib.get_data(h),pawn) and lib.get_object_for_blueprint(lib.get_data(h),pawn).get_class()==life_cls),None)
if not life:
    handle,fail=sub.add_new_subobject(u.AddNewSubobjectParams(parent_handle=handles[0],new_class=life_cls,blueprint_context=pawn))
    life=lib.get_object_for_blueprint(lib.get_data(handle),pawn)
assert life
for name,value in [('sprint_drain',20),('recovery_delay',.75),('recovery_rate',25)]: life.set_editor_property(name,value)
pawn.set_editor_property('blueprint_description','작성자 : 임진혁\n팀 Pawn 상속. P1 E 홀드 + P2 생존/이동 예측. 전용 걷기 450, 공용 부모는 400 유지.')
u.BlueprintEditorLibrary.compile_blueprint(pawn)
cdo=u.get_default_object(pawn.generated_class())
movement=cdo.get_editor_property('character_movement')
assert 'BP_SP1PlayerCharacter' in movement.get_path_name()
movement.set_editor_property('max_walk_speed',450)
movement.set_editor_property('sprint_speed',700)
assert u.EditorAssetLibrary.save_loaded_asset(pawn,False)
hud=u.load_asset(ROOT+'/UI/WBP_SP1HUD')
assert u.SP1EditorLibrary.add_survival_hud(hud)
hud.set_editor_property('blueprint_description','작성자 : 임진혁\nP1 상태/진행/결과 + P2 W/S/M 복합 게이지. Designer에서 배치 편집, 연결 이름 유지.')
u.BlueprintEditorLibrary.compile_blueprint(hud)
assert u.EditorAssetLibrary.save_loaded_asset(hud,False)

path=ROOT+'/Blueprints/BP_SP1LaserHazard'
bp=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
native=u.load_class(None,'/Script/SpacePirate.SP1LaserHazard')
if not bp:
    factory=u.BlueprintFactory(); factory.set_editor_property('parent_class',native)
    bp=tools.create_asset('BP_SP1LaserHazard',ROOT+'/Blueprints',u.Blueprint,factory)
assert u.BlueprintEditorLibrary.get_blueprint_parent_class(bp)==native
bp.set_editor_property('blueprint_description','작성자 : 임진혁\n서버 2초 OFF(마지막 0.5초 예고)/2초 ON. 상처 20, 같은 위험 1초 간격. 빔은 보유 VFX, 피해는 DamageBounds 전체 높이.')
u.EditorAssetLibrary.set_metadata_tag(bp,'Author','임진혁')
u.BlueprintEditorLibrary.compile_blueprint(bp)
default=u.get_default_object(bp.generated_class())
beam=u.load_asset('/Game/Assets/VFX/FreeStylizedLaserBeamVFX/VFX/NS_Laser_01')
parameters={str(p.parameter_name):str(p.type_name) for p in u.NiagaraFunctionLibrary.get_all_user_parameters(beam)}
assert parameters=={'Color':'LinearColor','LaserEnd':'Vector3f'},parameters
default.set_editor_property('beam_system',beam)
for field,value in [('on_seconds',2),('off_seconds',2),('warning_seconds',.5),('wound_amount',20),('damage_interval',1)]: default.set_editor_property(field,value)
cue=u.load_asset('/Game/Assets/Sound/InterfaceAndItemSounds/Cues/Futuristic_Alarm_01_wav_Cue')
default.set_editor_property('warning_sound',cue)
default.set_editor_property('attenuation',u.load_asset(ROOT+'/Audio/ATT_SP1Cargo'))
cannon=u.load_asset('/Game/Assets/VFX/FreeStylizedLaserBeamVFX/Meshes/LaserCannon')
for field,y,yaw in [('emitter',-229,90),('receiver',229,-90)]:
    mesh=default.get_editor_property(field); mesh.set_static_mesh(cannon)
    mesh.set_relative_location(u.Vector(0,y,0),False,True)
    mesh.set_relative_rotation(u.Rotator(pitch=0,yaw=yaw,roll=0),False,True)
    mesh.set_relative_scale3d(u.Vector(.1,.1,.1))
assert u.EditorAssetLibrary.save_loaded_asset(bp,False)

levels=u.get_editor_subsystem(u.LevelEditorSubsystem); actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(MAP)
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
def place(cls,label,position,rotation=u.Rotator()):
    actor=existing.get(label)
    if actor: assert actor.get_class()==(cls.static_class() if isinstance(cls,type) else cls)
    else: actor=actors.spawn_actor_from_class(cls,u.Vector(*position),rotation)
    actor.set_actor_label(label); actor.set_actor_location_and_rotation(u.Vector(*position),rotation,False,True)
    actor.set_editor_property('tags',['Prototype01','P2','CarId=P01_C01','Author=임진혁'])
    actor.set_folder_path('Prototype01/C01/P2')
    return actor
laser=place(bp.generated_class(),'P2_C01_Laser01',(-950,100,330))
laser.set_editor_property('hazard_id','P01_C01_Laser01')
# 이전 생성 실행에서 맵에 저장된 None 오버라이드를 전용 BP 기본값과 일치시킨다.
for field in ['beam_system','warning_sound','attenuation','on_seconds','off_seconds','warning_seconds','wound_amount','damage_interval']:
    laser.set_editor_property(field,default.get_editor_property(field))
for field in ['emitter','receiver']:
    source=default.get_editor_property(field); target=laser.get_editor_property(field)
    target.set_static_mesh(source.get_editor_property('static_mesh'))
    target.set_relative_transform(source.get_relative_transform(),False,True)
def sign(label,text,pos,color,size=22):
    actor=place(u.TextRenderActor,label,pos,u.Rotator(pitch=0,yaw=180,roll=0))
    comp=actor.get_component_by_class(u.TextRenderComponent)
    comp.set_text(text); comp.set_world_size(size); comp.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER); comp.set_text_render_color(color)
sign('P2_C01_RiskSign','SHORT ROUTE\nLASER / 2s ON - 2s OFF\nWARNING 0.5s / W+20',(-1090,110,475),u.Color(255,150,35,255),18)
sign('P2_C01_SafeSign','SAFE WALK\nBYPASS THIS SIDE',(-1090,-270,390),u.Color(40,230,220,255),18)
cube=u.load_asset('/Engine/BasicShapes/Cube')
for i,(xyz,scale) in enumerate([((-950,-330,214),(3.5,.03,.025)),((-950,-220,214),(3.5,.03,.025)),((-1125,-275,214),(.03,1.1,.025)),((-775,-275,214),(.03,1.1,.025))]):
    actor=place(u.StaticMeshActor,'P2_C01_SafeBorder_'+str(i),xyz)
    mesh=actor.get_component_by_class(u.StaticMeshComponent); mesh.set_static_mesh(cube); mesh.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(u.Vector(*scale))
assert levels.save_current_level()
parent=u.get_default_object(u.load_asset('/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter').generated_class())
assert parent.get_editor_property('character_movement').get_editor_property('max_walk_speed')==400
report={'map':MAP,'pawn_walk':movement.get_editor_property('max_walk_speed'),'parent_walk':400,'laser':laser.get_path_name(),'beam_user_parameters':parameters,'warning_sound_duration':cue.get_editor_property('duration'),'routes':{'risk':[[-1150,100,308],[-800,100,308]],'safe':[[-1150,-160,308],[-1100,-275,308],[-775,-275,308],[-650,0,308]]},'edited_assets':[pawn.get_path_name(),hud.get_path_name(),bp.get_path_name()]}
Path(u.Paths.project_saved_dir(),'Prototype01/p2-setup.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('P2_SETUP_COMPLETE')


