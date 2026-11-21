"""Run in Blender to convert a textured TripoSR OBJ into an Unreal import bundle."""

import math
import sys
from pathlib import Path

import bmesh
import bpy

source, output = map(Path, sys.argv[sys.argv.index("--") + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
if bpy.app.version >= (3, 3, 0):
    bpy.ops.wm.obj_import(filepath=str(source / "mesh.obj"), forward_axis="NEGATIVE_Y", up_axis="Z")
else:
    bpy.ops.import_scene.obj(filepath=str(source / "mesh.obj"), axis_forward="-Y", axis_up="Z")
meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
if not meshes or not any(obj.data.polygons for obj in meshes):
    raise RuntimeError("Reconstruction produced no usable mesh")

texture = bpy.data.images.load(str(source / "texture.png"))
material = bpy.data.materials.new("Character_BaseColor")
material.use_nodes = True
nodes = material.node_tree.nodes
image = nodes.new("ShaderNodeTexImage")
image.image = texture
shader = nodes["Principled BSDF"]
material.node_tree.links.new(image.outputs["Color"], shader.inputs["Base Color"])
shader.inputs["Roughness"].default_value = 0.83
specular = shader.inputs.get("Specular IOR Level") or shader.inputs.get("Specular")
if specular is not None:
    specular.default_value = 0.14

bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
    if not obj.data.uv_layers:
        raise RuntimeError("Reconstruction is missing texture UVs")
    mesh = bmesh.new()
    try:
        mesh.from_mesh(obj.data)
        # The atlas splits vertices at UV seams. Weld coincident positions
        # while retaining each face corner's UV so normals remain continuous.
        bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=1e-6)
        bmesh.ops.recalc_face_normals(mesh, faces=list(mesh.faces))
        mesh.to_mesh(obj.data)
    finally:
        mesh.free()

    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    if obj.data.has_custom_normals:
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    # TripoSR emits Z-up coordinates with the reference camera at +X.
    obj.rotation_euler.z = -math.pi / 2
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    if len(obj.data.polygons) > 100:
        smooth = obj.modifiers.new("Surface polish", "LAPLACIANSMOOTH")
        smooth.iterations = 3
        smooth.lambda_factor = 0.18
        smooth.lambda_border = 0.0
        smooth.use_volume_preserve = True
        bpy.ops.object.modifier_apply(modifier=smooth.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    print(f"Smoothed upright mesh: {len(obj.data.polygons)} faces and {len(obj.data.uv_layers)} UV layers.", flush=True)
    obj.select_set(False)
    obj.data.materials.clear()
    obj.data.materials.append(material)

bpy.ops.export_scene.fbx(
    filepath=str(output),
    object_types={"MESH"},
    bake_anim=False,
    path_mode="COPY",
    embed_textures=True,
    mesh_smooth_type="FACE",
)
