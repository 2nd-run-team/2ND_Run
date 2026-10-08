"""Fresh two-player Listen Server PIE on Lvl_SPStealthTest; changes PIE only.

Real game ticks, owning-client Enhanced Input, server results and replicated
copies. Stop PIE after the result. Writes Saved/StealthTest/pie-verification.json.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

worlds = u.EditorLevelLibrary.get_pie_worlds(False)
assert len(worlds) == 2 and all('Lvl_SPStealthTest' in w.get_path_name() for w in worlds)
server = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
# Isolate this earlier-stage fixture from newly placed laser hazards (PIE only).
for laser in u.GameplayStatics.get_all_actors_of_class(server,u.SPLaserSecurityDevice):
    laser.set_editor_property("enabled",False);laser.refresh_device()
guards = sorted([g for g in u.GameplayStatics.get_all_actors_of_class(server,u.SPGuardCharacter) if str(g.get_editor_property("alert_group"))=="StealthTest_Baseline"],key=lambda g:g.get_name())
assert len(guards) == 2
def players(w):
    return [p for p in u.GameplayStatics.get_all_actors_of_class(w,u.SPPlayerCharacter) if not isinstance(p,u.SPGuardCharacter)]
a = next(p for p in players(server) if not p.is_locally_controlled())
b = next(p for p in players(server) if p.is_locally_controlled())
def pid(p):
    return p.get_editor_property('player_state').get_editor_property('player_id')
def copies(p):
    return [c for w in worlds for c in players(w) if pid(c) == pid(p)]
def place(p, xyz, yaw=0, pitch=0):
    for c in copies(p):
        c.set_actor_location(u.Vector(*xyz),False,True)
        c.character_movement.stop_movement_immediately()
        if c.get_controller():
            c.get_controller().set_control_rotation(u.Rotator(yaw=yaw,pitch=pitch))
def freeze(g, xyz, yaw):
    g.set_editor_property('patrol_speed',0)
    g.set_editor_property('chase_speed',0)
    g.get_controller().stop_movement()
    g.character_movement.stop_movement_immediately()
    g.character_movement.set_editor_property('orient_rotation_to_movement',False)
    g.set_actor_location(u.Vector(*xyz),False,True)
    g.set_actor_rotation(u.Rotator(yaw=yaw),True)
def all_guards():
    return [g for w in worlds for g in u.GameplayStatics.get_all_actors_of_class(w,u.SPGuardCharacter) if str(g.get_editor_property("alert_group"))=="StealthTest_Baseline"]
local = next(p for p in copies(a) if p.is_locally_controlled())
inputs = next(s for s in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
              if not s.get_name().startswith('Default__') and u.GameplayStatics.get_player_controller(s,0) == local.get_controller())
actions = {key: local.get_editor_property(key+'_action') for key in ['move','crouch','interact','drop']}
assert all(actions.values())
performance = u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
performance_before = O.get_properties(performance,['bThrottleCPUWhenNotForeground'])
O.set_properties(performance,'{"bThrottleCPUWhenNotForeground":false}')
report = {'checks':[], 'input':'Remote owning client Enhanced Input', 'map':'Lvl_SPStealthTest'}
def check(name, ok):
    report['checks'].append({'name':name,'passed':bool(ok)})
    u.log('STEALTH_TEST_CHECK '+name+': '+str(bool(ok)))
initial = [g.get_actor_location() for g in guards]
held = set()
stage = 0
elapsed = 0
last_time = u.GameplayStatics.get_time_seconds(server)
handle = None
delays = [2,1.4,1.4,.45,.3,.55,.9,.6,1.4,13,1,.6,.2,.5,.15,.5,.75,.4,2.5,.6]
keycard = next(c for c in u.GameplayStatics.get_all_actors_of_class(server,u.SPCargo) if c.get_actor_label()=='Test_Keycard')
bundle = next(c for c in u.GameplayStatics.get_all_actors_of_class(server,u.SPLootBundle) if c.get_actor_label()=="Test_LootBundle")
bag_count = len(u.GameplayStatics.get_all_actors_of_class(server,u.SPCargo))
snapshot = None

def finish():
    global handle
    held.clear()
    if handle is not None:
        u.unregister_slate_post_tick_callback(handle)
        handle = None
    O.set_properties(performance,performance_before)
    report['passed'] = not report.get('error') and all(c['passed'] for c in report['checks']) and stage == 19
    out = Path(u.Paths.project_saved_dir(),'StealthTest','pie-verification.json')
    out.write_text(json.dumps(report,indent=2),encoding='utf-8')
    u.log('STEALTH_TEST_PIE_COMPLETE: '+str(report['passed']))

def tick(_):
    global stage, elapsed, last_time, snapshot, initial
    try:
        if not all(u.SystemLibrary.is_valid(w) for w in worlds):
            raise RuntimeError('PIE ended before verification completed')
        now = u.GameplayStatics.get_time_seconds(server)
        elapsed += max(0,now-last_time)
        last_time = now
        for key in held:
            inputs.inject_input_vector_for_action(actions[key],u.Vector(0,1,0) if key=='move' else u.Vector(1,0,0),[],[])
        if elapsed < delays[stage]:
            return
        elapsed = 0
        if stage == 0:
            check('Two guards patrol using saved NavMesh',all((g.get_actor_location()-p).length()>50 for g,p in zip(guards,initial)))
            check('Both players spawn grounded',all(p.character_movement.is_moving_on_ground() and 95<p.get_actor_location().z<102 for p in players(server)))
            freeze(guards[0],(300,-800,98),180); freeze(guards[1],(2250,800,98),0)
            place(a,(-200,-800,98));place(b,(-2600,400,98))
        elif stage == 1:
            check('Visible innocent player in public is ignored',guards[0].can_see_player(a) and all(not g.get_editor_property('target_player') for g in guards))
            freeze(guards[0],(1100,0,98),0);place(a,(1700,0,98))
        elif stage == 2:
            check('Tall cover blocks direct sight and identification',not guards[0].can_see_player(a) and all(not g.get_editor_property('target_player') for g in guards))
            freeze(guards[0],(900,-800,98),0);place(a,(1250,-800,98))
        elif stage == 3:
            check('Partial sight creates suspicion without identity',guards[0].get_editor_property('suspicion_progress')>0 and not guards[0].get_editor_property('target_player'))
            place(a,(900,-300,98))
        elif stage == 4:
            check('Breaking sight clears confirmation',guards[0].get_editor_property('suspicion_progress')==0)
            place(a,(1250,-800,98))
        elif stage == 5:
            check('A new partial glimpse does not accumulate old time',not guards[0].get_editor_property('target_player'))
        elif stage == 6:
            check('Confirmed A shared with both guards on both worlds',all(g.get_editor_property('target_player') and pid(g.get_editor_property('target_player'))==pid(a) for g in all_guards()))
            check('Innocent B is never selected',all(g.get_editor_property('target_player')!=b for g in guards))
            snapshot = guards[0].get_editor_property('last_seen_location')
            place(a,(-2200,1100,98))
        elif stage == 7:
            check('Hidden player location is not tracked',(guards[0].get_editor_property('last_seen_location')-snapshot).length()<5)
            initial = [g.get_actor_location() for g in guards]
            for g in guards:
                g.set_editor_property('chase_speed',340)
                g.character_movement.set_editor_property('orient_rotation_to_movement',True)
        elif stage == 8:
            check('Both guards move to the reported position',all((g.get_actor_location()-p).length()>30 for g,p in zip(guards,initial)))
        elif stage == 9:
            check('Lost target returns to patrol',all(not g.get_editor_property('target_player') and g.get_editor_property('guard_state')==u.SPGuardState.PATROL for g in guards))
            place(a,(-2400,300,98));held.update(['move','crouch'])
        elif stage == 10:
            check('Remote crouch replicates to server',all(p.get_editor_property('is_crouched') for p in copies(a)))
            report['crouched_speed'] = local.get_velocity().length()
            check('Existing crouch speed is 160',145<report['crouched_speed']<165)
            check('Crouch pose active on both worlds',all(p.mesh.get_post_process_instance().get_editor_property('crouch_alpha')>.9 for p in copies(a)))
            held.clear()
        elif stage == 11:
            check('Release restores standing on both worlds',all(not p.get_editor_property('is_crouched') for p in copies(a)))
            # Let the camera manager update after teleport before injecting Started.
            place(a,(-2050,-850,98),0,-8.7)
        elif stage == 12:
            held.add('interact')
        elif stage == 13:
            held.clear()
            check('Remote E picks up existing cargo on server',keycard.get_owner()==a)
            check('Cargo owner replicated to client inventory',all(p.get_editor_property('inventory').find_item_of_type(keycard.get_editor_property('item_type')) is not None for p in copies(a)))
            held.add('drop')
        elif stage == 14:
            held.clear()
        elif stage == 15:
            check('G release drops the selected cargo',not keycard.get_owner())
            place(a,(-1360,-630,98),-90,-13.4);held.add('interact')
        elif stage == 16:
            check('E hold progress is visible on owning client',0<local.get_hold_progress()<1)
            held.clear()
        elif stage == 17:
            check('Releasing E cancels packing',u.SystemLibrary.is_valid(bundle) and bundle.get_component_by_class(u.SPInteractableComponent).get_progress()==0)
            held.add('interact')
        elif stage == 18:
            held.clear()
        elif stage == 19:
            check('Completed E hold destroys bundle once',not u.SystemLibrary.is_valid(bundle))
            check('Packing creates exactly one bag per world',all(len(u.GameplayStatics.get_all_actors_of_class(w,u.SPCargo))==bag_count+1 for w in worlds))
            finish();return
        stage += 1
    except Exception as exc:
        report['error'] = repr(exc)
        finish()

handle = u.register_slate_post_tick_callback(tick)
u.log('STEALTH_TEST_PIE_STARTED')
