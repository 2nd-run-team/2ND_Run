"""Fresh saved test map, two-player listen PIE. Real owning-client E/movement.

Only PIE actors are repositioned. Observer speed is frozen except the anonymous
noise journey so witness timing is measured independently of chase navigation.
Writes Saved/StealthCrime/crime-pie.json; stop PIE after completion.
"""
import json
from pathlib import Path
import unreal as u
from editor_toolset.toolsets.object import ObjectTools as O

def run():
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==2
    server=next(w for w in worlds if u.GameplayStatics.get_game_state(w).has_authority())
    # Isolate this earlier-stage fixture from newly placed laser hazards (PIE only).
    for laser in u.GameplayStatics.get_all_actors_of_class(server,u.SPLaserSecurityDevice):
        laser.set_editor_property("enabled",False);laser.refresh_device()
    actors=lambda w,t:u.GameplayStatics.get_all_actors_of_class(w,t)
    s=next(x for x in u.ObjectIterator(u.SPGuardAlertSubsystem) if x.get_outer()==server)
    players=lambda w:[p for p in actors(w,u.SPPlayerCharacter) if p.is_player_controlled()]
    a=next(p for p in players(server) if not p.is_locally_controlled())
    b=next(p for p in players(server) if p.is_locally_controlled())
    prop=lambda o,n:o.get_editor_property(n)
    pid=lambda p:prop(prop(p,'player_state'),'player_id')
    copies=lambda p:[c for w in worlds for c in players(w) if pid(c)==pid(p)]
    owner=lambda p:next(c for c in copies(p) if c.is_locally_controlled())
    activity=lambda p:p.get_component_by_class(u.SPStealthActivityComponent)
    g=next(x for x in actors(server,u.SPGuardCharacter) if x.get_actor_label()=='Crime_ObserverGuard')
    for x in actors(server,u.SPGuardCharacter):
        if x!=g:x.set_editor_property('sight_distance',1)
    buttons={x.get_actor_label():x for x in actors(server,u.SPStealthTestConsole)}
    bundle=next(x for x in actors(server,u.SPLootBundle) if x.get_actor_label()=='Crime_PublicLootBundle')
    inputs={pid(p):next(x for x in u.ObjectIterator(u.EnhancedInputLocalPlayerSubsystem)
        if not x.get_name().startswith('Default__') and u.GameplayStatics.get_player_controller(x,0)==owner(p).get_controller()) for p in [a,b]}
    held={pid(p):{} for p in [a,b]}
    report={'checks':[],'source':'Saved map, actual host/participant Enhanced Input and replicated state'}
    perf=u.load_object(None,'/Script/UnrealEd.Default__EditorPerformanceSettings')
    before=O.get_properties(perf,['bThrottleCPUWhenNotForeground']);O.set_properties(perf,'{"bThrottleCPUWhenNotForeground":false}')
    handle=None
    def check(name,ok):
        report['checks'].append({'name':name,'passed':bool(ok)})
        u.log('CRIME_PIE_CHECK '+name+': '+str(bool(ok)))
    def state(ia=False,ib=False,alarm=False):
        return all(prop(p,'player_state').get_component_by_class(u.SPStealthPlayerStateComponent).is_identified()==expected
            for original,expected in [(a,ia),(b,ib)] for p in copies(original)) and all(
            u.GameplayStatics.get_game_state(w).get_component_by_class(u.SPStealthGameStateComponent).get_alarm_state().global_alarm==alarm for w in worlds)
    def move(p,loc,yaw=90,pitch=0):
        for c in copies(p):
            c.set_actor_location(u.Vector(*loc),False,True);c.character_movement.stop_movement_immediately()
            if c.get_controller():c.get_controller().set_control_rotation(u.Rotator(yaw=yaw,pitch=pitch))
    def pose_guard(loc=(-1100,100,98),yaw=90):
        g.get_controller().stop_movement();g.character_movement.stop_movement_immediately()
        g.set_actor_location(u.Vector(*loc),False,True);g.set_actor_rotation(u.Rotator(yaw=yaw),True)
    def reset():
        for h in held.values():h.clear()
        s.restart_stage()
        for name,value in [('sight_distance',1200),('patrol_speed',0),('chase_speed',0),('stuck_timeout',100),('move_timeout',120)]:g.set_editor_property(name,value)
        g.character_movement.set_editor_property('orient_rotation_to_movement',False)
        pose_guard();move(a,(-1450,550,98));move(b,(-2700,-800,98))
    def key(p,name,on=True,value=None):
        if on:held[pid(p)][name]=value or u.Vector(1,0,0)
        else:held[pid(p)].pop(name,None)
    def prep(p,button):
        pos=button.get_actor_location();move(p,(pos.x,pos.y-185,98),90,-12.8)
    def press(p,button,seconds=.25):
        prep(p,button);yield .4
        key(p,'interact');yield seconds;key(p,'interact',False);yield .4
    def hud():
        return [str(w.get_status_text()).replace(' [나]','') for w in u.ObjectIterator(u.SPStealthStatusWidget)
            if w.get_owning_player() in [owner(a).get_controller(),owner(b).get_controller()]]
    def steps():
        reset();yield .4
        check('Visible public normal player ignored on both peers',g.can_see_player(a) and state())
        start=a.get_actor_location();key(a,'move',value=u.Vector(0,1,0));yield .4
        key(a,'sprint');yield .35;key(a,'jump');yield .15
        check('Actual remote movement sprint and jump are legal',(a.get_actor_location()-start).length()>100 and state() and not activity(a).get_active_crimes())
        held[pid(a)].clear();yield .8
        bag=next(x for x in actors(server,u.SPCargo) if x.get_actor_label()=='Test_LootBag')
        for w in worlds:
            x=next(x for x in actors(w,u.SPCargo) if x.get_actor_label()=='Test_LootBag')
            x.set_actor_location(u.Vector(-1550,700,110),False,True)
        move(a,(-1550,480,98),90,-13.4);yield .4
        key(a,'interact');yield .25;key(a,'interact',False);yield .5
        check('Floor bag pickup is normal E with replicated inventory',bag.get_owner()==a and all(prop(c,'inventory').find_item_of_type(prop(bag,'item_type')) for c in copies(a)) and state())
        move(a,(-1450,550,98));key(a,'move',value=u.Vector(0,1,0));yield .4;held[pid(a)].clear()
        check('Visible bag carrying does not register crime',g.can_see_player(a) and state() and not activity(a).get_active_crimes())
        # Independently authored volume stays fixed when route is edited.
        area=actors(server,u.SPRestrictedArea)[0];route=actors(server,u.SPGuardPatrolRoute)[0]
        original=route.get_actor_location();route.set_actor_location(original+u.Vector(5000,0,0),False,True)
        move(a,(900,-800,98));yield .5
        check('Independent area still contains player after patrol relocation',all(activity(c).get_current_area() is not None for c in copies(a)) and all(activity(c).get_current_area() is None for c in copies(b)))
        texts=hud();check('PUBLIC and RESTRICTED player UI agrees on host and participant',len(texts)==2 and texts[0]==texts[1] and '제한 구역' in texts[0] and '일반 구역' in texts[0])
        report['zone_hud']=texts
        route.set_actor_location(original,False,True)
        reset();move(a,(-1000,380,98),90,-13.4);yield .4
        key(a,'interact');yield .55
        check('Actual public loot packing registers before one-second confirmation',u.SPCrimeKind.LOOT_PACKING in activity(a).get_active_crimes() and state())
        key(a,'interact',False);yield .4
        check('E release clears packing registration and progress',not activity(a).get_active_crimes() and bundle.get_component_by_class(u.SPInteractableComponent).get_progress()==0)
        yield 1.2;check('Cancelled packing cannot cause a later identification',state())
        key(a,'interact');yield 1.35;key(a,'interact',False);yield .3
        check('Observed public packing exposes only A and alarms both peers',state(True,False,True))
        receipts=s.get_recent_incidents()
        check('Packing witness records category and direct sustained event',any(x.context.crime_kind==u.SPCrimeKind.LOOT_PACKING and x.kind==u.SPStealthIncident.SUSTAINED_CRIME_CONFIRMED for x in receipts))
        reset();prep(a,buttons['Crime_Terminal']);prep(b,buttons['Crime_Vault']);yield .5
        key(a,'interact');yield .5;key(b,'interact');yield .3
        check('Two players each remain hidden below one second',state() and bool(activity(a).get_active_crimes()) and bool(activity(b).get_active_crimes()))
        yield .35;key(b,'interact',False);yield .15
        check('A accumulated time never confirms later-starting B',state(True,False,True))
        key(a,'interact',False);yield .3
        reset();prep(a,buttons['Crime_Terminal']);yield .4
        key(a,'interact');yield .55
        pose_guard(yaw=-90);yield .25;pose_guard();yield .55
        check('Broken sight resets the observer-player confirmation',state() and bool(activity(a).get_active_crimes()))
        yield .65;key(a,'interact',False);yield .25
        check('A full new visible second confirms after sight returns',state(True,False,True))
        for label in ['Crime_Pickpocket','Crime_Tool']:
            reset();yield .3
            yield from press(a,buttons[label])
            check('Visible instant E '+label+' identifies at action time',state(True,False,True) and not activity(a).get_active_crimes())
        reset();pose_guard((-1500,550,98),180)
        tool=buttons['Crime_Tool'];old=tool.get_actor_location()
        for w in worlds:
            x=next(x for x in actors(w,u.SPStealthTestConsole) if x.get_actor_label()=='Crime_Tool')
            x.set_actor_location(u.Vector(-2350,700,120),False,True)
        yield from press(a,tool)
        check('Wall blocks instantaneous crime at its actual E completion',not g.can_see_player(a) and state() and not activity(a).get_active_crimes())
        pose_guard();move(a,(-1450,550,98));yield 1.3
        check('Later visibility does not retrospectively witness instant crime',g.can_see_player(a) and state())
        for w in worlds:
            x=next(x for x in actors(w,u.SPStealthTestConsole) if x.get_actor_label()=='Crime_Tool')
            x.set_actor_location(old,False,True)
        reset();pose_guard((-1500,550,98),180);g.set_editor_property('patrol_speed',170)
        start=g.get_actor_location();yield from press(a,buttons['Crime_WorkNoise'])
        last=s.get_recent_incidents()[-1];yield .5
        guards=[next(x for x in actors(w,u.SPGuardCharacter) if x.get_actor_label()=='Crime_ObserverGuard') for w in worlds]
        check('Occluded work noise dispatches snapshot investigation without identity',last.kind==u.SPStealthIncident.WORK_NOISE and last.dispatched_guards==1 and state() and not g.can_see_player(a) and (last.context.location-u.Vector(-2300,550,0)).length()<1)
        check('Anonymous wall-noise movement and no-target state replicate',(g.get_actor_location()-start).length()>20 and all(prop(x,'guard_state')==u.SPGuardState.INVESTIGATING and not prop(x,'target_player') for x in guards))
        reset();prep(a,buttons['Crime_Terminal']);yield .4
        key(a,'interact');yield .4;activity(a).set_incapacitated(True);yield .4
        check('Incapacitation API cancels actual held E and replicates',not activity(a).get_active_crimes() and all(activity(c).is_incapacitated() for c in copies(a)) and buttons['Crime_Terminal'].get_component_by_class(u.SPInteractableComponent).get_progress()==0)
        key(a,'interact',False);activity(a).set_incapacitated(False);yield .4
        key(a,'interact');yield .4;s.restart_stage();key(a,'interact',False);yield .4
        check('Stage restart cancels in-progress criminal E',not activity(a).get_active_crimes() and state() and buttons['Crime_Terminal'].get_component_by_class(u.SPInteractableComponent).get_progress()==0)
        reset();move(a,(-1000,380,98),90,-13.4);yield .4
        cargo_count=len(actors(server,u.SPCargo));key(a,'interact');yield 2.5;key(a,'interact',False);yield .4
        check('Completed real packing clears registration and destroys source once',not u.SystemLibrary.is_valid(bundle) and not activity(a).get_active_crimes())
        check('Completed packing creates exactly one replicated bag',all(len(actors(w,u.SPCargo))==cargo_count+1 for w in worlds))
        texts=hud();check('Final identity alarm and area UI agree',len(texts)==2 and texts[0]==texts[1] and state(True,False,True));report['final_hud']=texts
    iterator=steps();remaining=next(iterator);last_time=u.GameplayStatics.get_time_seconds(server)
    def finish(error=None):
        nonlocal handle
        for h in held.values():h.clear()
        if handle is not None:u.unregister_slate_post_tick_callback(handle);handle=None
        O.set_properties(perf,before)
        if error:report['error']=repr(error)
        report['passed']=not error and all(c['passed'] for c in report['checks'])
        path=Path(u.Paths.project_saved_dir(),'StealthCrime','crime-pie.json');path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(json.dumps(report,indent=2),encoding='utf-8');u.log('CRIME_PIE_COMPLETE '+str(report['passed']))
    def tick(_):
        nonlocal remaining,last_time
        try:
            if not all(u.SystemLibrary.is_valid(w) for w in worlds):raise RuntimeError('PIE ended')
            for p in [a,b]:
                for name,value in held[pid(p)].items():inputs[pid(p)].inject_input_vector_for_action(prop(owner(p),name+'_action'),value,[],[])
            now=u.GameplayStatics.get_time_seconds(server);remaining-=max(0,now-last_time);last_time=now
            if remaining<=0:remaining=next(iterator)
        except StopIteration:finish()
        except Exception as exc:finish(exc)
    handle=u.register_slate_post_tick_callback(tick)
    u.log('CRIME_PIE_STARTED')
run()

