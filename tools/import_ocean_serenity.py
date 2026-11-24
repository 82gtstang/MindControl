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
clips=("Idle","Walk","Run","WalkDiagonal","RunDiagonal","Jump","Attack","Spin","Swim")
unreal.EditorAssetLibrary.save_directory(destination,only_if_is_dirty=False,recursive=True)
for clip in clips:
    path=destination+"/OceanSerenityOcean_"+clip
    sequence=unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(sequence,unreal.AnimSequence):
        raise RuntimeError("Required Ocean animation missing after import: "+path)
print("OCEAN_IMPORT_SUCCESS "+expected+" animations="+",".join(clips))


# Generate and import original game audio before Android cooking.
import runpy
import sys
import tempfile
from pathlib import Path
audio_script = Path(__file__).with_name("generate_fantasy_audio.py")
with tempfile.TemporaryDirectory(prefix="mindcontrol_audio_") as audio_dir:
    old_argv = sys.argv[:]
    try:
        sys.argv = [str(audio_script), audio_dir]
        runpy.run_path(str(audio_script), run_name="__main__")
    finally:
        sys.argv = old_argv
    audio_tasks = []
    for wav in sorted(Path(audio_dir).glob("*.wav")):
        audio_task = unreal.AssetImportTask()
        audio_task.set_editor_property("filename", str(wav))
        audio_task.set_editor_property("destination_path", "/Game/Environment/MindControl/Audio")
        audio_task.set_editor_property("automated", True)
        audio_task.set_editor_property("replace_existing", True)
        audio_task.set_editor_property("save", True)
        audio_tasks.append(audio_task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(audio_tasks)
    for audio_task in audio_tasks:
        if not audio_task.imported_object_paths:
            raise RuntimeError("Audio import failed: " + audio_task.filename)
    print("MINDCONTROL_AUDIO_IMPORT_SUCCESS", len(audio_tasks))

# Mobile Sky Atmosphere requires an opaque, unlit IsSky surface.
sky_dir = "/Game/Environment/MindControl"
sky_path = sky_dir + "/M_MobileAtmosphereSky"
sky = unreal.EditorAssetLibrary.load_asset(sky_path) if unreal.EditorAssetLibrary.does_asset_exist(sky_path) else asset_tools.create_asset("M_MobileAtmosphereSky", sky_dir, unreal.Material, unreal.MaterialFactoryNew())
if not sky:
    raise RuntimeError("Unable to create mobile sky material")
sky.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
sky.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
sky.set_editor_property("is_sky", True)
sky.set_editor_property("two_sided", True)
mel = unreal.MaterialEditingLibrary
mel.delete_all_material_expressions(sky)
luminance = mel.create_material_expression(sky, unreal.MaterialExpressionSkyAtmosphereViewLuminance, -300, 0)
if not mel.connect_material_property(luminance, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Unable to connect sky atmosphere luminance")
mel.recompile_material(sky)
unreal.EditorAssetLibrary.save_loaded_asset(sky, only_if_is_dirty=False)
print("MOBILE_SKY_MATERIAL_SUCCESS", sky_path)
