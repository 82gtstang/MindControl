"""Render FBX animation diagnostic frames with Blender (headless)."""
import bpy,sys,math,json,re
from pathlib import Path
from mathutils import Vector
args=sys.argv[sys.argv.index("--")+1:]
src,out=Path(args[0]).resolve(),Path(args[1]).resolve();out.mkdir(parents=True,exist_ok=True)
print("PREVIEW SOURCE:",src,"OUTPUT:",out,flush=True)
bpy.ops.object.select_all(action="SELECT");bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(src),use_anim=True)
meshes=[o for o in bpy.context.scene.objects if o.type=="MESH"]
rigs=[o for o in bpy.context.scene.objects if o.type=="ARMATURE"]
if not meshes or not rigs: raise RuntimeError("FBX missing mesh or armature")
rig=rigs[0]
actions=list(bpy.data.actions)
if not actions:
    raise RuntimeError("FBX imported without animation actions; inspect FBX export settings")
print("DIAGNOSTIC ACTIONS:",[a.name for a in actions])
scene=bpy.context.scene
scene.render.engine="BLENDER_WORKBENCH"
scene.render.resolution_x=720;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.display.shading.light="STUDIO"
scene.display.shading.color_type="VERTEX"
scene.render.film_transparent=False
points=[o.matrix_world@Vector(corner) for o in meshes for corner in o.bound_box]
lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
center=(lo+hi)*.5
dimensions=hi-lo
# Blender 2.92 imports this FBX with its height on Y, not Z.
# Detect the vertical axis from the largest rest-pose extent.
up_axis=max(range(3),key=lambda i:dimensions[i])
height=max(dimensions[up_axis],.01)
horizontal=[i for i in range(3) if i!=up_axis]
# For the Ocean Serenity FBX the wide horizontal axis is X and depth is Z.
wide_axis=max(horizontal,key=lambda i:dimensions[i])
depth_axis=next(i for i in horizontal if i!=wide_axis)
width=max(dimensions[wide_axis],.01)
depth=max(dimensions[depth_axis],.01)
head_bone=rig.data.bones.get("head")
foot_bone=rig.data.bones.get("foot_L") or rig.data.bones.get("foot_R")
up_sign=1
if head_bone and foot_bone:
    head_world=rig.matrix_world @ head_bone.head_local
    foot_world=rig.matrix_world @ foot_bone.head_local
    up_sign=1 if head_world[up_axis]>=foot_world[up_axis] else -1
print("Camera axes: up",up_axis,"sign",up_sign,"width",wide_axis,"depth",depth_axis,flush=True)
bpy.ops.object.camera_add()
cam=bpy.context.object
scene.camera=cam
cam.data.type="ORTHO"
cam.data.ortho_scale=max(height*1.35,width*1.25,depth*1.25)
distance=max(height,width,depth)*2.5
def set_view(view):
    position=center.copy()
    axis=depth_axis if view=="front" else wide_axis
    position[axis]-=distance
    cam.location=position
    # Align camera image vertical to the imported model's actual up axis.
    up=Vector((0,0,0));up[up_axis]=up_sign
    direction=center-cam.location
    cam.rotation_euler=direction.to_track_quat("-Z","Y").to_euler() if up_axis==2 else direction.to_track_quat("-Z","X").to_euler()
    # Blender's default camera up convention is Z; for Y-up, rotate
    # the camera's local frame so image vertical follows world Y.
    from mathutils import Matrix
    right=direction.cross(up).normalized()
    image_up=right.cross(direction).normalized()
    backward=-direction.normalized()
    matrix=Matrix(((right.x,image_up.x,backward.x),(right.y,image_up.y,backward.y),(right.z,image_up.z,backward.z)))
    cam.rotation_euler=matrix.to_euler()
report={"actions":[],"mesh_count":len(meshes),"bones":len(rig.data.bones)}
if not rig.animation_data: rig.animation_data_create()
for action in actions:
    name=action.name
    if not any(s in name.lower() for s in ("idle","walk","run","jump","attack","spin")):
        print("Skipping unmatched action:",name,flush=True)
        continue
    # Temporarily disable NLA strips so the selected action is evaluated directly.
    for track in rig.animation_data.nla_tracks:track.mute=True
    rig.animation_data.action=action
    start,end=action.frame_range
    fractions=(0,.125,.25,.375,.5,.625,.75,.875,1) if "spin" in name.lower() else (.1,.5,.9)
    frames=[int(start+(end-start)*f) for f in fractions]
    samples=[]
    for index,frame in enumerate(frames):
        scene.frame_set(frame)
        depsgraph=bpy.context.evaluated_depsgraph_get()
        evaluated=meshes[0].evaluated_get(depsgraph)
        corners=[evaluated.matrix_world@Vector(c) for c in evaluated.bound_box]
        mins=[min(v[i] for v in corners) for i in range(3)]
        maxs=[max(v[i] for v in corners) for i in range(3)]
        samples.append({"frame":frame,"bbox_min":mins,"bbox_max":maxs})
        safe_name=re.sub(r'[^A-Za-z0-9_.-]+', '_', name)
        for view in ("front","side"):
            set_view(view)
            scene.render.filepath=str(out/(safe_name+"_"+view+"_"+str(index)+".png"))
            bpy.ops.render.render(write_still=True)
    report["actions"].append({"name":name,"samples":samples})
if not report["actions"]:
    raise RuntimeError("No matching animation actions found. Available: "+repr([a.name for a in actions]))
# Shoulder rig diagnostic: rest-space joint axes and visible bone markers.
# Render the imported rig at rest, without relying on viewport-only bone overlays.
from mathutils import Quaternion
rig.animation_data.action=None
for track in rig.animation_data.nla_tracks: track.mute=True
scene.frame_set(1)
for pb in rig.pose.bones:
    pb.rotation_mode="QUATERNION"
    pb.rotation_quaternion=(1,0,0,0)
    pb.location=(0,0,0)
bpy.context.view_layer.update()
shoulders=[]
markers=[]
axis_colors={"X":(1.0,.15,.15,1),"Y":(.15,1.0,.15,1),"Z":(.15,.35,1.0,1)}
def add_marker(a,b,radius,color,label):
    direction=b-a
    if direction.length<1e-6:return
    bpy.ops.mesh.primitive_cylinder_add(vertices=10,radius=radius,depth=direction.length,location=(a+b)*.5)
    obj=bpy.context.object
    obj.name=label
    obj.rotation_euler=direction.to_track_quat("Z","Y").to_euler()
    mat=bpy.data.materials.new(label+"_material")
    mat.diffuse_color=color
    obj.data.materials.append(mat)
    markers.append(obj)
for name in ("clavicle_L","upperarm_L","forearm_L","clavicle_R","upperarm_R","forearm_R"):
    bone=rig.data.bones.get(name)
    if not bone:continue
    head=rig.matrix_world @ bone.head_local
    tail=rig.matrix_world @ bone.tail_local
    rot=rig.matrix_world.to_3x3() @ bone.matrix_local.to_3x3()
    entry={"bone":name,"head":list(head),"tail":list(tail),"axes":{}}
    add_marker(head,tail,height*.007,(.95,.82,.15,1),name+"_bone")
    for axis,index in (("X",0),("Y",1),("Z",2)):
        unit=Vector(tuple(1.0 if i==index else 0.0 for i in range(3)))
        vector=(rot @ unit).normalized()
        entry["axes"][axis]=list(vector)
        add_marker(head,head+vector*height*.10,height*.004,axis_colors[axis],name+"_"+axis)
    shoulders.append(entry)
report["shoulder_rig"]={"axes_legend":{"X":"red","Y":"green","Z":"blue"},"bones":shoulders}
# Skinning probe: rotate each shoulder 50 degrees around anatomical world Y.
# Capture before/after mesh views to reveal whether the weighted vertices follow.
from mathutils import Quaternion
for obj in markers:obj.hide_render=True
report["shoulder_weight_tests"]=[]
# Capture evaluated skin coordinates before the test, not just bone positions.
# A moving bone with stationary mesh vertices indicates lost/incorrect FBX skinning.
def evaluated_mesh_positions():
    deps=bpy.context.evaluated_depsgraph_get()
    return [[tuple(obj.matrix_world @ v.co) for v in obj.evaluated_get(deps).to_mesh().vertices] for obj in meshes]
baseline_mesh_positions=evaluated_mesh_positions()
for side in ("L","R"):
    pb=rig.pose.bones.get("upperarm_"+side)
    if not pb:continue
    rest=pb.bone.matrix_local.to_3x3()
    axis=rest.inverted() @ Vector((0,1,0))
    pb.rotation_mode="QUATERNION"
    pb.rotation_quaternion=Quaternion(axis,math.radians(-50 if side=="L" else 50))
    bpy.context.view_layer.update()
    rest_head=rig.matrix_world @ pb.bone.head_local
    rest_tail=rig.matrix_world @ pb.bone.tail_local
    posed_head=rig.matrix_world @ pb.head
    posed_tail=rig.matrix_world @ pb.tail
    posed_mesh_positions=evaluated_mesh_positions()
    moved_count=0
    max_movement=0.0
    shoulder_region_count=0
    for obj_index, (before,after) in enumerate(zip(baseline_mesh_positions,posed_mesh_positions)):
        for orig,now in zip(before,after):
            # Check arm-side vertices outside the torso near the shoulder height.
            if (orig[0]<-0.25 if side=="L" else orig[0]>0.25) and orig[2]>0.95:
                shoulder_region_count+=1
                delta=(Vector(now)-Vector(orig)).length
                if delta>.02:moved_count+=1
                max_movement=max(max_movement,delta)
    print("SKIN_TEST",side,"region",shoulder_region_count,"moved",moved_count,"max_m",max_movement,flush=True)
    report["shoulder_weight_tests"].append({
        "shoulder_region_vertices":shoulder_region_count,
        "vertices_moved_over_2cm":moved_count,
        "max_vertex_movement_m":max_movement,
        "side":side,"angle_degrees":-50 if side=="L" else 50,
        "rest_head":list(rest_head),"rest_tail":list(rest_tail),
        "posed_head":list(posed_head),"posed_tail":list(posed_tail),
        "tail_displacement_m":(posed_tail-rest_tail).length,
        "rotation_mode":pb.rotation_mode
    })
    print("SHOULDER_TEST",side,"tail_displacement_m",(posed_tail-rest_tail).length,flush=True)
    set_view("front")
    scene.render.filepath=str(out/("Shoulder_Weight_Test_"+side+".png"))
    bpy.ops.render.render(write_still=True)
    pb.rotation_quaternion=(1,0,0,0)
    bpy.context.view_layer.update()
for obj in markers:obj.hide_render=False
# Hide skinned geometry so joint and local-axis markers are unobstructed.
for obj in meshes:obj.hide_render=True
for view in ("front","side"):
    set_view(view)
    scene.render.filepath=str(out/("Shoulder_Rig_Axes_"+view+".png"))
    bpy.ops.render.render(write_still=True)
for obj in meshes:obj.hide_render=False
for obj in markers:bpy.data.objects.remove(obj,do_unlink=True)
images=list(out.glob("*.png"))
if not images:
    raise RuntimeError("No PNG images rendered to "+str(out))
print("Rendered",len(images),"PNG files",flush=True)
(out/"diagnostics.json").write_text(json.dumps(report,indent=2))
print("Saved animation previews:",out)

