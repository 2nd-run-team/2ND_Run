"""작성자 : 임진혁
UE 5.8.2 에디터 API로 P3 전용 자산과 여섯 객차를 배치한다.
P0~P2 스크립트를 재실행하지 않는다. 변경/신규 패키지의 본인 LFS 락을 먼저 확인한다.
원본 팩/열차 BP는 참조만 한다. 이 스크립트의 P3 이름 액터만 갱신한다.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools
ROOT='/Game/SpacePirate/Prototype01'
MAP='/Game/SpacePirate/Maps/Lvl_SPPrototype01'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
tools=u.AssetToolsHelpers.get_asset_tools()
def load(path):
    asset=u.load_asset(path); assert asset,path
    return asset
def bp(name,native,note):
    path=ROOT+'/Blueprints/'+name
    asset=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    parent=u.load_class(None,'/Script/SpacePirate.'+native)
    if not asset:
        factory=u.BlueprintFactory(); factory.set_editor_property('parent_class',parent)
        asset=tools.create_asset(name,ROOT+'/Blueprints',u.Blueprint,factory)
    assert u.BlueprintEditorLibrary.get_blueprint_parent_class(asset)==parent
    asset.set_editor_property('blueprint_description','작성자 : 임진혁\n'+note)
    u.EditorAssetLibrary.set_metadata_tag(asset,'Author','임진혁')
    u.BlueprintEditorLibrary.compile_blueprint(asset)
    return asset
region_bp=bp('BP_SP1CarRegion','SP1CarRegion','객차 내부 범위 / 위험 UI. 연결 통로를 포함하지 않는다.')
panel_bp=bp('BP_SP1Panel','SP1Panel','기존 E 유지 판정과 격벽 연결. 서로 다른 두 플레이어 또는 단독 우회.')
gate_bp=bp('BP_SP1Bulkhead','SP1Bulkhead','2인 2초 / 단독 12초. 서버 점유와 영구 개방. 문짝 외형은 보유 팩 참조.')
s01_bp=bp('BP_SP1GravityCargo','SP1GravityCargo','S01 전송 후 참조된 C04 중력 영역만 변경. 고정 받침 / 링 / Galaxy 코어.')
camera_bp=bp('BP_SP1SecurityCamera','SP1SecurityCamera','서버 시각 회전, 개인별 노출, 1초 예고 후 객차 내 시야 피해. 별도 CCTV 화면 없음.')
terminal=load('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01')
door=load('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Door_half_01')
cube=load('/Engine/BasicShapes/Cube')
panel=u.get_default_object(panel_bp.generated_class())
panel.mesh.set_static_mesh(terminal)
panel.mesh.set_relative_scale3d(u.Vector(.4,.4,.4))
panel.mesh.set_relative_rotation(u.Rotator(yaw=90),False,True)
panel.set_editor_property('interaction_offset',u.Vector(0,0,30))
gate=u.get_default_object(gate_bp.generated_class())
for field,y in [('door_left',-125),('door_right',125)]:
    mesh=gate.get_editor_property(field); mesh.set_static_mesh(door)
    mesh.set_relative_location(u.Vector(0,y,0),False,True)
    mesh.set_relative_rotation(u.Rotator(yaw=90),False,True)
    mesh.set_relative_scale3d(u.Vector(3.52,1,1.91))
cam=u.get_default_object(camera_bp.generated_class())
cam.head.set_static_mesh(load('/Game/Assets/Object/SecurityCameras/Meshes/SM_Security_Camera'))
cam.head.set_relative_scale3d(u.Vector(1.5,1.5,1.5))
cam.set_editor_property('warning_sound',load('/Game/Assets/Sound/InterfaceAndItemSounds/Cues/Futuristic_Alarm_01_wav_Cue'))
cam.set_editor_property('attenuation',load(ROOT+'/Audio/ATT_SP1Cargo'))

# 실제 조사된 파라미터만 사용한다. 원본 Galaxy MI를 저장하지 않는다.
mi_path=ROOT+'/Materials/MI_SP1S01Galaxy'
mi=u.load_asset(mi_path) if u.EditorAssetLibrary.does_asset_exist(mi_path) else tools.create_asset('MI_SP1S01Galaxy',ROOT+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
u.MaterialEditingLibrary.set_material_instance_parent(mi,load('/Game/Assets/VFX/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader'))
for name,value in [('Fresnel Emission Intensity',2),('Stars Emission Intensity',2),('Galaxy Emission Intensity',1.5)]:
    # 5.8.2 구현은 성공해도 false를 반환하므로 읽어서 실제 값을 검증한다.
    u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi,name,value)
    assert u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,name)==value
u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi,'Fresnel Color',u.LinearColor(.05,.7,1,1))
u.EditorAssetLibrary.set_metadata_tag(mi,'Author','임진혁')
assert u.EditorAssetLibrary.save_loaded_asset(mi,False)
data_path=ROOT+'/Data/DA_SP1S01'
data=u.load_asset(data_path) if u.EditorAssetLibrary.does_asset_exist(data_path) else None
if not data:
    factory=u.DataAssetFactory(); factory.set_editor_property('data_asset_class',u.SP1CargoDefinition)
    data=tools.create_asset('DA_SP1S01',ROOT+'/Data',u.DataAsset,factory)
base=load('/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_7')
for field,value in [('display_name','S01 중력 교란기 / C04 무중력'),('value',350),('hold_seconds',3),('mesh',base),('mesh_scale',u.Vector(.75,.75,.75)),('interaction_offset',base.get_bounds().origin*.75)]: data.set_editor_property(field,value)
source=load(ROOT+'/Data/DA_SP1Core')
for field in ['complete_sound','complete_attenuation']: data.set_editor_property(field,source.get_editor_property(field))
u.EditorAssetLibrary.set_metadata_tag(data,'Author','임진혁')
assert u.EditorAssetLibrary.save_loaded_asset(data,False)
special=u.get_default_object(s01_bp.generated_class()); special.set_editor_property('definition',data)
special.core.set_static_mesh(load('/Engine/BasicShapes/Sphere')); special.core.set_material(0,mi)
special.core.set_relative_location(u.Vector(0,0,100),False,True); special.core.set_relative_scale3d(u.Vector(.6,.6,.6))
for segment in special.ring_segments: segment.set_static_mesh(cube)
for asset in [region_bp,panel_bp,gate_bp,s01_bp,camera_bp]:
    assert asset.get_editor_property('status')==u.BlueprintStatus.BS_UP_TO_DATE
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
hud=load(ROOT+'/UI/WBP_SP1HUD'); assert u.SP1EditorLibrary.add_mechanic_hud(hud)
hud.set_editor_property('blueprint_description','작성자 : 임진혁\nP1 라운드, P2 생존, P3 위험/패널/S01/감시 방향. Designer에서 편집.')
u.BlueprintEditorLibrary.compile_blueprint(hud); assert u.EditorAssetLibrary.save_loaded_asset(hud,False)
# 여섯 칸의 선택/귀환 시간을 포함한 시험 임무 시간. 공용 GameState는 그대로 둔다.
gs=load(ROOT+'/Blueprints/BP_SP1GameState')
sub=u.get_engine_subsystem(u.SubobjectDataSubsystem); lib=u.SubobjectDataBlueprintFunctionLibrary
for handle in sub.k2_gather_subobject_data_for_blueprint(gs):
    obj=lib.get_object_for_blueprint(lib.get_data(handle),gs)
    if isinstance(obj,u.SP1RoundComponent): obj.set_editor_property('mission_seconds',420)
u.BlueprintEditorLibrary.compile_blueprint(gs); assert u.EditorAssetLibrary.save_loaded_asset(gs,False)

levels=u.get_editor_subsystem(u.LevelEditorSubsystem); actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(MAP)
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
consist=next(a for a in existing.values() if a.get_class().get_name()=='BP_SPTrainConsist_C')
assert ObjectTools.set_properties(consist,json.dumps({'CarSequence':[{'refPath':'/Game/SpacePirate/Train/Freight/Data/DA_SPCar_Long.DA_SPCar_Long'}]*6,'MaxCars':6}))
consist.set_actor_label('P3_TrainConsist_6Cars')
centers=[-12050,-9890,-7730,-5570,-3410,-1250]
def place(cls,label,pos,yaw=0,car=1):
    actor=existing.get(label)
    if not actor: actor=actors.spawn_actor_from_class(cls,u.Vector(*pos),u.Rotator(yaw=yaw))
    actor.set_actor_label(label); actor.set_actor_location_and_rotation(u.Vector(*pos),u.Rotator(yaw=yaw),False,True)
    actor.set_editor_property('tags',['Prototype01','P3','CarId=P01_C%02d'%car,'Author=임진혁'])
    actor.set_folder_path('Prototype01/C%02d/P3'%car)
    return actor
def sign(label,text,pos,car,size=22):
    a=place(u.TextRenderActor,label,pos,180,car); c=a.get_component_by_class(u.TextRenderComponent)
    c.set_text(text); c.set_world_size(size); c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER); c.set_text_render_color(u.Color(80,230,255,255))
    return a
regions=[]
titles=['입구 / 안전','장애물 / 레이저 또는 걷기 우회','협동 격벽 / 2인 2초 또는 혼자 12초','S01 / 전송 후 객차만 무중력','감시 / 노출 1.5초, 엄폐로 회피','탈출 / 안전 구역']
for i,x in enumerate(centers,1):
    region=place(region_bp.generated_class(),'P3_C%02d_Region'%i,(x,0,510),car=i)
    region.set_editor_property('car_id','P01_C%02d'%i); region.set_editor_property('hazard_label',titles[i-1]); regions.append(region)
    sign('P3_C%02d_Label'%i,'C%02d / '%i+['ENTRY','LASER / SAFE WALK LEFT','BULKHEAD','S01 / ZERO-G BONUS','CAMERA / USE COVER','SAFE EXTRACTION'][i-1],(x-650,0,575),i)
starts=sorted([a for a in existing.values() if isinstance(a,u.PlayerStart)],key=lambda a:a.get_actor_label())
for a,pos in zip(starts,[(-12700,-90,320),(-12700,90,320),(-12400,-90,320),(-12400,90,320)]): a.set_actor_location(u.Vector(*pos),False,True)
existing['P01_C01_Entry'].set_actor_location(u.Vector(-12930,0,320),False,True)
existing['P01_C01_EntrySign'].set_actor_location(u.Vector(-12920,0,500),False,True)
existing['P01_C01_ExitSign'].get_component_by_class(u.TextRenderComponent).set_text('EXIT / C06\nHOLD E 1s / DEPARTURE 10s')
for label,a in existing.items():
    if label.startswith('P01_C01_Exit') or isinstance(a,u.SP1ExtractionZone):
        a.set_folder_path('Prototype01/C06')
        a.set_editor_property('tags',['Prototype01','CarId=P01_C06','Author=임진혁'])
# P2 장치와 표식을 좌표 델타로 한 번만 이동한다. 재실행 시에는 P3 태그로 중복 이동을 막는다.
for label,a in existing.items():
    if label.startswith('P2_C01_') and 'MovedToC02' not in [str(t) for t in a.tags]:
        a.set_actor_location(a.get_actor_location()+u.Vector(-8640,0,0),False,True)
        a.set_editor_property('tags',['Prototype01','P3','CarId=P01_C02','MovedToC02','Author=임진혁'])
        a.set_folder_path('Prototype01/C02')
    if isinstance(a,u.SP1LaserHazard): a.set_editor_property('hazard_id','P01_C02_Laser01')
gate_actor=place(gate_bp.generated_class(),'P3_C03_Bulkhead',(-6810,0,420),car=3)
for kind,y,dx in [(u.SP1PanelKind.LEFT,-175,-180),(u.SP1PanelKind.RIGHT,175,-180),(u.SP1PanelKind.BYPASS,-350,-420)]:
    name=str(kind).split('.')[1].split(':')[0]
    a=place(panel_bp.generated_class(),'P3_C03_Panel_'+name,(-6810+dx,y,285),car=3)
    a.set_editor_property('bulkhead',gate_actor); a.set_editor_property('kind',kind); a.set_editor_property('panel_id','P01_C03_'+name)
    sign('P3_C03_PanelSign_'+name,('SOLO / HOLD E 12s' if kind==u.SP1PanelKind.BYPASS else name+' / TOGETHER 2s'),(-6810+dx,y,405),3,16)
zone=place(u.SPGravityZone,'P3_C04_GravityZone',(-5570,0,510),car=4)
zone.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(900,480,300),False)
zone.set_editor_property('initial_gravity_mode',u.SPGravityMode.GRAVITY)
s01=place(s01_bp.generated_class(),'P3_C04_S01',(-5670,0,212),car=4)
s01.set_editor_property('cargo_id','P01_C04_S01'); s01.set_editor_property('car_id','P01_C04'); s01.set_editor_property('definition',data); s01.set_editor_property('gravity_zone',zone)
sign('P3_C04_S01Sign','S01 / +350 / HOLD E 3s\nTHIS CAR BECOMES ZERO-G\nFLOOR FIRST?  HIGH SHELF BONUS',(-5510,0,450),4,18)
camera=place(camera_bp.generated_class(),'P3_C05_Camera',(-2860,0,440),180,5)
camera.set_editor_property('region',regions[4])
# 큰 실내 벽 원형은 실제 경계와 맞아 엄폐로 재사용한다. 둘레를 걸어서 통과할 수 있다.
wall=load('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Sci_fi_wall_straight_01')
for i,(x,y) in enumerate([(-3720,-190),(-3170,235)]):
    a=place(u.StaticMeshActor,'P3_C05_Cover%d'%i,(x,y,375),car=5)
    m=a.get_component_by_class(u.StaticMeshComponent); m.set_static_mesh(wall); m.set_collision_profile_name('BlockAll')
    # 팩 메시의 단순 충돌 유무와 무관하게 프로젝트 인스턴스의 고정 큐브를 판정으로 사용한다.
    b=place(u.StaticMeshActor,'P3_C05_CoverCollision%d'%i,(x,y,375),car=5)
    bm=b.get_component_by_class(u.StaticMeshComponent); bm.set_static_mesh(cube); bm.set_visibility(False); bm.set_collision_profile_name('BlockAll')
    b.set_actor_scale3d(u.Vector(.30,3.31,3.28))
def cargo(label,role,pos,car,cargo_id):
    cls=load(ROOT+'/Blueprints/BP_SP1TransferCargo').generated_class()
    a=place(cls,label,pos,car=car); a.set_editor_property('definition',load(ROOT+'/Data/DA_SP1'+role)); a.set_editor_property('cargo_id',cargo_id); a.set_editor_property('car_id','P01_C%02d'%car)
    return a
for label,role,pos,car in [('P1_C01_Cargo_Supply','Supply',(-11750,-300,212),1),('P1_C01_Cargo_Parts','Parts',(-9590,300,212),2),('P1_C01_Cargo_Alloy','Alloy',(-5970,-300,212),4),('P1_C01_Cargo_Core','Core',(-3310,-260,212),5)]: cargo(label,role,pos,car,'P01_C%02d_%s'%(car,role))
cargo('P3_C04_HighCore','Core',(-5220,300,640),4,'P01_C04_HighCore')
# 선반 폭/높이에 맞는 보유 원형이 없어 얇은 고정 발판만 제작한다.
shelf=place(u.StaticMeshActor,'P3_C04_HighShelf',(-5220,300,630),car=4)
sm=shelf.get_component_by_class(u.StaticMeshComponent); sm.set_static_mesh(cube); sm.set_collision_profile_name('BlockAll'); shelf.set_actor_scale3d(u.Vector(2.2,2.0,.16))
sign('P3_C04_HighSign','HIGH SHELF +300\nSPACE UP / SHIFT DOWN',(-5220,270,755),4,16)
assert levels.save_current_level()
report={'map':MAP,'centers':centers,'s01':s01.get_path_name(),'gate':gate_actor.get_path_name(),'camera':camera.get_path_name(),'actors':[{'label':a.get_actor_label(),'class':a.get_class().get_path_name(),'position':a.get_actor_location().to_tuple()} for a in actors.get_all_level_actors()]}
Path(u.Paths.project_saved_dir(),'Prototype01/p3-setup.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
u.log('P3_SETUP_COMPLETE')
