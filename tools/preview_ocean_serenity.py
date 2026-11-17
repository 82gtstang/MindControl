"""Render FBX animation diagnostic frames with Blender (headless)."""
import bpy,sys,math,json
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
bpy.ops.object.camera_add(location=(0,-5,2.1))
cam=bpy.context.object
scene.camera=cam
def aim(obj,point):
    direction=Vector(point)-obj.location
    obj.rotation_euler=direction.to_track_quat("-Z","Y").to_euler()
# Determine bounds from imported geometry in world coordinates.
from mathutils import Vector
points=[o.matrix_world@Vector(corner) for o in meshes for corner in o.bound_box]
lo=Vector(tuple(min(p[i] for p in points) for i in range(3)))
hi=Vector(tuple(max(p[i] for p in points) for i in range(3)))
center=(lo+hi)*.5
extent=max((hi-lo).length,1.)
cam.location=(center.x,center.y-extent*1.65,center.z+extent*.12)
aim(cam,center)
cam.data.type="ORTHO";cam.data.ortho_scale=extent*1.18
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
        scene.render.filepath=str(out/(name.replace("/","_")+"_"+str(index)+".png"))
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
