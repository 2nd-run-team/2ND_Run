"""작성자 : 임진혁
P5 필수 UI 아이콘/보유 음원 연결. 기존 전용 WBP와 데이터만 본인 LFS 락 확인 후 저장한다.
make_p5_icons.py를 먼저 실행한다. 원본 메시/재질/배경과 P4 배치를 저장하지 않는다.
"""
import unreal as u,json,hashlib
from pathlib import Path
ROOT='/Game/SpacePirate/Prototype01';PROJECT=Path(u.Paths.project_dir()).resolve();OUT=PROJECT/'Saved/Prototype01'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
tools=u.AssetToolsHelpers.get_asset_tools()
def load(path):
 a=u.load_asset(path);assert a,path;return a
def save(a):
 u.EditorAssetLibrary.set_metadata_tag(a,'Author','임진혁')
 if isinstance(a,u.Blueprint):
  u.BlueprintEditorLibrary.compile_blueprint(a);assert a.get_editor_property('status')==u.BlueprintStatus.BS_UP_TO_DATE,a.get_path_name()
 assert u.EditorAssetLibrary.save_loaded_asset(a,False)
icons={}
for name in ['Supply','Parts','Alloy','Core','S01','PingLocation','PingCargo','PingDanger']:
 path=ROOT+'/UI/Icons/T_SP1'+name
 if not u.EditorAssetLibrary.does_asset_exist(path):
  task=u.AssetImportTask();task.set_editor_property('filename',str(OUT/'P5Import'/('T_SP1'+name+'.png')));task.set_editor_property('destination_path',ROOT+'/UI/Icons');task.set_editor_property('automated',True);task.set_editor_property('save',False)
  tools.import_asset_tasks([task]);assert task.imported_object_paths,path
 a=load(path);a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON);a.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI);a.set_editor_property('never_stream',True);save(a);icons[name]=a
colors={'Supply':(.35,.95,.75,1),'Parts':(.4,.8,1,1),'Alloy':(1,.8,.4,1),'Core':(.8,.6,1,1),'S01':(1,.55,.25,1)}
labels={'Supply':'보급','Parts':'부품','Alloy':'합금','Core':'코어','S01':'S01'}
sound_root='/Game/Assets/Sound/InterfaceAndItemSounds/Cues/'
paths={'AcquireStart':sound_root+'Futuristic_Click_01_wav_Cue','Cancel':sound_root+'Back_Click_01_wav_Cue','Success':sound_root+'Special_Musical_01_wav_Cue','Failure':sound_root+'Error_Buzz_01_wav_Cue','S01Warning':sound_root+'Special_Powerup_02_wav_Cue','Time60':'/Game/Assets/Sound/SciFiUISFX/Cues/Rings/Ring_Pitched_Down_Cue','Time30':'/Game/Assets/Sound/SciFiUISFX/Cues/Tone1/Tritone/Tone1A_TritoneUp_Cue','Departure':sound_root+'Special_Powerup_01_wav_Cue','Countdown':'/Game/Assets/Sound/SciFiUISFX/Cues/Clicks/High_Click_1_Cue','Transfer':sound_root+'Item_Sell_Purchase_01_wav_Cue','GravityComplete':sound_root+'Special_Powerup_03_wav_Cue','CameraWarning':sound_root+'Futuristic_Alarm_01_wav_Cue'}
sounds={key:load(path) for key,path in paths.items()}
report={'icons':{},'definitions':{},'audio':{},'engine':u.SystemLibrary.get_engine_version()}
reg=u.AssetRegistryHelpers.get_asset_registry();options=u.AssetRegistryDependencyOptions(include_hard_package_references=True,include_soft_package_references=True)
for key,sound in sounds.items():
 nodes=[n for n in u.ObjectIterator(u.SoundNode) if n.get_outer()==sound];waves=[]
 for dep in reg.get_dependencies(paths[key],options):
  if not str(dep).startswith('/Game/'):continue
  a=u.load_asset(str(dep))
  if isinstance(a,u.SoundWave):waves.append(a)
 report['audio'][key]={'path':paths[key],'duration':sound.get_editor_property('duration'),'nodes':[n.get_class().get_name() for n in nodes],'waves':[{'path':w.get_path_name(),'duration':w.duration,'channels':w.get_editor_property('NumChannels')} for w in waves],'listening_evaluation':'not performed by audio-aware observer'}
 assert not any(isinstance(n,u.SoundNodeLooping) or (isinstance(n,u.SoundNodeWavePlayer) and n.get_editor_property('looping')) for n in nodes),paths[key]
for name in labels:
 a=load(ROOT+'/Data/DA_SP1'+name);a.set_editor_property('icon',icons[name]);a.set_editor_property('badge_label',labels[name]);a.set_editor_property('badge_color',u.LinearColor(*colors[name]));a.set_editor_property('complete_sound',sounds['GravityComplete' if name=='S01' else 'Transfer']);save(a)
 report['definitions'][name]={'mesh':a.mesh.get_path_name(),'icon':a.icon.get_path_name(),'value':a.value,'hold_seconds':a.hold_seconds}
hud=load(ROOT+'/UI/WBP_SP1HUD');assert u.SP1EditorLibrary.build_evaluation_hud(hud)
u.BlueprintEditorLibrary.compile_blueprint(hud);cdo=u.get_default_object(hud.generated_class());cdo.set_editor_property('cues',{u.Name(k):v for k,v in sounds.items() if k not in ['Transfer','GravityComplete','CameraWarning']})
for field,name in [('location_ping_icon','PingLocation'),('cargo_ping_icon','PingCargo'),('danger_ping_icon','PingDanger')]:cdo.set_editor_property(field,icons[name])
hud.set_editor_property('blueprint_description','작성자 : 임진혁\nP5 평가 UI. 임무/팀/장치/대상/결과, 고정 위치 핑, 색+아이콘+문구와 사건별 음원. Designer에서 배치 편집.');save(hud)
gs=load(ROOT+'/Blueprints/BP_SP1GameState');sub=u.get_engine_subsystem(u.SubobjectDataSubsystem);lib=u.SubobjectDataBlueprintFunctionLibrary
source_hash=hashlib.sha256(b''.join(p.read_bytes() for p in sorted((PROJECT/'Source/SpacePirate/Prototype01').rglob('*')) if p.suffix in ['.cpp','.h'])).hexdigest()[:12]
for h in sub.k2_gather_subobject_data_for_blueprint(gs):
 obj=lib.get_object_for_blueprint(lib.get_data(h),gs)
 if isinstance(obj,u.SP1RoundComponent):
  obj.set_editor_property('build_identifier','f2a6235+P5-'+source_hash);obj.set_editor_property('settings_version','Plan01-P05.1');obj.set_editor_property('layout_variant','A')
save(gs);report['build_id']='f2a6235+P5-'+source_hash
report['icons']={k:v.get_path_name() for k,v in icons.items()}
(OUT/'p5-setup.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf8')
u.log('P5_SETUP_COMPLETE')


