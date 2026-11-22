"""Import the townsperson mesh before cooking the Unreal build."""

from pathlib import Path
import json
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
def apply_enemy_surfaces(folder, names, directory):
    """Author shared mobile shaders and assign surface profiles to each FBX slot."""
    profiles = json.loads((Path(folder) / "enemy_surfaces.json").read_text(encoding="utf-8"))
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets = unreal.EditorAssetLibrary
    graph = unreal.MaterialEditingLibrary
    name = "M_EnemySurface"
    path = f"{directory}/{name}"
    master = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
        name, directory, unreal.Material, unreal.MaterialFactoryNew())
    if not master:
        raise RuntimeError(f"Could not create {path}")
    graph.delete_all_material_expressions(master)

    def expression(kind):
        return graph.create_material_expression(master, kind)

    def connect(source, target, input_name):
        if not graph.connect_material_expressions(source, "", target, input_name):
            raise RuntimeError(f"Enemy material connection failed for {input_name}")

    def output(source, surface_property):
        if not graph.connect_material_property(source, "", surface_property):
            raise RuntimeError(f"Enemy material output failed for {surface_property}")

    uv = expression(unreal.MaterialExpressionTextureCoordinate)
    tint = expression(unreal.MaterialExpressionVectorParameter)
    tint.set_editor_property("parameter_name", unreal.Name("SurfaceColor"))
    rough = expression(unreal.MaterialExpressionScalarParameter)
    rough.set_editor_property("parameter_name", unreal.Name("SurfaceRoughness"))
    rough.set_editor_property("default_value", .65)
    metal = expression(unreal.MaterialExpressionScalarParameter)
    metal.set_editor_property("parameter_name", unreal.Name("SurfaceMetallic"))
    detail = expression(unreal.MaterialExpressionScalarParameter)
    detail.set_editor_property("parameter_name", unreal.Name("SurfaceDetail"))
    detail.set_editor_property("default_value", .15)

    def pattern(code, inputs):
        custom = expression(unreal.MaterialExpressionCustom)
        custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        connections = []
        for label, _ in inputs:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", label)
            connections.append(entry)
        custom.set_editor_property("inputs", connections)
        custom.set_editor_property("code", code)
        for label, source in inputs:
            connect(source, custom, label)
        return custom

    color = pattern("""
float warp = sin(UV.y * 39.0 + sin(UV.x * 17.0)) * 0.33;
float grain = sin(UV.x * 173.0 + warp) * sin(UV.y * 137.0 - warp);
float mottling = sin(UV.x * 31.0 + UV.y * 11.0) * cos(UV.y * 29.0 - UV.x * 14.0);
float stains = sin(UV.x * 58.0 + sin(UV.y * 27.0)) * cos(UV.y * 71.0);
return saturate(Tint.rgb * (1.0 + Detail * (0.96 * mottling + 0.61 * stains + 0.32 * grain)));
""", [("UV", uv), ("Tint", tint), ("Detail", detail)])
    normal = pattern("""
float warp = sin(UV.y * 39.0 + sin(UV.x * 17.0));
float u = sin(UV.x * 107.0 + warp) + cos(UV.y * 148.0 - UV.x * 23.0) * 0.46;
float v = cos(UV.y * 123.0 - warp) + sin(UV.x * 137.0 + UV.y * 31.0) * 0.40;
return normalize(float3(u * Detail, v * Detail, 1.0));
""", [("UV", uv), ("Detail", detail)])
    output(color, unreal.MaterialProperty.MP_BASE_COLOR)
    output(normal, unreal.MaterialProperty.MP_NORMAL)
    output(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    output(metal, unreal.MaterialProperty.MP_METALLIC)
    graph.recompile_material(master)

    instances = {}
    for name in names:
        mesh = assets.load_asset(f"{directory}/{name}")
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            imported_name = str(slot.imported_material_slot_name)
            slot_name = str(slot.material_slot_name)
            key = next((key for key in (imported_name, slot_name) if key in profiles), None)
            if key is None:
                raise RuntimeError(f"Unknown enemy surface {name}/{imported_name}/{slot_name}")
            if key not in instances:
                entry = profiles[key]
                instance_name = f"MI_Enemy_{key}"
                path = f"{directory}/{instance_name}"
                instance = assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(
                    instance_name, directory, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                if not instance:
                    raise RuntimeError(f"Could not create {path}")
                graph.set_material_instance_parent(instance, master)
                graph.set_material_instance_vector_parameter_value(
                    instance, unreal.Name("SurfaceColor"), unreal.LinearColor(*entry["color"], 1.0))
                graph.set_material_instance_scalar_parameter_value(
                    instance, unreal.Name("SurfaceRoughness"), entry["roughness"])
                graph.set_material_instance_scalar_parameter_value(
                    instance, unreal.Name("SurfaceMetallic"), entry["metallic"])
                strength = .045 if key.startswith("Eye") else .21 if key.startswith(("Hide", "Shell")) else .14
                graph.set_material_instance_scalar_parameter_value(instance, unreal.Name("SurfaceDetail"), strength)
                graph.update_material_instance(instance)
                instances[key] = instance
            mesh.set_material(index, instances[key])
        if not assets.save_loaded_asset(mesh, only_if_is_dirty=False):
            raise RuntimeError(f"Could not save enemy surface assignments for {name}")


with tempfile.TemporaryDirectory(prefix="mindcontrol-dungeon-assets-") as folder:
    subprocess.run([str(blender), "-b", "-t", "2", "--python-exit-code", "1", "--python",
                    str(Path(__file__).with_name("bake_dungeon_assets.py")),
                    "--", folder], check=True, timeout=240)
    enemies = [f"Enemy{i:02}" for i in range(20)]
    names = enemies + [f"Loot{i:02}" for i in range(16)]
    names += ["HeroBow", "HeroArmor", "HeroQuiver"]
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
    apply_enemy_surfaces(folder, enemies, destination + "/DungeonAssets")
unreal.EditorAssetLibrary.save_directory(destination + "/DungeonAssets", only_if_is_dirty=False)
print("SETTLEMENT_PEOPLE_IMPORT_SUCCESS")
