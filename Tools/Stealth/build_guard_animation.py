"""Build velocity-driven guard locomotion; run after compiling SPGuardAnimInstance.

Rebuilds this generated graph only. Acquire LFS locks for the guard animation,
guard Blueprint and freight map first. Save the current map after inspection.
"""
import unreal as u
from editor_toolset.toolsets.blueprint import BlueprintTools as B

PATH = '/Game/SpacePirate/Stealth/Animation'
NAME = 'ABP_SPGuardLocomotion'
bp = u.load_asset(PATH+'/'+NAME)
if not bp:
    factory = u.AnimBlueprintFactory()
    factory.set_editor_property('parent_class',u.SPGuardAnimInstance)
    factory.set_editor_property('target_skeleton',u.load_asset(
        '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple').get_editor_property('skeleton'))
    bp = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME,PATH,u.AnimBlueprint,factory)
graph = next(g for g in B.list_graphs(bp) if g.get_name()=='AnimGraph')
root = next(n for n in B.find_nodes(graph) if isinstance(n,u.AnimGraphNode_Root))
for n in B.find_nodes(graph):
    if n!=root: B.delete_node(n)

def node(search,x,y):
    candidates = B.find_node_types(graph,search)
    if search=='BS_Idle_Walk_Run':
        candidates=[n for n in candidates if 'BlendSpacePlayer' in n or '블렌드스페이스플레이어' in n]
    else:
        candidates=[n for n in candidates if n.startswith('Variables|') and n.endswith('|'+search)]
    assert len(candidates)==1,(search,list(candidates))
    return B.create_node(graph,candidates[0],u.IntPoint(x,y))

def connect(src,output,dst,input_name):
    B.connect_pins(B._pin_to_id(src.find_output_pin(output)),B._pin_to_id(dst.find_input_pin(input_name)))

blend = node('BS_Idle_Walk_Run',-300,0)
optional = blend.get_editor_property('show_pin_for_properties')
for i,p in enumerate(optional):
    if str(p.get_editor_property('property_name'))=='PlayRate':
        p.set_editor_property('show_pin',True)
        optional[i]=p
blend.set_editor_property('show_pin_for_properties',optional)
# Force the editor's normal node reconstruction after changing optional pins.
blend.set_editor_property('update_function',blend.get_editor_property('update_function'),
                          u.PropertyAccessChangeNotifyMode.ALWAYS)
for index,(variable,pin) in enumerate([('Direction','X'),('BlendSpeed','Y'),('StridePlayRate','PlayRate')]):
    getter = node('Get'+variable,-650,index*130)
    connect(getter,variable,blend,pin)
connect(blend,'Pose',root,'Result')
root.set_node_pos(u.IntPoint(50,0))
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property('status')==u.BlueprintStatus.BS_UP_TO_DATE
assert u.EditorAssetLibrary.save_loaded_asset(bp)

guard_bp = u.load_asset('/Game/SpacePirate/Stealth/Blueprints/BP_SPGuardCharacter')
u.get_default_object(guard_bp.generated_class()).get_editor_property('mesh').set_anim_instance_class(bp.generated_class())
u.BlueprintEditorLibrary.compile_blueprint(guard_bp)
assert u.EditorAssetLibrary.save_loaded_asset(guard_bp)
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.SPGuardCharacter):
        actor.get_editor_property('mesh').set_anim_instance_class(bp.generated_class())
u.log('STEALTH_GUARD_ANIMATION_BUILT')
