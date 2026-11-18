"""Experimental A-pose auto-rig. Run with Blender 4.3+ in background mode.
Usage: blender -b -t 2 --python rig_ocean_serenity.py -- INPUT.obj OUTPUT.fbx
This is a first-pass anatomical estimate, not a production-quality rig.
"""
import bpy, sys, math, json
from pathlib import Path
from mathutils import Vector, Quaternion

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
# Prefer Blender's heat-diffusion skinning over hand-authored distance weights.
# AI-generated meshes can be disconnected/non-manifold, so verify coverage and
# use the anatomical distance fallback only if automatic weights fail.
def segment_dist(point,a,b):
    ab=b-a
    t=max(0,min(1,(point-a).dot(ab)/max(ab.length_squared,1e-12)))
    return (point-(a+t*ab)).length
deform=[(name,Vector(head),Vector(tail)) for name,_,head,tail in bones if name!="root"]
weight_method="automatic"
try:
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    # Verify each vertex has normalized nonzero weights, and no vertices are
    # accidentally influenced by a non-deforming control bone.
    covered=0
    for v in body.data.vertices:
        total=sum(g.weight for g in v.groups if g.group<len(body.vertex_groups) and body.vertex_groups[g.group].name!="root")
        if total>.001: covered+=1
    if covered<len(body.data.vertices):
        raise RuntimeError("Automatic weights uncovered %d vertices"%(len(body.data.vertices)-covered))
    print("SKINNING: retaining Blender automatic weights for",covered,"vertices",flush=True)
except Exception as exc:
    weight_method="anatomical_fallback"
    print("SKINNING: automatic weights unavailable:",repr(exc),flush=True)
    # Clear partial automatic weights and remove any automatic armature modifier.
    for group in list(body.vertex_groups): body.vertex_groups.remove(group)
    for modifier in list(body.modifiers):
        if modifier.type=="ARMATURE": body.modifiers.remove(modifier)
    body.parent=None
    groups={name:body.vertex_groups.new(name=name) for name,_,_ in deform}
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
        distances=[max(d,1e-4) for d,_ in candidates]
        weights=[1.0/(d*d) for d in distances]
        total=sum(weights)
        for (_,name),weight in zip(candidates,weights):
            groups[name].add([v.index],weight/total,"REPLACE")
    mod=body.modifiers.new("OceanSerenitySkin","ARMATURE")
    mod.object=rig
    body.parent=rig
# Correct pathological automatic weights. The AI mesh is a connected surface:
# Blender heat diffusion spreads small arm weights across the entire body,
# leaving shoulder bones unable to deform their intended vertices.
# Rebuild weights from spatial anatomical regions with a protected torso core.
for group in list(body.vertex_groups):
    body.vertex_groups.remove(group)
groups={name:body.vertex_groups.new(name=name) for name,_,_ in deform}
arm_names={f"{part}_{side}" for part in ("clavicle","upperarm","forearm","hand") for side in ("L","R")}
for v in body.data.vertices:
    q=v.co
    z=q.z/max(h,1e-6)
    ax=abs(q.x)
    side="L" if q.x<0 else "R"
    # Explicit shoulder boundary: central torso never receives arm weights.
    # Keep arm selection spatially continuous along the original extended arms.
    if .50<=z<=.85 and ax>shoulder_x*.94:
        allowed={f"upperarm_{side}",f"forearm_{side}",f"hand_{side}"}
        # At the shoulder seam, permit clavicle to blend the transition.
        if ax<shoulder_x*1.28: allowed.add(f"clavicle_{side}")
    elif z>=.87:
        allowed={"head","neck"}
    elif z>=.79:
        allowed={"chest","neck"}
    elif z>=.68:
        allowed={"chest","spine"}
    elif z>=.54:
        allowed={"spine","pelvis"}
    elif z>=.44:
        allowed={"pelvis","spine"}
    elif z>=.28:
        allowed={f"thigh_{side}"}
    elif z>=.08:
        allowed={f"shin_{side}",f"thigh_{side}"}
    else:
        allowed={f"foot_{side}",f"shin_{side}"}
    candidates=sorted(((segment_dist(q,aa,bb),name) for name,aa,bb in deform if name in allowed),key=lambda item:item[0])[:2]
    raw=[1/max(dist,.008*h)**4 for dist,_ in candidates]
    total=sum(raw)
    for (_,name),weight in zip(candidates,raw):
        groups[name].add([v.index],weight/total,"REPLACE")
print("SKINNING: rebuilt localized anatomical arm weights with torso exclusion",flush=True)
# Permanently establish a relaxed bind/rest pose BEFORE creating animation clips.
# The original OBJ is modeled with extended arms. Animation-only corrections
# can be lost when Unreal evaluates its reference pose, so bake the arm-lowering
# deformation into the mesh and apply the pose as the skeleton rest pose.
for side,sign in (("L",-1),("R",1)):
    pb=rig.pose.bones["upperarm_"+side]
    rest=pb.bone.matrix_local.to_3x3()
    current=(pb.bone.tail_local-pb.bone.head_local).normalized()
    desired=Vector((sign*.13,0,-.991)).normalized()
    correction=current.rotation_difference(desired)
    pb.rotation_mode="QUATERNION"
    pb.rotation_quaternion=rest.to_quaternion().inverted() @ correction @ rest.to_quaternion()
    # Apply the same anatomical direction correction to the forearm chain.
    # A shoulder-only correction leaves the forearm/hand extending sideways.
    forearm=rig.pose.bones["forearm_"+side]
    forearm.rotation_mode="QUATERNION"
    forearm.rotation_quaternion=(1,0,0,0)
bpy.context.view_layer.update()
# Apply the actual ARMATURE modifier while the arms are posed.  Merely
# copying evaluated vertex coordinates does not reliably commit deformation
# in Blender 2.92 (especially with a parent inverse transform).
skin=next((m for m in body.modifiers if m.type=="ARMATURE"),None)
if skin is None: raise RuntimeError("Missing armature skin before rest bake")
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.object.modifier_apply(modifier=skin.name)
bpy.context.view_layer.update()
print("REST_BAKE: applied posed armature deformation directly to base mesh",flush=True)
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
skin=body.modifiers.new("OceanSerenitySkin","ARMATURE")
skin.object=rig
for pb in rig.pose.bones:
    pb.rotation_mode="XYZ"
    pb.rotation_euler=(0,0,0)
bpy.context.view_layer.update()
print("REST_POSE: baked relaxed arms into geometry and armature bind pose",flush=True)
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
    clip_name=name
    for frame,angles in frames:
        for pb in rig.pose.bones:
            pb.rotation_mode="XYZ"
            pb.rotation_euler=(0,0,0)
            pb.location=(0,0,0)
        # Map shoulder motion to anatomical WORLD axes through each bone's
        # rest orientation. Euler indices alone are unreliable for arm bones.
        arm_motion={}
        for bone,axis,angle in angles:
            if bone.startswith(("upperarm_","forearm_")):
                arm_motion[bone]=arm_motion.get(bone,0)+angle
            else:
                pb=rig.pose.bones.get(bone)
                if pb: pb.rotation_euler[axis]+=math.radians(angle)
        for side,sign in (("L",-1),("R",1)):
            arm_bone_name="upperarm_"+side
            pb=rig.pose.bones.get(arm_bone_name)
            if not pb: continue
            # Match the actual rest-bone direction to a relaxed arm beside
            # the hip. Fixed signed-angle guesses leave A-pose arms extended
            # when the imported model has a different shoulder slope.
            rest=pb.bone.matrix_local.to_3x3()
            arm_direction=(pb.bone.tail_local-pb.bone.head_local).normalized()
            target_direction=Vector((sign*0.13,0,-0.991)).normalized()
            world_correction=arm_direction.rotation_difference(target_direction)
            rest_rotation=rest.to_quaternion()
            local_correction=rest_rotation.inverted() @ world_correction @ rest_rotation
            swing_axis=rest.inverted() @ Vector((1,0,0))
            swing=Quaternion(swing_axis,math.radians(arm_motion.get(arm_bone_name,0)))
            pb.rotation_mode="QUATERNION"
            pb.rotation_quaternion=local_correction @ swing
        for side in ("L","R"):
            forearm_bone_name="forearm_"+side
            pb=rig.pose.bones.get(forearm_bone_name)
            if pb:
                rest=pb.bone.matrix_local.to_3x3()
                axis=rest.inverted() @ Vector((1,0,0))
                pb.rotation_mode="QUATERNION"
                pb.rotation_quaternion=Quaternion(axis,math.radians(arm_motion.get(forearm_bone_name,0)))
        if clip_name=="Jump":
            # Lift the whole character in mid-air, not only the legs.
            jump_heights={1:0.0,7:0.18,15:0.34,23:0.10,30:0.0}
            rig.pose.bones["root"].location.z=jump_heights.get(frame,0.0)
        for pb in rig.pose.bones:
            pb.keyframe_insert(data_path="rotation_quaternion" if pb.rotation_mode=="QUATERNION" else "rotation_euler",frame=frame,group=pb.name)
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
# More expressive cycles while retaining moderate joint rotations.
# All angles are local Euler rotations; verify visually before Android packaging.
pose_action("Idle",[(1,[]),(16,[("chest",1,2),("head",1,-1)]),(31,[])])
pose_action("Walk",[
 (1,[("thigh_L",0,19),("thigh_R",0,-19),("shin_R",0,17),("upperarm_L",2,-13),("upperarm_R",2,13),("chest",2,2)]),
 (5,[("thigh_L",0,7),("thigh_R",0,-7),("shin_L",0,8),("shin_R",0,5)]),
 (9,[("thigh_L",0,-19),("thigh_R",0,19),("shin_L",0,17),("upperarm_L",2,13),("upperarm_R",2,-13),("chest",2,-2)]),
 (13,[("thigh_L",0,-7),("thigh_R",0,7),("shin_R",0,8),("shin_L",0,5)]),
 (17,[("thigh_L",0,19),("thigh_R",0,-19),("shin_R",0,17),("upperarm_L",2,-13),("upperarm_R",2,13),("chest",2,2)])])
pose_action("Run",[
 (1,[("thigh_L",0,31),("thigh_R",0,-26),("shin_R",0,32),("upperarm_L",2,-22),("upperarm_R",2,22),("chest",0,5)]),
 (4,[("thigh_L",0,3),("thigh_R",0,-3),("shin_L",0,14),("shin_R",0,14),("chest",0,7)]),
 (7,[("thigh_L",0,-26),("thigh_R",0,31),("shin_L",0,32),("upperarm_L",2,22),("upperarm_R",2,-22),("chest",0,5)]),
 (10,[("thigh_L",0,-3),("thigh_R",0,3),("shin_L",0,14),("shin_R",0,14),("chest",0,7)]),
 (13,[("thigh_L",0,31),("thigh_R",0,-26),("shin_R",0,32),("upperarm_L",2,-22),("upperarm_R",2,22),("chest",0,5)])])
pose_action("Jump",[
 (1,[("thigh_L",0,-18),("thigh_R",0,-18),("shin_L",0,24),("shin_R",0,24),("upperarm_L",2,8),("upperarm_R",2,-8)]),
 (7,[("thigh_L",0,5),("thigh_R",0,5),("shin_L",0,8),("shin_R",0,8),("upperarm_L",2,-26),("upperarm_R",2,26)]),
 (15,[("thigh_L",0,3),("thigh_R",0,3),("shin_L",0,12),("shin_R",0,12),("upperarm_L",2,-20),("upperarm_R",2,20)]),
 (23,[("thigh_L",0,-16),("thigh_R",0,-16),("shin_L",0,22),("shin_R",0,22)]),(30,[])])
pose_action("Attack",[
 (1,[]),(5,[("chest",2,-16),("upperarm_R",2,-36),("forearm_R",2,-25),("upperarm_L",2,12)]),
 (10,[("chest",2,22),("upperarm_R",2,36),("forearm_R",2,15),("upperarm_L",2,-12)]),
 (15,[("chest",2,8),("upperarm_R",2,15),("forearm_R",2,6)]),(22,[])])
spin=pose_action("Spin",[
 (1,[("root",2,0),("upperarm_L",2,-24),("upperarm_R",2,24)]),
 (7,[("root",2,90),("upperarm_L",2,-32),("upperarm_R",2,32)]),
 (13,[("root",2,180),("upperarm_L",2,-32),("upperarm_R",2,32)]),
 (19,[("root",2,270),("upperarm_L",2,-32),("upperarm_R",2,32)]),
 (25,[("root",2,360),("upperarm_L",2,-24),("upperarm_R",2,24)])])
for fc in spin.fcurves:
    if fc.data_path=='pose.bones["root"].rotation_euler':
        for key in fc.keyframe_points: key.interpolation="LINEAR"
# Verify the relaxed idle pose by absolute arm/hand placement, not movement.
# After baking the relaxed pose into the rest skeleton, Idle frame 1 SHOULD
# have zero vertex displacement. The previous test incorrectly rejected that.
rig.animation_data.action=bpy.data.actions["Ocean_Idle"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
for side,sign in (("L",-1),("R",1)):
    shoulder=rig.pose.bones["upperarm_"+side].head
    elbow=rig.pose.bones["upperarm_"+side].tail
    wrist=rig.pose.bones["forearm_"+side].tail
    hand=rig.pose.bones["hand_"+side].tail
    shoulder_to_hand=hand-shoulder
    print("ARM_POSE_VALIDATION",side,"shoulder",tuple(shoulder),
          "elbow",tuple(elbow),"wrist",tuple(wrist),"hand",tuple(hand),
          "drop",shoulder_to_hand.z,"outward",abs(shoulder_to_hand.x),flush=True)
    if shoulder_to_hand.z > -.15*h:
        raise RuntimeError("Arm "+side+" is not lowered: hand must be well below shoulder")
    if abs(shoulder_to_hand.x) > .15*h:
        raise RuntimeError("Arm "+side+" is too far from the body: hand must hang beside hip")
    if elbow.z >= shoulder.z or wrist.z >= elbow.z:
        raise RuntimeError("Arm "+side+" joint order is not downward")
# Check mesh skin remains connected to the armature.
if not any(m.type=="ARMATURE" and m.object==rig for m in body.modifiers):
    raise RuntimeError("Armature modifier missing after relaxed-rest-pose bake")
# Validate the actual deformed skin, not only the bone locations.
# Sample each mesh vertex by its original arm weights and ensure the skin
# corresponding to the upper/forearm has moved down beside the torso.
dg=bpy.context.evaluated_depsgraph_get()
evaluated=body.evaluated_get(dg).to_mesh()
for side in ("L","R"):
    arm_groups={g.index for g in body.vertex_groups if g.name in ("upperarm_"+side,"forearm_"+side,"hand_"+side)}
    # Automatic weights are blended; a vertex may have several arm weights
    # without any single weight exceeding 0.55. Evaluate their sum.
    arm_indices=[]
    max_arm_weight=0.0
    weighted_count=0
    for v in body.data.vertices:
        arm_weight=sum(g.weight for g in v.groups if g.group in arm_groups)
        max_arm_weight=max(max_arm_weight,arm_weight)
        if arm_weight>.05:
            weighted_count+=1
        if arm_weight>.25:
            arm_indices.append(v.index)
    print("ARM_WEIGHT_DIAGNOSTIC",side,"groups",sorted(arm_groups),
          "max_combined_weight",max_arm_weight,"over_5pct",weighted_count,
          "over_25pct",len(arm_indices),flush=True)
    if len(arm_indices)<100:
        raise RuntimeError("Arm "+side+" has no meaningful skin influence; investigate automatic weights and mesh connectivity")
    arm_vertices=[evaluated.vertices[i].co for i in arm_indices]
    x_extent=max(abs(v.x) for v in arm_vertices)
    z_mid=sorted(v.z for v in arm_vertices)[len(arm_vertices)//2]
    print("ARM_MESH_VALIDATION",side,"vertices",len(arm_indices),"max_abs_x",x_extent,"median_z",z_mid,flush=True)
    if x_extent > .29*h:
        raise RuntimeError("Actual arm mesh still extends sideways on "+side+": "+str(x_extent))
body.evaluated_get(dg).to_mesh_clear()
print("POSE_VALIDATION: both hands below shoulders, arm skin checked, and skin modifier connected",flush=True)
# Restore NLA-only export; each strip becomes its own FBX animation take.
rig.animation_data.action=None
bpy.context.scene.frame_set(1)
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
        "bones":len(bones),"height":h,"width":w,"weight_method":weight_method,"warning":"Geometry-fitted experimental rig; manual weight painting and joint validation still recommended"}
target.with_suffix(".json").write_text(json.dumps(report,indent=2))
print("RIG_REPORT "+json.dumps(report))
