"""Fresh procedural Ocean Serenity character; independent of broken Meshy OBJ.
Blender 4.3+. Generates fully weighted humanoid with articulated fingers.
This is a clean game-ready prototype, not a scan-quality photorealistic sculpt.
"""
import bpy, math, sys, json
from pathlib import Path
from mathutils import Vector
args=sys.argv[sys.argv.index("--")+1:]
target=Path(args[0]); target.parent.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
def mat(name,color,metal=0,rough=.7):
 m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); m.use_nodes=True
 p=m.node_tree.nodes.get('Principled BSDF'); p.inputs['Base Color'].default_value=(*color,1)
 p.inputs['Metallic'].default_value=metal;p.inputs['Roughness'].default_value=rough
 return m
skin=mat('Fair natural skin',(.77,.58,.49),0,.53)
# Subsurface scattering gives a more natural skin response in Blender renders.
sp=skin.node_tree.nodes.get('Principled BSDF')
if sp.inputs.get('Subsurface Weight'): sp.inputs['Subsurface Weight'].default_value=.18
if sp.inputs.get('Subsurface Radius'): sp.inputs['Subsurface Radius'].default_value=(1.0,.45,.3)
cloth=skin  # Neutral anatomical reference model; no outfit
trim=mat('Silver trim',(.52,.59,.62),.65)
hair=mat('Deep brown hair',(.075,.037,.026))
boot=skin  # Bare feet for anatomical reference
eye=mat('Iris dark',(.065,.12,.12))
parts=[]
def ell(name,loc,scale,material,bone):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=16,location=loc)
 o=bpy.context.object;o.name=name
 o.scale=scale;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 bpy.ops.object.shade_smooth();o.data.materials.append(material)
 parts.append((o,bone));return o
def link(name,a,b,r1,r2,material,bone):
 a,b=Vector(a),Vector(b);delta=b-a
 bpy.ops.mesh.primitive_cone_add(vertices=20,radius1=r1,radius2=r2,depth=delta.length,location=(a+b)*.5)
 o=bpy.context.object;o.name=name;o.rotation_euler=delta.to_track_quat('Z','Y').to_euler()
 bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
 bpy.ops.object.shade_smooth();o.data.materials.append(material);parts.append((o,bone))
# Coordinate system: Z up, face toward -Y, arms relaxed beside body.
bones=[('root',None,(0,0,0), (0,0,.12)),
 ('pelvis','root',(0,0,.91),(0,0,1.06)),
 ('spine','pelvis',(0,0,1.06),(0,0,1.30)),
 ('chest','spine',(0,0,1.30),(0,0,1.51)),
 ('neck','chest',(0,0,1.51),(0,0,1.61)),
 ('head','neck',(0,0,1.61),(0,0,1.83))]
ell('hips',(0,0,1.00),(.22,.135,.18),cloth,'pelvis')
ell('waist',(0,0,1.20),(.16,.115,.19),cloth,'spine')
ell('upper torso',(0,0,1.42),(.235,.145,.18),cloth,'chest')
ell('neck skin',(0,0,1.565),(.062,.062,.085),skin,'neck')
ell('head face',(0,-.012,1.725),(.108,.091,.155),skin,'head')
ell('hair cap',(0,.021,1.81),(.117,.104,.085),hair,'head')
ell('nose',(0,-.108,1.715),(.019,.026,.037),skin,'head')
ell('upper lip',(0,-.096,1.666),(.032,.012,.009),skin,'head')
ell('lower lip',(0,-.095,1.649),(.029,.012,.012),skin,'head')
ell('chin',(0,-.078,1.625),(.045,.026,.026),skin,'head')
for s,side in ((-1,'L'),(1,'R')):
 ell('eye '+side,(s*.043,-.098,1.745),(.015,.009,.011),eye,'head')
 ell('hair lock '+side,(s*.097,.043,1.67),(.035,.05,.17),hair,'head')
 shoulder=(s*.225,0,1.475);elbow=(s*.285,0,1.205);wrist=(s*.305,-.012,.985)
 bones += [('clavicle_'+side,'chest',(s*.10,0,1.49),shoulder),
 ('upperarm_'+side,'clavicle_'+side,shoulder,elbow),
 ('forearm_'+side,'upperarm_'+side,elbow,wrist),
 ('hand_'+side,'forearm_'+side,wrist,(s*.309,-.012,.875))]
 ell('shoulder '+side,shoulder,(.076,.075,.082),cloth,'upperarm_'+side)
 link('upper arm '+side,shoulder,elbow,.070,.058,cloth,'upperarm_'+side)
 ell('elbow '+side,elbow,(.058,.058,.059),skin,'forearm_'+side)
 link('forearm '+side,elbow,wrist,.053,.038,skin,'forearm_'+side)
 ell('palm '+side,(s*.31,-.013,.925),(.047,.026,.067),skin,'hand_'+side)
 # Five separate fingers, 3 phalanges each, downward at rest.
 for f in range(5):
  fx=s*(.278+f*.016)
  fy=-.015 + (.018 if f==0 else 0)
  start=(fx,fy,.880 if f else .935)
  lengths=([.036,.028,.020] if f==0 else
           [.041,.031,.024] if f in (1,4) else [.052,.038,.026])
  prev='hand_'+side;pt=Vector(start)
  for j,length in enumerate(lengths):
   end=pt+Vector((s*(-.012 if f==0 else 0),-.008 if f==0 else 0,-length))
   bn=f'finger{f}_{j}_{side}'
   bones.append((bn,prev,tuple(pt),tuple(end)))
   link(bn+' mesh',pt,end,.009-j*.0015,.007-j*.0015,skin,bn)
   ell(bn+' joint',end,(.008-j*.001,.008-j*.001,.008-j*.001),skin,bn)
   prev=bn;pt=end
 hip=(s*.13,0,.94);knee=(s*.145,-.01,.53);ankle=(s*.145,0,.105)
 bones += [('thigh_'+side,'pelvis',hip,knee),
           ('shin_'+side,'thigh_'+side,knee,ankle),
           ('foot_'+side,'shin_'+side,ankle,(s*.145,-.17,.075))]
 link('thigh '+side,hip,knee,.105,.073,cloth,'thigh_'+side)
 ell('knee '+side,knee,(.074,.075,.072),cloth,'shin_'+side)
 link('boot shaft '+side,knee,ankle,.075,.062,boot,'shin_'+side)
 ell('boot foot '+side,(s*.145,-.10,.078),(.085,.17,.065),boot,'foot_'+side)
 ell('toe '+side,(s*.145,-.224,.056),(.062,.046,.031),skin,'foot_'+side)
# Build skeleton. Each separate anatomical mesh component is assigned 100%
# to its matching bone; no automatic heat weights or cross-body leakage.
bpy.ops.object.armature_add()
rig=bpy.context.object;rig.name='OceanSerenity_NewRig'
bpy.ops.object.mode_set(mode='EDIT')
rig.data.edit_bones.remove(rig.data.edit_bones[0])
for name,parent,a,b in bones:
 eb=rig.data.edit_bones.new(name);eb.head=a;eb.tail=b
 if parent:eb.parent=rig.data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
# Join all objects, preserving material slots and explicit vertex membership.
bpy.ops.object.select_all(action='DESELECT')
for o,_ in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0][0]
# Assign per-piece vertex groups BEFORE join; Blender joins groups by name.
for o,bone in parts:
 g=o.vertex_groups.new(name=bone);g.add(list(range(len(o.data.vertices))),1,'REPLACE')
bpy.ops.object.join();body=bpy.context.object;body.name='OceanSerenity_Rebuilt'
mod=body.modifiers.new('Skin','ARMATURE');mod.object=rig
body.parent=rig;body.matrix_parent_inverse=rig.matrix_world.inverted()
# No disconnected fin geometry: hands are individually modeled fingers.
assert len([b for b in rig.data.bones if b.name.startswith('finger')])==30
assert len(body.data.vertices)>10000
for side,sign in (('L',-1),('R',1)):
 hand=rig.data.bones['hand_'+side]
 assert hand.tail_local.z<hand.head_local.z
 for f in range(5):
  assert rig.data.bones[f'finger{f}_2_{side}'].tail_local.z < hand.head_local.z
# Save editable Blender source and FBX; Unreal export axis settings.
blend=target.with_suffix('.blend')
bpy.ops.wm.save_as_mainfile(filepath=str(blend))
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,add_leaf_bones=False,
                         apply_unit_scale=True,axis_forward='-Y',axis_up='Z',
                         bake_anim=False)
report={'character':'Ocean Serenity rebuilt prototype','source':'new procedural geometry',
 'bones':len(rig.data.bones),'finger_bones':30,'vertices':len(body.data.vertices),
 'artifacts':[str(blend),str(target)],
 'limitations':'Anatomical reference prototype only; NOT photorealistic, lacks seamless topology, scanned skin textures, strand hair and facial rig. Requires visual approval and animation testing'}
target.with_suffix('.json').write_text(json.dumps(report,indent=2))
print('REBUILT_CHARACTER_REPORT',json.dumps(report),flush=True)
