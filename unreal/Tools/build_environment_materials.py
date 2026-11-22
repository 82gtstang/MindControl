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


def animated_expression(asset, uv, time, code, with_splash=False):
    expression = node(asset, unreal.MaterialExpressionCustom)
    expression.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    sources = [(uv, "", "UV"), (time, "", "Clock")]
    if with_splash:
        sources.append((node(asset, unreal.MaterialExpressionVertexColor), "R", "Splash"))
    inputs = []
    for _, _, label in sources:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", label)
        inputs.append(item)
    expression.set_editor_property("inputs", inputs)
    expression.set_editor_property("code", code)
    for source, outlet, label in sources:
        connect(source, outlet, expression, label)
    return expression


def water_material(name, waterfall):
    asset = material(name)
    uv = node(asset, unreal.MaterialExpressionTextureCoordinate)
    time = node(asset, unreal.MaterialExpressionTime)
    speed = "2.45" if waterfall else "0.59"
    waves = animated_expression(asset, uv, time, """
float2 p = float2(UV.x, UV.y - Clock * SPEED);
float bend = sin(p.y * 7.1 + p.x * 8.0) * 0.13;
float ribbons = sin((p.x + bend) * 31.0 + p.y * 3.3);
float ripples = sin(p.y * 22.0 + p.x * 16.0 + ribbons * 0.9);
float grain = sin(p.y * 60.0 - p.x * 56.0) * sin(p.y * 37.0 + p.x * 65.0);
float edge = smoothstep(0.72, 0.98, abs(p.x * 2.0 - 1.0));
float strands = smoothstep(0.52, 0.94, ribbons * 0.58 + ripples * 0.28 + grain * 0.14 + WHITEWATER);
float bankFoam = edge * smoothstep(0.12, 0.86, ripples * 0.55 + grain * 0.30 + 0.25);
float plunge = Splash * smoothstep(-0.2, 0.82, ribbons * 0.36 + ripples * 0.30 + grain * 0.34 + 0.2);
float3 body = lerp(float3(0.019,0.083,0.099), float3(0.085,0.25,0.29),
    saturate(0.43 + ribbons * 0.19 + ripples * 0.12 + edge * 0.2));
return lerp(body, float3(0.74,0.86,0.84), saturate(strands * FOAM + bankFoam * 0.44 + plunge * 0.82));
""".replace("SPEED", speed).replace("WHITEWATER", "0.29" if waterfall else "0.0").replace(
        "FOAM", "0.79" if waterfall else "0.36"), with_splash=True)
    normal = animated_expression(asset, uv, time, """
float y = UV.y - Clock * SPEED;
float crosswave = cos(UV.x * 24.0 + y * 7.5) * 0.16;
float longitudinal = cos(y * 21.0 + UV.x * 12.0) * 0.13;
float fine = cos(y * 43.0 - UV.x * 26.0) * 0.052;
return normalize(float3(crosswave - fine, longitudinal + fine, 1.0));
""".replace("SPEED", speed))
    roughness = node(asset, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.35 if waterfall else 0.22)
    output(waves, "", unreal.MaterialProperty.MP_BASE_COLOR)
    output(normal, "", unreal.MaterialProperty.MP_NORMAL)
    output(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MATERIALS.recompile_material(asset)


def overworld_material(grass, dirt, stone, cobble):
    asset = material("M_Overworld")
    position = node(asset, unreal.MaterialExpressionWorldPosition)
    masks = node(asset, unreal.MaterialExpressionVertexColor)
    inputs = [(position, "", "P"), (masks, "", "Masks")]
    for image, label in ((grass, "Grass"), (dirt, "Dirt"), (stone, "Rock"), (cobble, "Gravel")):
        sampler = node(asset, unreal.MaterialExpressionTextureObject)
        sampler.set_editor_property("texture", image)
        inputs.append((sampler, "", label))
    blend = node(asset, unreal.MaterialExpressionCustom)
    blend.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom_inputs = []
    for _, _, label in inputs:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", label)
        custom_inputs.append(item)
    blend.set_editor_property("inputs", custom_inputs)
    blend.set_editor_property("code", """
float2 xy = P.xy;
float variation = sin(xy.x * 0.0023 + sin(xy.y * 0.003)) * cos(xy.y * 0.0027 - xy.x * 0.0008);
float3 meadow = Texture2DSample(Grass, GrassSampler, xy / 535.0).rgb * (1.07 + variation * 0.12);
float3 soil = Texture2DSample(Dirt, DirtSampler, xy / 295.0).rgb;
float3 rock = Texture2DSample(Rock, RockSampler, xy / 365.0 + P.z * float2(0.0016,0.0011)).rgb;
float3 gravel = Texture2DSample(Gravel, GravelSampler, xy / 89.0).rgb;
float3 woodland = soil * float3(0.61,0.47,0.34);
float3 moss = lerp(rock * 0.66, meadow * float3(0.72,0.93,0.52), 0.68);
woodland = lerp(woodland, moss, smoothstep(-0.22, 0.54, variation));
float3 ground = lerp(meadow, woodland, Masks.r);
ground = lerp(ground, rock * float3(0.77,0.75,0.70), Masks.g);
ground = lerp(ground, gravel * float3(0.75,0.76,0.73), Masks.b);
return lerp(ground, soil * float3(0.98,0.81,0.60), Masks.a);
""")
    for source, outlet, label in inputs:
        connect(source, outlet, blend, label)
    output(blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
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
    overworld_material(grass, dirt, stone, cobble)
    water_material("M_River", False)
    water_material("M_Waterfall", True)
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
