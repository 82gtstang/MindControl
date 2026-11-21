"""Run in Blender to convert a textured TripoSR OBJ into an Unreal import bundle."""

import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np


def refine_face(mesh, correction):
    vertices = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", vertices)
    xyz = vertices.reshape(-1, 3)
    x, y, z = xyz.T
    with np.load(correction, allow_pickle=False) as data:
        cx, cy, fw, fh = data["face_box"]
        image_center, head_center, x_scale, crown, top, y_scale = data["mapping"]
        front, target, weight = data["front"], data["target"], data["weight"]
    perspective = (1.9 - x) * .728
    u, v = y / perspective, -z / perspective
    qx = (image_center + (u - head_center) * x_scale - cx) / fw + .5
    qy = (crown + (v - top) * y_scale - cy) / fh + .5
    valid = (qx >= 0) & (qx <= 1) & (qy >= 0) & (qy <= 1)
    ids = np.flatnonzero(valid)
    if ids.size == 0:
        return
    gx = qx[ids] * (front.shape[1] - 1)
    gy = qy[ids] * (front.shape[0] - 1)
    ix = np.minimum(gx.astype(int), front.shape[1] - 2)
    iy = np.minimum(gy.astype(int), front.shape[0] - 2)
    fx, fy = gx - ix, gy - iy

    def sample(grid):
        return ((1 - fy) * ((1 - fx) * grid[iy, ix] + fx * grid[iy, ix + 1])
                + fy * ((1 - fx) * grid[iy + 1, ix] + fx * grid[iy + 1, ix + 1]))

    visible = np.clip(1 - np.abs(x[ids] - sample(front)) / .029, 0, 1)
    adjustment = np.clip(sample(target) - x[ids], -.032, .032)
    adjustment *= visible * sample(weight) * .88
    x[ids] += adjustment
    mesh.vertices.foreach_set("co", vertices)
    mesh.update()
    print(f"Face geometry refined on {np.count_nonzero(np.abs(adjustment) > .0003)} vertices.", flush=True)


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

    correction = source / "face_surface.npz"
    if correction.is_file():
        refine_face(obj.data, correction)
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
