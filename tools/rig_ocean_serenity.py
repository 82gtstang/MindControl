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
# Geometry-fitted humanoid rig. Infer body landmarks from horizontal mesh slices
# rather than scaling every joint from the character's overall width.
# The full width includes extended A-pose arms and must not determine torso size.
def slice_points(z_fraction, tolerance=.022):
    z_target=z_fraction*h
    return [v.co for v in body.data.vertices if abs(v.co.z-z_target)<tolerance*h]
def percentile(values,fraction):
    values=sorted(values)
    if not values: return 0.
    return values[min(len(values)-1,max(0,int(fraction*(len(values)-1))))]
def center_y(z_fraction,central_x=None):
    pts=slice_points(z_fraction)
    if central_x is not None:
        pts=[v for v in pts if abs(v.x)<central_x]
    return percentile([v.y for v in pts],.5) if pts else 0.
# Estimate the torso half-width from the waist, avoiding the extended hands.
waist=slice_points(.55)
waist_center=[v for v in waist if abs(v.x)<w*.24]
torso_half=percentile([abs(v.x) for v in waist_center],.85) if waist_center else w*.13
torso_half=max(w*.075,min(w*.20,torso_half))
hip_y=center_y(.49,torso_half)
chest_y=center_y(.73,torso_half)
shoulder_x=min(w*.22,max(torso_half*.92,w*.105))
elbow_x=min(w*.36,max(shoulder_x+w*.10,w*.28))
wrist_x=min(w*.46,max(elbow_x+w*.075,w*.40))
def p(x,y,z): return (x,y,z*h)
bones=[
 ("root",None,p(0,hip_y,0),p(0,hip_y,.08)),
 ("pelvis","root",p(0,hip_y,.49),p(0,hip_y,.56)),
 ("spine","pelvis",p(0,hip_y,.56),p(0,chest_y,.70)),
 ("chest","spine",p(0,chest_y,.70),p(0,chest_y,.82)),
 ("neck","chest",p(0,chest_y,.82),p(0,chest_y,.89)),
 ("head","neck",p(0,chest_y,.89),p(0,chest_y,.985)),
]
for side,s in (("L",-1),("R",1)):
    leg_x=torso_half*.52*s
    bones += [
      (f"clavicle_{side}","chest",p(s*torso_half*.55,chest_y,.81),p(s*shoulder_x,chest_y,.79)),
      (f"upperarm_{side}",f"clavicle_{side}",p(s*shoulder_x,chest_y,.79),p(s*elbow_x,chest_y,.67)),
      (f"forearm_{side}",f"upperarm_{side}",p(s*elbow_x,chest_y,.67),p(s*wrist_x,chest_y,.565)),
      (f"hand_{side}",f"forearm_{side}",p(s*wrist_x,chest_y,.565),p(s*min(w*.49,wrist_x+w*.045),chest_y,.53)),
      (f"thigh_{side}","pelvis",p(leg_x,hip_y,.49),p(leg_x,hip_y,.285)),
      (f"shin_{side}",f"thigh_{side}",p(leg_x,hip_y,.285),p(leg_x,hip_y,.065)),
      (f"foot_{side}",f"shin_{side}",p(leg_x,hip_y,.065),p(leg_x,hip_y-w*.065,.035)),
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
# Anatomical masks prevent body vertices from attaching to distant limb bones.
# Bone labels are selected from the geometry's height and lateral position.
for v in body.data.vertices:
    q=v.co
    z=q.z/max(h,1e-6)
    ax=abs(q.x)
    side="L" if q.x<0 else "R"
    if z>=.87:
        allowed={"head","neck"}
    elif z>=.81 and ax<shoulder_x*.82:
        allowed={"neck","chest","head"}
    elif z>=.52 and ax>shoulder_x*.85:
        allowed={f"upperarm_{side}",f"forearm_{side}",f"hand_{side}",f"clavicle_{side}"}
    elif z>=.71:
        allowed={"chest","spine",f"clavicle_{side}"}
    elif z>=.54:
        allowed={"spine","chest","pelvis"}
    elif z>=.43:
        allowed={"pelvis","spine",f"thigh_{side}"}
    elif z>=.27:
        allowed={f"thigh_{side}",f"shin_{side}"}
    elif z>=.08:
        allowed={f"shin_{side}",f"thigh_{side}"}
    else:
        allowed={f"foot_{side}",f"shin_{side}"}
    candidates=sorted(((segment_dist(q,aa,bb),name) for name,aa,bb in deform if name in allowed),key=lambda item:item[0])[:2]
    if not candidates: raise RuntimeError("No matching deform bones")
    # Keep influences local; avoid rubber-band stretching from four distant bones.
    distances=[max(d,1e-4) for d,_ in candidates]
    weights=[1.0/(d*d) for d in distances]
    total=sum(weights)
    for (_,name),weight in zip(candidates,weights):
        groups[name].add([v.index],weight/total,"REPLACE")
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
# Conservative movements: shoulders rotate around local Y, legs around local X.
# Large rotations on the estimated rig tear skin and look like rubber limbs.
pose_action("Idle",[(1,[]),(16,[("chest",1,1.0)]),(31,[])])
pose_action("Walk",[
 (1,[("thigh_L",0,15),("thigh_R",0,-15),("shin_R",0,12),("upperarm_L",1,-8),("upperarm_R",1,8)]),
 (9,[("thigh_L",0,-15),("thigh_R",0,15),("shin_L",0,12),("upperarm_L",1,8),("upperarm_R",1,-8)]),
 (17,[("thigh_L",0,15),("thigh_R",0,-15),("shin_R",0,12),("upperarm_L",1,-8),("upperarm_R",1,8)])])
pose_action("Run",[
 (1,[("thigh_L",0,27),("thigh_R",0,-27),("shin_R",0,22),("upperarm_L",1,-16),("upperarm_R",1,16)]),
 (7,[("thigh_L",0,-27),("thigh_R",0,27),("shin_L",0,22),("upperarm_L",1,16),("upperarm_R",1,-16)]),
 (13,[("thigh_L",0,27),("thigh_R",0,-27),("shin_R",0,22),("upperarm_L",1,-16),("upperarm_R",1,16)])])
pose_action("Jump",[(1,[("thigh_L",0,-12),("thigh_R",0,-12),("shin_L",0,15),("shin_R",0,15)]),
 (10,[("thigh_L",0,8),("thigh_R",0,8),("upperarm_L",1,-10),("upperarm_R",1,10)]),
 (24,[("thigh_L",0,-12),("thigh_R",0,-12),("shin_L",0,15),("shin_R",0,15)]),(30,[])])
pose_action("Attack",[(1,[]),(7,[("chest",2,-10),("upperarm_R",1,-25),("forearm_R",1,-15)]),
 (13,[("chest",2,12),("upperarm_R",1,28),("forearm_R",1,-10)]),(22,[])])
pose_action("Spin",[(1,[]),(7,[("chest",2,18),("upperarm_L",1,-18),("upperarm_R",1,18)]),
 (13,[("chest",2,30),("upperarm_L",1,-18),("upperarm_R",1,18)]),
 (19,[("chest",2,18)]),(25,[])])
rig.animation_data.action=None
# Preserve textures through FBX texture copying when Blender finds the MTL.
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active=rig
target.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,
    object_types={"ARMATURE","MESH"},add_leaf_bones=False,
    bake_anim=True,bake_anim_use_nla_strips=True,bake_anim_use_all_actions=False,
    bake_anim_simplify_factor=0.0,path_mode="COPY",embed_textures=True)
report={"source":str(source),"fbx":str(target),"vertices":len(body.data.vertices),
        "bones":len(bones),"height":h,"width":w,"warning":"Geometry-fitted experimental rig; manual weight painting and joint validation still recommended"}
target.with_suffix(".json").write_text(json.dumps(report,indent=2))
print("RIG_REPORT "+json.dumps(report))
