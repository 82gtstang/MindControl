"""Generate original dungeon creatures and loot base meshes for Unreal."""
import json
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
    (.19,.18,.17),(.31,.18,.10),(.23,.28,.34),(.15,.31,.19),
    (.41,.17,.11),(.23,.30,.39),(.42,.35,.22),(.17,.44,.65),
]

enemy_surfaces = {}

def material(name, rgb, rough=.65, metal=0.):
    mat = bpy.data.materials.new(name)
    enemy_surfaces[name] = {"color": rgb, "roughness": rough, "metallic": metal}
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
            if index == 2:
                for joint in (1,2):
                    a,b=Vector(points[joint-1]),Vector(points[joint])
                    for bristle in range(4):
                        root=a.lerp(b,(bristle+1)/5)
                        tip=root+Vector((side*.055,(-1)**bristle*.047,.036))
                        tube(root,tip,.008,.001,plates,5)
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
        if index == 5:
            for left,right in zip(tips,tips[1:]):
                a,b=Vector(left),Vector(right)
                for vein in (.31,.56):
                    root=Vector(wrist).lerp(a,vein)
                    end=Vector(wrist).lerp(a.lerp(b,.50),min(vein+.24,.90))
                    tube(root,end,.0065,.0018,hide,5)
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
        for side in (-1,1):
            for rib in range(4):
                height=1.35-rib*.084
                tube((side*.035,-.197,height),(side*.211,-.154,height+.025),.022,.013,hide,8)
            tube((side*.017,-.244,1.854),(side*.103,-.217,1.872),.036,.025,plates,8)
            ellipsoid((side*.098,-.228,1.766),(.056,.033,.056),hide,.09)
        tube((0,-.267,1.68),(.041,-.36,1.534),.035,.027,cloth)
        tube((.041,-.36,1.534),(.17,-.41,1.54),.027,.006,cloth)

def quadruped(index, hide, plates, teeth, eye):
    """Build hounds, boars and cave lizards with different profiles."""
    boar, lizard = index == 13, index == 15
    height = .42 if lizard else .72 if boar else .88
    ellipsoid((0,.22,height),(.40 if boar else .28,.66,.39 if boar else .27),hide,.09)
    ellipsoid((0,-.36,height+.06),(.32,.30,.30),hide,.06)
    ellipsoid((0,-.69,height+.06),(.21,.33,.19),plates,.06)
    ellipsoid((0,-.97,height+.04),(.15,.09,.084),plates)
    for side in (-1,1):
        ellipsoid((side*.134,-.82,height+.15),(.043,.046,.036),eye)
        tube((side*.12,-.54,height+.22),(side*.22,-.59,height+(.43 if boar else .50)),.09,.007,hide)
        if boar:
            tube((side*.15,-.93,height-.03),(side*.26,-1.04,height+.16),.052,.006,teeth)
        for y in (-.30,.62):
            hip=(side*.22,y,height-.15)
            knee=(side*(.48 if lizard else .24),y+.12,height*.36)
            foot=(side*(.58 if lizard else .25),y-.05,.09)
            tube(hip,knee,.126,.067,hide)
            tube(knee,foot,.065,.045,hide)
            ellipsoid(foot,(.09,.15,.07),plates)
    tail=(0,.77,height)
    tip=(.11,1.57,.15 if lizard else height+.22)
    tube(tail,(0,1.16,height*.83),.12,.063,hide)
    tube((0,1.16,height*.83),tip,.066,.006,hide)
    for spine in range(7):
        y=-.42+spine*.17
        tube((0,y,height+.21),(0,y+.11,height+(.38 if lizard else .32)),.09,.005,plates)


def shell_creature(index, hide, plates, teeth, eye):
    scorpion = index == 16
    ellipsoid((0,.12,.42),(.49,.61,.27),plates,.09)
    for row in range(5):
        ellipsoid((0,-.34+row*.19,.59),(.5-abs(row-2)*.045,.15,.13),hide,.06)
    for side in (-1,1):
        for leg in range(4 if scorpion else 3):
            y=-.34+leg*.24
            bend=(side*.61,y+.06,.31)
            tube((side*.34,y,.43),bend,.079,.044,hide)
            tube(bend,(side*(.78+leg*.025),y-.07,.06),.045,.014,plates)
        ellipsoid((side*.14,-.43,.54),(.059,.074,.056),eye)
        palm=(side*.60,-.74,.36)
        tube((side*.27,-.36,.38),palm,.10,.081,hide)
        ellipsoid(palm,(.15,.22,.13),plates,.06)
        for direction in (-1,1):
            claw=(palm[0]+direction*.115,-1.03,.35)
            tube(palm,claw,.080,.034,plates)
            tube(claw,(palm[0]+direction*.025,-1.18,.35),.039,.006,teeth)
    if scorpion:
        curve=[(0,.60,.48),(0,.88,.65),(0,1.05,.95),(0,.99,1.26),(0,.75,1.48),(0,.44,1.46)]
        for i,(a,b) in enumerate(zip(curve,curve[1:])):
            tube(a,b,.13-i*.014,.118-i*.014,plates)
            ellipsoid(b,(.105-i*.009,.105-i*.009,.105-i*.009),hide)
        tube(curve[-1],(.02,.29,1.20),.067,.003,teeth)


def revenant(index, hide, plates, cloth, teeth, eye):
    if index == 14:
        ellipsoid((0,0,1.62),(.23,.23,.27),cloth,.07)
        ellipsoid((0,-.20,1.60),(.144,.10,.16),plates)
        for side in (-1,1):
            ellipsoid((side*.058,-.290,1.64),(.029,.019,.024),eye)
            tube((side*.18,.03,1.35),(side*.54,-.14,.99),.15,.034,cloth)
            panel([(side*.17,.1,1.44),(side*.55,-.14,1.02),(side*.53,.13,.68),
                   (side*.12,.21,.84)],cloth,.04)
        for strip in range(8):
            a=strip*math.tau/8
            x,y=math.cos(a),math.sin(a)
            panel([(x*.20,y*.20,1.48),(x*.38,y*.38,.88),
                   (x*.23-y*.14,y*.23+x*.14,.31+strip%3*.17),
                   (x*.19-y*.16,y*.19+x*.16,1.31)],cloth,.028)
    else:
        humanoid(6 if index==19 else 1,hide,plates,cloth,teeth,eye)
        if index==18:
            ellipsoid((0,-.06,1.99),(.43,.36,.125),plates,.12)
            for x,z in ((-.33,1.51),(.35,1.49),(-.20,1.28)):
                ellipsoid((x,.03,z),(.22,.24,.092),hide,.17)
            for x in (-.22,0,.22):
                ellipsoid((x,-.28,1.94),(.047,.025,.035),teeth)
        else:
            for i in range(7):
                x=(i-3)*.09
                tube((x,0,1.93),(x*1.45,-.06,2.31-abs(i-3)*.06),.095,.012,plates,5)
            for side in (-1,1):
                tube((side*.30,-.10,1.40),(side*.59,-.23,1.74),.17,.012,plates,5)
                ellipsoid((side*.21,-.24,1.24),(.065,.04,.068),eye)


for index,rgb in enumerate(skins):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    suffix=str(index)
    hide=material("Hide"+suffix,rgb,.74)
    plates=material("Shell"+suffix,tuple(v*.52 for v in rgb),.47,.42 if index in (0,6,9,11) else .06)
    cloth=material("Wrap"+suffix,(.26,.155,.085))
    teeth=material("Ivory"+suffix,(.63,.54,.36),.48)
    eye=material("Eye"+suffix,(.88,.36,.08) if index!=7 else (.47,.80,.92),.28)
    if index in (12,13,15):
        quadruped(index,hide,plates,teeth,eye)
    elif index in (16,17):
        shell_creature(index,hide,plates,teeth,eye)
    elif index in (14,18,19):
        if index==14:
            cloth=material("WraithCloth",(.055,.077,.106),.91)
        revenant(index,hide,plates,cloth,teeth,eye)
    elif index in (2,4):
        arachnid(index,hide,plates,teeth,eye)
    elif index in (5,8):
        flyer(index,hide,plates,teeth,eye)
    else:
        humanoid(index,hide,plates,cloth,teeth,eye)
    export(f"Enemy{index:02}")

(output / "enemy_surfaces.json").write_text(json.dumps(enemy_surfaces), encoding="utf-8")

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

# Equipment is exported around its in-game attachment origin.
bpy.ops.wm.read_factory_settings(use_empty=True)
wood=material("BowWalnut",(.24,.096,.035),.74)
leather=material("BowGrip",(.072,.045,.027),.86)
cord=material("BowString",(.61,.52,.36),.83)
curve=[(0,.04,-.67),(0,-.11,-.51),(0,-.21,-.28),(0,-.235,0),
       (0,-.21,.28),(0,-.11,.51),(0,.04,.67)]
for index,(start,stop) in enumerate(zip(curve,curve[1:])):
    tube(start,stop,(.022,.028,.039,.040,.039,.028)[index],
         (.028,.039,.040,.039,.028,.022)[index],wood)
tube(curve[0],curve[-1],.006,.006,cord)
tube((0,-.232,-.085),(0,-.232,.085),.046,.046,leather)
for obj in bpy.context.scene.objects:
    obj.location.y+=.232
export("HeroBow")

bpy.ops.wm.read_factory_settings(use_empty=True)
steel=material("ValkyrieSteel",(.18,.31,.44),.34,.78)
trim=material("ValkyrieBrass",(.50,.36,.093),.36,.69)
breast=[(-.229,-.085,.15),(-.17,-.134,.176),(0,-.169,.162),(.17,-.134,.176),
        (.229,-.085,.15),(.168,-.162,-.105),(0,-.199,-.160),(-.168,-.162,-.105)]
panel(breast,steel,.037)
for start,stop in zip(breast,breast[1:]+breast[:1]):
    tube(start,stop,.013,.013,trim)
for side in (-1,1):
    ellipsoid((side*.23,.008,.139),(.108,.135,.051),steel)
    tube((side*.20,-.09,.15),(side*.20,.091,.15),.012,.012,trim)
box((0,-.192,-.020),(.047,.024,.082),trim)
export("HeroArmor")

bpy.ops.wm.read_factory_settings(use_empty=True)
leather=material("QuiverHide",(.20,.082,.029),.72)
metal=material("QuiverRim",(.51,.35,.095),.36,.66)
shaft=material("ArrowWood",(.42,.255,.12))
fletch=material("Fletching",(.13,.28,.38),.63)
tube((0,0,-.29),(0,0,.26),.083,.105,leather)
tube((0,0,.238),(0,0,.271),.108,.108,metal)
for index in range(5):
    angle=index*math.tau/5
    x,y=.055*math.cos(angle),.055*math.sin(angle)
    high=.43+index*.028
    tube((x,y,-.15),(x,y,high),.009,.009,shaft)
    panel([(x,y,high),(x+.037,y,high-.022),(x+.031,y,high-.105),(x,y,high-.105)],fletch)
export("HeroQuiver")
