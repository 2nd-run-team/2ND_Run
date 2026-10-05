"""Run in a TWO-player, single-process Listen Server PIE session.

Moves only PIE actors. Exercises real guard ticks, replication, movement and
Enhanced Input (including the remote owner's crouch). Writes a JSON report to
Saved/Stealth/pie-verification.json. Stop PIE after inspection to reset the fixture.
"""
import json
from pathlib import Path
import unreal as u

worlds = u.EditorLevelLibrary.get_pie_worlds(False)
assert len(worlds)==2, "Start a two-player Listen Server PIE session first."
server = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
client = next(w for w in worlds if w!=server)
guards = sorted(u.GameplayStatics.get_all_actors_of_class(server,u.SPGuardCharacter),key=lambda g:g.get_name())
assert len(guards)==2
players = [p for p in u.GameplayStatics.get_all_actors_of_class(server,u.SPPlayerCharacter)
           if not isinstance(p,u.SPGuardCharacter)]
a = next(p for p in players if not p.is_locally_controlled())
b = next(p for p in players if p.is_locally_controlled())

def pid(p):
    return p.get_editor_property("player_state").get_editor_property("player_id")

def copies(player):
    return [p for w in worlds for p in u.GameplayStatics.get_all_actors_of_class(w,u.SPPlayerCharacter)
            if not isinstance(p,u.SPGuardCharacter) and pid(p)==pid(player)]

def place(player, xyz, yaw=0):
    for p in copies(player):
        p.set_actor_location(u.Vector(*xyz),False,True)
        p.get_editor_property("character_movement").stop_movement_immediately()
        if p.get_controller():
            p.get_controller().set_control_rotation(u.Rotator(yaw=yaw))

def freeze(g, xyz, yaw):
    g.set_editor_property("patrol_speed",0)
    g.set_editor_property("chase_speed",0)
    g.set_actor_location(u.Vector(*xyz),False,True)
    g.set_actor_rotation(u.Rotator(yaw=yaw),True)
    g.get_controller().stop_movement()
    movement = g.get_editor_property("character_movement")
    movement.stop_movement_immediately()
    movement.set_editor_property("orient_rotation_to_movement",False)

report={"checks":[],"snapshots":[],"input":"Enhanced Input injection on the remote owning client"}
def check(name, ok):
    report["checks"].append({"name":name,"passed":bool(ok)})
    u.log("STEALTH_CHECK " + name + ": " + str(bool(ok)))

def snapshot(label):
    rows=[]
    for w in worlds:
        rows.append({"server":w==server,"guards":[
            {"name":g.get_name(),"state":str(g.get_editor_property("guard_state")),
             "target_id":pid(g.get_editor_property("target_player")) if g.get_editor_property("target_player") else None,
             "position":g.get_actor_location().to_tuple(),
             "last_seen":g.get_editor_property("last_seen_location").to_tuple()}
            for g in u.GameplayStatics.get_all_actors_of_class(w,u.SPGuardCharacter)]})
    report["snapshots"].append({"stage":label,"worlds":rows})

def all_target_a():
    return all(g.get_editor_property("target_player") and pid(g.get_editor_property("target_player"))==pid(a)
               for w in worlds for g in u.GameplayStatics.get_all_actors_of_class(w,u.SPGuardCharacter))

freeze(guards[0],(-4900,0,308),0)
freeze(guards[1],(-4900,650,308),180)
place(a,(-4300,0,308))
place(b,(-3800,120,308))
local_a = next(p for p in copies(a) if p.is_locally_controlled())
input_subsystem = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
                       if not s.get_name().startswith("Default__")
                       and u.GameplayStatics.get_player_controller(s,0)==local_a.get_controller())
crouch_action = local_a.get_editor_property("crouch_action")
move_action = local_a.get_editor_property("move_action")
assert crouch_action and move_action

stage=0
elapsed=0.0
inject=False
last_time=u.GameplayStatics.get_time_seconds(server)
last_seen=None
move_start=None
handle=None

def tick(_delta):
    global stage,elapsed,last_time,inject,last_seen,move_start,handle
    try:
        now=u.GameplayStatics.get_time_seconds(server)
        elapsed+=max(0,now-last_time)
        last_time=now
        if inject:
            input_subsystem.inject_input_vector_for_action(crouch_action,u.Vector(1,0,0),[],[])
            input_subsystem.inject_input_vector_for_action(move_action,u.Vector(0,1,0),[],[])
        delay=[1.3,1.2,.5,.3,.6,.6,.7,.9,8.0,.8,1.0,.8][stage]
        if elapsed<delay: return
        elapsed=0
        if stage==0:
            check("Wall occludes player",not guards[0].can_see_player(a))
            check("Wall prevents identification",all(not g.get_editor_property("target_player") for g in guards))
            place(a,(-5050,0,308))
        elif stage==1:
            check("Outside field of view stays hidden",all(not g.get_editor_property("target_player") for g in guards))
            place(a,(-4750,0,308))
        elif stage==2:
            check("Partial glimpse shows suspicion only",not guards[0].get_editor_property("target_player") and guards[0].get_editor_property("suspicion_progress")>0)
            place(a,(-4300,0,308))
        elif stage==3:
            place(a,(-4750,0,308))
        elif stage==4:
            check("Occlusion resets confirmation time",not guards[0].get_editor_property("target_player"))
        elif stage==5:
            check("Both worlds target only witnessed A",all_target_a())
            check("Receiving guard cannot directly see A",not guards[1].can_see_player(a))
            snapshot("confirmed")
            last_seen=[g.get_editor_property("last_seen_location") for g in guards]
            place(a,(-4300,0,308))
        elif stage==6:
            check("Hidden target position is not tracked",all((g.get_editor_property("last_seen_location")-p).length()<.1 for g,p in zip(guards,last_seen)))
            move_start=[g.get_actor_location() for g in guards]
            for g in guards:
                g.set_editor_property("chase_speed",340)
                g.get_editor_property("character_movement").set_editor_property("orient_rotation_to_movement",True)
        elif stage==7:
            check("Both guards travel toward the report",all((g.get_actor_location()-p).length()>30 for g,p in zip(guards,move_start)))
        elif stage==8:
            check("Search ends and patrol resumes",all(not g.get_editor_property("target_player") for g in guards))
            snapshot("returned_to_patrol")
            freeze(guards[0],(-4210,0,308),0)
            place(a,(-3980,0,308))
            place(b,(-3840,140,308))
        elif stage==9:
            check("Known A is reacquired outside restricted area",guards[0].get_editor_property("target_player")==a)
            check("Visible innocent B never becomes the target",all(not g.get_editor_property("target_player") or pid(g.get_editor_property("target_player"))!=pid(b) for g in guards))
            freeze(guards[0],(-4900,-630,308),180)
            freeze(guards[1],(-4900,630,308),180)
            place(a,(-3600,-120,308))
            place(b,(-3200,-120,308),180)
            inject=True
        elif stage==10:
            check("Remote crouch replicates to server",all(p.get_editor_property("is_crouched") for p in copies(a)))
            speed=local_a.get_velocity().length()
            check("Held crouch movement uses 160 cm/s",145<=speed<=165)
            check("Crouch animation applied on both worlds",all(p.get_editor_property("mesh").get_post_process_instance().get_editor_property("crouch_alpha")>.9 for p in copies(a)))
            report["crouched_speed"]=speed
            report["crouched_capsule_half_height"]=local_a.get_editor_property("capsule_component").get_scaled_capsule_half_height()
            inject=False
        elif stage==11:
            check("Releasing Ctrl action uncrouches on both worlds",all(not p.get_editor_property("is_crouched") for p in copies(a)))
            finish()
            return
        stage+=1
    except Exception as exc:
        report["error"]=repr(exc)
        finish()

def finish():
    report["passed"]=not report.get("error") and all(c["passed"] for c in report["checks"]) and len(report["checks"])==16
    Path(u.Paths.project_saved_dir(),"Stealth","pie-verification.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.log("STEALTH_PIE_VERIFICATION_COMPLETE: " + str(report["passed"]))

handle=u.register_slate_post_tick_callback(tick)
u.log("STEALTH_PIE_VERIFICATION_STARTED")
