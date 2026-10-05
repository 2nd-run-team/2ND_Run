"""UE 5.8 editor: place the direct-sight prototype in the first SPCar_Big.

Acquire the freight map and new asset LFS locks before running. Re-running updates
only actors with the explicit labels below; existing train modules are untouched.
Run: py "<project>/Tools/Stealth/setup_freight.py"
"""
import unreal as u

LEVEL = "/Game/SpacePirate/Maps/Lvl_SPTrainFreight"
ROOT = "/Game/SpacePirate/Stealth"
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
if world.get_path_name().split('.')[0] != LEVEL:
    raise RuntimeError("Open Lvl_SPTrainFreight first; this script never discards another map.")

car = min((a for a in actors.get_all_level_actors()
           if a.get_class().get_name() == "BP_SPFreightCar_Big_C"),
          key=lambda a: abs(a.get_actor_location().x))
origin = car.get_actor_location()
transform = car.get_actor_transform()

def pos(x, y, z):
    return u.MathLibrary.transform_location(transform, u.Vector(x, y, z))

def actor(cls, label, location, rotation=u.Rotator()):
    found = [a for a in actors.get_all_level_actors() if a.get_actor_label() == label]
    if len(found) > 1:
        raise RuntimeError("Duplicate prototype actor: " + label)
    result = found[0] if found else actors.spawn_actor_from_class(cls, location, rotation)
    result.set_actor_label(label)
    result.set_actor_location(location, False, True)
    result.set_actor_rotation(rotation, True)
    result.set_folder_path("Stealth_DirectSight_SPCarBig")
    return result

guard_path = ROOT + "/Blueprints/BP_SPGuardCharacter"
guard_bp = u.load_asset(guard_path)
if not guard_bp:
    factory = u.BlueprintFactory()
    factory.set_editor_property("parent_class", u.SPGuardCharacter)
    guard_bp = u.AssetToolsHelpers.get_asset_tools().create_asset(
        "BP_SPGuardCharacter", ROOT + "/Blueprints", u.Blueprint, factory)
guard_cdo = u.get_default_object(guard_bp.generated_class())
player_cdo = u.get_default_object(u.load_asset(
    "/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter").generated_class())
mesh = guard_cdo.get_editor_property("mesh")
source_mesh = player_cdo.get_editor_property("mesh")
mesh.set_skeletal_mesh_asset(source_mesh.get_editor_property("skeletal_mesh_asset"))
guard_anim = u.load_asset(ROOT + '/Animation/ABP_SPGuardLocomotion')
# Bootstrap the guard Blueprint before running build_guard_animation.py on a fresh checkout.
mesh.set_anim_instance_class(guard_anim.generated_class() if guard_anim else source_mesh.get_editor_property('anim_class'))
mesh.set_relative_transform(source_mesh.get_relative_transform(), False, True)
mesh.set_owner_no_see(False)
u.BlueprintEditorLibrary.compile_blueprint(guard_bp)
assert guard_bp.get_editor_property("status") == u.BlueprintStatus.BS_UP_TO_DATE
assert u.EditorAssetLibrary.save_loaded_asset(guard_bp)

route = actor(u.SPGuardPatrolRoute, "Stealth_Big_PatrolRoute_EditPoints", origin)
route.set_editor_property("points", [u.Vector(350,-630,2), u.Vector(-350,-630,2),
                                    u.Vector(-350,630,2), u.Vector(350,630,2)])
route.get_editor_property("restricted_area").set_box_extent(u.Vector(530,880,300))

wall = actor(u.StaticMeshActor, "Stealth_Big_CenterCover", pos(0,0,130))
wall_mesh = wall.get_component_by_class(u.StaticMeshComponent)
wall_mesh.set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube"))
wall_mesh.set_collision_profile_name("BlockAll")
wall.set_actor_scale3d(u.Vector(.9, 8.0, 2.6))

# The modular train deliberately marks its generated collision floor as not
# affecting navigation. A hidden, slightly lower surface makes this test fixture
# self-contained across construction-script rebuilds without editing train BPs.
floor = actor(u.StaticMeshActor, "Stealth_Big_NavigationFloor", pos(0,0,-2))
floor.get_component_by_class(u.StaticMeshComponent).set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube"))
floor.get_component_by_class(u.StaticMeshComponent).set_collision_profile_name("BlockAll")
floor.set_actor_scale3d(u.Vector(10.7,17.6,.02))
floor.set_actor_hidden_in_game(True)
floor.set_is_temporarily_hidden_in_editor(True)

for index, (x,y,yaw,start) in enumerate([(350,-630,180,0),(-350,630,0,2)],1):
    guard = actor(guard_bp.generated_class(), f"Stealth_Big_Guard_{index}",
                  pos(x,y,98),u.Rotator(yaw=yaw))
    guard.set_editor_property("patrol_route",route)
    guard.set_editor_property("start_point_index",start)
    guard.set_editor_property("alert_group","FreightBig_DirectSight")

# Actor factory gives the nav volume its standard cube brush. Scale from its
# measured bounds instead of relying on a version-specific brush default size.
nav = actor(u.NavMeshBoundsVolume, "Stealth_Big_NavMeshBounds", pos(0,0,250))
nav.set_actor_scale3d(u.Vector(1,1,1))
_, extent = nav.get_actor_bounds(False)
if min(extent.x,extent.y,extent.z) <= 0:
    raise RuntimeError("Navigation volume has no brush; create a box brush before saving.")
nav.set_actor_scale3d(u.Vector(1500/extent.x,1000/extent.y,450/extent.z))
nav_system = u.NavigationSystemV1.get_navigation_system(world)
nav_system.on_navigation_bounds_updated(nav)

# Place all four starts immediately before the tested car, facing its entrance.
starts = sorted((a for a in actors.get_all_level_actors() if isinstance(a,u.PlayerStart)),
                key=lambda a:a.get_actor_label())
for start,(x,y) in zip(starts,[(740,-120),(740,120),(920,-120),(920,120)]):
    start.set_actor_location(pos(x,y,110),False,True)
    start.set_actor_rotation(u.Rotator(yaw=180),True)

u.SystemLibrary.execute_console_command(world,"RebuildNavigation")
u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(pos(490,-760,240),u.Rotator(pitch=-8,yaw=135))
actors.set_selected_level_actors([route])
u.log("STEALTH_SETUP_READY: build navigation and save the current level after validation.")
