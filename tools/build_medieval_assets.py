import bpy, math, os, sys, random
from mathutils import Vector
out=os.path.abspath(sys.argv[sys.argv.index("--")+1])
os.makedirs(out,exist_ok=True)
random.seed(42)
def mat(name,color,rough=.8,metal=0):
 m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
 p=m.node_tree.nodes.get("Principled BSDF"); p.inputs["Base Color"].default_value=(*color,1); p.inputs["Roughness"].default_value=rough; p.inputs["Metallic"].default_value=metal
 return m
wood=mat("Weathered oak",(.16,.075,.035)); plaster=mat("Lime plaster",(.68,.56,.39)); stone=mat("Limestone",(.32,.31,.28)); clay=[mat("Terracotta %d"%i,(.30+i*.035,.105+i*.012,.045)) for i in range(5)]
iron=mat("Forged iron",(.24,.26,.28),.38,.8); glass=mat("Amber glass",(.55,.33,.08),.3); leather=mat("Leather",(.22,.09,.04))
def box(n,p,d,m,rot=(0,0,0),bevel=.012):
 bpy.ops.mesh.primitive_cube_add(size=1,location=p); o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.rotation_euler=rot;o.data.materials.append(m)
 if bevel:
  b=o.modifiers.new("Rounded edges","BEVEL");b.width=bevel;b.segments=2
  o.modifiers.new("Weighted normals","WEIGHTED_NORMAL")
 return o
def beam(n,a,b,w,m):
 a,b=Vector(a),Vector(b);o=box(n,(a+b)/2,(w,w,(b-a).length),m);o.rotation_euler=(b-a).to_track_quat("Z","Y").to_euler()
def reset(): bpy.ops.object.select_all(action="SELECT");bpy.ops.object.delete(use_global=False)
def export(n):
 bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out,n+".blend"))
 bpy.ops.export_scene.fbx(filepath=os.path.join(out,n+".fbx"),apply_unit_scale=True,axis_forward="-Y",axis_up="Z",bake_anim=False)
reset()
# Four metre cottage, open doorway, solid roof tiles and hollow chimney.
for x in (-2.1,2.1): box("Side wall",(x,0,1.35),(.18,4.2,2.7),plaster)
box("Rear wall",(0,2.1,1.35),(4.2,.18,2.7),plaster)
for x in (-1.38,1.38): box("Front bay",(x,-2.1,1.35),(1.44,.18,2.7),plaster)
box("Door lintel",(0,-2.1,2.48),(1.32,.18,.44),plaster)
box("Floor",(0,0,.05),(4.2,4.2,.1),wood)
for y in (-2.21,2.21):
 for x in (-2.05,-.68,.68,2.05): beam("Oak upright",(x,y,.1),(x,y,2.7),.13,wood)
 for z in (.3,1.4,2.65): beam("Oak rail",(-2.1,y,z),(2.1,y,z),.14,wood)
 for side in (-1,1): beam("Diagonal brace",(side*.7,y,1.4),(side*2,y,2.65),.12,wood)
 for row in range(3):
  for col in range(12):
   x=-1.95+col*.35
   if y<0 and abs(x)<.75: continue
   box("Foundation stone",(x,y,.12+row*.18),(.33,.16,.16),stone)
for side in (-1,1):
 angle=math.atan2(1.05,2.1)
 for row in range(11):
  y=side*(row+.5)*.205;z=3.75-abs(y)*.5
  for col in range(15):
   x=-2.24+col*.32+(row%2)*.08
   box("Clay shingle",(x,y,z),(.34,.26,.055),random.choice(clay),(side*-angle,0,0),.008)
 for x in (-2.2,2.2): beam("Gable rafter",(x,0,3.78),(x,side*2.3,2.6),.17,wood)
for side in (-1,1):
 box("Chimney wall",(1.25+side*.61,.65,3.85),(.16,1.38,1.6),stone)
 box("Chimney wall",(1.25,.65+side*.61,3.85),(1.06,.16,1.6),stone)
for x in (-1.4,1.4):
 box("Window glass",(x,-2.22,1.65),(.65,.04,.8),glass)
 for dx in (-.34,0,.34): box("Window mullion",(x+dx,-2.26,1.65),(.045,.07,.85),wood)
 for z in (1.23,1.65,2.07): box("Window rail",(x,-2.26,z),(.72,.07,.045),wood)
 box("Flower box",(x,-2.35,1.13),(.8,.28,.22),wood)
export("H5_Cottage")
reset()
for x in (-.35,.35): box("Stand foot",(x,0,.05),(.5,.45,.1),wood)
box("Stand post",(0,0,.85),(.1,.1,1.7),wood)
def sphere(n,p,d,m):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=p);o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(m)
 for f in o.data.polygons:f.use_smooth=True
sphere("Breastplate",(0,0,1.27),(.65,.37,.68),iron)
sphere("Helmet",(0,0,1.86),(.34,.36,.42),iron)
box("Visor slit",(0,-.18,1.87),(.25,.025,.045),wood)
for side in (-1,1):
 sphere("Pauldron",(side*.38,0,1.5),(.27,.33,.24),iron)
 for i in range(5): box("Armor lame",(side*.36,0,1.33-i*.075),(.18,.23,.085),iron)
export("A2_ArmorStand")
reset()
for i in range(4): box("Shelf",(0,0,.2+i*.48),(1.8,.48,.055),wood)
for x in (-.87,.87): box("Shelf upright",(x,0,.95),(.08,.48,1.9),wood)
export("A2_Shelves")
