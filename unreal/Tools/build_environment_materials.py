"""Create the mobile environment materials before the Android cook.

Source photographs: Poly Haven, CC0 (polyhaven.com/a/sparse_grass and
polyhaven.com/a/stone_wall). Keep the downloads out of the shipped game.
"""
import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.request import urlopen

import unreal


DESTINATION = "/Game/Environment/MindControl"
CDN = "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k"
MATERIALS = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def texture(directory, asset, channel, checksum):
    url = f"{CDN}/{asset}/{asset}_{channel}_1k.jpg"
    with urlopen(url, timeout=45) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != checksum:
        raise RuntimeError(f"Unexpected texture checksum for {asset}/{channel}")
    name = f"T_{asset}_{'N' if channel == 'nor_dx' else 'BaseColor'}"
    source = Path(directory) / f"{name}.jpg"
    source.write_bytes(data)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.factory = unreal.TextureFactory()
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    imported = ASSETS.load_asset(f"{DESTINATION}/{name}")
    if not isinstance(imported, unreal.Texture2D):
        raise RuntimeError(f"Texture import failed: {name}")
    if channel == "nor_dx":
        imported.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        imported.set_editor_property("srgb", False)
    return imported


def material(name):
    path = f"{DESTINATION}/{name}"
    result = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else TOOLS.create_asset(
        name, DESTINATION, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not result:
        raise RuntimeError(f"Material creation failed: {path}")
    MATERIALS.delete_all_material_expressions(result)
    roughness = node(result, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.9)
    output(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    return result


def node(material_asset, kind):
    return MATERIALS.create_material_expression(material_asset, kind)


def connect(source, outlet, target, inlet):
    if not MATERIALS.connect_material_expressions(source, outlet, target, inlet):
        raise RuntimeError(f"Could not connect material input {inlet}")


def output(source, outlet, target):
    if not MATERIALS.connect_material_property(source, outlet, target):
        raise RuntimeError(f"Could not connect material property {target}")


def grass_material(color, normal):
    asset = material("M_Grass")
    uv = node(asset, unreal.MaterialExpressionTextureCoordinate)
    uv.set_editor_property("u_tiling", 44.0)
    uv.set_editor_property("v_tiling", 28.0)
    for image, sampler, property_name in (
        (color, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialProperty.MP_BASE_COLOR),
        (normal, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialProperty.MP_NORMAL),
    ):
        sample = node(asset, unreal.MaterialExpressionTextureSample)
        sample.set_editor_property("texture", image)
        sample.set_editor_property("sampler_type", sampler)
        connect(uv, "", sample, "")
        output(sample, "RGB", property_name)
    MATERIALS.recompile_material(asset)


def projected_material(color, name, size):
    asset = material(name)
    position = node(asset, unreal.MaterialExpressionWorldPosition)
    normal = node(asset, unreal.MaterialExpressionPixelNormalWS)
    photo = node(asset, unreal.MaterialExpressionTextureObject)
    photo.set_editor_property("texture", color)
    projection = node(asset, unreal.MaterialExpressionCustom)
    projection.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom_inputs = []
    for name in ("P", "N", "Stone"):
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        custom_inputs.append(custom_input)
    projection.set_editor_property("inputs", custom_inputs)
    projection.set_editor_property("code", """
float3 coordinates = P / TILING;
float3 weights = pow(abs(N), 4.0);
weights /= max(dot(weights, float3(1, 1, 1)), 0.001);
return Texture2DSample(Stone, StoneSampler, coordinates.yz).rgb * weights.x
     + Texture2DSample(Stone, StoneSampler, coordinates.xz).rgb * weights.y
     + Texture2DSample(Stone, StoneSampler, coordinates.xy).rgb * weights.z;
""".replace("TILING", str(float(size))))
    for expression, input_name in ((position, "P"), (normal, "N"), (photo, "Stone")):
        connect(expression, "", projection, input_name)
    output(projection, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MATERIALS.recompile_material(asset)


def water_material(name, waterfall, pool=False):
    asset = material(name)
    uv = node(asset, unreal.MaterialExpressionTextureCoordinate)
    time = node(asset, unreal.MaterialExpressionTime)
    waves = node(asset, unreal.MaterialExpressionCustom)
    waves.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for label in ("UV", "Clock"):
        item = unreal.CustomInput()
        item.set_editor_property("input_name", label)
        inputs.append(item)
    waves.set_editor_property("inputs", inputs)
    waves.set_editor_property("code", """
float t = Clock * SPEED;
float crossflow = sin(UV.x * 47.0 + UV.y * 6.0 - t);
float ripples = sin(UV.y * 10.0 - t * 2.0 + crossflow * 0.65);
float edge = smoothstep(0.72, 0.99, abs(UV.x * 2.0 - 1.0));
float foam = smoothstep(0.72, 0.98, ripples * 0.58 + crossflow * 0.42 + BIAS);
float3 body = lerp(float3(0.022, 0.095, 0.12), float3(0.10, 0.33, 0.36), ripples * 0.28 + 0.45);
return lerp(body, float3(0.72, 0.87, 0.85), saturate(foam * 0.82 + edge * 0.65));
""".replace("SPEED", "4.8" if waterfall else "1.8").replace("BIAS", "0.42" if waterfall else "0.0"))
    connect(uv, "", waves, "UV")
    connect(time, "", waves, "Clock")
    output(waves, "", unreal.MaterialProperty.MP_BASE_COLOR)
    if pool:
        asset.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        asset.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        asset.set_editor_property("two_sided", True)
        opacity = node(asset, unreal.MaterialExpressionConstant)
        opacity.set_editor_property("r", 0.48)
        output(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        output(waves, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MATERIALS.recompile_material(asset)


with TemporaryDirectory() as directory:
    grass = texture(directory, "sparse_grass", "diff", "ae94f2b34597b9108eefd88217f55eccaec6d6b382e858a478ee92df90e66617")
    grass_normal = texture(directory, "sparse_grass", "nor_dx", "52bf18e78e2b24c9d0900012ba4c70ee1805fa2ddd32dfc87a7bcc127324af33")
    stone = texture(directory, "stone_wall", "diff", "390969aead48d9a10ac731a5e0af8761cc4ed89faa02ba775b913930b4398d87")
    grass_material(grass, grass_normal)
    cobble = texture(directory, "cobblestone_floor_001", "diff", "893d3e7b4506c5935cbc6aca5b914d637bd4464484bcaa5d69e2ee205bb9ae9c")
    dirt = texture(directory, "dirt", "diff", "6ce6ca7e28ed046f7f93fa60dcb29f8cf42cd5a539adc5676675acccfa34efd3")
    projected_material(stone, "M_CastleStone", 275)
    projected_material(cobble, "M_Cobblestone", 210)
    projected_material(dirt, "M_Dirt", 320)
    water_material("M_River", False)
    water_material("M_Waterfall", True)
    water_material("M_GravehollowPool", False, pool=True)
    satchel_task = unreal.AssetImportTask()
    satchel_task.filename = str(Path(__file__).resolve().parents[1] / "Art" / "T_Satchel.png")
    satchel_task.destination_path = DESTINATION
    satchel_task.factory = unreal.TextureFactory()
    satchel_task.automated = True
    satchel_task.replace_existing = True
    satchel_task.save = False
    TOOLS.import_asset_tasks([satchel_task])
    satchel = ASSETS.load_asset(f"{DESTINATION}/T_Satchel")
    if not isinstance(satchel, unreal.Texture2D):
        raise RuntimeError("Could not import the satchel art")
    satchel.set_editor_property("never_stream", True)
    if not ASSETS.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("Could not save the environment materials")
