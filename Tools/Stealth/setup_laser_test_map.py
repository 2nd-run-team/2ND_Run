"""Stage 5 one-time laser authoring in the UE 5.8.2 editor.

Verify our LFS locks, refuse dirty packages/existing BP, back up the saved map.
Creates editable BP/materials and synthesizes two original PCM signal sounds.
After navigation finishes, save the level again and run validate_laser_map.py.
"""
import json,math,shutil,struct,subprocess,wave
from datetime import datetime
from pathlib import Path
import unreal as u

project=Path(u.Paths.project_dir()).resolve()
bp_root='/Game/SpacePirate/Stealth/Blueprints'
proto='/Game/SpacePirate/Stealth/Prototype'
required=['Content/SpacePirate/Maps/Lvl_SPStealthTest.umap',
          'Content/SpacePirate/Stealth/Blueprints/BP_SPLaserSecurityDevice.uasset']
required += ['Content/SpacePirate/Stealth/Prototype/Materials/M_SPLaser'+n+'.uasset' for n in ['On','Off','Warning','Text']]
required += ['Content/SpacePirate/Stealth/Prototype/Audio/'+n+'.uasset' for n in ['S_SPLaserWarning','S_SPLaserContact']]
required += ['Content/SpacePirate/Stealth/Prototype/Audio/Source/'+n+'.wav' for n in ['S_SPLaserWarning','S_SPLaserContact']]
locks=json.loads(subprocess.check_output(['git','lfs','locks','--verify','--json'],cwd=project,
    creationflags=subprocess.CREATE_NO_WINDOW,text=True,encoding='utf-8'))
assert set(required)<={r['path'] for r in locks.get('ours',[])},'Acquire all listed LFS locks first.'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLevelLibrary.get_pie_worlds(False)
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
assert not u.EditorAssetLibrary.does_asset_exist(bp_root+'/BP_SPLaserSecurityDevice'),'Already authored; inspect saved assets.'
backup=project/'Saved/StealthLaser'/('before-map-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
shutil.copy2(project/required[0],backup/'Lvl_SPStealthTest.umap')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level('/Game/SpacePirate/Maps/Lvl_SPStealthTest')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert not u.GameplayStatics.get_all_actors_of_class(world,u.SPLaserSecurityDevice)
materials={}
for name,color in [('On',(4,.02,.008)),('Off',(.005,.025,.06)),('Warning',(3,1.1,.005))]:
    asset='M_SPLaser'+name
    assert not u.EditorAssetLibrary.does_asset_exist(proto+'/Materials/'+asset)
    mat=u.AssetToolsHelpers.get_asset_tools().create_asset(asset,proto+'/Materials',u.Material,u.MaterialFactoryNew())
    mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
    tint=u.MaterialEditingLibrary.create_material_expression(mat,u.MaterialExpressionVectorParameter,-250,0)
    tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',u.LinearColor(*color,1))
    u.MaterialEditingLibrary.connect_material_property(tint,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    u.MaterialEditingLibrary.recompile_material(mat)
    assert u.EditorAssetLibrary.save_loaded_asset(mat);materials[name]=mat

# Preserve the engine font atlas/mask, but make state colors readable in shadow.
text_mat=u.EditorAssetLibrary.duplicate_asset('/Engine/EngineMaterials/DefaultTextMaterialOpaque',proto+'/Materials/M_SPLaserText')
assert text_mat
text_color=u.MaterialEditingLibrary.get_material_property_input_node(text_mat,u.MaterialProperty.MP_BASE_COLOR)
assert text_color
text_mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
assert u.MaterialEditingLibrary.connect_material_property(text_color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
u.MaterialEditingLibrary.recompile_material(text_mat)
assert u.EditorAssetLibrary.save_loaded_asset(text_mat)

# Original synthesized tones. This recipe is the editable source; no download.
sounds={}
for name,duration,f0,f1 in [('S_SPLaserWarning',.16,700,1000),('S_SPLaserContact',.24,900,420)]:
    path=project/'Content/SpacePirate/Stealth/Prototype/Audio/Source'/(name+'.wav')
    assert not path.exists(),'Do not overwrite manually edited source audio.'
    path.parent.mkdir(parents=True,exist_ok=True)
    rate=22050;frames=[]
    for i in range(int(rate*duration)):
        t=i/rate;envelope=min(t/.015,1,(duration-t)/.03)
        phase=2*math.pi*(f0*t+(f1-f0)*t*t/(2*duration))
        frames.append(struct.pack('<h',int(32767*.12*max(0,envelope)*math.sin(phase))))
    with wave.open(str(path),'wb') as out:
        out.setnchannels(1);out.setsampwidth(2);out.setframerate(rate);out.writeframes(b''.join(frames))
    task=u.AssetImportTask();task.set_editor_property('filename',str(path));task.set_editor_property('destination_path',proto+'/Audio')
    task.set_editor_property('automated',True);task.set_editor_property('save',True);task.set_editor_property('replace_existing',False)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sounds[name]=u.load_asset(proto+'/Audio/'+name);assert sounds[name]

factory=u.BlueprintFactory();factory.set_editor_property('parent_class',u.SPLaserSecurityDevice)
bp=u.AssetToolsHelpers.get_asset_tools().create_asset('BP_SPLaserSecurityDevice',bp_root,u.Blueprint,factory)
defaults=u.get_default_object(bp.generated_class())
for component,mesh,scale in [('emitter_visual','Cube',(.24,.32,.32)),
    ('receiver_visual','Sphere',(.3,.3,.3)),('beam_visual','Cube',(1,1,1))]:
    visual=defaults.get_editor_property(component)
    visual.set_static_mesh(u.load_asset('/Engine/BasicShapes/'+mesh))
    visual.set_relative_scale3d(u.Vector(*scale))
    visual.set_collision_profile_name('NoCollision')
defaults.get_editor_property('state_indicator').set_relative_rotation(u.Rotator(yaw=-90),False,False)
defaults.get_editor_property('state_indicator').set_text_material(text_mat)
for name in ['On','Off','Warning']:defaults.set_editor_property(name.lower()+'_material',materials[name])
defaults.set_editor_property('warning_sound',sounds['S_SPLaserWarning'])
defaults.set_editor_property('contact_sound',sounds['S_SPLaserContact'])
u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp)
folder='StealthTest/80_LaserSecurity'
def spawn(cls,label,loc,rotation=None):
    actor=actors.spawn_actor_from_class(cls,u.Vector(*loc),rotation or u.Rotator())
    actor.set_actor_label(label);actor.set_folder_path(folder);return actor
def shape(label,loc,size,material,collision='NoCollision'):
    actor=spawn(u.StaticMeshActor,label,loc)
    actor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    actor.static_mesh_component.set_material(0,u.load_asset(proto+'/Materials/M_SPStealth'+material))
    actor.static_mesh_component.set_collision_profile_name(collision)
    actor.set_actor_scale3d(u.Vector(*[v/100 for v in size]));return actor
def sign(label,text,loc,size=22):
    actor=spawn(u.TextRenderActor,label,loc,u.Rotator(yaw=-90))
    c=actor.get_component_by_class(u.TextRenderComponent);c.set_text(text);c.set_world_size(size)
    c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    c.set_text_material(text_mat)

for name,x,height,instruction in [('Crouch',-2100,145,'HOLD CTRL / KEEP CROUCHED'),
    ('Jump',0,40,'WALK THEN SPACE / JUMP OVER'),('Wait',2100,95,'BLUE OFF = SAFE / AMBER = CHANGING')]:
    device=spawn(bp.generated_class(),'Laser_'+name,(x-250,1800,0))
    device.set_editor_property('device_id','TestLaser_'+name)
    device.set_editor_property('local_start',u.Vector(0,0,height));device.set_editor_property('local_end',u.Vector(500,0,height))
    device.set_editor_property('detection_thickness',4)
    device.set_editor_property('dispatch_group','StealthTest_Laser');device.set_editor_property('response_radius',5000)
    device.set_editor_property('max_responders',1);device.set_editor_property('call_interval',2)
    if name=='Wait':
        device.set_editor_property('mode',u.SPLaserMode.PERIODIC)
        device.set_editor_property('on_seconds',4);device.set_editor_property('off_seconds',4)
        device.set_editor_property('warning_seconds',1);device.set_editor_property('initial_phase_seconds',0)
    device.refresh_device()
    shape('Laser_'+name+'_SafePad',(x,1450,2),(650,380,4),'Public')
    shape('Laser_'+name+'_ExitPad',(x,2040,2),(650,230,4),'Public')
    shape('Laser_'+name+'_Threshold',(x,1800,3),(500,12,4),'Future')
    sign('Laser_'+name+'_Sign',name.upper()+' / '+str(height)+'cm\n'+instruction,(x,1870,275),21)
    sign('Laser_'+name+'_SafeSign','SAFE WAITING AREA',(x,1320,80),18)
    # Visible endpoint supports are separate replaceable representation, no collision.
    for side,dx in [('Emitter',-250),('Receiver',250)]:
        shape('Laser_'+name+'_'+side+'_Stand',(x+dx,1800,height/2),(20,20,height),'Cover')

guard=spawn(u.load_class(None,bp_root+'/BP_SPGuardCharacter.BP_SPGuardCharacter_C'),'Laser_ResponseGuard',(-800,1450,100),u.Rotator(yaw=90))
guard.set_editor_property('alert_group','StealthTest_Laser');guard.set_editor_property('identity_scope','StageSecurity')
guard.set_editor_property('hear_footsteps',False);guard.set_editor_property('patrol_route',None)
guard.get_editor_property('alert_indicator').set_world_size(24)
assert levels.save_current_level()
u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
u.log('STEALTH_LASER_MAP_SAVED: three authored devices, safe approaches, procedural audio. Save after nav rebuild.')
