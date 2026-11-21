"""Run in Blender to convert a textured TripoSR OBJ into an Unreal import bundle."""

import sys
from pathlib import Path

import bpy

source, output = map(Path, sys.argv[sys.argv.index("--") + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(source / "mesh.obj"))
meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
if not meshes or not any(obj.data.polygons for obj in meshes):
    raise RuntimeError("Reconstruction produced no usable mesh")

texture = bpy.data.images.load(str(source / "texture.png"))
material = bpy.data.materials.new("Character_BaseColor")
material.use_nodes = True
nodes = material.node_tree.nodes
image = nodes.new("ShaderNodeTexImage")
image.image = texture
material.node_tree.links.new(image.outputs["Color"], nodes["Principled BSDF"].inputs["Base Color"])
for obj in meshes:
    if not obj.data.uv_layers:
        raise RuntimeError("Reconstruction is missing texture UVs")
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
