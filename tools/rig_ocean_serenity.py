"""Experimental A-pose auto-rig. Run with Blender 4.3+ in background mode.
Usage: blender -b -t 2 --python rig_ocean_serenity.py -- INPUT.obj OUTPUT.fbx
This is a first-pass anatomical estimate, not a production-quality rig.
"""
import bpy, sys, math, json
from pathlib import Path
from mathutils import Vector

args = sys.argv[sys.argv.index("--")+1:]
if len(args) != 2:
    raise SystemExit("Expected OBJ input and FBX output")
source, target = map(Path, args)
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
if bpy.app.version < (3, 0, 0):
    bpy.ops.import_scene.obj(filepath=str(source))
else:
    bpy.ops.wm.obj_import(filepath=str(source))
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
if not meshes:
    raise RuntimeError("No meshes imported")
# Merge mesh pieces to create one armature binding; preserve material slots.
bpy.ops.object.select_all(action="DESELECT")
for obj in meshes: obj.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.join()
body = bpy.context.object
body.name = "OceanSerenity"
# Normalize the vertical axis to Z and center the body at ground.
coords = [v.co.copy() for v in body.data.vertices]
mins = [min(v[i] for v in coords) for i in range(3)]
maxs = [max(v[i] for v in coords) for i in range(3)]
ranges = [maxs[i]-mins[i] for i in range(3)]
vertical = max(range(3), key=lambda i:ranges[i])
if vertical != 2:
    # OBJ may be Y-up. Rotate object geometry so largest dimension is Z.
    if vertical == 1:
        for v in body.data.vertices: v.co = (v.co.x, -v.co.z, v.co.y)
    else:
        raise RuntimeError("X is longest dimension; verify model orientation before rigging")
coords = [v.co for v in body.data.vertices]
lo = Vector([min(v[i] for v in coords) for i in range(3)])
hi = Vector([max(v[i] for v in coords) for i in range(3)])
h = hi.z-lo.z
w = hi.x-lo.x
if h <= 0 or w <= 0 or w/h > 1.3:
    raise RuntimeError(f"Suspicious body proportions: width={w:.3f}, height={h:.3f}")
mid_x=(lo.x+hi.x)/2
mid_y=(lo.y+hi.y)/2
for v in body.data.vertices:
    v.co.x -= mid_x
    v.co.y -= mid_y
    v.co.z -= lo.z
# Standard humanoid proportions for A-pose; adjust after inspecting preview.
def p(x,y,z): return (x*w,y*w,z*h)
bones=[
 ("root",None,p(0,0,0),p(0,0,.08)),
 ("pelvis","root",p(0,0,.49),p(0,0,.57)),
 ("spine","pelvis",p(0,0,.57),p(0,0,.72)),
 ("chest","spine",p(0,0,.72),p(0,0,.83)),
 ("neck","chest",p(0,0,.83),p(0,0,.89)),
 ("head","neck",p(0,0,.89),p(0,0,.99)),
]
for side,s in (("L",-1),("R",1)):
    bones += [
      (f"clavicle_{side}","chest",p(s*.10,0,.82),p(s*.20,0,.80)),
      (f"upperarm_{side}",f"clavicle_{side}",p(s*.20,0,.80),p(s*.36,0,.67)),
      (f"forearm_{side}",f"upperarm_{side}",p(s*.36,0,.67),p(s*.47,0,.56)),
      (f"hand_{side}",f"forearm_{side}",p(s*.47,0,.56),p(s*.51,0,.52)),
      (f"thigh_{side}","pelvis",p(s*.105,0,.49),p(s*.105,0,.285)),
      (f"shin_{side}",f"thigh_{side}",p(s*.105,0,.285),p(s*.105,0,.065)),
      (f"foot_{side}",f"shin_{side}",p(s*.105,0,.065),p(s*.105,-.11,.035)),
    ]
bpy.ops.object.armature_add(enter_editmode=False)
rig=bpy.context.object
rig.name="OceanSerenityRig"
bpy.ops.object.mode_set(mode="EDIT")
edit=rig.data.edit_bones
edit.remove(edit[0])
for name,parent,head,tail in bones:
    bone=edit.new(name)
    bone.head=head
    bone.tail=tail
    if parent: bone.parent=edit[parent]
bpy.ops.object.mode_set(mode="OBJECT")
# Distance-based smooth vertex weighting: robust headless fallback when Blender
# heat weights fail on non-manifold or disconnected AI meshes.
def segment_dist(point,a,b):
    ab=b-a
    t=max(0,min(1,(point-a).dot(ab)/max(ab.length_squared,1e-12)))
    return (point-(a+t*ab)).length
deform=[(name,Vector(head),Vector(tail)) for name,_,head,tail in bones if name!="root"]
groups={name:body.vertex_groups.new(name=name) for name,_,_ in deform}
for v in body.data.vertices:
    q=v.co
    # Bias to nearest bones and taper influence by distance.
    ds=sorted(((segment_dist(q,a,b),name) for name,a,b in deform),key=lambda x:x[0])[:4]
    scale=max(h*.035,ds[0][0]*.6)
    weights=[math.exp(-((d-ds[0][0])/scale)**2) for d,_ in ds]
    total=sum(weights)
    for (_,name),weight in zip(ds,weights):
        if weight/total >= .001: groups[name].add([v.index],weight/total,"REPLACE")
mod=body.modifiers.new("OceanSerenitySkin","ARMATURE")
mod.object=rig
body.parent=rig
# Generate distinct baked motion clips on the experimental humanoid rig.
# All actions share the same rest pose and skeleton for Unreal retarget-free playback.
from mathutils import Quaternion
fps=30
bpy.context.scene.render.fps=fps
motions=("Idle","Walk","Run","Jump","Attack","Spin")
def pose_action(name,frames):
    action=bpy.data.actions.new("Ocean_"+name)
    rig.animation_data_create()
    rig.animation_data.action=action
    for frame,angles in frames:
        for pb in rig.pose.bones:
            pb.rotation_mode="XYZ"
            pb.rotation_euler=(0,0,0)
            pb.location=(0,0,0)
        for bone,axis,angle in angles:
            pb=rig.pose.bones.get(bone)
            if pb: pb.rotation_euler[axis]=math.radians(angle)
        for pb in rig.pose.bones:
            pb.keyframe_insert(data_path="rotation_euler",frame=frame,group=pb.name)
            if pb.name=="root": pb.keyframe_insert(data_path="location",frame=frame,group=pb.name)
    action.use_fake_user=True
    # Keep all animations in the FBX as separate named NLA takes.
    track=rig.animation_data.nla_tracks.new()
    track.name="Ocean_"+name
    strip=track.strips.new("Ocean_"+name,1,action)
    strip.action_frame_start=1
    strip.action_frame_end=max(f for f,_ in frames)
    strip.mute=False
    return action
pose_action("Idle",[(1,[]),(16,[("chest",0,1.5),("upperarm_L",1,-5),("upperarm_R",1,5)]),(31,[])])
pose_action("Walk",[(1,[("thigh_L",0,26),("thigh_R",0,-26),("upperarm_L",0,-20),("upperarm_R",0,20)]),
 (9,[("thigh_L",0,-26),("thigh_R",0,26),("upperarm_L",0,20),("upperarm_R",0,-20)]),
 (17,[("thigh_L",0,26),("thigh_R",0,-26),("upperarm_L",0,-20),("upperarm_R",0,20)])])
pose_action("Run",[(1,[("thigh_L",0,45),("thigh_R",0,-45),("upperarm_L",0,-40),("upperarm_R",0,40),("chest",0,10)]),
 (7,[("thigh_L",0,-45),("thigh_R",0,45),("upperarm_L",0,40),("upperarm_R",0,-40),("chest",0,10)]),
 (13,[("thigh_L",0,45),("thigh_R",0,-45),("upperarm_L",0,-40),("upperarm_R",0,40),("chest",0,10)])])
pose_action("Jump",[(1,[("thigh_L",0,-25),("thigh_R",0,-25),("shin_L",0,35),("shin_R",0,35)]),
 (10,[("thigh_L",0,15),("thigh_R",0,15),("upperarm_L",0,-35),("upperarm_R",0,-35)]),
 (24,[("thigh_L",0,-20),("thigh_R",0,-20),("shin_L",0,30),("shin_R",0,30)]),
 (30,[])])
pose_action("Attack",[(1,[]),(7,[("chest",2,-18),("upperarm_R",0,-65),("forearm_R",0,-55)]),
 (13,[("chest",2,25),("upperarm_R",0,70),("forearm_R",0,-25)]),(22,[])])
pose_action("Spin",[(1,[]),(7,[("chest",2,45),("upperarm_L",0,65),("upperarm_R",0,65)]),
 (13,[("chest",2,90),("upperarm_L",0,65),("upperarm_R",0,65)]),
 (19,[("chest",2,45)]),(25,[])])
rig.animation_data.action=None
# Preserve textures through FBX texture copying when Blender finds the MTL.
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active=rig
target.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,
    object_types={"ARMATURE","MESH"},add_leaf_bones=False,
    bake_anim=True,bake_anim_use_nla_strips=True,bake_anim_use_all_actions=False,\n    bake_anim_simplify_factor=0.0,path_mode="COPY",embed_textures=True)
report={"source":str(source),"fbx":str(target),"vertices":len(body.data.vertices),
        "bones":len(bones),"height":h,"width":w,"warning":"Approximate rig: joint positions and deformation require visual validation"}
target.with_suffix(".json").write_text(json.dumps(report,indent=2))
print("RIG_REPORT "+json.dumps(report))
