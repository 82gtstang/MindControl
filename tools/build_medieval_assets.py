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

reset()
# Rustic armory shell with open doorway and timber roof.
for x in (-3,3):
 box("Stone side wall",(x,0,1.6),(.3,5,3.2),stone)
box("Rear wall",(0,2.5,1.6),(6,.3,3.2),stone)
for x in (-1.95,1.95):
 box("Front stone bay",(x,-2.5,1.6),(2.1,.3,3.2),stone)
box("Lintel",(0,-2.5,2.85),(1.8,.35,.7),wood)
box("Stone floor",(0,0,-.08),(6,5,.16),stone)
for side in (-1,1):
 for row in range(12):
  y=side*(row+.5)*.23
  for col in range(20):
   box("Roof tile",(-3.15+col*.33,y,4.5-abs(y)*.5),(.35,.29,.06),random.choice(clay),(-side*math.atan(.5),0,0))
 for x in (-3,0,3): beam("Rafter",(x,0,4.5),(x,side*2.7,3.15),.19,wood)
export("A2_Armory")
reset()
sphere("Shield body",(0,0,.55),(.65,.12,.95),wood)
sphere("Iron boss",(0,-.075,.62),(.23,.12,.23),iron)
for x in (-.2,.2): box("Iron strap",(x,-.07,.55),(.055,.035,.65),iron)
export("A2_Shield")
reset()
box("Sword blade",(0,0,.64),(.065,.025,.85),iron,bevel=.009)
box("Crossguard",(0,0,.22),(.24,.05,.055),iron)
box("Leather grip",(0,0,.12),(.045,.045,.17),leather)
sphere("Pommel",(0,0,.025),(.075,.075,.075),iron)
export("Blacksmith_Sword")
reset()
box("Anvil base",(0,0,.12),(.55,.4,.24),iron)
box("Anvil waist",(0,0,.35),(.28,.25,.3),iron)
box("Anvil face",(0,0,.56),(.85,.32,.15),iron)
export("Blacksmith_Anvil")
reset()
sphere("Potion flask",(0,0,.12),(.19,.19,.23),glass)
box("Bottle neck",(0,0,.26),(.065,.065,.12),glass)
box("Cork",(0,0,.33),(.07,.07,.045),wood)
export("Sorcerer_Potion")

# Separate shop shells, open front forge and enclosed apothecary.
for name in ("Blacksmith","Sorcerer"):
 reset()
 for x in (-2.8,2.8): box("Side masonry",(x,0,1.5),(.25,5,3),stone)
 box("Back masonry",(0,2.5,1.5),(5.6,.25,3),stone)
 for x in (-2.65,2.65): box("Front oak post",(x,-2.5,1.5),(.22,.22,3),wood)
 if name=="Sorcerer":
  for x in (-1.9,1.9): box("Front plaster",(x,-2.5,1.5),(1.8,.2,3),plaster)
 box("Floor",(0,0,-.06),(5.6,5,.12),stone)
 for side in (-1,1):
  for row in range(12):
   y=side*(row+.5)*.23
   for col in range(19): box("Clay tile",(-2.95+col*.33,y,4.25-abs(y)*.46),(.35,.28,.06),random.choice(clay),(-side*math.atan(.46),0,0))
  for x in (-2.8,0,2.8): beam("Rafter",(x,0,4.25),(x,side*2.7,3),.16,wood)
 if name=="Blacksmith":
  box("Forge hearth",(1.7,1.65,.65),(1.5,1.3,1.3),stone)
  box("Forge chimney",(1.7,2,2.65),(.9,.9,3.4),stone)
  ember=mat("Hot coals",(.95,.16,.025))
  for i in range(16): sphere("Coal",(1.15+random.random()*1.1,1.2+random.random()*.7,1.34),(.14,.13,.08),ember)
 else:
  for i in range(4): box("Potion shelf",(0,2.25,.35+i*.5),(4.8,.4,.07),wood)
 export(name+"_Shop")
leaf=mat("Herb foliage",(.12,.36,.08))
for i,name in enumerate(("Lavender","Chamomile","Bloodleaf","Mint","Silverleaf","BlueSage","Firebloom","Moonflower","LemonBalm","Yarrow")):
 reset()
 colors=[(.45,.15,.65),(.85,.72,.12),(.75,.12,.16),(.22,.55,.25),(.65,.72,.85),(.28,.35,.85),(.95,.45,.12),(.72,.28,.55),(.55,.72,.32),(.85,.85,.72)]
 flower=mat(name,colors[i])
 for stem in range(3+i%3):
  a=stem*2.4; x=.035*math.cos(a);y=.035*math.sin(a);h=.19+.035*(i%4)+random.random()*.06
  beam("Stem",(0,0,0),(x,y,h),.006,leaf)
  for j in range(3):
   o=sphere("Leaf",(x+.025*math.cos(a+j*2),y+.025*math.sin(a+j*2),h*(.3+j*.19)),(.075,.025,.008),leaf)
  for petal in range(5+i%3):
   t=petal*2*math.pi/(5+i%3)
   sphere("Petal",(x+.016*math.cos(t),y+.016*math.sin(t),h),(.024,.018,.012),flower)
 export("Herb_"+name)
