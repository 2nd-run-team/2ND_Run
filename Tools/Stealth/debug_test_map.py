"""Toggle PIE-only debug for the test map. Does not edit or save any asset.

Orange: actual restricted box; cyan: enabled hearing radius; pale blue: actual
eye-local distance and horizontal/vertical angular limits; red/green sample
rays: CanSeePlayer's current result. The old floor fan is only a horizontal guide.
Labels also expose the actual CanHearPlayer result. Re-run to
stop; ending PIE removes the callback automatically.
"""
import unreal as u
import math

def _start_stealth_debug():
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    assert worlds and all('Lvl_SPStealthTest' in w.get_path_name() for w in worlds)
    handle = None
    def draw(_delta):
        nonlocal handle
        if not u.EditorLevelLibrary.get_pie_worlds(False):
            u.unregister_slate_post_tick_callback(handle)
            u._sp_stealth_debug_handle = None
            return
        for world in worlds:
            if not u.SystemLibrary.is_valid(world):
                continue
            for route in u.GameplayStatics.get_all_actors_of_class(world,u.SPRestrictedArea):
                box = route.get_editor_property('volume')
                u.SystemLibrary.draw_debug_box(world,box.get_world_location(),box.get_scaled_box_extent(),
                    u.LinearColor(1,.25,.05,1),box.get_world_rotation(),0,2)
            for actor in u.GameplayStatics.get_all_actors_of_class(world,u.Actor):
                for observer in actor.get_components_by_class(u.SPStealthObserverComponent):
                    if not observer.get_editor_property('observation_enabled'):continue
                    settings=observer.get_editor_property('sight')
                    distance=settings.distance
                    horizontal=settings.horizontal_angle*.5
                    vertical=settings.vertical_angle*.5
                    if isinstance(actor,u.SPGuardCharacter):
                        distance=actor.get_editor_property('sight_distance')
                        horizontal=actor.get_editor_property('sight_angle')*.5
                        vertical=actor.get_editor_property('vertical_sight_angle')*.5
                    eye=observer.get_world_location()
                    forward=observer.get_forward_vector();right=observer.get_right_vector();up=observer.get_up_vector()
                    def point(h,v):
                        h=math.radians(h);v=math.radians(v)
                        return eye+(forward*(math.cos(v)*math.cos(h))+right*(math.cos(v)*math.sin(h))+up*math.sin(v))*distance
                    color=u.LinearColor(.4,.7,1,1)
                    for h in [-horizontal,horizontal]:
                        for i in range(12):
                            u.SystemLibrary.draw_debug_line(world,point(h,-vertical+2*vertical*i/12),point(h,-vertical+2*vertical*(i+1)/12),color,0,1)
                        for v in [-vertical,vertical]:u.SystemLibrary.draw_debug_line(world,eye,point(h,v),color,0,1)
                    for v in [-vertical,vertical]:
                        for i in range(12):
                            u.SystemLibrary.draw_debug_line(world,point(-horizontal+2*horizontal*i/12,v),point(-horizontal+2*horizontal*(i+1)/12,v),color,0,1)
            for guard in u.GameplayStatics.get_all_actors_of_class(world,u.SPGuardCharacter):
                if guard.get_editor_property('investigating_anonymous_incident'):
                    destination = guard.get_editor_property('investigation_location')
                    u.SystemLibrary.draw_debug_sphere(world,destination,100,16,u.LinearColor(1,1,0,1),0,2)
                    u.SystemLibrary.draw_debug_line(world,guard.get_actor_location(),destination,u.LinearColor(1,1,0,1),0,2)
                if guard.get_editor_property('hear_footsteps'):
                    u.SystemLibrary.draw_debug_sphere(world,guard.get_actor_location(),
                        guard.get_editor_property('hearing_distance'),24,u.LinearColor(0,.6,1,1),0,1)
                # Match SPStealthObserverComponent, including custom-gravity capsule rotation.
                eye = guard.get_editor_property('observer').get_world_location()
                for player in u.GameplayStatics.get_all_actors_of_class(world,u.SPPlayerCharacter):
                    if not player.is_player_controlled():
                        continue
                    visible = guard.can_see_player(player)
                    color = u.LinearColor(0,1,.2,1) if visible else u.LinearColor(1,.1,.1,1)
                    capsule = player.capsule_component
                    center = capsule.get_world_location()
                    height = capsule.get_scaled_capsule_half_height()
                    for point in [center,center+capsule.get_up_vector()*(height*.65)]:
                        u.SystemLibrary.draw_debug_line(world,eye,point,color,0,1)
                    label = '보임=%s 들림=%s 거리=%g 시야=%g/%g' % (
                        '예' if visible else '아니오','예' if guard.can_hear_player(player) else '아니오',guard.get_editor_property('sight_distance'),
                        guard.get_editor_property('sight_angle'),guard.get_editor_property('vertical_sight_angle'))
                    u.SystemLibrary.draw_debug_string(world,eye+u.Vector(0,0,45),label,None,color,0)
            for terminal in u.GameplayStatics.get_all_actors_of_class(world,u.SPStealthTestConsole):
                box = terminal.get_editor_property('interaction_volume')
                u.SystemLibrary.draw_debug_box(world,box.get_world_location(),box.get_scaled_box_extent(),
                    u.LinearColor(.7,.3,1,1),box.get_world_rotation(),0,2)
            # Draw the nearest local terminal's actual dispatch sphere, not its visual mesh.
            local = u.GameplayStatics.get_player_pawn(world,0)
            terminals = u.GameplayStatics.get_all_actors_of_class(world,u.SPStealthTestConsole)
            if local and terminals:
                terminal = min(terminals,key=lambda t:(t.get_actor_location()-local.get_actor_location()).length())
                if (terminal.get_actor_location()-local.get_actor_location()).length()<300:
                    destination = terminal.get_editor_property('incident_location')
                    radius = terminal.get_editor_property('response_radius')
                    u.SystemLibrary.draw_debug_sphere(world,destination,radius,32,u.LinearColor(.2,.6,1,1),0,1)
                    u.SystemLibrary.draw_debug_string(world,terminal.get_actor_location()+u.Vector(0,0,140),
                        '출동 반경=%g 최대 인원=%d 그룹=%s' % (radius,terminal.get_editor_property('max_responders'),
                        terminal.get_editor_property('dispatch_group')),None,u.LinearColor(.2,.6,1,1),0)
    handle = u.register_slate_post_tick_callback(draw)
    return handle

if getattr(u,'_sp_stealth_debug_handle',None) is not None:
    u.unregister_slate_post_tick_callback(u._sp_stealth_debug_handle)
    u._sp_stealth_debug_handle = None
    u.log('STEALTH_TEST_DEBUG_OFF')
else:
    u._sp_stealth_debug_handle = _start_stealth_debug()
    u.log('STEALTH_TEST_DEBUG_ON')
