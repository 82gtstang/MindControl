"""Bake lightweight settlement variations from MindControl's authored town mesh."""

import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix

source, output = map(Path, sys.argv[sys.argv.index("--") + 1:])
output.mkdir(parents=True, exist_ok=True)
variants = (
    ("Villager", .98, (0.50, .27, .155), (.095, .24, .15)),
    ("Artisan", 1.13, (.19, .085, .044), (.33, .115, .059)),
    ("Elf", .87, (.56, .39, .29), (.16, .23, .32)),
    ("Dwarf", 1.20, (.36, .18, .10), (.28, .19, .10)),
    ("Halfling", 1.04, (.72, .45, .29), (.31, .23, .09)),
    ("Wizard", 1.02, (.44, .25, .16), (.185, .075, .28)),
)


def paint(material, color):
    material.diffuse_color = (*color, 1)
    if material.use_nodes:
        shader = next((node for node in material.node_tree.nodes
                       if node.type == "BSDF_PRINCIPLED"), None)
        if shader:
            shader.inputs["Base Color"].default_value = (*color, 1)


for name, width, skin, cloth in variants:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    person, = (obj for obj in bpy.context.scene.objects if obj.type == "MESH")
    # Work in meters and keep the feet on the same origin for every frame.
    person.data.transform(person.matrix_world)
    person.matrix_world = Matrix.Identity(4)
    points = [vertex.co.copy() for vertex in person.data.vertices]
    for material in person.data.materials:
        if material.name == "Skin":
            paint(material, skin)
        elif material.name in {"Forest wool", "Hood lining", "Oat linen"}:
            factor = {"Forest wool": 1, "Hood lining": .52, "Oat linen": 1.48}[material.name]
            paint(material, tuple(value * factor for value in cloth))
        material.name = f"{name}_{material.name}"
    for frame in range(5):
        # Frame zero is neutral. Frames one to four sample a complete walk cycle.
        wave = 0 if frame == 0 else math.cos((frame - 1) * math.pi / 2)
        for vertex, point in zip(person.data.vertices, points):
            x, y, z = point
            side = math.copysign(1, x)
            split = min(1, abs(x) / .065)
            lower = max(0, min(1, (.77 - z) / .60))
            arm = max(0, min(1, (abs(x) - .145) / .09)) * max(0, min(1, (1.34-z)/.4))
            leg_forward = wave * side * split
            y += lower * .145 * leg_forward - arm * .10 * wave * side
            if frame:
                lift = max(0, math.sin((frame - 1) * math.pi / 2) * side)
                z += .065 * lift * lower
            head = max(0, min(1, (z - 1.34) / .14))
            head_width = {"Elf": .87, "Dwarf": 1.14, "Halfling": 1.19}.get(name, 1)
            vertex.co = (x * (width * (1 - head) + head_width * head), y, z)
        bpy.ops.export_scene.fbx(filepath=str(output / f"{name}{frame}.fbx"),
                                 use_selection=True, bake_anim=False, add_leaf_bones=False)
