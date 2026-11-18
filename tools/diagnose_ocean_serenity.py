"""Inspect a rejected Ocean Serenity .blend on the Windows Actions runner."""
import bpy, json, sys, math
from pathlib import Path
from mathutils import Vector
out=Path(sys.argv[sys.argv.index("--")+1])
out.mkdir(parents=True,exist_ok=True)
body=bpy.data.objects.get("OceanSerenity")
rig=bpy.data.objects.get("OceanSerenityRig")
if not body or not rig: raise RuntimeError("Diagnostic mesh or rig missing")
h=max(v.co.z for v in body.data.vertices)-min(v.co.z for v in body.data.vertices)
groups=body.vertex_groups
results={"blender":bpy.app.version_string,"vertices":len(body.data.vertices),"height":h,"arm_groups":{},"outliers":[]}
for side in ("L","R"):
    ids={g.index for g in groups if g.name in ("upperarm_"+side,"forearm_"+side,"hand_"+side)}
    verts=[v for v in body.data.vertices if any(g.group in ids and g.weight>.5 for g in v.groups)]
    results["arm_groups"][side]={"strong_vertices":len(verts),"x_min":min((v.co.x for v in verts),default=None),"x_max":max((v.co.x for v in verts),default=None),"z_min":min((v.co.z for v in verts),default=None),"z_max":max((v.co.z for v in verts),default=None)}
for v in body.data.vertices:
    if .55*h<v.co.z<.84*h and abs(v.co.x)>.32*h:
        best=max(v.groups,key=lambda g:g.weight) if v.groups else None
        results["outliers"].append({"index":v.index,"xyz":[round(float(x),4) for x in v.co],"group":groups[best.group].name if best else None,"weight":round(best.weight,3) if best else None})
(out/"arm_diagnostics.json").write_text(json.dumps(results,indent=2))
print("DIAGNOSTIC outliers",len(results["outliers"]),"arm_groups",json.dumps(results["arm_groups"]),flush=True)
# Render the actual saved mesh in neutral studio lighting, with both sides.
bpy.ops.object.select_all(action="DESELECT")
for obj in bpy.context.scene.objects:
    if obj.type not in {"MESH","ARMATURE"}: obj.hide_render=True
rig.hide_render=True
scene=bpy.context.scene
scene.render.engine="BLENDER_EEVEE_NEXT"
scene.render.resolution_x=900
scene.render.resolution_y=1000
scene.render.resolution_percentage=100
scene.render.image_settings.file_format="PNG"
scene.world.color=(.25,.25,.25)
mat=bpy.data.materials.new("DiagnosticGray")
mat.diffuse_color=(.65,.67,.72,1)
body.data.materials.clear()
body.data.materials.append(mat)
cam_data=bpy.data.cameras.new("DiagnosticCamera")
cam=bpy.data.objects.new("DiagnosticCamera",cam_data)
scene.collection.objects.link(cam)
scene.camera=cam
cam_data.type="ORTHO"
cam_data.ortho_scale=h*1.25
center=Vector((0,0,h*.53))
for name,pos in (("front",(0,-h*2,h*.55)),("side",(h*2,0,h*.55)),("back",(0,h*2,h*.55))):
    cam.location=pos
    direction=center-cam.location
    cam.rotation_euler=direction.to_track_quat("-Z","Y").to_euler()
    scene.render.filepath=str(out/("failed_rig_"+name+".png"))
    bpy.ops.render.render(write_still=True)
    print("DIAGNOSTIC_IMAGE",scene.render.filepath,flush=True)
