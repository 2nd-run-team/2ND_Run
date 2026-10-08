"""Add stage-3 scene markers and two E terminals to the saved stage-2 map, once.
Requires our existing map LFS lock; no new binary assets. Backs up before saving.
"""
import json
import shutil
import subprocess
from pathlib import Path
from datetime import datetime
import unreal as u

project=Path(u.Paths.project_dir()).resolve()
asset='Content/SpacePirate/Maps/Lvl_SPStealthTest.umap'
locks=json.loads(subprocess.check_output(['git','lfs','locks','--verify','--json'],cwd=project,
    creationflags=subprocess.CREATE_NO_WINDOW,text=True,encoding='utf-8'))
assert asset in {x['path'] for x in locks.get('ours',[])}
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLevelLibrary.get_pie_worlds(False)
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name()=='/Game/SpacePirate/Maps/Lvl_SPStealthTest.Lvl_SPStealthTest'
a=u.get_editor_subsystem(u.EditorActorSubsystem)
assert not any(x.get_actor_label()=='Investigation_Point_Public' for x in a.get_all_level_actors())
backup=project/'Saved/StealthInvestigation'/('before-map-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
shutil.copy2(project/asset,backup/'Lvl_SPStealthTest.umap')
folder='StealthTest/45_InvestigationTests'
material=u.load_asset('/Game/SpacePirate/Stealth/Prototype/Materials/M_SPStealthPublic')
warning=u.load_asset('/Game/SpacePirate/Stealth/Prototype/Materials/M_SPStealthRestricted')
console=u.load_class(None,'/Game/SpacePirate/Stealth/Blueprints/BP_SPStealthTestConsole.BP_SPStealthTestConsole_C')
for suffix,pos,button_x,title,mat in [
    ('Public',u.Vector(-650,600,0),-2400,'PUBLIC SCENE / 1 GUARD',material),
    ('Unreachable',u.Vector(-650,600,900),-1800,'UNREACHABLE / 1 GUARD',warning)]:
    point=a.spawn_actor_from_class(u.TargetPoint,pos)
    point.set_actor_label('Investigation_Point_'+suffix);point.set_folder_path(folder)
    # Visuals are deliberately collision-free and are not used to determine arrival.
    visual=a.spawn_actor_from_class(u.StaticMeshActor,pos+u.Vector(0,0,4))
    visual.set_actor_label('Investigation_Visual_'+suffix);visual.set_folder_path(folder)
    c=visual.static_mesh_component
    c.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cylinder'))
    visual.set_actor_scale3d(u.Vector(3,3,.06))
    c.set_collision_profile_name('NoCollision');c.set_material(0,mat)
    sign=a.spawn_actor_from_class(u.TextRenderActor,pos+u.Vector(0,0,120),u.Rotator(yaw=-90))
    sign.set_actor_label('Investigation_Sign_'+suffix);sign.set_folder_path(folder)
    text=sign.get_component_by_class(u.TextRenderComponent)
    text.set_text(title);text.set_world_size(24);text.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    b=a.spawn_actor_from_class(console,u.Vector(button_x,2900,120),u.Rotator(yaw=-90))
    b.set_actor_label('Investigation_'+suffix);b.set_folder_path(folder)
    b.set_editor_property('button_label',title+'\nE: Work noise')
    b.get_editor_property('label').set_text(title+'\nE: Work noise')
    b.set_editor_property('incident_kind',u.SPStealthIncident.WORK_NOISE)
    b.set_editor_property('incident_location',pos)
    b.set_editor_property('response_radius',5000)
    b.set_editor_property('max_responders',1)
    b.get_editor_property('interactable').set_editor_property('prompt',title)
    v=b.get_editor_property('visual');v.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    v.set_relative_scale3d(u.Vector(.7,.9,.9));v.set_material(0,mat)
for guard in u.GameplayStatics.get_all_actors_of_class(world,u.SPGuardCharacter):
    guard.get_editor_property('alert_indicator').set_world_size(24)
    guard.set_editor_property('show_state_indicator',True)
assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
u.log('STEALTH_INVESTIGATION_MAP_SAVED: public scene, unreachable elevated scene, 2 E buttons')
