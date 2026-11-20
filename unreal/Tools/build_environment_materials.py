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


def stone_material(color):
    asset = material("M_CastleStone")
    position = node(asset, unreal.MaterialExpressionWorldPosition)
    normal = node(asset, unreal.MaterialExpressionPixelNormalWS)
    photo = node(asset, unreal.MaterialExpressionTextureObject)
    photo.set_editor_property("texture", color)
    projection = node(asset, unreal.MaterialExpressionCustom)
    projection.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    projection.set_editor_property("inputs", [unreal.CustomInput(input_name=name) for name in ("P", "N", "Stone")])
    projection.set_editor_property("code", """
float3 coordinates = P / 275.0;
float3 weights = pow(abs(N), 4.0);
weights /= max(dot(weights, float3(1, 1, 1)), 0.001);
return Texture2DSample(Stone, StoneSampler, coordinates.yz).rgb * weights.x
     + Texture2DSample(Stone, StoneSampler, coordinates.xz).rgb * weights.y
     + Texture2DSample(Stone, StoneSampler, coordinates.xy).rgb * weights.z;
""")
    for expression, input_name in ((position, "P"), (normal, "N"), (photo, "Stone")):
        connect(expression, "", projection, input_name)
    output(projection, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MATERIALS.recompile_material(asset)


with TemporaryDirectory() as directory:
    grass = texture(directory, "sparse_grass", "diff", "ae94f2b34597b9108eefd88217f55eccaec6d6b382e858a478ee92df90e66617")
    grass_normal = texture(directory, "sparse_grass", "nor_dx", "52bf18e78e2b24c9d0900012ba4c70ee1805fa2ddd32dfc87a7bcc127324af33")
    stone = texture(directory, "stone_wall", "diff", "390969aead48d9a10ac731a5e0af8761cc4ed89faa02ba775b913930b4398d87")
    grass_material(grass, grass_normal)
    stone_material(stone)
    if not ASSETS.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True):
        raise RuntimeError("Could not save the environment materials")
