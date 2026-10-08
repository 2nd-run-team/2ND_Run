"""Read-only validation of the OPEN, saved Lvl_SPStealthTest (not PIE).

Run after Build Paths and Save Current Level, preferably after reopening the map.
Produces Saved/StealthTest/map-validation.json; never saves assets.
"""
import json
from pathlib import Path
import unreal as u

MAP = '/Game/SpacePirate/Maps/Lvl_SPStealthTest'
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == MAP, 'Open Lvl_SPStealthTest first.'
assert not u.EditorLevelLibrary.get_pie_worlds(False), 'Stop PIE first.'
actors = list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())
report = {'map': MAP, 'engine': u.SystemLibrary.get_engine_version(), 'checks': [], 'paths': []}

def check(name, value):
    report['checks'].append({'name': name, 'passed': bool(value)})

def named(label):
    matches = [a for a in actors if a.get_actor_label() == label]
    assert len(matches) == 1, label
    return matches[0]


# Reopen the editor before running this check: the previous live CDO can mask
# an unsaved Blueprint default even after reloading just the map.
font = u.load_asset('/Game/SpacePirate/Stealth/Prototype/UI/F_SPStealthKorean')
text_material = u.load_asset('/Game/SpacePirate/Stealth/Prototype/Materials/M_SPStealthKoreanText')
for bp_name in ['BP_SPGuardCharacter', 'BP_SPLaserSecurityDevice', 'BP_SPStealthTestConsole']:
    bp = u.load_asset('/Game/SpacePirate/Stealth/Blueprints/' + bp_name)
    component = u.get_default_object(bp.generated_class()).get_component_by_class(u.TextRenderComponent)
    check(bp_name + ' persisted Korean font and material', font and text_material and component
          and component.get_editor_property('font') == font
          and component.get_editor_property('text_material') == text_material)
text_components = [c for a in actors for c in a.get_components_by_class(u.TextRenderComponent)]
check('All placed signs and actor indicators use Korean text assets', len(text_components) == 46 and font and text_material
      and all(c.get_editor_property('font') == font and c.get_editor_property('text_material') == text_material
              for c in text_components))

route = named('Restricted_PatrolRoute')
area = named('Restricted_Area_Main')
guards = [a for a in actors if isinstance(a, u.SPGuardCharacter) and str(a.get_editor_property("alert_group"))=="StealthTest_Baseline"]
starts = [a for a in actors if isinstance(a, u.PlayerStart)]
check('Existing team GameMode', world.get_world_settings().get_editor_property('default_game_mode').get_path_name()
      == '/Game/SpacePirate/Core/Blueprints/BP_SPGameMode.BP_SPGameMode_C')
check('Two guards reference the same route', len(guards) == 2 and all(g.get_editor_property('patrol_route') == route for g in guards))
check('Four safe starts outside restricted box', len(starts) == 4 and all(not area.contains_location(s.get_actor_location()) for s in starts))
check('Restricted box includes center, excludes public area', area.contains_location(u.Vector(1400,0,96)) and not area.contains_location(u.Vector(-1400,0,96)))
check('Guard animation references present', all(g.mesh.get_editor_property('anim_class') for g in guards))
check('Visible zone panels do not collide', all(named(n).static_mesh_component.get_collision_enabled() == u.CollisionEnabled.NO_COLLISION
      for n in ['Public_VisualFloor','Restricted_VisualFloor','Restricted_Threshold']))
check('Tall cover blocks visibility', named('Cover_Tall').static_mesh_component.get_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY) == u.CollisionResponseType.ECR_BLOCK)
for i in range(4):
    start, end = route.get_patrol_location(i), route.get_patrol_location((i+1)%4)
    path = u.NavigationSystemV1.find_path_to_location_synchronously(world, start, end)
    ok = path is not None and path.is_valid() and not path.is_partial()
    check('Complete patrol path ' + str(i), ok)
    report['paths'].append({'from': start.to_tuple(), 'to': end.to_tuple(), 'complete': ok})
for label, point in [('Public to restricted',u.Vector(550,-800,0)),('Reserved CCTV',u.Vector(-2100,2500,0)),
                     ('Reserved reports',u.Vector(0,2500,0)),('Reserved API',u.Vector(2100,2500,0))]:
    path = u.NavigationSystemV1.find_path_to_location_synchronously(world, u.Vector(-2500,-450,0), point)
    check(label + ' reachable', path is not None and path.is_valid() and not path.is_partial())
check('Existing interaction actors', isinstance(named('Test_Keycard'),u.SPCargo)
      and isinstance(named('Test_LootBag'),u.SPCargo) and isinstance(named('Test_LootBundle'),u.SPLootBundle))
terminals = [a for a in actors if isinstance(a,u.SPStealthTestConsole)]
directors = [a for a in actors if isinstance(a,u.SPStealthTestDirector)]
check('Saved incident test director has editable status widget reference',len(directors)==1 and directors[0].get_editor_property('status_widget_class'))
check('Twenty-one test terminals cover all nine event kinds',len(terminals)==21 and len({a.get_editor_property('incident_kind') for a in terminals
      if a.get_editor_property('command')==u.SPStealthTestCommand.SUBMIT_INCIDENT})==9)
check('Terminal visuals do not own interaction collision',all(a.get_editor_property('visual').get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
      and a.get_editor_property('interaction_volume').get_collision_response_to_channel(u.CollisionChannel.ECC_VISIBILITY)==u.CollisionResponseType.ECR_BLOCK for a in terminals))
check('Continuous and report completion use editable server interaction duration',named('Incident_Sustained').get_editor_property('interactable').get_editor_property('hold_duration')==1
      and named('Incident_DirectReport').get_editor_property('interactable').get_editor_property('hold_duration')==2)
check('Guard identity scope configured separately from dispatch group',all(str(g.get_editor_property('identity_scope'))=='StageSecurity'
      and str(g.get_editor_property('alert_group'))=='StealthTest_Baseline' for g in guards))
scene=named('Investigation_Point_Public').get_actor_location()
unreachable=named('Investigation_Point_Unreachable').get_actor_location()
path=u.NavigationSystemV1.find_path_to_location_synchronously(world,u.Vector(550,-800,0),scene)
check('Anonymous public scene is reachable and outside restricted area',path and path.is_valid() and not path.is_partial() and not area.contains_location(scene))
projected=u.NavigationSystemV1.project_point_to_navigation(world,unreachable,None,None,u.Vector(80,80,160))
check('Elevated unreachable scene is outside bounded NavMesh projection',projected is None or (projected-unreachable).length()>160)
check('Investigation marker visuals are collision-free',all(named('Investigation_Visual_'+x).static_mesh_component.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION for x in ['Public','Unreachable']))
check('Investigation terminals use snapshot, radius and one responder',all(named('Investigation_'+x).get_editor_property('incident_location')==named('Investigation_Point_'+x).get_actor_location() and named('Investigation_'+x).get_editor_property('response_radius')==5000 and named('Investigation_'+x).get_editor_property('max_responders')==1 for x in ['Public','Unreachable']))
check('Restricted area is independent of patrol',isinstance(area,u.SPRestrictedArea) and area.get_attach_parent_actor() is None and len([a for a in actors if isinstance(a,u.SPRestrictedArea)])==1)
check('Every saved guard has MVP footsteps disabled',all(not a.get_editor_property('hear_footsteps') for a in actors if isinstance(a,u.SPGuardCharacter)))
crime_buttons=[a for a in terminals if a.get_editor_property('command')==u.SPStealthTestCommand.OBSERVED_CRIME]
check('Five opt-in crime categories use server interaction hooks',len(crime_buttons)==5 and len({a.get_editor_property('interactable').get_editor_property('crime_kind') for a in crime_buttons})==5)
check('Real public-area packing bundle opts into continuous crime',named('Crime_PublicLootBundle').get_editor_property('interactable').get_editor_property('crime_kind')==u.SPCrimeKind.LOOT_PACKING and not area.contains_location(named('Crime_PublicLootBundle').get_actor_location()))
check('Work noise button is anonymous and has separate dispatch group',named('Crime_WorkNoise').get_editor_property('interactable').get_editor_property('crime_kind')==u.SPCrimeKind.NONE and str(named('Crime_WorkNoise').get_editor_property('dispatch_group'))=='StealthTest_Crime')
check('Crime station observer is independent from baseline patrol',isinstance(named('Crime_ObserverGuard'),u.SPGuardCharacter) and named('Crime_ObserverGuard').get_editor_property('patrol_route') is None)
registry = u.AssetRegistryHelpers.get_asset_registry()
options = u.AssetRegistryDependencyOptions(True, True, False, False, False)
pending, visited = [MAP], set()
while pending:
    path = pending.pop()
    if path in visited:
        continue
    visited.add(path)
    pending.extend(str(p) for p in registry.get_dependencies(path, options) if str(p).startswith('/Game/'))
forbidden = [p for p in visited if p.startswith(('/Game/SpacePirate/Train/', '/Game/Train/', '/Game/Planet_Project/', '/Game/StarfieldFree/'))]
check('No train or external background dependencies', not forbidden)
report['forbidden_dependencies'] = forbidden
report['game_dependencies'] = sorted(visited)
report['actor_count'] = len(actors)
report['passed'] = all(c['passed'] for c in report['checks'])
output = Path(u.Paths.project_saved_dir(), 'StealthTest', 'map-validation.json')
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2), encoding='utf-8')
u.log('STEALTH_TEST_MAP_VALIDATION: ' + str(report['passed']))
assert report['passed'], str([c for c in report['checks'] if not c['passed']])
