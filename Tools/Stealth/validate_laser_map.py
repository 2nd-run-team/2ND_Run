"""Read-only saved laser lab checks; run after nav rebuild/save/reload, no PIE."""
import json,wave
from pathlib import Path
import unreal as u

world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0]=='/Game/SpacePirate/Maps/Lvl_SPStealthTest'
assert not u.EditorLevelLibrary.get_pie_worlds(False)
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
devices={a.get_actor_label():a for a in actors if isinstance(a,u.SPLaserSecurityDevice)}
report={'checks':[],'devices':[],'actor_count':len(actors)}
def check(name,ok):report['checks'].append({'name':name,'passed':bool(ok)})
def prop(a,n):return a.get_editor_property(n)
check('Three saved devices have distinct IDs',len(devices)==3 and len({str(prop(d,'device_id')) for d in devices.values()})==3)
area=next(a for a in actors if isinstance(a,u.SPRestrictedArea))
for name,x,height in [('Crouch',-2100,145),('Jump',0,40),('Wait',2100,95)]:
    d=devices['Laser_'+name];start=d.get_beam_start();end=d.get_beam_end()
    check(name+' beam length and actual height',(end-start).length()>499 and (end-start).length()<501 and abs(start.z-height)<.1 and abs(end.z-height)<.1)
    check(name+' safe approach and sensor are public',not area.contains_location(u.Vector(x,1450,98)) and not area.contains_location(d.get_actor_location()))
    check(name+' detection separate from collision-free visuals',prop(d,'detection_volume').get_collision_enabled()==u.CollisionEnabled.QUERY_ONLY and all(
        prop(d,n).get_collision_enabled()==u.CollisionEnabled.NO_COLLISION for n in ['emitter_visual','receiver_visual','beam_visual']))
    check(name+' response and throttle configured',str(prop(d,'dispatch_group'))=='StealthTest_Laser' and prop(d,'max_responders')==1 and prop(d,'call_interval')==2 and prop(d,'detection_thickness')==4)
    path=u.NavigationSystemV1.find_path_to_location_synchronously(world,u.Vector(-800,1450,0),d.get_actor_location())
    check(name+' sensor response position reachable',path is not None and path.is_valid() and not path.is_partial())
    text_material=prop(prop(d,'state_indicator'),'text_material')
    check(name+' editable visuals, unlit text and sound refs present',all(prop(d,n) for n in ['on_material','off_material','warning_material','warning_sound','contact_sound']) and
        text_material is not None and prop(text_material,'shading_model')==u.MaterialShadingModel.MSM_UNLIT)
    report['devices'].append({'name':name,'start':start.to_tuple(),'end':end.to_tuple(),'sensor':d.get_actor_location().to_tuple()})
wait=devices['Laser_Wait']
check('Wait device cycles four seconds each with one-second warning',prop(wait,'mode')==u.SPLaserMode.PERIODIC and prop(wait,'on_seconds')==4 and prop(wait,'off_seconds')==4 and prop(wait,'warning_seconds')==1)
check('Crouch and jump always armed',all(prop(devices['Laser_'+n],'mode')==u.SPLaserMode.ALWAYS_ON for n in ['Crouch','Jump']))
guard=next(a for a in actors if a.get_actor_label()=='Laser_ResponseGuard')
check('Laser response group remains independent of identity scope',str(prop(guard,'alert_group'))=='StealthTest_Laser' and str(prop(guard,'identity_scope'))=='StageSecurity' and not prop(guard,'hear_footsteps'))
for name,duration in [('S_SPLaserWarning',.16),('S_SPLaserContact',.24)]:
    path=Path(u.Paths.project_dir(),'Content/SpacePirate/Stealth/Prototype/Audio/Source',name+'.wav')
    with wave.open(str(path),'rb') as audio:
        check(name+' is original finite mono PCM',audio.getnchannels()==1 and audio.getsampwidth()==2 and abs(audio.getnframes()/audio.getframerate()-duration)<.001)
check('No unsaved map or content',not u.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not u.EditorLoadingAndSavingUtils.get_dirty_content_packages())
report['passed']=all(c['passed'] for c in report['checks'])
out=Path(u.Paths.project_saved_dir(),'StealthLaser','map-validation.json');out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('LASER_MAP_VALIDATION '+str(report['passed']))
assert report['passed'],str([c for c in report['checks'] if not c['passed']])
