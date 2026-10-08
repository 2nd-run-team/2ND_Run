"""Fresh Lvl_SPStealthTest, two-player listen PIE: health/security/help integration.

Uses real owning-client E for packing and server health/stage APIs. The separate
PlayerStatus suite verifies the full four-second rescue input. Only PIE objects
are changed; output is Saved/StealthMerge/status-integration.json.
"""
import json
import time
import traceback
from pathlib import Path

import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O


def run():
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds) == 2 and all('Lvl_SPStealthTest' in w.get_path_name() for w in worlds)
    gs = lambda w: u.GameplayStatics.get_game_state(w)
    server = next(w for w in worlds if gs(w).has_authority())
    actors = lambda w, cls: u.GameplayStatics.get_all_actors_of_class(w, cls)
    players = lambda w: [p for p in actors(w, u.SPPlayerCharacter) if p.is_player_controlled()]
    a = next(p for p in players(server) if not p.is_locally_controlled())
    b = next(p for p in players(server) if p.is_locally_controlled())
    ps = lambda p: p.get_editor_property('player_state')
    pid = lambda p: ps(p).get_editor_property('player_id')
    copies = lambda p: [c for w in worlds for c in players(w) if pid(c) == pid(p)]
    owner = lambda p: next(c for c in copies(p) if c.is_locally_controlled())
    activity = lambda p: p.get_component_by_class(u.SPStealthActivityComponent)
    identified = lambda p: ps(p).get_component_by_class(u.SPStealthPlayerStateComponent).is_identified()
    alarm = lambda w: gs(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state()
    s = next(x for x in u.ObjectIterator(u.SPGuardAlertSubsystem) if x.get_outer() == server)
    gm = u.GameplayStatics.get_game_mode(server)
    bundle = next(x for x in actors(server, u.SPLootBundle) if x.get_actor_label() == 'Crime_PublicLootBundle')
    work = bundle.get_component_by_class(u.SPInteractableComponent)
    for guard in actors(server, u.SPGuardCharacter):
        guard.set_editor_property('sight_distance', 1)
        guard.set_editor_property('hear_footsteps', False)
    for laser in actors(server, u.SPLaserSecurityDevice):
        laser.set_editor_property('enabled', False)
        laser.refresh_device()
    input_a = next(x for x in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
                   if not x.get_name().startswith('Default__')
                   and u.GameplayStatics.get_player_controller(x, 0) == owner(a).get_controller())
    perf = u.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
    before = O.get_properties(perf, ['bThrottleCPUWhenNotForeground'])
    O.set_properties(perf, '{"bThrottleCPUWhenNotForeground":false}')
    report = {'checks': [], 'source': 'Real client E; replicated health/security; local help widget'}
    held = False
    handle = None
    started = time.monotonic()

    def check(name, value):
        report['checks'].append({'name': name, 'passed': bool(value)})
        u.log('STEALTH_STATUS_CHECK ' + name + ': ' + str(bool(value)))

    def place(p, xyz):
        for c in copies(p):
            c.set_actor_location(u.Vector(*xyz), False, True)
            c.character_movement.stop_movement_immediately()
            if c.get_controller():
                c.get_controller().set_control_rotation(u.Rotator(yaw=90, pitch=-13.4))

    def steps():
        nonlocal held
        gm.reset_for_stage()
        place(a, (-1000, 380, 98))
        place(b, (-1150, 380, 98))
        yield .8
        check('Both worlds retain SPGameState and two active players', all(
            isinstance(gs(w), u.SPGameState) and gs(w).get_active_player_count() == 2 for w in worlds))
        held = True
        yield .55
        check('Remote E packing registers crime', u.SPCrimeKind.LOOT_PACKING in activity(a).get_active_crimes()
              and work.get_progress() > 0)
        a.get_status_component().apply_damage(1000)
        held = False
        check('Down immediately cancels packing and registrations', not activity(a).get_active_crimes()
              and work.get_progress() == 0 and activity(a).is_incapacitated())
        yield .8
        check('Down restriction and health agree on host and client', all(
            c.get_status_component().is_downed() and activity(c).is_incapacitated() for c in copies(a)))
        held = True
        yield .4
        held = False
        check('Downed client cannot restart packing', not activity(a).get_active_crimes() and work.get_progress() == 0)
        s.submit_direct_incident(u.SPStealthIncident.INSTANT_CRIME_WITNESSED, ps(a), 'StageSecurity',
                                s.make_incident_context(a.get_actor_location(), 'None'))
        yield .6
        check('Only A is identified and alarm is replicated', all(identified(c) for c in copies(a))
              and not any(identified(c) for c in copies(b)) and all(alarm(w).global_alarm for w in worlds))
        check('Server revive accepts active teammate', a.get_status_component().try_revive(b))
        yield .8
        check('Revive clears down restriction everywhere', all(not c.get_status_component().is_downed()
              and not activity(c).is_incapacitated() for c in copies(a)))
        check('Revive preserves A identity and B remains hidden', all(identified(c) for c in copies(a))
              and not any(identified(c) for c in copies(b)))
        place(a, (-1000, 380, 98))
        yield .3
        held = True
        yield .55
        check('Revived remote owner can pack again', bool(activity(a).get_active_crimes()) and work.get_progress() > 0)
        held = False
        yield .4
        a.get_status_component().apply_damage(1000)
        b.get_status_component().apply_damage(1000)
        yield .6
        check('All-down operation failure is replicated', all(gs(w).is_operation_failed() for w in worlds))
        old_stage = alarm(server).stage_id.to_string()
        s.restart_stage()
        yield .6
        check('Security-only restart does not revive players or clear operation failure', all(
            gs(w).is_operation_failed() and gs(w).get_active_player_count() == 0 for w in worlds)
            and all(c.get_status_component().is_downed() and activity(c).is_incapacitated()
                    for p in (a, b) for c in copies(p)))
        check('Security-only restart clears identity and alarm', not any(identified(c) for p in (a, b) for c in copies(p))
              and all(not alarm(w).global_alarm and alarm(w).stage_id.to_string() != old_stage for w in worlds))
        s.submit_anonymous_incident(u.SPStealthIncident.VICTIM_REPORT,
                                    s.make_incident_context(a.get_actor_location(), 'None'))
        old_stage = alarm(server).stage_id.to_string()
        gm.reset_for_stage()
        yield .8
        check('Full GameMode reset restores health and clears down everywhere', all(
            c.get_status_component().get_health() == 100 and not c.get_status_component().is_downed()
            and not activity(c).is_incapacitated() for p in (a, b) for c in copies(p)))
        check('Full reset clears failure and restores active team', all(
            not gs(w).is_operation_failed() and gs(w).get_active_player_count() == 2 for w in worlds))
        check('Full reset starts matching fresh security stage', all(alarm(w).stage_active
            and not alarm(w).global_alarm and alarm(w).stage_id.to_string() == alarm(server).stage_id.to_string()
            and alarm(w).stage_id.to_string() != old_stage for w in worlds))
        for p in (owner(a), owner(b)):
            check('Player resolves renamed help BP ' + str(pid(p)), p.get_editor_property('debug_help_widget_class').get_path_name().endswith('WBP_SPDebugHelp_C'))
            p.toggle_debug_help()
        yield .3
        widgets = [w for w in u.WidgetLibrary.get_all_widgets_of_class(server, u.SPDebugHelpWidget, False)
                   if w.get_owning_player_pawn() in (owner(a), owner(b))]
        # WidgetLibrary scopes to a world, so include the participant separately.
        widgets += [w for w in u.WidgetLibrary.get_all_widgets_of_class(owner(a), u.SPDebugHelpWidget, False)
                    if w.get_owning_player_pawn() == owner(a) and w not in widgets]
        check('Both owners open independent help widgets', len(widgets) == 2 and all(p.is_debug_help_open() for p in (owner(a), owner(b))))
        check('Saved help describes complete stage reset', len(widgets) == 2 and all(any(
            '발각' in str(e.description) and '경보' in str(e.description)
            for e in w.get_editor_property('help_entries')) for w in widgets))
        for w in widgets:
            w.change_page(1)
        check('Help page movement stays in bounds', all(1 <= w.get_current_page() <= w.get_page_count() for w in widgets))
        for p in (owner(a), owner(b)):
            p.toggle_debug_help()
        check('Both owners can close help', all(not p.is_debug_help_open() for p in (owner(a), owner(b))))

    def finish(error=None):
        nonlocal handle, held
        held = False
        if handle is not None:
            u.unregister_slate_post_tick_callback(handle)
            handle = None
        O.set_properties(perf, before)
        if error:
            report['error'] = error
        report['passed'] = not error and len(report['checks']) >= 20 and all(x['passed'] for x in report['checks'])
        output = Path(u.Paths.project_saved_dir(), 'StealthMerge', 'status-integration.json')
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2), encoding='utf-8')
        u.log('STEALTH_STATUS_COMPLETE: ' + str(report['passed']))

    iterator = steps()
    remaining = 0
    last = u.GameplayStatics.get_time_seconds(server)

    def tick(_):
        nonlocal remaining, last
        try:
            if time.monotonic() - started > 120 or not all(u.SystemLibrary.is_valid(w) for w in worlds):
                raise RuntimeError('PIE ended or verification timed out')
            if held:
                input_a.inject_input_vector_for_action(owner(a).get_editor_property('interact_action'), u.Vector(1, 0, 0), [], [])
            now = u.GameplayStatics.get_time_seconds(server)
            remaining -= max(0, now - last)
            last = now
            if remaining <= 0:
                remaining = next(iterator)
        except StopIteration:
            finish()
        except Exception:
            finish(traceback.format_exc())

    handle = u.register_slate_post_tick_callback(tick)


run()
