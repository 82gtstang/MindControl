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
    # Auto weights on this non-manifold OBJ assigned most vertices to foot_L.\n    # Use region-constrained anatomical weights instead.\n    raise RuntimeError("Automatic skinning disabled: verified cross-body foot influence")\n    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
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
    # Armature modifier alone deforms the mesh. Parenting without a matching
    # parent inverse can introduce an additional coordinate transform.
    body.parent=None
    body.matrix_world.identity()
    rig.matrix_world.identity()
# Fresh, non-destructive rest-pose conversion. Blender's armature modifier
# performs the actual weighted skin deformation, including shoulders.
# Never rotate/clamp individual vertices by hand.
if weight_method != "automatic":
    print("SKINNING: testing region-constrained anatomical fallback",flush=True)
original_count=len(body.data.vertices)
for side,sign in (("L",-1),("R",1)):
    pb=rig.pose.bones["upperarm_"+side]
    rest=pb.bone.matrix_local.to_3x3()
    direction=(pb.bone.tail_local-pb.bone.head_local).normalized()
    desired=Vector((sign*.12,0,-.993)).normalized()
    correction=direction.rotation_difference(desired)
    pb.rotation_mode="QUATERNION"
    # PoseBone rotation is expressed in bone-local axes. Limit the initial
    # rest correction to prevent a full cross-body swing on this OBJ.
    local_correction=rest.to_quaternion().inverted() @ correction @ rest.to_quaternion()
    pb.rotation_quaternion=Quaternion((1,0,0,0)).slerp(local_correction,.85)
    print("ARM_POSE_CORRECTION",side,"angle_degrees",
          math.degrees(local_correction.angle),"applied_fraction",.85,flush=True)
bpy.context.view_layer.update()
# Measure actual shoulder skin movement BEFORE baking. Bone positions alone
# cannot prove that the arm vertices follow the armature.
deps=bpy.context.evaluated_depsgraph_get()
def skin_positions():
    evaluated=body.evaluated_get(deps)
    mesh=evaluated.to_mesh()
    result=[v.co.copy() for v in mesh.vertices]
    evaluated.to_mesh_clear()
    return result
# The original imported mesh is still accessible in body.data.
rest_positions=[v.co.copy() for v in body.data.vertices]
posed_positions=skin_positions()
# Verify the identity pose is stable before applying any shoulder rotation.
# Catch coordinate-space and parenting errors rather than baking them.
for pb in rig.pose.bones:
    pb.rotation_mode="QUATERNION"
    pb.rotation_quaternion=(1,0,0,0)
bpy.context.view_layer.update()
identity_positions=skin_positions()
identity_errors=sorted((identity_positions[i]-rest_positions[i]).length for i in range(len(rest_positions)))
print("IDENTITY_SKIN_ERROR",identity_errors[-1],identity_errors[len(identity_errors)//2],flush=True)
if identity_errors[-1]>.005*h:
    raise RuntimeError("Armature identity skin unexpectedly moves mesh; aborting before rest bake")
# Restore the intended shoulder pose after the identity check.
for side,sign in (("L",-1),("R",1)):
    pb=rig.pose.bones["upperarm_"+side]
    rest=pb.bone.matrix_local.to_3x3()
    direction=(pb.bone.tail_local-pb.bone.head_local).normalized()
    desired=Vector((sign*.12,0,-.993)).normalized()
    correction=direction.rotation_difference(desired)
    local_correction=rest.to_quaternion().inverted() @ correction @ rest.to_quaternion()
    pb.rotation_quaternion=Quaternion((1,0,0,0)).slerp(local_correction,.85)
bpy.context.view_layer.update()
posed_positions=skin_positions()
for side,sign in (("L",-1),("R",1)):
    ids={g.index for g in body.vertex_groups if g.name in
         ("upperarm_"+side,"forearm_"+side,"hand_"+side)}
    strong=[]
    for v in body.data.vertices:
        strength=sum(g.weight for g in v.groups if g.group in ids)
        if strength>.70: strong.append(v.index)
    if len(strong)<100:
        counts={}
        for group in body.vertex_groups:
            if group.name.endswith("_"+side):
                counts[group.name]=sum(1 for v in body.data.vertices
                    for g in v.groups if g.group==group.index and g.weight>.10)
        print("ARM_WEIGHT_DIAGNOSTIC",side,"strong",len(strong),
              "groups_above_10pct",json.dumps(counts),flush=True)
        raise RuntimeError("Arm "+side+" has fewer than 100 strongly weighted vertices; reject deformed rig")
    movements=sorted((posed_positions[i]-rest_positions[i]).length for i in strong)
    median=movements[len(movements)//2]
    print("SHOULDER_SKIN_PROBE",side,"strong_vertices",len(strong),
          "median_displacement",median,"max_displacement",movements[-1],flush=True)
    if median<.04*h:
        raise RuntimeError("Arm "+side+" bone rotated but weighted skin barely moved")
# Bake the evaluated *skinned* geometry, not approximated vertex rotations.
# The armature's new rest pose is then made identical to that geometry.
skin=next((m for m in body.modifiers if m.type=="ARMATURE" and m.object==rig),None)
if skin is None: raise RuntimeError("Armature modifier missing before rest bake")
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.object.modifier_apply(modifier=skin.name)
if len(body.data.vertices)!=original_count:
    raise RuntimeError("Rest bake unexpectedly changed vertex count")
bpy.ops.object.select_all(action="DESELECT")
rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
for pb in rig.pose.bones:
    pb.rotation_mode="XYZ"
    pb.rotation_euler=(0,0,0)
skin=body.modifiers.new("OceanSerenitySkin","ARMATURE")
skin.object=rig
bpy.context.view_layer.update()
for side in ("L","R"):
    shoulder=rig.data.bones["upperarm_"+side].head_local
    wrist=rig.data.bones["forearm_"+side].tail_local
    hand=rig.data.bones["hand_"+side].tail_local
    print("REST_BAKE",side,"shoulder",tuple(shoulder),"wrist",tuple(wrist),"hand",tuple(hand),flush=True)
    if hand.z>shoulder.z-.15*h or abs(hand.x)>abs(shoulder.x)+.16*h:
        raise RuntimeError("Rest pose arm remains extended on "+side)
print("REST_BAKE: Blender evaluated skin applied; rest skeleton synchronized",flush=True)
# Validate visible mesh, not just bones. Extended arms must not be accepted.
outliers=[v for v in body.data.vertices if .55*h<v.co.z<.84*h and abs(v.co.x)>.32*h]
print("VISIBLE_ARM_EXTENT",len(outliers),"vertices beyond 32% of character height",flush=True)
if outliers:
    for side,sgn in (("L",-1),("R",1)):
        affected=[v for v in outliers if v.co.x*sgn>0]
        if not affected: continue
        by_group={}
        for v in affected:
            strongest=max(v.groups,key=lambda g:g.weight) if v.groups else None
            group_name=body.vertex_groups[strongest.group].name if strongest else "UNWEIGHTED"
            by_group[group_name]=by_group.get(group_name,0)+1
        print("ARM_OUTLIER_DIAGNOSTIC",side,"count",len(affected),
              "x_range",[min(v.co.x for v in affected),max(v.co.x for v in affected)],
              "z_range",[min(v.co.z for v in affected),max(v.co.z for v in affected)],
              "dominant_groups",json.dumps(by_group,sort_keys=True),flush=True)
    print("ARM_OUTLIER_SAMPLES",[(v.index,tuple(round(float(a),3) for a in v.co))
         for v in outliers[::max(1,len(outliers)//12)][:12]],flush=True)
if len(outliers)>80:
    # Preserve the failed rig for Blender inspection instead of exporting a
    # broken FBX. This file retains the evaluated mesh and weight groups.
    diagnostic=target.with_name("OceanSerenity_FailedRig_Diagnostic.blend")
    diagnostic.parent.mkdir(parents=True,exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(diagnostic))
    print("FAILED_RIG_DIAGNOSTIC",str(diagnostic),flush=True)
    raise RuntimeError("Visible arm mesh remains extended after rest bake: %d outlier vertices. Rejecting FBX."%len(outliers))

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
        # Arm lowering is already baked into the mesh and skeleton rest pose.
        # Animate relative to the relaxed rest pose only.
        for side in ("L","R"):
            for part in ("upperarm","forearm"):
                bone_name=part+"_"+side
                pb=rig.pose.bones[bone_name]
                rest=pb.bone.matrix_local.to_3x3()
                axis=rest.inverted() @ Vector((1,0,0))
                pb.rotation_mode="QUATERNION"
                pb.rotation_quaternion=Quaternion(axis,math.radians(arm_motion.get(bone_name,0)))
        if clip_name=="Jump":
            # Lift the whole character in mid-air, not only the legs.
            jump_heights={1:0.0,7:0.18,15:0.34,23:0.10,30:0.0}
            rig.pose.bones["root"].location.z=jump_heights.get(frame,0.0)*h
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
    if track.name != "Ocean_"+clip_name or strip.name != "Ocean_"+clip_name:
        raise RuntimeError("Animation take mislabeled: "+clip_name)
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
# Do not mistake an anatomically plausible bone pose for proof of good skinning.
# Preview images must be reviewed before this experimental FBX is shipped.
rig.animation_data.action=bpy.data.actions["Ocean_Idle"]
bpy.context.scene.frame_set(1)
bpy.context.view_layer.update()
for side in ("L","R"):
    shoulder=rig.pose.bones["upperarm_"+side].head
    hand=rig.pose.bones["hand_"+side].tail
    print("IDLE_ARM",side,"shoulder",tuple(shoulder),"hand",tuple(hand),flush=True)
    if hand.z > shoulder.z-.15*h:
        raise RuntimeError("Idle hand not below shoulder on "+side)
if not any(m.type=="ARMATURE" and m.object==rig for m in body.modifiers):
    raise RuntimeError("Missing armature modifier")
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
        "bones":len(bones),"height":h,"width":w,"weight_method":weight_method,"warning":"Experimental automatic skinning; inspect rendered shoulder geometry before use"}
target.with_suffix(".json").write_text(json.dumps(report,indent=2))
print("RIG_REPORT "+json.dumps(report))
