import bpy, math, os, sys, random
from mathutils import Vector
import numpy as np
out=os.path.abspath(sys.argv[sys.argv.index("--")+1])
os.makedirs(out,exist_ok=True)
random.seed(42)
def mat(name,color,rough=.8,metal=0):
 m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
 p=m.node_tree.nodes.get("Principled BSDF"); p.inputs["Base Color"].default_value=(*color,1); p.inputs["Roughness"].default_value=rough; p.inputs["Metallic"].default_value=metal
 if any(k in name.lower() for k in ("oak","plaster","limestone","terracotta","iron","leather")):
  n=1024; rng=np.random.default_rng(sum(map(ord,name)))
  y,x=np.mgrid[0:n,0:n].astype(np.float32)/n
  noise=rng.random((n,n),dtype=np.float32)-.5
  grain=np.sin(2*np.pi*(x*48+.6*np.sin(y*15)+.18*np.sin(y*73)))
  broad=np.sin(x*31+y*17)*np.cos(y*23-x*9)
  height=(.5+.16*grain+.09*broad+.13*noise) if "oak" in name.lower() else (.5+.12*broad+.23*noise+.06*np.sin(x*190)*np.cos(y*173))
  rgb=np.clip(np.asarray(color)[None,None,:]*(.65+.7*height[:,:,None]),0,1)
  gy,gx=np.gradient(height); normal=np.stack((-gx*2,-gy*2,np.ones_like(height)),axis=-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True);normal=normal*.5+.5
  for label,arr in (("Color",rgb),("Normal",normal)):
   im=bpy.data.images.new(name+" "+label,width=n,height=n,alpha=True)
   if label=="Normal": im.colorspace_settings.name="Non-Color"
   im.pixels.foreach_set(np.concatenate((arr,np.ones((n,n,1))),axis=-1).astype(np.float32).ravel());im.pack()
   tex=m.node_tree.nodes.new("ShaderNodeTexImage");tex.image=im
   if label=="Color": m.node_tree.links.new(tex.outputs["Color"],p.inputs["Base Color"])
   else:
    nm=m.node_tree.nodes.new("ShaderNodeNormalMap");nm.inputs["Strength"].default_value=.65;m.node_tree.links.new(tex.outputs["Color"],nm.inputs["Color"]);m.node_tree.links.new(nm.outputs["Normal"],p.inputs["Normal"])
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
 scene=bpy.context.scene
 bpy.context.view_layer.update()
 points=[o.matrix_world @ Vector(v) for o in scene.objects if o.type=="MESH" for v in o.bound_box]
 lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
 hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
 center=(lo+hi)/2;span=max((hi-lo).length,.1)
 bpy.ops.object.camera_add(location=center+Vector((1,-1.5,.85))*span)
 cam=bpy.context.object
 cam.rotation_euler=(center-cam.location).to_track_quat("-Z","Y").to_euler()
 cam.data.type="ORTHO";cam.data.ortho_scale=span*1.12;scene.camera=cam
 scene.render.engine="CYCLES"
 scene.cycles.samples=16
 scene.cycles.use_denoising=True
 scene.world.use_nodes=True
 scene.world.node_tree.nodes.get("Background").inputs[0].default_value=(.55,.55,.55,1)
 scene.world.node_tree.nodes.get("Background").inputs[1].default_value=.65
 bpy.ops.object.light_add(type="AREA",location=center+Vector((-.7,-1,1.5))*span)
 lamp=bpy.context.object;lamp.data.energy=180*span*span;lamp.data.shape="DISK";lamp.data.size=span;lamp.rotation_euler=(center-lamp.location).to_track_quat("-Z","Y").to_euler()
 scene.display.shading.light="STUDIO"
 scene.display.shading.color_type="MATERIAL"
 scene.display.shading.show_shadows=True
 scene.display.shading.show_cavity=True
 scene.render.resolution_x=1600;scene.render.resolution_y=1600;scene.render.resolution_percentage=100
 scene.render.image_settings.file_format="PNG";scene.render.filepath=os.path.join(out,n+".png")
 bpy.ops.render.render(write_still=True)
 bpy.data.objects.remove(cam,do_unlink=True)
 bpy.data.objects.remove(lamp,do_unlink=True)
 bpy.ops.export_scene.gltf(filepath=os.path.join(out,n+".glb"),export_format="GLB",export_materials="EXPORT")

 bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out,n+".blend"))
 bpy.ops.export_scene.fbx(filepath=os.path.join(out,n+".fbx"),apply_unit_scale=True,axis_forward="-Y",axis_up="Z",bake_anim=False)

def sphere(n,p,d,m):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=p);o=bpy.context.object;o.name=n;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(m)
 for f in o.data.polygons:f.use_smooth=True
def building(name,w,d,h,rise):
 reset()
 # Full-size timber and masonry structure with open entrance.
 for x in (-w/2,w/2):
  box("Side masonry",(x,0,h/2),(.32,d,h),stone)
 box("Rear masonry",(0,d/2,h/2),(w,.32,h),stone)
 door=1.5 if name=="H5_Cottage" else 2.4
 for side in (-1,1):
  box("Front wall",(side*(w+door)/4,-d/2,h/2),((w-door)/2,.32,h),plaster)
 box("Door header",(0,-d/2,(h+2.4)/2),(door,.32,h-2.4),wood)
 for j in range(int(d/.24)):
  box("Oak floor plank",(0,-d/2+.12+j*.24,.04),(w,.235,.08),wood)
 for y in (-d/2-.18,d/2+.18):
  for j in range(1+int(w/1.5)):
   x=-w/2+j*w/int(w/1.5)
   if y<0 and abs(x)<door/2: continue
   beam("Structural oak post",(x,y,.1),(x,y,h),.2,wood)
  for z in (.35,h*.55,h-.12):
   for side in (-1,1):
    beam("Horizontal timber",(side*door/2,y,z),(side*w/2,y,z),.18,wood)
  for side in (-1,1):
   beam("Diagonal timber",(side*door/2,y,h*.55),(side*w/2,y,h-.12),.16,wood)
 for side in (-1,1):
  angle=math.atan2(rise,d/2)
  box("Roof deck",(0,side*d/4,h+rise/2),(w+.65,math.hypot(d/2,rise)+.4,.16),wood,(-side*angle,0,0))
  for row in range(int((d/2+.3)/.32)+1):
   y=side*(row+.5)*.32
   for col in range(int((w+.65)/.42)+1):
    x=-w/2-.3+col*.42+(row%2)*.12
    box("Overlapping fired clay tile",(x,y,h+rise-abs(y)*rise/(d/2)+.12),(.43,.46,.065),random.choice(clay),(-side*angle,0,0),.008)
 for x in (-w/2,w/2):
  verts=[(x,-d/2,h),(x,d/2,h),(x,0,h+rise)]
  mesh=bpy.data.meshes.new("Gable");mesh.from_pydata(verts,[],[(0,1,2)]);mesh.update()
  ob=bpy.data.objects.new("Closed gable",mesh);bpy.context.collection.objects.link(ob);ob.data.materials.append(plaster)
  for side in (-1,1): beam("Gable rafter",(x,0,h+rise),(x,side*d/2,h),.2,wood)
 for x in (-w*.32,w*.32):
  box("Recessed window frame",(x,-d/2-.22,1.8),(1.3,.18,1.5),wood)
  box("Window glazing",(x,-d/2-.33,1.8),(1.12,.04,1.32),glass)
  for j in range(5): box("Lead mullion",(x-.56+j*.28,-d/2-.36,1.8),(.035,.06,1.32),iron)
  for j in range(5): box("Lead crossbar",(x,-d/2-.36,1.14+j*.33),(1.12,.06,.035),iron)
  box("Window sill",(x,-d/2-.4,1.06),(1.45,.4,.13),stone)
 for side in (-1,1):
  for row in range(4):
   for col in range(int(w/.48)):
    x=-w/2+.24+col*.48+(row%2)*.12
    if side<0 and abs(x)<door/2+.2: continue
    box("Individual foundation block",(x,side*(d/2+.18),.12+row*.22),(.46,.22,.205),stone)
 for side in (-1,1):
  for row in range(int(h/.28)):
   for j in range(int(d/.65)):
    yy=-d/2+.33+j*.65+(row%2)*.16
    box("Interior stone course",(side*(w/2-.19),yy,.14+row*.28),(.10,.63,.265),stone,bevel=.014)
  for yy in (-d*.32,0,d*.32):
   box("Interior oak pillar",(side*(w/2-.38),yy,h/2),(.35,.35,h),wood)
   box("Lantern frame",(side*(w/2-.65),yy,2.4),(.23,.23,.35),iron)
   box("Lantern amber pane",(side*(w/2-.78),yy,2.4),(.035,.17,.25),glass)
 if name!="H5_Cottage":
  for x in (-w*.32,w*.32):
   for z in (.45,1.05,1.65,2.25): box("Display shelf",(x,d/2-.6,z),(w*.28,.65,.09),wood)
   for dx in (-w*.13,w*.13): box("Shelf upright",(x+dx,d/2-.6,1.35),(.1,.65,2.7),wood)
  box("Merchant counter",(w*.22,-.6,.55),(w*.35,.85,1.1),wood)
 if name=="H5_Cottage":
  foliage=mat("Ivy green",(.08,.24,.035))
  blossom=mat("Rose blossom",(.65,.12,.18))
  for x in (-w*.32,w*.32):
   box("Oak flower planter",(x,-d/2-.48,.88),(1.5,.48,.32),wood)
   for j in range(12):
    xx=x-.62+j*.11
    beam("Flower stem",(xx,-d/2-.48,1.02),(xx,-d/2-.48,1.28),.018,foliage)
    sphere("Flower",(xx,-d/2-.48,1.28),(.13,.13,.09),blossom)
   for j in range(32):
    z=.3+j*.095
    sphere("Ivy leaf",(x+math.sin(j*.7)*.23,-d/2-.24,z),(.16,.06,.12),foliage)
 if name=="A2_Armory":
  for x in (-w*.34,0,w*.34):
   yy=d*.22
   box("Armor display pedestal",(x,yy,.14),(.85,.75,.28),wood)
   box("Armor support",(x,yy,.95),(.12,.12,1.5),wood)
   sphere("Steel breastplate",(x,yy,1.43),(.68,.39,.65),iron)
   sphere("Helmet",(x,yy,1.98),(.34,.35,.39),iron)
   box("Visor slit",(x,yy-.18,1.99),(.24,.025,.045),wood)
   for side in (-1,1):
    sphere("Shoulder plate",(x+side*.39,yy,1.65),(.29,.33,.25),iron)
    for j in range(5):
     box("Overlapping arm plate",(x+side*.39,yy,1.47-j*.09),(.19,.25,.105),iron)
   sphere("Display shield",(x,d/2-.95,2.7),(.68,.13,.9),wood)
   sphere("Shield boss",(x,d/2-1.04,2.7),(.23,.12,.23),iron)
 if name=="Sorcerer_Shop":
  for side in (-1,1):
   for row in range(4):
    for j in range(9):
     x=side*w*.32-w*.12+j*w*.03
     z=.57+row*.6
     sphere("Potion bottle",(x,d/2-.6,z),(.15,.15,.22),glass)
     box("Potion cork",(x,d/2-.6,z+.14),(.055,.055,.07),wood)
 if name=="Blacksmith_Shop":
  box("Stone forge",(w*.3,d*.3,.65),(2,1.8,1.3),stone)
  box("Anvil stand",(-1,0,.5),(.8,.8,1),wood)
  box("Anvil",(-1,0,1.1),(1.1,.45,.25),iron)
 if name=="H5_Cottage" or name=="Blacksmith_Shop":
  for side in (-1,1):
   box("Hollow chimney side",(w*.25+side*.6,d*.15,h+rise),(.2,1.4,2.6),stone)
   box("Hollow chimney end",(w*.25,d*.15+side*.6,h+rise),(1,.2,2.6),stone)
 export(name)

building("H5_Cottage",7.5,9,3.6,2.8)
reset()
for x in (-.35,.35): box("Stand foot",(x,0,.05),(.5,.45,.1),wood)
box("Stand post",(0,0,.85),(.1,.1,1.7),wood)
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

building("A2_Armory",9.144,12.192,4.2,3.1)
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

building("Blacksmith_Shop",9.144,12.192,4.2,3.1)
building("Sorcerer_Shop",9.144,12.192,4.2,3.5)
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
