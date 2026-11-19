"""Import the approved Ocean Serenity Heroine skeletal FBX, not the legacy Meshy model.

Required: OCEAN_HEROINE_FBX points to a genuinely authored/approved rigged
character matching the reference. This script deliberately fails when absent.
"""
import os
import unreal

source=os.environ.get("OCEAN_HEROINE_FBX","")
if not source or not os.path.isfile(source):
    raise RuntimeError("Missing approved Ocean Serenity Heroine FBX. Do not substitute the legacy procedural or Meshy FBX.")
destination="/Game/Characters/OceanSerenityHeroine"
task=unreal.AssetImportTask()
task.filename=source
task.destination_path=destination
task.automated=True
task.replace_existing=True
task.save=True
options=unreal.FbxImportUI()
options.set_editor_property("import_mesh",True)
options.set_editor_property("import_as_skeletal",True)
options.set_editor_property("import_animations",False)
options.set_editor_property("import_materials",True)
options.set_editor_property("import_textures",True)
options.set_editor_property("mesh_type_to_import",unreal.FBXImportType.FBXIT_SKELETAL_MESH)
task.options=options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
meshes=[]
for path in task.imported_object_paths:
 asset=unreal.EditorAssetLibrary.load_asset(path)
 if isinstance(asset,unreal.SkeletalMesh):
  meshes.append(asset)
if len(meshes)!=1:
 raise RuntimeError("Expected exactly one imported skeletal mesh; got "+repr(task.imported_object_paths))
mesh=meshes[0]
expected=destination+"/OceanSerenityHeroine"
if mesh.get_path_name()!=expected:
 if not unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),expected):
  raise RuntimeError("Unable to rename skeletal mesh to "+expected)
if not unreal.EditorAssetLibrary.does_asset_exist(expected):
 raise RuntimeError("Heroine skeletal asset not saved: "+expected)
unreal.EditorAssetLibrary.save_directory(destination,only_if_is_dirty=False,recursive=True)
print("OCEAN_HEROINE_IMPORT_SUCCESS "+expected)
