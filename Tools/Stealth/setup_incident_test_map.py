"""Add stage-2 incident terminals to the saved test map, once. Requires our LFS locks.

Run in UE 5.8.2 after rebuilding C++. Refuses dirty packages, another map, PIE, or
an existing director. Existing map actors/assets are preserved; only reserved
bay signs are updated. Runtime game logic never imports this script.
"""
import json
import shutil
import subprocess
from datetime import datetime
from pathlib import Path
import unreal as u

project = Path(u.Paths.project_dir()).resolve()
root = '/Game/SpacePirate/Stealth/Blueprints'
required = ['Content/SpacePirate/Maps/Lvl_SPStealthTest.umap'] + [
    f'Content/SpacePirate/Stealth/Blueprints/{name}.uasset'
    for name in ['BP_SPStealthTestConsole', 'BP_SPStealthTestDirector']]
locks = json.loads(subprocess.check_output(['git', 'lfs', 'locks', '--verify', '--json'],
    cwd=project, creationflags=subprocess.CREATE_NO_WINDOW, text=True, encoding='utf-8'))
assert set(required) <= {r['path'] for r in locks.get('ours', [])}, 'Acquire LFS locks first.'
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE.'
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save existing work first.'
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Save existing work first.'
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name() == '/Game/SpacePirate/Maps/Lvl_SPStealthTest.Lvl_SPStealthTest'
assert not u.GameplayStatics.get_all_actors_of_class(world, u.SPStealthTestDirector), 'Already configured; edit existing actors.'
backup = project / 'Saved/StealthEvents' / ('before-map-' + datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
shutil.copy2(project / required[0], backup / 'Lvl_SPStealthTest.umap')

def blueprint(name, parent):
    path = root + '/' + name
    assert not u.EditorAssetLibrary.does_asset_exist(path), 'New assets expected: ' + path
    factory = u.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    bp = u.AssetToolsHelpers.get_asset_tools().create_asset(name, root, u.Blueprint, factory)
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    assert u.EditorAssetLibrary.save_loaded_asset(bp)
    return bp.generated_class()

console = blueprint('BP_SPStealthTestConsole', u.SPStealthTestConsole)
director = blueprint('BP_SPStealthTestDirector', u.SPStealthTestDirector)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
d = actors.spawn_actor_from_class(director, u.Vector(-2400,1500,0))
d.set_actor_label('Stealth_IncidentTestDirector')
d.set_folder_path('StealthTest/00_Infrastructure')

kind = u.SPStealthIncident
cmd = u.SPStealthTestCommand
rows = [
    ('Laser', -2550,2250,40,kind.LASER_CONTACT,0,'LASER\nAnonymous search'),
    ('WorkNoise', -2100,2250,40,kind.WORK_NOISE,0,'VAULT / DRILL\nWork noise'),
    ('Indirect', -1650,2250,40,kind.INDIRECT_REPORT,0,'NPC / OPEN VAULT\nIndirect report'),
    ('Sustained', -600,2250,50,kind.SUSTAINED_CRIME_CONFIRMED,1,'DIRECT CONTINUOUS\nHold E: 1 sec'),
    ('Instant', 0,2250,50,kind.INSTANT_CRIME_WITNESSED,0,'DIRECT INSTANT\nIdentify self'),
    ('DirectReport', 600,2250,50,kind.DIRECT_REPORT_COMPLETED,2,'DIRECT REPORT\nHold E: 2 sec'),
    ('Victim', -600,2900,50,kind.VICTIM_REPORT,0,'AWAKENED VICTIM\nAlarm only'),
    ('Rediscover', 0,2900,50,kind.IDENTIFIED_PLAYER_REDISCOVERED,0,'REDISCOVER\nKnown identity only'),
    ('Escape', 600,2900,50,kind.ESCAPE_ACTIVATED,0,'ESCAPE ACTIVATED\nAlarm only'),
]
for name,x,y,bay,k,hold,label in rows:
    a = actors.spawn_actor_from_class(console,u.Vector(x,y,120),u.Rotator(yaw=-90))
    a.set_actor_label('Incident_'+name)
    a.set_folder_path(f'StealthTest/{bay}_IncidentTests')
    a.set_editor_property('incident_kind',k)
    a.set_editor_property('button_label',label)
    a.get_editor_property('label').set_text(label)
    interact = a.get_editor_property('interactable')
    interact.set_editor_property('hold_duration',hold)
    interact.set_editor_property('prompt',label.split('\n')[0])

for name,x,y,c in [('Start',1650,2250,cmd.START_STAGE),('End',2100,2250,cmd.END_STAGE),
                    ('Restart',2550,2250,cmd.RESTART_STAGE),('Replay',2100,2900,cmd.REPLAY_LAST_INCIDENT)]:
    a = actors.spawn_actor_from_class(console,u.Vector(x,y,120),u.Rotator(yaw=-90))
    a.set_actor_label('Stage_'+name)
    a.set_folder_path('StealthTest/60_StageTests')
    a.set_editor_property('command',c)
    label = name.upper() + ('\nSame event ID' if name=='Replay' else '\nStage lifecycle')
    a.set_editor_property('button_label',label)
    a.get_editor_property('label').set_text(label)
    a.get_editor_property('interactable').set_editor_property('prompt',label.split('\n')[0])

for a in u.GameplayStatics.get_all_actors_of_class(world,u.SPStealthTestConsole):
    visual = a.get_editor_property('visual')
    visual.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    visual.set_relative_scale3d(u.Vector(.7,.9,.9))
    visual.set_material(0,u.load_asset('/Game/SpacePirate/Stealth/Prototype/Materials/M_SPStealthCover'))
    assert visual.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION

signs = {'Reserved_Sign_40':'40 / ANONYMOUS EVENTS\nLASER - WORK NOISE - INDIRECT',
         'Reserved_Sign_50':'50 / DIRECT + ALARM EVENTS\nE: submit for yourself',
         'Reserved_Sign_60':'60 / STAGE API\nSTART - END - RESTART - REPLAY'}
for a in actors.get_all_level_actors():
    if a.get_actor_label() in signs:
        a.get_component_by_class(u.TextRenderComponent).set_text(signs[a.get_actor_label()])

assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
u.log('STEALTH_INCIDENT_MAP_SAVED: 13 E-interaction buttons + replicated status HUD director')
