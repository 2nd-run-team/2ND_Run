"""Build the Manny prototype crouch overlay without modifying team animation assets.

Requires UE 5.8 EditorToolset and the lock for ABP_SPCrouchPostProcess.
Run after compiling SPCrouchAnimInstance. Existing nodes in this generated asset
are rebuilt; hand-authored changes to this one asset must be preserved separately.
"""
import unreal as u
from editor_toolset.toolsets.blueprint import BlueprintTools as B

PATH = "/Game/SpacePirate/Stealth/Animation"
NAME = "ABP_SPCrouchPostProcess"
bp = u.load_asset(PATH + "/" + NAME)
if not bp:
    factory = u.AnimBlueprintFactory()
    factory.set_editor_property("parent_class", u.SPCrouchAnimInstance)
    factory.set_editor_property("target_skeleton", u.load_asset(
        "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple").get_editor_property("skeleton"))
    bp = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, PATH, u.AnimBlueprint, factory)
graph = next(g for g in B.list_graphs(bp) if g.get_name() == "AnimGraph")
root = next(n for n in B.find_nodes(graph) if isinstance(n,u.AnimGraphNode_Root))
for n in B.find_nodes(graph):
    if n != root:
        B.delete_node(n)

def node(candidates, x, y=0):
    available = {str(n) for n in u.BlueprintGraphEditor.get_graph_editor(graph).list_available_nodes([])}
    type_id = next((n for n in candidates if n in available),None)
    if not type_id:
        # BlueprintTools resolves normalized display names for English/Korean editors.
        for candidate in candidates:
            found = B.find_node_types(graph,candidate)
            if found:
                type_id=found[0]
                break
    if not type_id:
        raise RuntimeError("Missing animation node: " + str(candidates))
    return B.create_node(graph,type_id,u.IntPoint(x,y))

def connect(src, output, dest, input_name):
    B.connect_pins(B._pin_to_id(src.find_output_pin(output)), B._pin_to_id(dest.find_input_pin(input_name)))

input_pose = node(["애니메이션|링크된애님그래프|입력포즈", "InputPose"],-550)
to_component = node(["애니메이션|스페이스변환|로컬을컴포넌트로", "LocalToComponent"],-300)
alpha = node(["Variables|Stealth|GetCrouchAlpha"],-300,350)
connect(input_pose,"Pose",to_component,"LocalPose")
previous = to_component

# Component-space rotations about the Manny mesh X axis bend both knees forward.
# The overlay preserves the incoming walk cycle and upper-body cargo pose.
adjustments = [("pelvis",0,-40), ("thigh_l",-55,0), ("calf_l",110,0),
               ("foot_l",-55,0), ("thigh_r",-55,0), ("calf_r",110,0), ("foot_r",-55,0)]
for index,(bone,roll,z) in enumerate(adjustments):
    n = node(["Animation|SkeletalControls|본트랜스폼(변경)", "Transform(Modify)Bone"],index*290)
    data = n.get_editor_property("node")
    reference = u.BoneReference()
    reference.set_editor_property("bone_name",bone)
    data.set_editor_property("bone_to_modify",reference)
    data.set_editor_property("translation_mode",u.BoneModificationMode.BMM_ADDITIVE if z else u.BoneModificationMode.BMM_IGNORE)
    data.set_editor_property("translation_space",u.BoneControlSpace.BCS_COMPONENT_SPACE)
    data.set_editor_property("rotation_mode",u.BoneModificationMode.BMM_ADDITIVE if roll else u.BoneModificationMode.BMM_IGNORE)
    data.set_editor_property("rotation_space",u.BoneControlSpace.BCS_COMPONENT_SPACE)
    n.set_editor_property("node",data)
    B.set_pin_value(B._pin_to_id(n.find_input_pin("Translation")),f"(X=0,Y=0,Z={z})")
    # Rotator graph pins use comma-separated Pitch,Yaw,Roll, unlike Vector pins.
    B.set_pin_value(B._pin_to_id(n.find_input_pin("Rotation")),f"0,0,{roll}")
    assert abs(float(B.get_pin_value(B._pin_to_id(n.find_input_pin("Rotation"))).split(',')[2])-roll)<.01
    connect(previous,"ComponentPose" if previous==to_component else "Pose",n,"ComponentPose")
    connect(alpha,"CrouchAlpha",n,"Alpha")
    previous=n

to_local = node(["애니메이션|스페이스변환|컴포넌트에서로컬로", "ComponentToLocal"],len(adjustments)*290)
connect(previous,"Pose",to_local,"ComponentPose")
connect(to_local,"Pose",root,"Result")
root.set_node_pos(u.IntPoint((len(adjustments)+1)*290,0))
u.BlueprintEditorLibrary.compile_blueprint(bp)
assert bp.get_editor_property("status")==u.BlueprintStatus.BS_UP_TO_DATE
assert u.EditorAssetLibrary.save_loaded_asset(bp)
u.log("STEALTH_CROUCH_POSE_BUILT")
