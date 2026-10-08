"""Fresh two-player listen PIE, saved Lvl_SPStealthTest, laser stage.

Only PIE actors/settings are changed. Both owning players use existing Enhanced
Input for crouch, movement and jump. One server-authoritative launch at a capped
frame rate checks a complete thin-beam crossing between capsule samples.
Writes Saved/StealthLaser/laser-pie.json. Stop PIE after the test completes.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O


def run():
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds) == 2, 'Use a fresh two-player listen-server PIE.'
    server = next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    client = next(w for w in worlds if w != server)
    actors = lambda w, t: u.GameplayStatics.get_all_actors_of_class(w, t)
    prop = lambda o, n: o.get_editor_property(n)
    players = lambda w: [p for p in actors(w, u.SPPlayerCharacter) if p.is_player_controlled()]
    a = next(p for p in players(server) if not p.is_locally_controlled())
    b = next(p for p in players(server) if p.is_locally_controlled())
    pid = lambda p: prop(prop(p, 'player_state'), 'player_id')
    copies = lambda p: [q for w in worlds for q in players(w) if pid(q) == pid(p)]
    owner = lambda p: next(q for q in copies(p) if q.is_locally_controlled())
    subsystem = next(x for x in u.ObjectIterator(u.SPGuardAlertSubsystem) if x.get_outer() == server)
    devices = {x.get_actor_label(): x for x in actors(server, u.SPLaserSecurityDevice)}
    crouch, jump, timed = (devices['Laser_' + n] for n in ['Crouch', 'Jump', 'Wait'])
    guard = next(g for g in actors(server, u.SPGuardCharacter) if g.get_actor_label() == 'Laser_ResponseGuard')
    remote = lambda x: next(q for q in actors(client, x.get_class()) if q.get_actor_label() == x.get_actor_label())
    inputs = {pid(p): next(x for x in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
        if not x.get_name().startswith('Default__') and
        u.GameplayStatics.get_player_controller(x, 0) == owner(p).get_controller()) for p in [a, b]}
    held = {pid(p): {} for p in [a, b]}
    report = {'checks': [], 'crossings': [], 'phase_samples': [],
        'source': 'Saved three laser lanes, actual host/participant Enhanced Input, server movement and replicated state'}
    perf = u.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
    perf_before = O.get_properties(perf, ['bThrottleCPUWhenNotForeground'])
    O.set_properties(perf, '{"bThrottleCPUWhenNotForeground":false}')
    fps_before = u.SystemLibrary.get_console_variable_int_value('t.MaxFPS')
    handle = None
    capture = None

    def check(name, ok):
        report['checks'].append({'name': name, 'passed': bool(ok)})
        u.log('LASER_PIE_CHECK ' + name + ': ' + str(bool(ok)))

    def security(identified_a=False, identified_b=False, alarm=False):
        return all(prop(q, 'player_state').get_component_by_class(u.SPStealthPlayerStateComponent).is_identified() == expected
            for p, expected in [(a, identified_a), (b, identified_b)] for q in copies(p)) and all(
            u.GameplayStatics.get_game_state(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm == alarm for w in worlds)

    def move(p, xyz, yaw=90, pitch=0):
        for q in copies(p):
            q.set_actor_location(u.Vector(*xyz), False, True)
            q.character_movement.stop_movement_immediately()
            if q.get_controller(): q.get_controller().set_control_rotation(u.Rotator(yaw=yaw, pitch=pitch))

    def key(p, name, on=True, value=None):
        if on: held[pid(p)][name] = value or u.Vector(1, 0, 0)
        else: held[pid(p)].pop(name, None)

    def clear():
        for h in held.values(): h.clear()

    def reset():
        clear()
        move(a, (-2800, 1200, 98)); move(b, (-2600, 1200, 98))
        subsystem.restart_stage()
        for d in devices.values(): d.reset_device()
        for g in actors(server, u.SPGuardCharacter):
            g.set_editor_property('hear_footsteps', False)
            # The saved laser guard remains available for ordinary sight policy.
            if g != guard: g.set_editor_property('sight_distance', 1)

    def count(device): return prop(device, 'contact_count')
    def last(device): return prop(device, 'last_incident')
    def active(device): return device.is_beam_active()
    def phase(device): return str(prop(device, 'phase'))
    def synced(device):
        other = remote(device)
        return phase(device) == phase(other) and active(device) == active(other) and count(device) == count(other)

    def set_phase(seconds):
        timed.set_editor_property('initial_phase_seconds', seconds)
        timed.reset_device()

    def until(condition, timeout):
        elapsed = 0.0
        while not condition() and elapsed < timeout:
            yield .1
            elapsed += .1
        return condition()

    def crossing(p, device, name):
        nonlocal capture
        capture = {'name': name, 'player': 'participant' if p == a else 'host',
            'device': device.get_actor_label(), 'pawn': p, 'start_contacts': count(device), 'samples': []}

    def stop_crossing():
        nonlocal capture
        entry = dict(capture)
        entry.pop('pawn')
        report['crossings'].append(entry)
        capture = None
        return entry

    def steps():
        reset(); yield .5
        capsule = a.get_component_by_class(u.CapsuleComponent)
        movement = a.character_movement
        report['runtime_dimensions'] = {'capsule_radius': capsule.get_scaled_capsule_radius(),
            'standing_half_height': capsule.get_scaled_capsule_half_height(),
            'crouched_half_height': prop(movement, 'crouched_half_height'),
            'walk_speed': prop(movement, 'max_walk_speed'), 'crouch_speed': prop(movement, 'max_walk_speed_crouched'),
            'sprint_speed': prop(movement, 'sprint_speed'), 'jump_z': prop(movement, 'jump_z_velocity'),
            'gravity_scale': prop(movement, 'gravity_scale')}
        check('Three saved devices and independent visual/detection components',len(devices) == 3 and all(
            prop(d, 'beam_visual') != prop(d, 'detection_volume') for d in devices.values()))
        check('Safe waiting pads are outside beam and restricted area',all(
            not u.SPRestrictedArea.find_at_location(server, u.Vector(x, 1450, 98)) for x in [-2100, 0, 2100]))

        for p in [a, b]:
            label = 'Participant' if p == a else 'Host'
            reset(); move(p, (-2100, 1500, 98)); yield .4
            key(p, 'crouch'); yield .35
            check(label + ' uses actual replicated crouch capsule',all(
                q.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height() < 60 for q in copies(p)))
            crossing(p, crouch, label + ' crouched crossing')
            key(p, 'move', value=u.Vector(0, 1, 0)); yield 3.5
            key(p, 'move', False); yield .2
            trace = stop_crossing()
            check(label + ' crouches completely under 145 cm active beam',p.get_actor_location().y > 1900 and count(crouch) == 0 and security())
            check(label + ' crouch crossing result matches both peers',synced(crouch))
            key(p, 'crouch', False); yield .3
            reset(); move(p, (0, 1500, 98)); yield .4
            crossing(p, jump, label + ' running jump crossing')
            key(p, 'move', value=u.Vector(0, 1, 0)); yield .45
            key(p, 'jump'); yield .12; key(p, 'jump', False); yield .78
            key(p, 'move', False); yield .25
            trace = stop_crossing()
            check(label + ' jumps completely over 40 cm active beam',p.get_actor_location().y > 1900 and count(jump) == 0 and
                any(sample['z'] > 150 for sample in trace['samples']) and security())
            check(label + ' jump crossing result matches both peers',synced(jump))

        reset(); move(a, (-2100, 1500, 98)); yield .4
        key(a, 'move', value=u.Vector(0, 1, 0)); yield 1.35; key(a, 'move', False); yield .4
        first = last(crouch)
        check('Standing passage touches high beam but no identity or alarm',count(crouch) == 1 and first.kind == u.SPStealthIncident.LASER_CONTACT and security())
        check('Anonymous receipt contains only fixed sensor response location',not first.player and
            (first.context.location - crouch.get_actor_location()).length() < 1 and first.dispatched_guards == 1)
        check('Contact receipt and count replicate to participant',synced(crouch) and last(remote(crouch)).context.incident_id.to_string() == first.context.incident_id.to_string())
        reset(); move(b, (0, 1500, 98)); yield .4
        key(b, 'move', value=u.Vector(0, 1, 0)); yield 1.35; key(b, 'move', False); yield .4
        check('Walking instead of jumping touches low beam',count(jump) == 1 and security() and synced(jump))

        # OFF and warning-on are safe. No retrospective report after a complete
        # OFF crossing when the same beam later turns ON.
        for p in [a, b]:
            label = 'Participant' if p == a else 'Host'
            reset(); set_phase(4.1); move(p, (2100, 1450, 98)); yield .35
            check(label + ' sees the same OFF phase before passage',not active(timed) and synced(timed))
            crossing(p, timed, label + ' OFF crossing')
            key(p, 'move', value=u.Vector(0, 1, 0)); yield 1.65; key(p, 'move', False); yield .25
            stop_crossing()
            check(label + ' walks through OFF beam safely',p.get_actor_location().y > 1900 and count(timed) == 0 and security())
            yield from until(lambda: active(timed), 5); yield .35
            check(label + ' earlier OFF crossing is never reported later',count(timed) == 0 and security() and synced(timed))

        reset(); set_phase(4.1); move(a, (2100, 1800, 98)); yield .5
        check('Standing inside OFF sensor is safe',count(timed) == 0 and not active(timed) and security())
        yield from until(lambda: active(timed), 5); yield .4
        check('Beam activation detects stationary inside player',count(timed) == 1 and security() and synced(timed))
        report['activation_contact_type'] = str(prop(timed, 'last_contact_type'))

        # Continuous occupancy uses the same event ID. Leaving and re-entering
        # creates another ID, while the device-wide minimum call time still holds.
        reset(); move(a, (-2100, 1800, 98)); yield .5
        original = last(crouch).context.incident_id.to_string()
        yield 2.1
        same = last(crouch)
        check('Continuous contact keeps the same anonymous ID',count(crouch) == 1 and same.context.incident_id.to_string() == original and
            same.result == u.SPStealthResult.DUPLICATE)
        move(a, (-2100, 1620, 98)); yield .35; move(a, (-2100, 1800, 98)); yield .35
        check('Re-entry is recorded separately while call interval applies',count(crouch) == 2)
        yield 2.1
        check('Re-entry submits a new anonymous ID after cooldown',last(crouch).context.incident_id.to_string() != original and security())
        move(b, (0, 1800, 98)); yield .4
        check('Another sensor has its own contact ID and cooldown',count(jump) == 1 and
            last(jump).context.incident_id.to_string() != last(crouch).context.incident_id.to_string())
        times = [r.server_time for r in subsystem.get_recent_incidents() if r.kind == u.SPStealthIncident.LASER_CONTACT and
            (r.context.location - crouch.get_actor_location()).length() < 1]
        check('One device never submits more often than two seconds',len(times) >= 2 and all(y - x >= 1.99 for x, y in zip(times, times[1:])))

        # A true server movement launch at 10fps travels far beyond capsule
        # diameter in one sample. It is not a SetActorLocation crossing.
        reset(); move(b, (-2100, 1520, 98)); yield .4
        u.SystemLibrary.execute_console_command(server, 't.MaxFPS 10'); yield .4
        crossing(b, crouch, 'Host actual high-speed launch at 10 fps')
        b.launch_character(u.Vector(0, 9000, 40), True, True)
        yield .35
        key(b, 'move', False)
        b.character_movement.stop_movement_immediately()
        trace = stop_crossing()
        u.SystemLibrary.execute_console_command(server, 't.MaxFPS ' + str(fps_before)); yield .4
        samples_y = [1520] + [sample['y'] for sample in trace['samples']]
        skipped = any(start < 1760 and end > 1840 for start, end in zip(samples_y, samples_y[1:]))
        report['high_speed_contact_type'] = str(prop(crouch, 'last_contact_type'))
        check('Fast movement skips both thin-beam endpoint overlaps',skipped)
        check('Server swept detection catches the fast crossing',count(crouch) >= 1 and security() and synced(crouch))

        # Replace/remove every visible mesh, then make another physical passage.
        reset()
        for w in worlds:
            d = next(x for x in actors(w, u.SPLaserSecurityDevice) if x.get_actor_label() == 'Laser_Crouch')
            prop(d, 'beam_visual').set_static_mesh(None)
            prop(d, 'emitter_visual').set_static_mesh(u.load_asset('/Engine/BasicShapes/Sphere'))
            prop(d, 'receiver_visual').set_static_mesh(None)
        move(a, (-2100, 1500, 98)); yield .4
        key(a, 'move', value=u.Vector(0, 1, 0)); yield 1.35; key(a, 'move', False); yield .4
        check('Changing/removing visual meshes leaves contact detection intact',count(crouch) == 1 and security() and synced(crouch))

        # Independently verify the real anonymous response lifecycle, then the
        # existing crime witness pathway without passing a player to the laser.
        reset()
        guard.get_controller().stop_movement()
        guard.set_actor_location(u.Vector(-2850, 1450, 98), False, True)
        guard.set_editor_property('sight_distance', 1200)
        guard.set_editor_property('search_time', 3)
        move(a, (-2100, 1800, 98)); yield .4
        incident = last(crouch).context.incident_id.to_string()
        move(a, (-2100, 1900, 98)); yield .5
        check('Laser sends guard to sensor snapshot with no target',prop(guard, 'investigating_anonymous_incident') and
            not prop(guard, 'target_player') and (prop(guard, 'investigation_location') - crouch.get_actor_location()).length() < 1 and security())
        yield from until(lambda: prop(guard, 'guard_state') == u.SPGuardState.SCENE_SEARCHING, 12); yield .35
        check('Guard reaches the sensor and searches on both peers',all(prop(g, 'guard_state') == u.SPGuardState.SCENE_SEARCHING and
            not prop(g, 'target_player') for g in [guard, remote(guard)]) and security())
        yield from until(lambda: prop(guard, 'last_investigation').incident_id.to_string() == incident, 5); yield .3
        check('Anonymous sensor search completes and returns to patrol',all(prop(g, 'last_investigation').result == u.SPGuardInvestigationResult.COMPLETED and
            prop(g, 'guard_state') == u.SPGuardState.PATROL for g in [guard, remote(guard)]) and security())
        # New sensor event, then an actual marked E task next to its search scene.
        move(a, (-2100, 1800, 98)); yield .4; move(a, (-2100, 1900, 98)); yield .4
        guard.set_editor_property('patrol_speed', 0); guard.set_editor_property('chase_speed', 0)
        guard.set_editor_property('stuck_timeout', 100)
        guard.set_actor_rotation(u.Rotator(yaw=35), True)
        guard.character_movement.set_editor_property('orient_rotation_to_movement', False)
        for w in worlds:
            terminal = next(x for x in actors(w, u.SPStealthTestConsole) if x.get_actor_label() == 'Crime_Terminal')
            terminal.set_actor_location(u.Vector(-2100, 2110, 120), False, True)
        move(a, (-2100, 1925, 98), 90, -12.8); yield .4
        check('Normal player beside sensor search is visible and still hidden',guard.can_see_player(a) and security())
        key(a, 'interact'); yield 1.35; key(a, 'interact', False); yield .35
        check('Only later directly witnessed crime identifies A',security(True, False, True) and prop(guard, 'target_player') == a and
            any(r.kind == u.SPStealthIncident.SUSTAINED_CRIME_CONFIRMED for r in subsystem.get_recent_incidents()))

        clear(); move(a, (-2800, 1200, 98)); move(b, (-2600, 1200, 98))
        subsystem.end_stage(); yield .4
        check('Stage end disables reporting and clears pending work',security() and all(not active(d) for d in devices.values()))
        subsystem.restart_stage(); yield .5
        check('Stage restart resets device counters and security on both peers',security() and all(count(d) == 0 and synced(d) for d in devices.values()))

    iterator = steps()
    remaining = next(iterator)
    last_time = u.GameplayStatics.get_time_seconds(server)

    def finish(error=None):
        nonlocal handle
        clear()
        if handle is not None: u.unregister_slate_post_tick_callback(handle); handle = None
        O.set_properties(perf, perf_before)
        u.SystemLibrary.execute_console_command(server, 't.MaxFPS ' + str(fps_before))
        if error: report['error'] = repr(error)
        report['passed'] = not error and all(c['passed'] for c in report['checks'])
        path = Path(u.Paths.project_saved_dir(), 'StealthLaser', 'laser-pie.json')
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2), encoding='utf-8')
        u.log('LASER_PIE_COMPLETE ' + str(report['passed']))

    def tick(_):
        nonlocal remaining, last_time
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds): raise RuntimeError('PIE ended')
            for p in [a, b]:
                for name, value in held[pid(p)].items():
                    inputs[pid(p)].inject_input_vector_for_action(prop(owner(p), name + '_action'), value, [], [])
            now = u.GameplayStatics.get_time_seconds(server)
            if capture is not None:
                p = capture['pawn']; location = p.get_actor_location()
                capture['samples'].append({'time': now, 'x': location.x, 'y': location.y, 'z': location.z,
                    'half_height': p.get_component_by_class(u.CapsuleComponent).get_scaled_capsule_half_height()})
            state = phase(timed)
            if state not in report['phase_samples']: report['phase_samples'].append(state)
            remaining -= max(0, now - last_time); last_time = now
            if remaining <= 0: remaining = next(iterator)
        except StopIteration: finish()
        except Exception as exc: finish(exc)

    handle = u.register_slate_post_tick_callback(tick)
    u.log('LASER_PIE_STARTED')


run()
