"""작성자 : 임진혁
P3 맵을 정확히 10량으로 확장한다. 본인 LFS 잠금 확인 후 UE 5.8.2 에디터에서 실행.
원본 열차/보유 팩은 참조만 하고, P4 자산과 전용 맵만 저장한다.
"""
import json,re
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools
ROOT='/Game/SpacePirate/Prototype01'
MAP='/Game/SpacePirate/Maps/Lvl_SPPrototype01'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
tools=u.AssetToolsHelpers.get_asset_tools()
def load(p):
    a=u.load_asset(p); assert a,p; return a
def save(a):
    u.EditorAssetLibrary.set_metadata_tag(a,'Author','임진혁')
    if isinstance(a,u.Blueprint):
        u.BlueprintEditorLibrary.compile_blueprint(a)
        assert a.get_editor_property('status')==u.BlueprintStatus.BS_UP_TO_DATE
    assert u.EditorAssetLibrary.save_loaded_asset(a,False)
def data(name,cls):
    p=ROOT+'/Data/'+name
    if u.EditorAssetLibrary.does_asset_exist(p): return load(p)
    f=u.DataAssetFactory();f.set_editor_property('data_asset_class',cls)
    return tools.create_asset(name,ROOT+'/Data',u.DataAsset,f)
settings=[]
for name,seconds,players in [('FourPlayer420',420,4),('TwoPlayer600',600,2),('Original600',600,4)]:
    a=data('DA_SP1'+name,u.SP1RunSettings)
    a.set_editor_property('settings_id',name);a.set_editor_property('mission_seconds',seconds);a.set_editor_property('intended_players',players)
    save(a);settings.append(a)
definitions={k:load(ROOT+'/Data/DA_SP1'+k) for k in ['Supply','Parts','Alloy','Core','S01']}
s01data=definitions['S01'];s01data.set_editor_property('display_name','S01 중력 교란기 / 이 객차만 무중력');save(s01data)
budget=[{'Supply':4},{'Parts':2,'Alloy':2},{'Parts':4},{'Core':2,'Parts':2,'S01':1},{},{'Core':2,'Alloy':2},{'Parts':6,'Alloy':4},{'Core':2,'Parts':2,'S01':1},{'Core':2,'Alloy':2},{'Core':2}]
scenario=data('DA_SP1Plan04',u.SP1ScenarioDefinition)
rows=[]
for car,roles in enumerate(budget,1):
    for role,count in roles.items():
        row=u.SP1CargoBudgetRow();row.set_editor_property('car_id','P01_C%02d'%car);row.set_editor_property('definition',definitions[role]);row.set_editor_property('count',count);row.set_editor_property('gravity_cargo',role=='S01');rows.append(row)
scenario.set_editor_property('car_order',['P01_C%02d'%i for i in range(1,11)]);scenario.set_editor_property('cargo_budget',rows);save(scenario)
path=ROOT+'/Blueprints/BP_SP1MaintenanceStation'
if u.EditorAssetLibrary.does_asset_exist(path): station_bp=load(path)
else:
    f=u.BlueprintFactory();f.set_editor_property('parent_class',u.SP1MaintenanceStation)
    station_bp=tools.create_asset('BP_SP1MaintenanceStation',ROOT+'/Blueprints',u.Blueprint,f)
station_bp.set_editor_property('blueprint_description','작성자 : 임진혁\n정비 30초 / 부활 3초. 판정은 기존 GameState의 Round, 표현과 안전 스폰은 이 BP/맵.')
u.BlueprintEditorLibrary.compile_blueprint(station_bp)
terminal=load('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01')
cdo=u.get_default_object(station_bp.generated_class());cdo.terminal.set_static_mesh(terminal)
cdo.terminal.set_relative_scale3d(u.Vector(.4,.4,.4));cdo.terminal.set_relative_rotation(u.Rotator(yaw=90),False,True)
save(station_bp)
gs=load(ROOT+'/Blueprints/BP_SP1GameState');sub=u.get_engine_subsystem(u.SubobjectDataSubsystem);lib=u.SubobjectDataBlueprintFunctionLibrary
for h in sub.k2_gather_subobject_data_for_blueprint(gs):
    obj=lib.get_object_for_blueprint(lib.get_data(h),gs)
    if isinstance(obj,u.SP1RoundComponent):
        for field,value in [('scenario',scenario),('two_player_settings',settings[1]),('four_player_settings',settings[0]),('settings_override',None),('use_settings_assets',True)]:obj.set_editor_property(field,value)
gs.set_editor_property('blueprint_description','작성자 : 임진혁\nPlanetTrain 부모/배경 보존. P4 10량/정비/부활/2개 출발. 2인600/4인420 자동 선택; 원안600은 SettingsOverride.')
save(gs)
hud=load(ROOT+'/UI/WBP_SP1HUD');hud.set_editor_property('blueprint_description','작성자 : 임진혁\nP4 임무/정비 시계, 종료 이유, 부활, 출발 장소, 전송/정산/탑승 결과. Designer 편집.');save(hud)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level(MAP)
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
consist=next(a for a in existing.values() if a.get_class().get_name()=='BP_SPTrainConsist_C')
consist.set_actor_location(u.Vector(0,0,0),False,True)
assert ObjectTools.set_properties(consist,json.dumps({'CarSequence':[{'refPath':'/Game/SpacePirate/Train/Freight/Data/DA_SPCar_Long.DA_SPCar_Long'}]*10,'MaxCars':10}))
consist.set_actor_label('P4_TrainConsist_10Cars_IncludingMaintenanceAndEngine')
centers={i:-1250-(10-i)*2160 for i in range(1,11)}
# P3의 장치와 표식은 기존 Actor를 이동한다. 재실행 시에는 P4 배치 태그로 중복 이동을 막는다.
for label,a in existing.items():
    if a==consist:continue
    tags=[str(t) for t in a.tags]
    if 'P4Layout' in tags:continue
    match=next((re.fullmatch(r'CarId=P01_C(\d+)',t) for t in tags if t.startswith('CarId=')),None)
    if not match:continue
    old=int(match.group(1));new={1:1,2:2,3:3,4:4,5:6,6:10}.get(old)
    if not new:continue
    delta=centers[new]-(-1250-(6-old)*2160)
    a.set_actor_location(a.get_actor_location()+u.Vector(delta,0,0),False,True)
    a.set_editor_property('tags',['Prototype01','P4Layout','CarId=P01_C%02d'%new,'Author=임진혁']);a.set_folder_path('Prototype01/C%02d'%new)
def place(cls,label,pos,car,yaw=0):
    a=existing.get(label)
    if not a:a=actors.spawn_actor_from_class(cls,u.Vector(*pos),u.Rotator(yaw=yaw));existing[label]=a
    a.set_actor_label(label);a.set_actor_location_and_rotation(u.Vector(*pos),u.Rotator(yaw=yaw),False,True)
    a.set_editor_property('tags',['Prototype01','P4Layout','CarId=P01_C%02d'%car,'Author=임진혁']);a.set_folder_path('Prototype01/C%02d'%car)
    return a
def sign(label,text,pos,car,size=20):
    a=place(u.TextRenderActor,label,pos,car,180);c=a.get_component_by_class(u.TextRenderComponent)
    c.set_text(text);c.set_world_size(size);c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);c.set_text_render_color(u.Color(80,230,255,255));return a
cube=load('/Engine/BasicShapes/Cube')
def mesh(label,asset,pos,car,scale=(1,1,1),collision='NoCollision',yaw=0):
    a=place(u.StaticMeshActor,label,pos,car,yaw);c=a.get_component_by_class(u.StaticMeshComponent);c.set_static_mesh(asset);c.set_collision_profile_name(collision);a.set_actor_scale3d(u.Vector(*scale));return a
titles=['입구 / 보급 4개','레이저 위험길 / 왼쪽 걷기 우회','협동 격벽 / 2인 2초 또는 혼자 12초','S01 / 바닥 부품 먼저? 높은 코어 보상','정비 30초 / 부활 또는 중간 탈출','감시 / 엄폐와 보상','화물 10개 / 세 작업 구역','S01 / 반대쪽 높은 선반','레이저 / 오른쪽 걷기 우회','기관실 / 입구 감시 · 최종 탈출 안전']
labels=['ENTRY','LASER / SAFE LEFT','BULKHEAD','S01 / HIGH CORES','MAINTENANCE / MID EXIT','CAMERA / COVER','CARGO / 3 WORK AREAS','S01 / OTHER SHELF','LASER / SAFE RIGHT','ENGINE ROOM / FINAL EXIT']
region_cls=load(ROOT+'/Blueprints/BP_SP1CarRegion').generated_class();regions={}
for i,x in centers.items():
    old={1:1,2:2,3:3,4:4,6:5,10:6}.get(i)
    name='P3_C%02d_Region'%old if old else 'P4_C%02d_Region'%i
    a=place(region_cls,name,(x,0,510),i);a.set_editor_property('car_id','P01_C%02d'%i);a.set_editor_property('hazard_label',titles[i-1]);regions[i]=a
    sign('P3_C%02d_Label'%old if old else 'P4_C%02d_Label'%i,'C%02d / '%i+labels[i-1],(x-650,0,575),i)
starts=sorted([a for a in existing.values() if isinstance(a,u.PlayerStart)],key=lambda a:a.get_actor_label());assert len(starts)==4
for a,(dx,y) in zip(starts,[(-650,-100),(-650,100),(-350,-100),(-350,100)]):a.set_actor_location(u.Vector(centers[1]+dx,y,320),False,True)
existing['P01_C01_Entry'].set_actor_location(u.Vector(centers[1]-880,0,320),False,True)
existing['P01_C01_EntrySign'].set_actor_location(u.Vector(centers[1]-870,0,500),False,True)
# 정비 패널과 부활 위치는 중간 출발 구역과 겹치지 않게 분리한다.
station=place(station_bp.generated_class(),'P4_C05_Revive',(centers[5]-100,-300,285),5)
station.set_editor_property('region',regions[5]);station.set_editor_property('revive_offsets',[u.Vector(x,y,35) for x,y in [(-550,120),(-550,380),(-330,120),(-330,380)]])
sign('P4_C05_ReviveSign','REVIVE / E 3s / W50 S50\nONCE EACH / DROPPED ITEMS STAY',(centers[5]-100,-300,430),5,17)
sign('P4_C05_Rules','30s MISSION CLOCK PAUSE ONLY\nC06 ENTRY CLOSES MAINTENANCE\nAHEAD: 4,830 VALUE',(centers[5]+40,0,555),5,18)
# 조사한 의료 상자 원형은 몸통+뚜껑을 같은 원점에서 조립하고 장식으로만 쓴다.
for part in ['Medical_Box_SM','Medical_Box_Top_SM']:
    mesh('P4_C05_'+part,load('/Game/Assets/Cargo/AE_BR_Props/Models/'+part),(centers[5]+20,-350,212),5,(2,2,2))
exit_cls=load(ROOT+'/Blueprints/BP_SP1ExtractionZone').generated_class()
final=next(a for a in existing.values() if isinstance(a,u.SP1ExtractionZone) and not a.intermediate)
exit_material=existing['P01_C01_ExitBorder_0'].get_component_by_class(u.StaticMeshComponent).get_material(0)
exits=[]
for car,name,a in [(5,'P4_C05_MidExit',None),(10,final.get_actor_label(),final)]:
    x=centers[car]+(350 if car==5 else 400)
    a=place(exit_cls,name,(x,0,350),car);a.zone.set_box_extent(u.Vector(200,200,140),False)
    a.set_editor_property('departure_id','P01_C%02d_%s'%(car,'Mid' if car==5 else 'Final'))
    a.set_editor_property('departure_label','C05 정비 중간 탈출' if car==5 else 'C10 기관실 최종 탈출')
    a.set_editor_property('intermediate',car==5);a.set_editor_property('protect_from_hazards',car==10);a.set_editor_property('safe_approach_centimeters',200)
    # 전방 단말 앞에 네 캡슐이 설 수 있는 4m x 4m 영역. 판정과 바닥 표식을 일치시킨다.
    a.terminal.set_static_mesh(terminal);a.terminal.set_relative_scale3d(u.Vector(.6,.6,.6));a.terminal.set_relative_rotation(u.Rotator(yaw=90),False,True)
    a.terminal.set_relative_location(u.Vector(180,0,-40),False,True);a.set_editor_property('interaction_offset',u.Vector(180,0,0));exits.append(a)
    for j,(dx,y,sx,sy) in enumerate([(-200,0,.04,4),(200,0,.04,4),(0,-200,4,.04),(0,200,4,.04)]):
        m=mesh('P01_C01_ExitBorder_%d'%j if car==10 else 'P4_C05_ExitBorder_%d'%j,cube,(x+dx,y,214),car,(sx,sy,.04))
        m.get_component_by_class(u.StaticMeshComponent).set_material(0,exit_material)
    sign('P01_C01_ExitSign' if car==10 else 'P4_C05_ExitSign',('FINAL / ENGINE ROOM' if car==10 else 'MID EXIT / ACTIVE MAINTENANCE ONLY')+'\nE 1s / DEPARTURE 10s',(x+215,0,525),car,18)
sign('P4_C10_SafeApproach','SAFE / EXIT + 2m APPROACH',(-1250,0,500),10,18)
for y in [-200,200]:
    m=mesh('P4_C10_Approach_%d'%y,cube,(-1150,y,214),10,(2,.04,.04));m.get_component_by_class(u.StaticMeshComponent).set_material(0,exit_material)
# 이미 검증한 카메라/레이저를 추가 종류 없이 재배치한다.
camera_cls=load(ROOT+'/Blueprints/BP_SP1SecurityCamera').generated_class()
for car,name,pos in [(6,'P3_C05_Camera',(centers[6]+550,0,440)),(10,'P4_C10_Camera',(centers[10]-280,-50,440))]:
    a=place(camera_cls,name,pos,car,180);a.set_editor_property('region',regions[car]);a.set_editor_property('hazard_id','P01_C%02d_Camera01'%car)
wall=load('/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Sci_fi_wall_straight_01')
mesh('P4_C10_Cover',wall,(-1840,-180,375),10,collision='BlockAll')
laser=place(load(ROOT+'/Blueprints/BP_SP1LaserHazard').generated_class(),'P4_C09_Laser',(centers[9]+250,-100,330),9)
laser.set_editor_property('hazard_id','P01_C09_Laser01')
sign('P4_C09_SafeSign','SAFE WALK / RIGHT',(centers[9]+50,290,410),9,18)
# 두 중력 객차는 서로 반대쪽 선반을 써도 바닥 통과로가 남는다.
gravity={}
for car,name in [(4,'P3_C04_GravityZone'),(8,'P4_C08_GravityZone')]:
    z=place(u.SPGravityZone,name,(centers[car],0,510),car);z.get_component_by_class(u.BoxComponent).set_box_extent(u.Vector(900,480,300),False);z.set_editor_property('initial_gravity_mode',u.SPGravityMode.GRAVITY);gravity[car]=z
    for j,(dx,y) in enumerate([(350,300),(600,300)] if car==4 else [(-250,-300),(100,-300)]):
        name='P3_C04_HighShelf' if car==4 and j==0 else 'P4_C%02d_Shelf%d'%(car,j)
        mesh(name,cube,(centers[car]+dx,y,630),car,(2.2,2,.16),'BlockAll')
    sign('P3_C04_HighSign' if car==4 else 'P4_C08_HighSign','HIGH CORES +600\nSPACE UP / SHIFT DOWN',(centers[car]+(350 if car==4 else -250),270 if car==4 else -270,755),car,16)
normal_cls=load(ROOT+'/Blueprints/BP_SP1TransferCargo').generated_class();special_cls=load(ROOT+'/Blueprints/BP_SP1GravityCargo').generated_class()
reuse={(1,'Supply',0):'P1_C01_Cargo_Supply',(2,'Parts',0):'P1_C01_Cargo_Parts',(2,'Alloy',0):'P1_C01_Cargo_Alloy',(4,'Core',0):'P3_C04_HighCore',(4,'S01',0):'P3_C04_S01',(6,'Core',0):'P1_C01_Cargo_Core'}
positions={
1:{'Supply':[(-300,-300),(0,300),(350,-300),(650,300)]},
2:{'Parts':[(-400,300),(450,300)],'Alloy':[(-450,-100),(500,-100)]},
3:{'Parts':[(-600,-300),(-400,300),(-100,-300),(170,300)]},
4:{'Core':[(350,300,640),(600,300,640)],'Parts':[(-400,-300),(-400,300)],'S01':[(-100,0)]},
6:{'Core':[(-520,300),(100,-260)],'Alloy':[(-450,0),(570,-300)]},
7:{'Parts':[(-650,-330),(-650,300),(-100,-330),(-100,300),(550,-330),(550,300)],'Alloy':[(-430,-330),(-430,300),(120,-330),(330,300)]},
8:{'Core':[(-250,-300,640),(100,-300,640)],'Parts':[(-500,300),(450,300)],'S01':[(-400,0)]},
9:{'Core':[(400,-300),(650,-300)],'Alloy':[(-500,100),(500,100)]},
10:{'Core':[(-700,-300),(-430,300)]}}
for car,roles in enumerate(budget,1):
    for role,count in roles.items():
        for index,offset in enumerate(positions[car][role]):
            dx,y=offset[:2];z=offset[2] if len(offset)==3 else 212
            name=reuse.get((car,role,index),'P4_C%02d_%s_%02d'%(car,role,index+1))
            a=place(special_cls if role=='S01' else normal_cls,name,(centers[car]+dx,y,z),car)
            a.set_editor_property('definition',definitions[role]);a.set_editor_property('cargo_id','P01_C%02d_%s_%02d'%(car,role,index+1));a.set_editor_property('car_id','P01_C%02d'%car)
            if role=='S01':a.set_editor_property('gravity_zone',gravity[car])
sign('P4_C07_WorkAreas','3 WORK AREAS / 10 CARGO / +1,240',(centers[7]+750,0,555),7)
assert levels.save_current_level()
current=actors.get_all_level_actors();cargo=[a for a in current if isinstance(a,u.SP1TransferCargo)]
assert len(cargo)==42 and sum(a.definition.value for a in cargo)==7060
assert sum(isinstance(a,u.SP1GravityCargo) for a in cargo)==2
report={'map':MAP,'centers':centers,'cargo_count':len(cargo),'total_value':sum(a.definition.value for a in cargo),'actors':[{'label':a.get_actor_label(),'class':a.get_class().get_path_name(),'position':a.get_actor_location().to_tuple()} for a in current]}
Path(u.Paths.project_saved_dir(),'Prototype01/p4-setup.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
u.log('P4_SETUP_COMPLETE')
