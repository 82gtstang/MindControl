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
scene.display.shading.color_type="MATERIAL"
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
    frames=[int(start+(end-start)*f) for f in (.1,.5,.9)]
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
images=list(out.glob("*.png"))
if not images:
    raise RuntimeError("No PNG images rendered to "+str(out))
print("Rendered",len(images),"PNG files",flush=True)
(out/"diagnostics.json").write_text(json.dumps(report,indent=2))
print("Saved animation previews:",out)
