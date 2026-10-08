"""Start two-player Listen Server PIE and verify status over real replication.

작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 검증
작업자: 김세훈 | 2026-10-08 | 다운 캡슐 회전·머리/몸통 조준·화물 운반 중 구조 회귀 검사 추가
Checks damage, down/revive input, replication, cancellation, and each owner's HUD.

Launch the normal UnrealEditor.exe (not a commandlet), opening Lvl_SPTestMap:
  -ExecutePythonScript="<project>/Tools/PlayerStatus/run_pie_verification.py"
  -unattended -nosplash -nosound -NoLiveCoding

Only PIE actors are moved. No asset is saved. Play settings are restored directly
after the engine copies them for PIE. The editor exits after the JSON report is
written to Saved/PlayerStatus/pie-verification.json. The script must not be run
inside an existing PIE session by default. For an existing two-player session use
runpy.run_path(..., init_globals={'SP_STATUS_USE_EXISTING_PIE': True}); that mode
leaves PIE/editor running for the caller to stop. Uses Enhanced Input on both owners.
Set Editor Play Net Mode to Listen Server before launching this script.
"""
import json
import time
import traceback
from pathlib import Path

import unreal as u

USE_EXISTING_PIE = bool(globals().get("SP_STATUS_USE_EXISTING_PIE", False))
OUTPUT = Path(u.Paths.project_saved_dir()) / "PlayerStatus"
OUTPUT.mkdir(parents=True, exist_ok=True)
REPORT_PATH = OUTPUT / "pie-verification.json"
report = {"checks": [], "snapshots": [], "input": "Enhanced Input, remote owner and listen host"}
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
started_at = time.monotonic()
handle = None
stage = -1
elapsed = 0.0
last_time = 0.0
worlds = []
server = None
client = None
a = None  # Remote client player's server copy.
b = None  # Listen host player's server copy.
local_a = None
local_b = None
inputs = {}
held = set()
down_start = None
position_a = None
position_b = None
performance_settings = None
previous_throttle = None


def check(name, passed):
    report["checks"].append({"name": name, "passed": bool(passed)})
    u.log("PLAYER_STATUS_CHECK " + name + ": " + str(bool(passed)))


def players(world):
    return [p for p in u.GameplayStatics.get_all_actors_of_class(world, u.SPPlayerCharacter)
            if not isinstance(p, u.SPGuardCharacter)]


def pid(player):
    state = player.get_editor_property("player_state")
    return state.get_editor_property("player_id") if state else None


def copies(player):
    return [p for world in worlds for p in players(world) if pid(p) == pid(player)]


def status(player):
    return player.get_status_component()


def health_is(player, expected):
    replicas = copies(player)
    return len(replicas) == 2 and all(abs(status(p).get_health() - expected) < 0.01 for p in replicas)


def down_is(player, expected):
    replicas = copies(player)
    return len(replicas) == 2 and all(status(p).is_downed() == expected for p in replicas)


def body_is_horizontal(player):
    for p in copies(player):
        mesh = p.get_editor_property("mesh")
        torso = mesh.get_socket_location("head") - mesh.get_socket_location("pelvis")
        if torso.length() < 1 or abs(torso.z) / torso.length() > .25:
            return False
    return True


def hud(player):
    # StatusHUD is private/transient and deliberately not exposed to Python.
    return next((widget for widget in u.WidgetLibrary.get_all_widgets_of_class(
        player, u.SPPlayerStatusHUDWidget, False)
        if widget.get_owning_player_pawn() == player), None)


def hud_text(player, field):
    widget = hud(player)
    text = widget.get_editor_property(field) if widget else None
    return str(text.get_text()) if text else ""


def snapshot(label):
    report["snapshots"].append({"stage": label, "worlds": [
        {"server": world == server, "players": [
            {"id": pid(p), "local": p.is_locally_controlled(),
             "health": status(p).get_health(), "down": status(p).is_downed(),
             "progress": status(p).get_revive_progress(),
             "position": p.get_actor_location().to_tuple(),
             "capsule_up": p.get_actor_up_vector().to_tuple(),
             "hud_health": hud_text(p, "health_text") if p.is_locally_controlled() else "",
             "hud_state": hud_text(p, "state_text") if p.is_locally_controlled() else ""}
            for p in players(world)]}
        for world in worlds]})


def screenshot(label):
    filename = (OUTPUT / (label + ".png")).resolve().as_posix()
    u.SystemLibrary.execute_console_command(server, 'Shot SHOWUI filename="' + filename + '" -nosuffix',
                                            local_b.get_controller())
    report.setdefault("screenshots_requested", []).append(filename)


def create_floor(world):
    # Spawn separately into each PIE world; no level asset or editor world changes.
    transform = u.Transform(location=u.Vector(10000, 0, 10000), scale=u.Vector(20, 20, 1))
    # These BlueprintInternalUseOnly functions have no generated Python methods.
    # Object.call_method invokes their UFUNCTIONs through supported reflection.
    gameplay_statics = u.get_default_object(u.GameplayStatics)
    scale_method = u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT
    floor = gameplay_statics.call_method(
        "BeginDeferredActorSpawnFromClass",
        args=(world, u.StaticMeshActor, transform,
              u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, scale_method))
    assert floor, "Could not spawn temporary PIE floor."
    mesh = floor.get_component_by_class(u.StaticMeshComponent)
    mesh.set_mobility(u.ComponentMobility.MOVABLE)
    assert mesh.set_static_mesh(u.load_asset("/Engine/BasicShapes/Cube")), "Temporary floor mesh was rejected."
    mesh.set_collision_profile_name("BlockAll")
    gameplay_statics.call_method("FinishSpawningActor", args=(floor, transform, scale_method))


def place(player, xyz):
    for p in copies(player):
        p.set_actor_location(u.Vector(*xyz), False, True)
        p.get_editor_property("character_movement").stop_movement_immediately()


def aim(rescuer, target, socket="spine_05"):
    camera = rescuer.get_editor_property("first_person_camera")
    mesh = target.get_editor_property("mesh")
    assert mesh.does_socket_exist(socket), "Missing test aim bone: " + socket
    rescuer.get_controller().set_control_rotation(u.MathLibrary.find_look_at_rotation(
        camera.get_world_location(), mesh.get_socket_location(socket)))
    report.setdefault("aims", []).append({
        "socket": socket, "camera": camera.get_world_location().to_tuple(),
        "target": mesh.get_socket_location(socket).to_tuple(),
        "capsule": target.get_actor_location().to_tuple(),
        "mesh_location": mesh.get_world_location().to_tuple(),
        "mesh_rotation": mesh.get_world_rotation().to_tuple(),
        "range": (mesh.get_socket_location(socket) - camera.get_world_location()).length()})


def give_test_cargo(player):
    """Use the real server pickup RPC on a temporary replicated cargo actor."""
    cargo_class = u.load_class(None, "/Game/SpacePirate/Cargo/BP_SPSmallLoot.BP_SPSmallLoot_C")
    assert cargo_class, "Test cargo blueprint is missing."
    transform = u.Transform(location=player.get_actor_location() + u.Vector(0, 120, 0))
    library = u.get_default_object(u.GameplayStatics)
    scale_method = u.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT
    cargo = library.call_method("BeginDeferredActorSpawnFromClass", args=(
        server, cargo_class, transform, u.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None, scale_method))
    library.call_method("FinishSpawningActor", args=(cargo, transform, scale_method))
    player.get_component_by_class(u.SPInteractorComponent).call_method("ServerStartInteract", args=(cargo,))
    assert player.is_carrying_cargo(), "Server pickup did not put test cargo in the patient's hand."


def make_input(player):
    controller = player.get_controller()
    subsystem = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
                     if not s.get_name().startswith("Default__")
                     and u.GameplayStatics.get_player_controller(s, 0) == controller)
    interact = player.get_editor_property("interact_action")
    move = player.get_editor_property("move_action")
    assert interact and move, "Player blueprint must supply InteractAction and MoveAction."
    return subsystem, interact, move


def inject():
    for key in held:
        who, action = key.split(":")
        subsystem, interact, move = inputs[who]
        subsystem.inject_input_vector_for_action(
            interact if action == "e" else move,
            u.Vector(1, 0, 0) if action == "e" else u.Vector(0, 1, 0), [], [])


def start_fixture():
    global server, client, a, b, local_a, local_b, last_time, stage, position_a, position_b
    server = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    client = next(w for w in worlds if w != server)
    a = next(p for p in players(server) if not p.is_locally_controlled())
    b = next(p for p in players(server) if p.is_locally_controlled())
    local_a = next(p for p in copies(a) if p.is_locally_controlled())
    local_b = b
    for guard in u.GameplayStatics.get_all_actors_of_class(server, u.SPGuardCharacter):
        guard.destroy_actor()
    for world in worlds:
        create_floor(world)
    # Cube top is z=10050. Place capsules just above it, away from map geometry.
    position_a = (10000, 0, 10052 + a.get_editor_property("capsule_component").get_scaled_capsule_half_height())
    position_b = (10150, 0, 10052 + b.get_editor_property("capsule_component").get_scaled_capsule_half_height())
    place(a, position_a)
    place(b, position_b)
    inputs["a"] = make_input(local_a)
    inputs["b"] = make_input(local_b)
    u.GameplayStatics.get_game_mode(server).reset_for_stage()
    last_time = u.GameplayStatics.get_time_seconds(server)
    stage = 0
    u.log("PLAYER_STATUS_PIE_FIXTURE_READY")


def finish(error=None):
    global handle
    if error:
        report["error"] = error
    report["passed"] = not report.get("error") and len(report["checks"]) >= 20 and all(
        item["passed"] for item in report["checks"])
    report["duration_seconds"] = round(time.monotonic() - started_at, 2)
    REPORT_PATH.write_text(json.dumps(report, indent=2), encoding="utf-8")
    held.clear()
    if handle is not None:
        u.unregister_slate_post_tick_callback(handle)
        handle = None
    try:
        if performance_settings is not None and previous_throttle is not None:
            performance_settings.set_editor_property("bThrottleCPUWhenNotForeground", previous_throttle)
        if not USE_EXISTING_PIE:
            level.editor_request_end_play()
    finally:
        u.log("PLAYER_STATUS_PIE_VERIFICATION_COMPLETE: " + str(report["passed"]))
        if not USE_EXISTING_PIE:
            u.EditorPythonScripting.set_keep_python_script_alive(False)


def tick(_delta):
    global worlds, stage, elapsed, last_time, down_start
    try:
        if time.monotonic() - started_at > 180:
            raise TimeoutError("PIE verification exceeded 180 seconds, stage=" + str(stage))
        if stage < 0:
            worlds = u.EditorLevelLibrary.get_pie_worlds(False)
            if len(worlds) != 2 or any(len(players(w)) != 2 for w in worlds):
                return
            if any(pid(p) is None for w in worlds for p in players(w)):
                return
            start_fixture()
            return

        inject()
        now = u.GameplayStatics.get_time_seconds(server)
        elapsed += max(0, now - last_time)
        last_time = now
        delays = [1.0, .7, .6, .7, .7, 1.2, .6, 1.0, 1.5, .7, .6, 4.6, .6, .7, 1.3, 3.3, .7, .7]
        if elapsed < delays[stage]:
            return
        elapsed = 0.0
        if stage == 0:
            assert all(abs(p.get_actor_location().z - position_a[2]) < 10 for p in copies(a)), "Remote player did not settle on the test floor."
            assert all(abs(p.get_actor_location().z - position_b[2]) < 10 for p in copies(b)), "Host did not settle on the test floor."
            check("Both worlds start at 100 HP", health_is(a, 100) and health_is(b, 100))
            check("Each owning player has its own HUD", hud(local_a) and hud(local_b)
                  and hud(local_a).get_owning_player_pawn() == local_a
                  and hud(local_b).get_owning_player_pawn() == local_b)
            snapshot("initial")
            screenshot("hud-initial")
            status(a).apply_damage(25)
        elif stage == 1:
            check("Server damage replicates remote owner HP 75", health_is(a, 75))
            check("Owner HUD shows numeric 75 HP", hud_text(local_a, "health_text") == "체력 75 / 100")
            status(local_a).apply_damage(25)
        elif stage == 2:
            check("Client cannot modify authority HP directly", health_is(a, 75))
            u.GameplayStatics.apply_damage(b, 1000.0, a.get_controller(), a, u.DamageType)
        elif stage == 3:
            check("Empty-hand downed capsule is horizontal on server and client", all(abs(p.get_actor_up_vector().z) < .01 for p in copies(b)))
            check("Empty-hand mesh follows the downed capsule everywhere", body_is_horizontal(b))
            check("Engine ApplyDamage downs host on both worlds", down_is(b, True) and health_is(b, 0))
            check("Downed owner HUD shows zero and down state", hud_text(local_b, "health_text") == "체력 0 / 100"
                  and hud_text(local_b, "state_text") == "다운 · 구조 대기")
            check("Active crew count replicates", all(u.GameplayStatics.get_game_state(w).get_active_player_count() == 1 for w in worlds))
            screenshot("hud-downed")
            down_start = b.get_actor_location()
            held.add("b:move")
        elif stage == 4:
            check("Downed input cannot move host", (b.get_actor_location() - down_start).length() < 1.0)
            held.clear()
            aim(local_a, next(p for p in copies(b) if not p.has_authority()), "head")
            held.add("a:e")
        elif stage == 5:
            report["view_at_first_rescue"] = {
                "location": local_a.get_controller().get_editor_property("player_camera_manager").get_camera_location().to_tuple(),
                "rotation": local_a.get_controller().get_editor_property("player_camera_manager").get_camera_rotation().to_tuple()}
            check("Remote E starts server rescue", status(b).get_rescuer() == a)
            check("Rescue progress replicates before completion", all(.05 < status(p).get_revive_progress() < .8 for p in copies(b)))
            check("Target stays down before four seconds", down_is(b, True))
            snapshot("remote_rescue_in_progress")
            held.clear()
        elif stage == 6:
            check("Remote E release resets progress on both worlds", all(status(p).get_revive_progress() == 0 for p in copies(b)) and down_is(b, True))
            held.add("a:e")
        elif stage == 7:
            check("Remote rescue restarts after release", status(b).get_revive_progress() > .05)
            held.add("a:move")
        elif stage == 8:
            check("Movement cancels remote rescue", status(b).get_revive_progress() == 0 and down_is(b, True))
            held.clear()
        elif stage == 9:
            place(a, position_a)
            aim(local_a, next(p for p in copies(b) if not p.has_authority()))
        elif stage == 10:
            held.add("a:e")
        elif stage == 11:
            held.clear()
            check("Remote four-second rescue restores host HP30 everywhere", health_is(b, 30) and down_is(b, False))
            check("Host HUD reflects restored HP30", hud_text(local_b, "health_text") == "체력 30 / 100")
            snapshot("host_revived")
            check("Revived host capsule is upright everywhere", all(p.get_actor_up_vector().z > .99 for p in copies(b)))
            give_test_cargo(a)
            status(a).apply_damage(1000)
        elif stage == 12:
            check("Held cargo is still present on server and client", all(p.is_carrying_cargo() for p in copies(a)))
            check("Cargo-carrying downed capsule is horizontal everywhere", all(abs(p.get_actor_up_vector().z) < .01 for p in copies(a)))
            check("Cargo-carrying mesh follows the downed capsule everywhere", body_is_horizontal(a))
            check("Remote player down state replicates", down_is(a, True) and health_is(a, 0))
            check("Remote HUD reflects its own down state", hud_text(local_a, "state_text") == "다운 · 구조 대기")
            place(b, position_b)
            aim(local_b, a, "head")
        elif stage == 13:
            held.add("b:e")
        elif stage == 14:
            screenshot("revive-capsule-cargo")
            check("Host E rescues remote player", status(a).get_rescuer() == b)
            check("Remote downed owner receives rescue progress", status(local_a).get_revive_progress() > .05)
            snapshot("host_rescue_in_progress")
        elif stage == 15:
            held.clear()
            check("Host rescue restores remote HP30 everywhere", health_is(a, 30) and down_is(a, False))
            check("Remote owner HUD reflects restored HP30", hud_text(local_a, "health_text") == "체력 30 / 100")
            check("Revived cargo carrier capsule is upright everywhere", all(p.get_actor_up_vector().z > .99 for p in copies(a)))
            check("Rescue keeps the held cargo", all(p.is_carrying_cargo() for p in copies(a)))
            status(a).apply_damage(1000)
            status(b).apply_damage(1000)
        elif stage == 16:
            check("All-down operation failure replicates", all(u.GameplayStatics.get_game_state(w).is_operation_failed() for w in worlds))
            check("Both local HUDs display operation failure", all(
                hud(p).get_editor_property("failure_text").get_visibility() != u.SlateVisibility.COLLAPSED
                for p in (local_a, local_b)))
            snapshot("all_down")
            u.GameplayStatics.get_game_mode(server).reset_for_stage()
        elif stage == 17:
            check("Stage reset restores both players on both worlds", health_is(a, 100) and health_is(b, 100)
                  and down_is(a, False) and down_is(b, False))
            check("Stage reset clears replicated operation failure", all(not u.GameplayStatics.get_game_state(w).is_operation_failed() for w in worlds))
            snapshot("reset")
            finish()
            return
        stage += 1
    except Exception:
        finish(traceback.format_exc())


try:
    assert (len(u.EditorLevelLibrary.get_pie_worlds(False)) == 2 if USE_EXISTING_PIE
            else not u.EditorLevelLibrary.get_pie_worlds(False)), "Expected two-player PIE or an idle editor."
    if not USE_EXISTING_PIE:
        u.EditorPythonScripting.set_keep_python_script_alive(True)
    performance_settings = u.get_default_object(u.load_class(None, "/Script/UnrealEd.EditorPerformanceSettings"))
    previous_throttle = performance_settings.get_editor_property("bThrottleCPUWhenNotForeground")
    performance_settings.set_editor_property("bThrottleCPUWhenNotForeground", False)
    settings = u.get_default_object(u.load_class(None, "/Script/UnrealEd.LevelEditorPlaySettings"))
    # EPlayNetMode is not exported to Python in UE 5.8. Use the editor's saved
    # Listen Server setting; the fixture verifies one server and one client.
    overrides = {"RunUnderOneProcess": True, "PlayNumberOfClients": 2,
                 "bLaunchSeparateServer": False}
    previous = {key: settings.get_editor_property(key) for key in overrides}
    try:
        for key, value in overrides.items():
            settings.set_editor_property(key, value)
        if not USE_EXISTING_PIE:
            level.editor_request_begin_play()
    finally:
        # RequestPlaySession synchronously duplicates these settings in UE5.8.
        for key, value in previous.items():
            settings.set_editor_property(key, value)
    handle = u.register_slate_post_tick_callback(tick)
    u.log("PLAYER_STATUS_PIE_VERIFICATION_STARTED")
except Exception:
    finish(traceback.format_exc())
