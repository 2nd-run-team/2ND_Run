"""Stage 4: migrate two known maps to independent areas, add public crime station.
Requires map + new functional BP LFS locks. Refuses dirty packages, duplicate
migration and existing crime station. Backs up original maps before saving.
"""
import json,shutil,subprocess
from datetime import datetime
from pathlib import Path
import unreal as u
project=Path(u.Paths.project_dir()).resolve()
required=['Content/SpacePirate/Maps/'+name+'.umap' for name in ['Lvl_SPStealthTest','Lvl_SPTrainFreight']]+['Content/SpacePirate/Stealth/Blueprints/BP_SPRestrictedArea.uasset']
locks=json.loads(subprocess.check_output(['git','lfs','locks','--verify','--json'],cwd=project,creationflags=subprocess.CREATE_NO_WINDOW,text=True,encoding='utf-8'))
assert set(required)<={r['path'] for r in locks.get('ours',[])}
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-')
assert not u.EditorLevelLibrary.get_pie_worlds(False)
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
root='/Game/SpacePirate/Stealth/Blueprints'
assert not u.EditorAssetLibrary.does_asset_exist(root+'/BP_SPRestrictedArea'),'Already migrated; inspect current map instead.'
backup=project/'Saved/StealthCrime'/('before-map-'+datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
for asset in required[:2]:shutil.copy2(project/asset,backup/Path(asset).name)
factory=u.BlueprintFactory();factory.set_editor_property('parent_class',u.SPRestrictedArea)
bp=u.AssetToolsHelpers.get_asset_tools().create_asset('BP_SPRestrictedArea',root,u.Blueprint,factory)
u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);actors=u.get_editor_subsystem(u.EditorActorSubsystem)
for name in ['Lvl_SPTrainFreight','Lvl_SPStealthTest']:
    assert levels.load_level('/Game/SpacePirate/Maps/'+name)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert not u.GameplayStatics.get_all_actors_of_class(world,u.SPRestrictedArea)
    routes=u.GameplayStatics.get_all_actors_of_class(world,u.SPGuardPatrolRoute)
    assert len(routes)==1
    for route in routes:
        box=route.get_editor_property('restricted_area')
        area=actors.spawn_actor_from_class(bp.generated_class(),box.get_world_location(),box.get_world_rotation())
        area.set_actor_label('Restricted_Area_Main');area.set_folder_path('StealthTest/20_Restricted' if name=='Lvl_SPStealthTest' else 'Stealth/RestrictedAreas')
        area.set_actor_scale3d(box.get_world_scale())
        area.get_editor_property('volume').set_box_extent(box.get_unscaled_box_extent(),False)
        area.set_editor_property('area_name','Cargo Security')
        # Do not attach to the route. This is a one-time saved data migration.
        assert area.get_attach_parent_actor() is None
    for g in u.GameplayStatics.get_all_actors_of_class(world,u.SPGuardCharacter):
        g.set_editor_property('hear_footsteps',False)
        g.set_editor_property('confirm_sight_time',1)
    assert levels.save_current_level()
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
folder='StealthTest/70_CrimeObservation'
def spawn(cls,label,loc,rotation=None):
    a=actors.spawn_actor_from_class(cls,u.Vector(*loc),rotation or u.Rotator())
    a.set_actor_label(label);a.set_folder_path(folder);return a
matroot='/Game/SpacePirate/Stealth/Prototype/Materials/M_SPStealth'
def shape(label,loc,size,material,collision='NoCollision'):
    a=spawn(u.StaticMeshActor,label,loc);a.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    a.set_actor_scale3d(u.Vector(*[x/100 for x in size]))
    a.static_mesh_component.set_collision_profile_name(collision)
    a.static_mesh_component.set_material(0,u.load_asset(matroot+material));return a

def sign(label,text,loc,size=24):
    a=spawn(u.TextRenderActor,label,loc,u.Rotator(yaw=-90));c=a.get_component_by_class(u.TextRenderComponent)
    c.set_text(text);c.set_world_size(size);c.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER);return a
shape('Crime_PublicNormal_Floor',(-1550,450,4),(600,500,4),'Public')
sign('Crime_PublicNormal_Sign','PUBLIC / WALK - RUN - JUMP - CARRY',(-1550,300,120),18)
shape('Crime_PublicWork_Floor',(-1100,1000,4),(1700,400,4),'Future')
sign('Crime_PublicWork_Sign','PUBLIC CRIME TESTS / ONLY MARKED ACTIONS',(-1100,1400,200),24)
shape('Crime_NoiseWall',(-2050,550,140),(80,600,280),'Cover','BlockAll')
sign('Crime_NoiseWall_Sign','OPAQUE COVER / WORK NOISE IS ANONYMOUS',(-2450,950,170),18)
console=u.load_class(None,root+'/BP_SPStealthTestConsole.BP_SPStealthTestConsole_C')
k=u.SPCrimeKind
rows=[('Pickpocket',-1800,k.PICKPOCKET,True,0),('Terminal',-1450,k.SECURITY_TERMINAL,False,3),
      ('Vault',-1100,k.VAULT_WORK,False,3),('Packing',-750,k.LOOT_PACKING,False,3),('Tool',-400,k.SUPPRESSION_TOOL,True,0)]
for name,x,kind,instant,duration in rows:
    a=spawn(console,'Crime_'+name,(x,1000,120),u.Rotator(yaw=-90))
    a.set_editor_property('command',u.SPStealthTestCommand.OBSERVED_CRIME)
    text=name.upper()+(' / INSTANT' if instant else ' / HOLD E')
    a.set_editor_property('button_label',text);a.get_editor_property('label').set_text(text)
    a.get_editor_property('label').set_world_size(18)
    inter=a.get_editor_property('interactable');inter.set_editor_property('crime_kind',kind)
    inter.set_editor_property('instant_crime',instant);inter.set_editor_property('hold_duration',duration);inter.set_editor_property('prompt',text)
    visual=a.get_editor_property('visual');visual.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'));visual.set_relative_scale3d(u.Vector(.65,.65,.75))
    visual.set_material(0,u.load_asset(matroot+'Future'))
a=spawn(console,'Crime_WorkNoise',(-2450,500,120),u.Rotator(yaw=-90))
a.set_editor_property('incident_kind',u.SPStealthIncident.WORK_NOISE)
a.set_editor_property('incident_location',u.Vector(-2300,550,0));a.set_editor_property('dispatch_group','StealthTest_Crime')
a.set_editor_property('max_responders',1);a.set_editor_property('button_label','DRILL SOUND / NO ID');a.get_editor_property('label').set_text('DRILL SOUND / NO ID')
a.get_editor_property('label').set_world_size(18)
v=a.get_editor_property('visual');v.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'));v.set_relative_scale3d(u.Vector(.7,.9,.9));v.set_material(0,u.load_asset(matroot+'Cover'))
# Existing production loot bundle and guard blueprints; no cloned character or animation assets.
bundle=spawn(u.load_class(None,'/Game/SpacePirate/Cargo/BP_SPLootBundle.BP_SPLootBundle_C'),'Crime_PublicLootBundle',(-1000,600,90))
bundle.get_editor_property('interactable').set_editor_property('crime_kind',k.LOOT_PACKING)
shape('Crime_PackingTable',(-1000,600,35),(160,100,70),'Cover','BlockAll')
guard=spawn(u.load_class(None,root+'/BP_SPGuardCharacter.BP_SPGuardCharacter_C'),'Crime_ObserverGuard',(-1100,100,100),u.Rotator(yaw=90))
guard.set_editor_property('alert_group','StealthTest_Crime');guard.set_editor_property('identity_scope','StageSecurity')
guard.set_editor_property('hear_footsteps',False);guard.set_editor_property('patrol_route',None)
guard.get_editor_property('alert_indicator').set_world_size(24)
assert levels.save_current_level()
u.SystemLibrary.execute_console_command(world,'RebuildNavigation')
u.log('STEALTH_CRIME_MAP_SAVED: independent areas in both maps; crime station authored. Save again after nav build.')
