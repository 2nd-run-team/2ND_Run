"""Read-only validation of the migrated freight level (UE 5.8 Python).

Run from the editor console:
  py "<project>/Tools/TrainFreight/validate_assets.py"
Or UnrealEditor-Cmd <project>.uproject -run=pythonscript -script=<this file>
  -unattended -nullrhi -nosound -nop4
Writes Saved/TrainFreightMigration/asset-validation.json; never saves assets.
"""
import json
from pathlib import Path
import unreal as u

PROJECT = Path(u.Paths.project_dir()).resolve()
MAPPING = json.loads((PROJECT / "Docs/TrainFreight/asset-paths.json").read_text(encoding="utf-8"))
LEVEL = "/Game/SpacePirate/Maps/Lvl_SPTrainFreight"
ROOT = "/Game/SpacePirate/Train"
reg = u.AssetRegistryHelpers.get_asset_registry()
reg.search_all_assets(True)
options = u.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=False, include_soft_management_references=False,
    include_hard_management_references=False)
report = {"engine": u.SystemLibrary.get_engine_version(), "assets": [], "blueprints": [],
          "missing": [], "old_references": [], "source_project_references": [], "errors": []}
objects = []
for path in MAPPING.values():
    obj = u.load_asset(path)
    if obj is None:
        report["missing"].append(path)
    else:
        objects.append(obj)
        report["assets"].append(path)

def depth(bp):
    n, cls = 0, u.BlueprintEditorLibrary.get_blueprint_parent_class(bp)
    while cls and cls.get_path_name().startswith('/Game/'):
        n += 1
        parent_bp = u.load_asset(cls.get_path_name().split('.', 1)[0])
        cls = u.BlueprintEditorLibrary.get_blueprint_parent_class(parent_bp)
    return n

for bp in sorted((o for o in objects if isinstance(o, u.Blueprint)), key=depth):
    u.BlueprintEditorLibrary.compile_blueprint(bp)
    status = bp.get_editor_property("status")
    report["blueprints"].append({"path": bp.get_path_name(), "status": str(status)})
    if status != u.BlueprintStatus.BS_UP_TO_DATE:
        report["errors"].append("Blueprint compile: " + bp.get_path_name())

visited, pending = set(), list(MAPPING.values())
while pending:
    path = pending.pop()
    if path in visited:
        continue
    visited.add(path)
    for dep_name in reg.get_dependencies(path, options):
        dep = str(dep_name)
        if dep.startswith("/Script/Test2") or dep.startswith("/Script/TrainBlueprintBuilder"):
            report["source_project_references"].append([path, dep])
        if dep.startswith("/Game/TrainGame/"):
            report["old_references"].append([path, dep])
        if dep.startswith("/Game/"):
            if not reg.get_assets_by_package_name(dep):
                report["missing"].append(dep)
            elif dep not in visited:
                pending.append(dep)
report["game_dependency_packages"] = sorted(visited)

def cdo(path):
    return u.get_default_object(u.load_asset(path).generated_class())

gm = cdo(ROOT + "/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameMode")
pc = cdo(ROOT + "/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainPlayerController")
report["pawn"] = gm.get_editor_property("default_pawn_class").get_path_name()
report["input_contexts"] = [a.get_path_name() for a in pc.get_editor_property("default_mapping_contexts")]
if report["pawn"] != "/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter.BP_SPPlayerCharacter_C":
    report["errors"].append("Unexpected default pawn")
if "/Game/SpacePirate/Player/Input/IMC_SPPlayer.IMC_SPPlayer" not in report["input_contexts"]:
    report["errors"].append("Team input mapping context missing")
world = u.load_asset(LEVEL)
if world:
    report["game_mode"] = world.get_world_settings().get_editor_property("default_game_mode").get_path_name()
    if "BP_SPPlanetTrainGameMode" not in report["game_mode"]:
        report["errors"].append("Freight level game mode override missing")
report["passed"] = not any(report[k] for k in ("missing", "old_references", "source_project_references", "errors"))
out = PROJECT / "Saved/TrainFreightMigration/asset-validation.json"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
u.log("TRAIN_ASSET_VALIDATION: " + str(report["passed"]) + " " + str(out))
if not report["passed"]:
    raise RuntimeError("Freight validation failed; see " + str(out))
