"""Create Lvl_SPStealthTest once, in UE 5.8.2 editor Python.

Requires our LFS locks for the map and all five materials. Refuses to replace an
existing map or discard dirty packages. Edit the saved map normally thereafter;
this is a creation recipe, not a destructive regeneration command.
"""
import json
import subprocess
from pathlib import Path
import unreal as u

MAP = '/Game/SpacePirate/Maps/Lvl_SPStealthTest'
MATERIAL_ROOT = '/Game/SpacePirate/Stealth/Prototype/Materials'
COLORS = {
    'Neutral': (0.19, 0.23, 0.28),
    'Public': (0.035, 0.31, 0.28),
    'Restricted': (0.48, 0.08, 0.035),
    'Cover': (0.12, 0.20, 0.32),
    'Future': (0.48, 0.31, 0.045),
}
project = Path(u.Paths.project_dir()).resolve()
required = ['Content/SpacePirate/Maps/Lvl_SPStealthTest.umap'] + [
    'Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealth' + n + '.uasset'
    for n in COLORS]
locks = json.loads(subprocess.check_output(
    ['git', 'lfs', 'locks', '--verify', '--json'], cwd=str(project),
    creationflags=subprocess.CREATE_NO_WINDOW, text=True, encoding='utf-8'))
owned = {row['path'] for row in locks.get('ours', [])}
assert set(required) <= owned, 'Acquire our LFS locks before authoring: ' + str(set(required) - owned)
assert u.SystemLibrary.get_engine_version().startswith('5.8.2-'), 'Use UE 5.8.2.'
assert not u.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE before creating a map.'
assert not u.EditorAssetLibrary.does_asset_exist(MAP), 'Map exists; open and edit it instead.'
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), 'Save your map first.'
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), 'Save your assets first.'

levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.new_level(MAP)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
materials = {}
for name, rgb in COLORS.items():
    path = MATERIAL_ROOT + '/M_SPStealth' + name
    material = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if material is None:
        material = u.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_SPStealth' + name, MATERIAL_ROOT, u.Material, u.MaterialFactoryNew())
        color = u.MaterialEditingLibrary.create_material_expression(
            material, u.MaterialExpressionVectorParameter, -400, 0)
        color.set_editor_property('parameter_name', 'Tint')
        color.set_editor_property('default_value', u.LinearColor(*rgb, 1))
        u.MaterialEditingLibrary.connect_material_property(color, '', u.MaterialProperty.MP_BASE_COLOR)
        rough = u.MaterialEditingLibrary.create_material_expression(
            material, u.MaterialExpressionScalarParameter, -400, 180)
        rough.set_editor_property('parameter_name', 'Roughness')
        rough.set_editor_property('default_value', 0.8)
        u.MaterialEditingLibrary.connect_material_property(rough, '', u.MaterialProperty.MP_ROUGHNESS)
        u.MaterialEditingLibrary.recompile_material(material)
        assert u.EditorAssetLibrary.save_loaded_asset(material)
    materials[name] = material

def spawn(cls, name, position, folder, yaw=0):
    result = actors.spawn_actor_from_class(cls, u.Vector(*position), u.Rotator(yaw=yaw))
    assert result, name
    result.set_actor_label(name)
    result.set_folder_path('StealthTest/' + folder)
    return result

def box(name, position, size, material='Neutral', folder='00_Infrastructure', collision=True):
    result = spawn(u.StaticMeshActor, name, position, folder)
    mesh = result.get_component_by_class(u.StaticMeshComponent)
    mesh.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
    mesh.set_material(0, materials[material])
    mesh.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    result.set_actor_scale3d(u.Vector(*(n / 100 for n in size)))
    return result

def sign(name, text, position, folder, size=55, yaw=180):
    result = spawn(u.TextRenderActor, name, position, folder, yaw)
    component = result.get_component_by_class(u.TextRenderComponent)
    component.set_text(text)
    component.set_world_size(size)
    component.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    component.set_text_render_color(u.Color(235, 245, 255, 255))
    return result

world.get_world_settings().set_editor_property('default_game_mode',
    u.load_asset('/Game/SpacePirate/Core/Blueprints/BP_SPGameMode').generated_class())
box('Floor_Main', (0, 1100, -25), (6400, 5000, 50))
for name, pos, size in [
    ('West', (-3200,1100,150), (50,5000,300)),
    ('East', (3200,1100,150), (50,5000,300)),
    ('South', (0,-1400,150), (6400,50,300)),
    ('North', (0,3600,150), (6400,50,300)),
]:
    box('Wall_' + name, pos, size)

# These thin panels have NO collision or game rule. The route's box is the rule.
box('Public_VisualFloor', (-1450,0,0.5), (2900,2400,1), 'Public', '10_Public', False)
box('Restricted_VisualFloor', (1400,0,0.5), (2600,2200,1), 'Restricted', '20_Restricted/Visuals', False)
box('Restricted_Threshold', (100,0,1.5), (14,2200,2), 'Future', '20_Restricted/Visuals', False)
sign('Public_Sign', '01 / PUBLIC\nMOVE - SPRINT - CROUCH', (-900,1000,230), '10_Public', 55)
sign('Restricted_Sign', '02 / RESTRICTED\nDIRECT SIGHT + COVER', (2200,1000,260), '20_Restricted/Visuals', 55)
sign('Welcome', 'STEALTH TEST LAB\nWASD  |  SHIFT  |  CTRL  |  E  |  G\nFuture bays: north corridor', (-2850,1000,220), '10_Public', 42, -90)
box('Cover_Tall', (1400,0,140), (100,900,280), 'Cover', '20_Restricted/Cover')
box('Cover_Low', (600,-250,55), (180,420,110), 'Cover', '20_Restricted/Cover')
box('Cover_Return', (2400,250,125), (140,400,250), 'Cover', '20_Restricted/Cover')

route = spawn(u.SPGuardPatrolRoute, 'Restricted_PatrolRoute', (1400,0,0), '20_Restricted/Rules')
route.set_editor_property('points', [u.Vector(-850,-800,0),u.Vector(850,-800,0),
                                    u.Vector(850,800,0),u.Vector(-850,800,0)])
area = route.get_editor_property('restricted_area')
area.set_box_extent(u.Vector(1300,1100,300))
area.set_hidden_in_game(False)
area.set_editor_property('shape_color', u.Color(255,70,30,255))
guard_class = u.load_asset('/Game/SpacePirate/Stealth/Blueprints/BP_SPGuardCharacter').generated_class()
for index, (x,y,yaw) in enumerate([(550,-800,0),(2250,800,180)]):
    guard = spawn(guard_class, 'Guard_' + str(index+1), (x,y,100), '20_Restricted/Guards', yaw)
    guard.set_editor_property('patrol_route', route)
    guard.set_editor_property('start_point_index', index*2)
    guard.set_editor_property('alert_group', 'StealthTest_Baseline')
    guard.set_editor_property('show_vision', True)

for index, (x,y) in enumerate([(-2500,-450),(-2500,-150),(-2800,-450),(-2800,-150)],1):
    spawn(u.PlayerStart, 'PlayerStart_' + str(index), (x,y,110), '10_Public/Starts')

box('Interaction_Table', (-1600,-850,40), (650,160,80), 'Cover', '30_Interaction')
sign('Interaction_Sign', '03 / EXISTING INTERACTION\nE: pick up / hold to pack   G: drop\n1-4: slots', (-1600,-960,235), '30_Interaction', 38, 90)
for name, path, pos in [
    ('Test_Keycard', 'BP_SPCargo', (-1840,-850,130)),
    ('Test_LootBag', 'BP_SPLootBag', (-1600,-850,110)),
    ('Test_LootBundle', 'BP_SPLootBundle', (-1360,-850,110)),
]:
    spawn(u.load_asset('/Game/SpacePirate/Cargo/'+path).generated_class(), name, pos, '30_Interaction')

# Reserved spaces contain no cameras, lasers, crime triggers or objective rules.
for number, x, title in [(40,-2100,'CCTV / LASER'),(50,0,'REPORT / SEARCH'),(60,2100,'SECURITY API')]:
    folder = str(number) + '_Reserved'
    box('Reserved_'+str(number), (x,2500,0.5), (1800,1600,1), 'Future', folder, False)
    sign('Reserved_Sign_'+str(number), f'{number} / {title}\nRESERVED - NO GAMEPLAY YET', (x,3300,240), folder, 50, -90)
    for offset in [-920,920]:
        box('Reserved_Edge_'+str(number)+'_'+str(offset), (x+offset,2500,65), (25,1600,130), 'Neutral', folder)

nav = spawn(u.NavMeshBoundsVolume, 'Navigation_AllBays', (0,1100,200), '00_Infrastructure')
_, extent = nav.get_actor_bounds(False)
assert min(extent.x, extent.y, extent.z) > 0, 'Nav volume needs a cube brush.'
nav.set_actor_scale3d(u.Vector(3175/extent.x,2475/extent.y,500/extent.z))
u.NavigationSystemV1.get_navigation_system(world).on_navigation_bounds_updated(nav)

sun = spawn(u.DirectionalLight, 'Light_Sun', (0,0,1000), '00_Infrastructure')
sun.set_actor_rotation(u.Rotator(pitch=-55,yaw=-35),False)
sun.light_component.set_editor_property('intensity', 3.0)
sky = spawn(u.SkyLight, 'Light_Ambient', (0,0,900), '00_Infrastructure')
sky.light_component.set_editor_property('intensity', 1.0)
spawn(u.SkyAtmosphere, 'Sky_EngineOnly', (0,0,0), '00_Infrastructure')
sun.light_component.set_editor_property('atmosphere_sun_light', True)
sky.light_component.set_editor_property('real_time_capture', True)
exposure = spawn(u.PostProcessVolume, 'Exposure_TestLab', (0,0,0), '00_Infrastructure')
exposure.set_editor_property('unbound', True)
settings = exposure.get_editor_property('settings')
for prop, value in [('override_auto_exposure_min_brightness', True),
                    ('override_auto_exposure_max_brightness', True),
                    ('auto_exposure_min_brightness', 2.0), ('auto_exposure_max_brightness', 2.0)]:
    settings.set_editor_property(prop, value)
exposure.set_editor_property('settings', settings)

u.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(
    u.Vector(-4000,-4500,5500), u.Rotator(pitch=-43,yaw=55))
actors.set_selected_level_actors([])
assert levels.save_current_level()
u.log('STEALTH_TEST_MAP_CREATED: allow navigation build to finish, then save and validate_test_map.py.')
