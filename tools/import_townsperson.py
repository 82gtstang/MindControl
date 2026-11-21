"""Import the townsperson mesh before cooking the Unreal build."""

from pathlib import Path
import os
import subprocess
import tempfile

import unreal


source = Path(__file__).resolve().parents[1] / "unreal" / "SourceAssets" / "Townsperson.fbx"
destination = "/Game/Characters/Town"
asset_path = destination + "/Townsperson"
if not source.is_file():
    raise RuntimeError(f"Townsperson source missing: {source}")

options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_as_skeletal = False
options.import_materials = True
options.import_textures = False
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.auto_generate_collision = False

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = destination
task.destination_name = "Townsperson"
task.automated = True
task.replace_existing = True
task.save = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
if not task.imported_object_paths or not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError(f"Townsperson import failed: {task.imported_object_paths}")
unreal.EditorAssetLibrary.save_directory(destination, only_if_is_dirty=False)
print(f"TOWNSPERSON_IMPORT_SUCCESS {asset_path}")

# Match the same Windows Blender installation used by the character bake.
install_root = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Blender Foundation"
def version(exe):
    return tuple(int(part) for part in exe.parent.name.removeprefix("Blender ").split(".")
                 if part.isdigit())
blender = max(install_root.glob("Blender */blender.exe"), key=version, default=None)
if not blender:
    raise RuntimeError(f"Blender install missing under {install_root}")

with tempfile.TemporaryDirectory(prefix="mindcontrol-settlers-") as folder:
    subprocess.run([str(blender), "-b", "-t", "2", "--python-exit-code", "1", "--python",
                    str(Path(__file__).with_name("bake_settlement_people.py")),
                    "--", str(source), folder], check=True, timeout=180)
    variants = sorted(Path(folder).glob("*.fbx"))
    if len(variants) != 30:
        raise RuntimeError(f"Expected 30 NPC mesh frames, found {len(variants)}")
    tasks = []
    for variant in variants:
        frame = unreal.AssetImportTask()
        frame.filename = str(variant)
        frame.destination_path = destination + "/Settlers"
        frame.destination_name = variant.stem
        frame.automated = True
        frame.replace_existing = True
        frame.save = True
        frame.options = options
        tasks.append(frame)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    missing = [item.destination_name for item in tasks if not item.imported_object_paths]
    if missing:
        raise RuntimeError(f"NPC frames failed to import: {missing}")
unreal.EditorAssetLibrary.save_directory(destination, only_if_is_dirty=False)
with tempfile.TemporaryDirectory(prefix="mindcontrol-entrances-") as folder:
    subprocess.run([str(blender), "-b", "-t", "2", "--python-exit-code", "1", "--python",
                    str(Path(__file__).with_name("bake_dungeon_entrances.py")),
                    "--", folder], check=True, timeout=120)
    tasks = []
    for name in ("CaveMouth", "CliffTomb", "ForestBasin", "RuneGate"):
        task = unreal.AssetImportTask()
        task.filename = str(Path(folder) / f"{name}.fbx")
        task.destination_path = destination + "/Entrances"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = options
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    missing = [item.destination_name for item in tasks if not item.imported_object_paths]
    if missing:
        raise RuntimeError(f"Entrance models failed to import: {missing}")
unreal.EditorAssetLibrary.save_directory(destination + "/Entrances", only_if_is_dirty=False)
with tempfile.TemporaryDirectory(prefix="mindcontrol-dungeon-assets-") as folder:
    subprocess.run([str(blender), "-b", "-t", "2", "--python-exit-code", "1", "--python",
                    str(Path(__file__).with_name("bake_dungeon_assets.py")),
                    "--", folder], check=True, timeout=240)
    names = [f"Enemy{i:02}" for i in range(12)] + [f"Loot{i:02}" for i in range(16)]
    tasks = []
    for name in names:
        task = unreal.AssetImportTask()
        task.filename = str(Path(folder) / f"{name}.fbx")
        task.destination_path = destination + "/DungeonAssets"
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = options
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    missing = [name for name in names if not isinstance(
        unreal.EditorAssetLibrary.load_asset(destination + "/DungeonAssets/" + name), unreal.StaticMesh)]
    if missing:
        raise RuntimeError(f"Missing required dungeon meshes: {missing}")
unreal.EditorAssetLibrary.save_directory(destination + "/DungeonAssets", only_if_is_dirty=False)
print("SETTLEMENT_PEOPLE_IMPORT_SUCCESS")
