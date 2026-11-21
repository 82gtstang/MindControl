"""Model terrain silhouettes from the MindControl entrance reference footage.

Coordinates are meters; the walkable opening faces negative Y in Blender.
The FBX origin sits at the center of the threshold.
"""
import math
import random
import sys
from pathlib import Path

import bpy

out = Path(sys.argv[sys.argv.index("--") + 1])
out.mkdir(parents=True, exist_ok=True)

def boulder(center, size, rng, roughness):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=1)
    obj = bpy.context.object
    for vertex in obj.data.vertices:
        p = vertex.co
        ripple = math.sin(12*p.x+7*p.z)*math.cos(9*p.y-5*p.z)
        scale = 1 + roughness*(.72*ripple + .28*rng.uniform(-1,1))
        vertex.co = tuple(center[i]+p[i]*size[i]*scale for i in range(3))
    for face in obj.data.polygons:
        face.use_smooth = True
    return obj

def slab(center, size, rng):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    bevel = obj.modifiers.new("Worn stone edges","BEVEL")
    bevel.width = .085
    bevel.segments = 2
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    for v in obj.data.vertices:
        v.co += type(v.co)((rng.uniform(-.035,.035),rng.uniform(-.035,.035),rng.uniform(-.035,.035)))
    return obj

for name in ("CaveMouth","CliffTomb","ForestBasin","RuneGate"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    rng = random.Random(name)
    pieces = []
    if name == "ForestBasin":
        for i in range(14):
            angle = math.radians(-30+i*18)
            pieces.append(boulder((2.0*math.cos(angle),1.75*math.sin(angle)+1.2,.25),
                                  (.72,.71,.47),rng,.26))
    elif name in ("CliffTomb","RuneGate"):
        for side in (-1,1):
            for course in range(4):
                pieces.append(slab((side*(1.49+.05*(course%2)),.34, .33+course*.63),
                                    (.67,.93,.64),rng))
        pieces.append(slab((0,.31,2.81),(3.65,1.06,.46),rng))
        pieces.append(boulder((0,1.48,3.09),(3.31,1.57,.94),rng,.22))
    else:
        for side in (-1,1):
            for course in range(5):
                height = .41+.60*course
                x = side*(2.20-.11*course)
                pieces.append(boulder((x,.76,height),(1.03,.94,.69),rng,.28))
        pieces.append(boulder((0,.97,3.17),(2.48,1.25,.72),rng,.29))
        for i in range(5):
            pieces.append(boulder((-.95+i*.51,.23,2.91-rng.random()*.09),
                                  (.17,.29,.26+rng.random()*.15),rng,.22))
    bpy.ops.object.select_all(action="DESELECT")
    for piece in pieces:
        piece.select_set(True)
    bpy.context.view_layer.objects.active = pieces[0]
    bpy.ops.object.join()
    result = bpy.context.object
    result.name = name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(out/f"{name}.fbx"),use_selection=True,
                             bake_anim=False,axis_forward="-Y",axis_up="Z")
