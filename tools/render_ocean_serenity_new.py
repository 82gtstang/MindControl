"""Render actual Ocean Serenity Blender model from saved .blend, without altering it."""
import bpy, sys, math
from pathlib import Path
from mathutils import Vector
args=sys.argv[sys.argv.index("--")+1:]
out=Path(args[0]);out.mkdir(parents=True,exist_ok=True)
scene=bpy.context.scene
scene.render.engine='BLENDER_EEVEE_NEXT' if bpy.app.version >= (4, 2, 0) else 'BLENDER_EEVEE'
scene.render.resolution_x=900
scene.render.resolution_y=1200
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.world.color=(.16,.17,.20)
rig=bpy.data.objects.get('OceanSerenity_NewRig')
mesh=bpy.data.objects.get('OceanSerenity_Rebuilt')
if not rig or not mesh: raise RuntimeError('Expected new Ocean Serenity mesh and rig missing')
def light(name,xyz,power,size):
 d=bpy.data.lights.new(name,'AREA');d.energy=power;d.shape='DISK';d.size=size
 o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=xyz
 direction=Vector((0,0,1.0))-o.location;o.rotation_euler=direction.to_track_quat('-Z','Y').to_euler()
light('Key',(2,-3,4),650,4)
light('Fill',(-3,-1,2),400,4)
light('Rim',(1,3,3),800,3)
camera_data=bpy.data.cameras.new('Preview Camera');camera=bpy.data.objects.new('Preview Camera',camera_data);scene.collection.objects.link(camera);scene.camera=camera
camera_data.type='ORTHO';camera_data.ortho_scale=2.3
for name,xyz in [('front',(0,-4,1.1)),('side',(4,0,1.1)),('back',(0,4,1.1)),('three_quarter',(3,-4,1.3))]:
 camera.location=xyz
 direction=Vector((0,0,1.0))-camera.location
 camera.rotation_euler=direction.to_track_quat('-Z','Y').to_euler()
 scene.render.filepath=str(out/(name+'.png'))
 bpy.ops.render.render(write_still=True)
 print('RENDERED_PREVIEW',name,scene.render.filepath,flush=True)
