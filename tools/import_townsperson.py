"""Import the townsperson mesh before cooking the Unreal build."""

from pathlib import Path

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
