"""Generate original dungeon creatures and loot base meshes for Unreal."""
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

output = Path(sys.argv[sys.argv.index("--") + 1])
output.mkdir(parents=True, exist_ok=True)
skins = [
    (.26,.16,.09),(.47,.44,.29),(.12,.10,.07),(.54,.16,.07),
    (.19,.28,.15),(.24,.19,.27),(.22,.25,.29),(.29,.40,.48),
    (.35,.15,.06),(.12,.12,.20),(.25,.26,.10),(.43,.27,.09),
]

def material(name, rgb, rough=.65, metal=0.):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*rgb, 1)
    shader.inputs["Roughness"].default_value = rough
    shader.inputs["Metallic"].default_value = metal
    return mat

def finish(obj, mat, smooth=True):
    obj.data.materials.append(mat)
    for poly in obj.data.polygons:
        poly.use_smooth = smooth
    return obj

def ellipsoid(center, scale, mat, roughness=0.):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3 if roughness else 2, location=center)
    obj=bpy.context.object
    for vertex in obj.data.vertices:
        p=vertex.co.copy()
        displace=1+roughness*math.sin(13*p.x+9*p.z)*math.cos(11*p.y-7*p.z)
        vertex.co=tuple(p[i]*scale[i]*displace for i in range(3))
    return finish(obj,mat)

def tube(a,b,r1,r2,mat,edges=10):
    a,b=Vector(a),Vector(b)
    delta=b-a
    bpy.ops.mesh.primitive_cone_add(vertices=edges,radius1=r1,radius2=r2,depth=delta.length,location=(a+b)/2)
    obj=bpy.context.object
    obj.rotation_euler=delta.to_track_quat('Z','Y').to_euler()
    return finish(obj,mat)

def box(center,scale,mat,angle=0.):
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    obj=bpy.context.object
    obj.dimensions=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    modifier=obj.modifiers.new("Beveled edges","BEVEL")
    modifier.width=min(scale)*.16
    modifier.segments=2
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.rotation_euler.x=angle
    return finish(obj,mat,False)

def panel(points, mat, thickness=.022):
    mesh=bpy.data.meshes.new("Surface")
    mesh.from_pydata(points,[],[tuple(range(len(points)))])
    mesh.update()
    obj=bpy.data.objects.new("Surface",mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj
    modifier=obj.modifiers.new("Thickness","SOLIDIFY")
    modifier.thickness=thickness
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    return finish(obj,mat)

def export(name):
    bpy.ops.object.select_all(action="SELECT")
    bpy.context.view_layer.objects.active=next(obj for obj in bpy.context.selected_objects if obj.type=="MESH")
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(output/f"{name}.fbx"),use_selection=True,bake_anim=False)

def arachnid(index, hide, plates, teeth, eye):
    ellipsoid((0,.29,.49),(.39,.54,.32),hide,.10)
    ellipsoid((0,-.26,.44),(.29,.35,.24),plates,.12)
    for side in (-1,1):
        for leg in range(4 if index==2 else 3):
            y=-.45+leg*.24
            spread=(leg-1.5)*.35
            points=[(.18*side,y,.42),((.63+.09*(leg%2))*side,y+spread,.92-.06*leg),
                    ((.94+.06*(leg%2))*side,y+spread*1.7,.24),(1.10*side,y+spread*1.88,.04)]
            for a,b,ra,rb in zip(points,points[1:],(.078,.058,.034),(.055,.030,.008)):
                tube(a,b,ra,rb,hide)
            ellipsoid(points[1],(.074,.075,.067),plates)
        for j in range(3):
            ellipsoid((side*(.065+j*.072),-.545+j*.017,.52-j*.028),(.041,.022,.041),eye)
        tube((side*.15,-.49,.35),(side*.20,-.66,.23),.070,.018,teeth)
        tube((side*.20,-.66,.23),(side*.09,-.73,.22),.023,.001,teeth)

def flyer(index, hide, plates, teeth, eye):
    drake=index==8
    z=.89 if drake else 1.34
    ellipsoid((0,.10,z),(.24,.57 if drake else .33,.285),hide,.11)
    ellipsoid((0,-.39,z+.16),(.19,.34 if drake else .20,.19),hide,.08)
    for side in (-1,1):
        shoulder=(side*.13,.04,z+.15)
        wrist=(side*.89,-.15,z+.65)
        tips=[(side*1.7,-.04,z+.60),(side*1.52,.60,z+.02),
              (side*1.03,.82,z-.11),(side*.43,.70,z-.18)]
        tube(shoulder,wrist,.075,.048,plates)
        for j, tip in enumerate(tips):
            tube(wrist,tip,.033,.005,hide)
            if j:
                prior=tips[j-1]
                inset=tuple((prior[k]+tip[k])*.42+wrist[k]*.16 for k in range(3))
                panel([wrist,prior,inset,tip],plates)
        panel([wrist,tips[-1],(0,.56,z-.22),shoulder],plates)
        ellipsoid((side*.085,-.54,z+.22),(.038,.041,.034),eye)
        tube((side*.085,-.32,z+.26),(side*.15,-.25,z+.73 if not drake else z+.53),.095,.008,hide)
        if drake:
            tube((side*.14,.34,z-.10),(side*.33,.29,.14),.11,.065,hide)
            ellipsoid((side*.33,.22,.115),(.11,.16,.068),teeth)
    if drake:
        for a,b,r1,r2 in [((0,.47,z),(0,.98,z-.13),.19,.11),((0,.98,z-.13),(.13,1.51,z-.34),.11,.011)]:
            tube(a,b,r1,r2,hide)

def humanoid(index, hide, plates, cloth, teeth, eye):
    armored=index in (0,1,6,9,10,11)
    width=1.22 if index in (6,10,11) else .90 if index==7 else 1.
    ellipsoid((0,.03,1.27),(.285*width,.20,.35),hide,.08)
    ellipsoid((0,.03,.93),(.225*width,.153,.19),cloth,.05)
    tube((0,0,1.46),(0,-.04,1.72),.132,.104,hide)
    ellipsoid((0,-.09,1.79),(.159,.16,.20),hide,.07)
    ellipsoid((0,-.232,1.772),(.056,.061,.085),hide)
    ellipsoid((0,-.221,1.705),(.095,.054,.067),plates)
    if armored:
        for j in range(4):
            box((0,-.14,1.45-j*.106),(.49*width-j*.058,.125,.106),plates,(-1)**j*.07)
        box((0,-.02,.96),(.49*width,.33,.095),plates)
    else:
        panel([(-.21,-.14,.99),(.21,-.14,.99),(.13,-.17,.69),(.05,-.19,.64),(-.13,-.15,.71)],cloth,.045)
    for side in (-1,1):
        ellipsoid((side*.065,-.223,1.829),(.031,.035,.024),eye)
        tube((side*.130,-.07,1.80),(side*.24,-.05,1.92 if index!=3 else 2.00),.056,.005,hide)
        tube((side*.050,-.260,1.71),(side*.058,-.273,1.655),.024,.002,teeth)
        shoulder=(side*.31*width,.01,1.46)
        elbow=(side*.44*width,-.04,1.18)
        wrist=(side*.53*width,-.20,.94)
        tube(shoulder,elbow,.129,.080,hide)
        tube(elbow,wrist,.096,.054,hide)
        ellipsoid(shoulder,(.159,.145,.143),plates if armored else hide,.07)
        ellipsoid(wrist,(.083,.052,.060),hide)
        for finger in range(3):
            x=wrist[0]+side*(finger-1)*.039
            tip=(x+side*.04,-.28,.80+finger*.015)
            tube((x,-.20,.91),tip,.029,.013,hide)
            tube(tip,(tip[0]-side*.04,-.32,tip[2]-.036),.013,.002,teeth)
        hip=(side*.12*width,.02,.92)
        knee=(side*.16*width,-.037,.53)
        ankle=(side*.155*width,.02,.15)
        tube(hip,knee,.123,.087,hide)
        tube(knee,ankle,.090,.053,hide)
        ellipsoid((side*.155*width,-.086,.086),(.094,.17,.084),plates if armored else hide)
        for j in range(3):
            if index in (1,3,7,10):
                tube((side*.16*width,.012,.28+j*.065),(side*.16*width,.012,.31+j*.065),.088,.087,cloth)
    if index==7:
        tube((0,-.267,1.68),(.041,-.36,1.534),.035,.027,cloth)
        tube((.041,-.36,1.534),(.17,-.41,1.54),.027,.006,cloth)

for index,rgb in enumerate(skins):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    suffix=str(index)
    hide=material("Hide"+suffix,rgb,.74)
    plates=material("Shell"+suffix,tuple(v*.52 for v in rgb),.47,.42 if index in (0,6,9,11) else .06)
    cloth=material("Wrap"+suffix,(.26,.155,.085))
    teeth=material("Ivory"+suffix,(.63,.54,.36),.48)
    eye=material("Eye"+suffix,(.88,.36,.08) if index!=7 else (.47,.80,.92),.28)
    if index in (2,4):
        arachnid(index,hide,plates,teeth,eye)
    elif index in (5,8):
        flyer(index,hide,plates,teeth,eye)
    else:
        humanoid(index,hide,plates,cloth,teeth,eye)
    export(f"Enemy{index:02}")

for index in range(16):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    metal=material("RelicMetal"+str(index),(.50,.35,.16),.30,.7)
    stone=material("RelicCore"+str(index),(.08,.34,.41),.36,.24)
    if index<4:
        tube((0,0,.03),(0,0,.21),.026,.024,metal)
        box((0,0,.21),(.19,.043,.035),metal)
        tube((0,0,.22),(0,0,.68+index*.10),.072,.001,stone,4)
    elif index<8:
        ellipsoid((0,0,.24),(.20,.075,.24),metal)
        ellipsoid((0,-.064,.26),(.065,.065,.09),stone)
        for side in (-1,1):
            box((side*.104,-.065,.24),(.026,.047,.29+index*.013),stone)
    elif index<12:
        ellipsoid((0,0,.18),(.105,.105,.155),stone)
        tube((0,0,.27),(0,0,.395),.037,.039,metal)
        box((0,0,.399),(.094,.094,.054),metal)
    else:
        for i in range(index-10):
            angle=i*math.tau/(index-10)
            x,y=math.cos(angle)*.10,math.sin(angle)*.10
            tube((x,y,.028),(x*.8,y*.8,.25+(i%3)*.07),.067,.003,stone,5)
        ellipsoid((0,0,.046),(.19,.19,.031),metal)
    export(f"Loot{index:02}")
