"""Import Ocean Serenity rigged FBX in Unreal Editor commandlet (UE 5.7).
Executed by UnrealEditor-Cmd.exe -run=pythonscript -script=<this file>.
"""
import os
import unreal

fbx=os.environ.get("OCEAN_FBX")
if not fbx or not os.path.isfile(fbx):
    raise RuntimeError("OCEAN_FBX missing or nonexistent")
destination="/Game/Characters/OceanSerenity"
asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
task=unreal.AssetImportTask()
task.filename=fbx
task.destination_path=destination
task.automated=True
task.replace_existing=True
task.save=True
options=unreal.FbxImportUI()
options.set_editor_property("import_mesh",True)
options.set_editor_property("import_as_skeletal",True)
options.set_editor_property("import_animations",True)
options.set_editor_property("import_materials",True)
options.set_editor_property("import_textures",True)
options.set_editor_property("mesh_type_to_import",unreal.FBXImportType.FBXIT_SKELETAL_MESH)
options.skeletal_mesh_import_data.set_editor_property("import_morph_targets", True)
options.anim_sequence_import_data.set_editor_property("use_default_sample_rate", False)
options.anim_sequence_import_data.set_editor_property("custom_sample_rate", 60)
task.options=options
asset_tools.import_asset_tasks([task])
meshes=[]
for path in task.imported_object_paths:
    obj=unreal.EditorAssetLibrary.load_asset(path)
    if isinstance(obj,unreal.SkeletalMesh):
        meshes.append(obj)
if not meshes:
    raise RuntimeError("Unreal FBX import produced no skeletal mesh; imported: "+repr(task.imported_object_paths))
mesh=meshes[0]
# Runtime constructor expects this exact path; imported name can vary.
expected=destination+"/OceanSerenity"
if mesh.get_path_name()!=expected:
    renamed=unreal.EditorAssetLibrary.rename_asset(mesh.get_path_name(),expected)
    if not renamed:
        raise RuntimeError("Unable to rename imported skeletal mesh to "+expected)
# Keep the imported animation names used by the runtime constructor and cook.
clips=("Idle","Walk","Run","WalkDiagonal","RunDiagonal","Jump","Attack","Spin")
unreal.EditorAssetLibrary.save_directory(destination,only_if_is_dirty=False,recursive=True)
for clip in clips:
    path=destination+"/OceanSerenityOcean_"+clip
    sequence=unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(sequence,unreal.AnimSequence):
        raise RuntimeError("Required Ocean animation missing after import: "+path)
print("OCEAN_IMPORT_SUCCESS "+expected+" animations="+",".join(clips))
