"""작성자 : 임진혁

P0 맵의 Listen Server + Client PIE에서 실행하는 입력·복제 점검.
에디터 Cmd: py "<project>/Tools/Prototype01/verify_pie.py"
약 8초 동안 로컬 Enhanced Input에 이동·시점·점프 액션을 주입한다.
키보드/마우스 하드웨어 검사는 별도이며 에셋은 저장하지 않는다.
"""
import json
import traceback
from pathlib import Path
import unreal as u

worlds = list(u.EditorLevelLibrary.get_pie_worlds(False))
assert len(worlds) == 2, 'Start Listen Server + one Client first'
assert all('Lvl_SPPrototype01' in w.get_path_name() for w in worlds)
local_pawns = [p for w in worlds for p in u.GameplayStatics.get_all_actors_of_class(w, u.Character) if p.is_locally_controlled()]
assert len(local_pawns) == 2
subsystems = []
for subsystem in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem):
    if subsystem.get_world() in worlds:
        subsystems.append(subsystem)
assert len(subsystems) == 2, 'Both local Enhanced Input subsystems required'
actions = {name: u.load_asset('/Game/SpacePirate/Player/Input/IA_SP' + name) for name in ['Move', 'Look', 'Jump']}
assert all(actions.values())
output = Path(u.Paths.project_saved_dir()) / 'Prototype01/pie-input.json'
report = {'method': 'Enhanced Input action injection; not OS keyboard/mouse', 'samples': [], 'passed': False}
elapsed = 0.0
next_sample = 0.0

def snapshot():
    rows = []
    for world in worlds:
        gs = u.GameplayStatics.get_game_state(world)
        row = {'world': world.get_path_name(), 'authority': gs.has_authority(), 'game_state': gs.get_class().get_path_name(), 'players': [], 'background': [], 'train': []}
        row['motion'] = gs.get_editor_property('MotionState').export_text()
        row['sky'] = gs.get_editor_property('SkyTransition').export_text()
        for pawn in u.GameplayStatics.get_all_actors_of_class(world, u.Character):
            ps = pawn.get_editor_property('player_state')
            row['players'].append({'id': ps.get_editor_property('player_id'), 'local': pawn.is_locally_controlled(), 'position': pawn.get_actor_location().to_tuple(), 'yaw': pawn.get_control_rotation().yaw, 'velocity': pawn.get_velocity().to_tuple(), 'movement_class': pawn.get_editor_property('character_movement').get_class().get_path_name(), 'mode': str(pawn.get_editor_property('character_movement').get_editor_property('movement_mode'))})
        for actor in u.GameplayStatics.get_all_actors_of_class(world, u.Actor):
            name = actor.get_class().get_name()
            if name == 'BP_SPTrainEnvironmentManager_C':
                row['background'].append({'initialized': actor.get_editor_property('bInitialized'), 'failed': actor.get_editor_property('bFailed'), 'speed': actor.get_editor_property('CurrentTrainSpeed'), 'distance': actor.get_editor_property('TravelDistance')})
            if name in ['BP_SPTrainConsist_C', 'BP_SPFreightCar_Long_C']:
                row['train'].append({'class': name, 'position': actor.get_actor_location().to_tuple()})
        rows.append(row)
    return {'elapsed': elapsed, 'worlds': rows}

def finish():
    u.unregister_slate_post_tick_callback(handle)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    u.log('P0_PIE_INPUT_RESULT ' + str(report['passed']))

def tick(delta):
    global elapsed, next_sample
    try:
        elapsed += delta
        if len(u.EditorLevelLibrary.get_pie_worlds(False)) != 2:
            raise RuntimeError('PIE stopped before verification finished')
        # 양쪽 로컬 Pawn의 실제 입력 바인딩을 거쳐 네트워크 이동을 발생시킨다.
        for subsystem in subsystems:
            if 1.0 <= elapsed < 1.8:
                subsystem.inject_input_vector_for_action(actions['Move'], u.Vector(0, 1, 0), [], [])
            if 2.5 <= elapsed < 3.0:
                subsystem.inject_input_vector_for_action(actions['Look'], u.Vector(60 * delta, 0, 0), [], [])
            if 3.5 <= elapsed < 3.6:
                subsystem.inject_input_vector_for_action(actions['Jump'], u.Vector(1, 0, 0), [], [])
        if elapsed >= next_sample:
            report['samples'].append(snapshot())
            next_sample = elapsed + 0.15
        if elapsed < 8:
            return
        first, last = report['samples'][0]['worlds'], report['samples'][-1]['worlds']
        assert all(len(w['players']) == 2 and sum(p['local'] for p in w['players']) == 1 for w in last)
        for start, end in zip(first, last):
            assert start['train'] == end['train'], 'Train terrain moved'
            # 기존 FreightPlanet 프리셋에는 근경과 원경 매니저가 각각 하나씩 있다.
            assert len(end['background']) == 2
            for before, after in zip(start['background'], end['background']):
                assert after['initialized'] and not after['failed'], 'Background initialization failed'
                assert after['distance'] > before['distance'], 'Background did not advance'
            local_start = next(p for p in start['players'] if p['local'])
            local_end = next(p for p in end['players'] if p['local'])
            assert local_end['position'][0] - local_start['position'][0] > 100, 'Move action failed'
            assert abs(local_end['yaw'] - local_start['yaw']) > 10, 'Look action failed'
            heights = [p['position'][2] for sample in report['samples'] for w in sample['worlds'] if w['world'] == end['world'] for p in w['players'] if p['local']]
            assert max(heights) - min(heights) > 20, 'Jump action failed'
            assert 300 < local_end['position'][2] < 320, 'Pawn did not land on interior floor'
        server = next(w for w in last if w['authority'])
        client = next(w for w in last if not w['authority'])
        assert server['motion'] == client['motion'], 'Train state differs'
        assert server['sky'] == client['sky'], 'Sky state differs'
        for p in server['players']:
            remote = next(q for q in client['players'] if q['id'] == p['id'])
            assert max(abs(a-b) for a,b in zip(p['position'], remote['position'])) < 5, 'Settled positions differ'
        report['passed'] = True
        finish()
    except Exception:
        report['error'] = traceback.format_exc()
        finish()

handle = u.register_slate_post_tick_callback(tick)
u.log('P0_PIE_INPUT_STARTED')
