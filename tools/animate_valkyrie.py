"""Rebake the approved Valkyrie FBX with 60 Hz clips and skinned hair.

Run Blender 4.3+ in background mode: --python this_file.py -- input.fbx output.fbx
The input is the textured Valkyrie from MindControl commit 850b4bf.
"""
import math
import sys

import bpy
from mathutils import Quaternion, Vector

FPS = 60
X = Vector((1, 0, 0))
Z = Vector((0, 0, 1))
CLIPS = ("Idle", "Walk", "Run", "WalkDiagonal", "RunDiagonal", "Jump", "Attack", "Spin", "Swim", "WalkAttack", "RunAttack")


def smooth(a, b, value):
    t = min(1.0, max(0.0, (value - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def separate_hand_hip_geometry(body):
    """Separate the fused contact surfaces on the approved Valkyrie mesh."""
    import bmesh

    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    deform = mesh.verts.layers.deform.active
    for side, sign in (("L", -1), ("R", 1)):
        arm_groups = tuple(body.vertex_groups[f"{part}_{side}"].index for part in ("forearm", "hand"))
        hip_groups = tuple(body.vertex_groups[name].index for name in ("pelvis", "spine", f"thigh_{side}"))
        contact = {v for v in mesh.verts if sign * v.co.x > .16 and .84 < v.co.z < 1.23}
        arm = {v for v in contact if sum(v[deform].get(i, 0) for i in arm_groups) > .5}
        hips = {v for v in contact if sum(v[deform].get(i, 0) for i in hip_groups) > .5}
        bridges = [f for f in mesh.faces if any(v in arm for v in f.verts) and any(v in hips for v in f.verts)]
        if not bridges:
            raise RuntimeError(f"Missing expected hand-to-hip contact on {side}")
        cut_edges = {edge for face in bridges for edge in face.edges}
        bmesh.ops.delete(mesh, geom=bridges, context="FACES_ONLY")
        boundary = [edge for edge in cut_edges if edge.is_boundary]
        bmesh.ops.delete(mesh, geom=[edge for edge in cut_edges if edge.is_wire], context="EDGES")
        # Fill each cut using only vertices from its own side of the contact.
        for region in (arm, hips):
            rim = [edge for edge in boundary if edge.is_valid and all(v in region for v in edge.verts)]
            caps = bmesh.ops.holes_fill(mesh, edges=rim, sides=0)["faces"]
            for face in caps:
                bmesh.ops.triangulate(mesh, faces=[face])
        print(f"Separated {len(bridges)} hand-to-hip faces on {side}")
    mesh.to_mesh(body.data)
    mesh.free()
    body.data.update()


def clean_arm_fragments(body):
    """Remove tiny disconnected islands in the hand and underarm regions only."""
    import bmesh
    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    pending = set(mesh.verts)
    remove = []
    islands = 0
    while pending:
        seed = pending.pop()
        component = [seed]
        stack = [seed]
        while stack:
            v = stack.pop()
            for edge in v.link_edges:
                other = edge.other_vert(v)
                if other in pending:
                    pending.remove(other)
                    component.append(other)
                    stack.append(other)
        if len(component) > 300:
            continue
        xs = [v.co.x for v in component]
        ys = [v.co.y for v in component]
        zs = [v.co.z for v in component]
        span = max(max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs))
        center = sum((v.co for v in component), Vector((0,0,0))) / len(component)
        # Exclude large armor, fingers, hair and the main connected skin.
        if span < .035 and abs(center.x) > .16 and .84 < center.z < 1.55:
            remove.extend(component)
            islands += 1
    if remove:
        bmesh.ops.delete(mesh, geom=remove, context="VERTS")
    mesh.to_mesh(body.data)
    mesh.free()
    body.data.update()
    print("ARM_FRAGMENT_CLEANUP", islands, "islands", len(remove), "vertices", flush=True)


def repair_cross_weighted_arm_regions(body, rig):
    """Correct arm/torso cross-influences away from the shoulder joint."""
    groups = {g.index: g.name for g in body.vertex_groups}
    changed = 0
    for side, sign in (("L", -1), ("R", 1)):
        names = {f"{part}_{side}" for part in ("upperarm", "forearm", "hand")}
        arm = {i for i, name in groups.items() if name in names or
               (name.endswith("_" + side) and any(k in name.lower() for k in ("finger", "thumb", "index", "middle", "ring", "pinky")))}
        torso = {i for i, name in groups.items() if name in ("pelvis", "spine", "chest", "thigh_" + side)}
        shoulder = rig.data.bones["upperarm_" + side].head_local
        elbow = rig.data.bones["forearm_" + side].head_local
        wrist = rig.data.bones["hand_" + side].head_local
        def distance(p, a, b):
            d = b-a
            t = max(0., min(1., (p-a).dot(d)/max(d.length_squared, 1e-9)))
            return (p-a-t*d).length
        for v in body.data.vertices:
            x,y,z = v.co
            if sign*x < .12 or not (.84 < z < shoulder.z-.09):
                continue
            weights = {g.group:g.weight for g in v.groups}
            aw = sum(weights.get(i,0) for i in arm)
            tw = sum(weights.get(i,0) for i in torso)
            if min(aw,tw) < .015:
                continue
            # Preserve ambiguous contact vertices and shoulder articulation.
            d = min(distance(v.co, shoulder, elbow), distance(v.co, elbow, wrist),
                    distance(v.co, wrist, rig.data.bones["hand_" + side].tail_local))
            if aw > .72 and d < .075:
                discard = torso
            elif tw > .72 and d > .095:
                discard = arm
            else:
                continue
            kept = {i:w for i,w in weights.items() if i not in discard}
            total = sum(kept.values())
            if total <= 0:
                continue
            for i in discard.intersection(weights):
                body.vertex_groups[i].remove([v.index])
            for i,w in kept.items():
                body.vertex_groups[i].add([v.index], w/total, "REPLACE")
            changed += 1
    print("ARM_CROSS_WEIGHT_REPAIR", changed, "vertices", flush=True)


def separate_arm_torso_bridges(body, rig):
    """Cut fused arm/torso contact faces below the anatomical shoulder."""
    import bmesh
    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    deform = mesh.verts.layers.deform.active
    names = {g.index:g.name for g in body.vertex_groups}
    for side, sign in (("L",-1),("R",1)):
        arm = {i for i,n in names.items() if n in {p+"_"+side for p in ("upperarm","forearm","hand")}}
        torso = {i for i,n in names.items() if n in ("chest","spine","pelvis","thigh_"+side)}
        shoulder = rig.data.bones["upperarm_"+side].head_local.z
        def region(v):
            w=v[deform]
            a=sum(w.get(i,0) for i in arm)
            t=sum(w.get(i,0) for i in torso)
            return 1 if a>t and a>.45 else 2 if t>a and t>.45 else 0
        faces=[]
        for f in mesh.faces:
            center=f.calc_center_median()
            if sign*center.x < .12 or not (.84<center.z<shoulder-.10):
                continue
            if {1,2}.issubset({region(v) for v in f.verts}):
                faces.append(f)
        edges={e for f in faces for e in f.edges}
        bmesh.ops.delete(mesh,geom=faces,context="FACES_ONLY")
        boundary=[e for e in edges if e.is_valid and e.is_boundary]
        wire=[e for e in edges if e.is_valid and e.is_wire]
        if wire:
            bmesh.ops.delete(mesh,geom=wire,context="EDGES")
        for label in (1,2):
            rim=[e for e in boundary if e.is_valid and all(region(v)==label for v in e.verts)]
            if rim:
                caps=bmesh.ops.holes_fill(mesh,edges=rim,sides=0)["faces"]
                if caps:
                    bmesh.ops.triangulate(mesh,faces=caps)
        print("ARM_TORSO_BRIDGE_CUT",side,len(faces),flush=True)
    mesh.to_mesh(body.data)
    mesh.free()
    body.data.update()


def repair_stretched_boundary_weights(body, rig):
    """Repair isolated conflicting weights using the connected local surface."""
    from collections import defaultdict
    names = {g.index:g.name for g in body.vertex_groups}
    neighbors = defaultdict(set)
    for e in body.data.edges:
        a,b=e.vertices
        neighbors[a].add(b)
        neighbors[b].add(a)
    weights = [{g.group:g.weight for g in v.groups} for v in body.data.vertices]
    changes = {}
    for side in ("L","R"):
        sign = math.copysign(1,rig.data.bones["forearm_"+side].head_local.x)
        arm = {i for i,n in names.items() if n in {p+"_"+side for p in ("upperarm","forearm","hand")}}
        torso = {i for i,n in names.items() if n in ("spine","chest","pelvis","thigh_"+side)}
        def label(w):
            a=sum(w.get(i,0) for i in arm)
            t=sum(w.get(i,0) for i in torso)
            return 1 if a>.7 else 2 if t>.7 else 0
        for v in body.data.vertices:
            if sign*v.co.x<.12 or not .84<v.co.z<rig.data.bones["upperarm_"+side].head_local.z-.12:
                continue
            own=label(weights[v.index])
            if not own: continue
            local=[j for j in neighbors[v.index] if (body.data.vertices[j].co-v.co).length<.04]
            if len(local)<3: continue
            opposite=[j for j in local if label(weights[j])==3-own]
            if len(opposite)/len(local)<.8: continue
            merged=defaultdict(float)
            for j in opposite:
                for i,w in weights[j].items(): merged[i]+=w/len(opposite)
            changes[v.index]=merged
    for j,w in changes.items():
        for i in weights[j]: body.vertex_groups[i].remove([j])
        total=sum(w.values())
        for i,value in w.items(): body.vertex_groups[i].add([j],value/total,"REPLACE")
    print("LOCAL_BOUNDARY_WEIGHT_REPAIR",len(changes),flush=True)


def add_hair_rig(body, rig):
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    parent = rig.data.edit_bones["head"]
    for name, head, tail in (
        ("hair_upper", (0, .089, 1.79), (0, .130, 1.54)),
        ("hair_lower", (0, .130, 1.54), (0, .116, 1.29)),
    ):
        bone = rig.data.edit_bones.new(name)
        bone.head, bone.tail, bone.parent = head, tail, parent
        parent = bone
    bpy.ops.object.mode_set(mode="OBJECT")

    upper = body.vertex_groups.new(name="hair_upper")
    lower = body.vertex_groups.new(name="hair_lower")
    changed = 0
    for vertex in body.data.vertices:
        x, y, z = vertex.co
        # Restrict moving weights to trailing hair, off the neck and back armor.
        back_edge = .107 - .032 * smooth(1.39, 1.59, z)
        amount = (
            smooth(back_edge, back_edge + .038, y)
            * smooth(1.245, 1.36, z)
            * (1 - smooth(1.70, 1.81, z))
            * (1 - smooth(.10, .155, abs(x)))
        )
        if amount < .004:
            continue
        for assignment in list(vertex.groups):
            body.vertex_groups[assignment.group].add(
                [vertex.index], assignment.weight * (1 - amount), "REPLACE"
            )
        tip = 1 - smooth(1.42, 1.58, z)
        upper.add([vertex.index], amount * (1 - tip), "REPLACE")
        lower.add([vertex.index], amount * tip, "REPLACE")
        changed += 1
    assert changed > 100, "No trailing hair region found"
    print("Hair-weighted vertices:", changed)


def rotate(bone, axis, radians):
    local = bone.bone.matrix_local.to_quaternion().inverted() @ axis
    bone.rotation_quaternion = Quaternion(local, radians)


def gait(rig, phase, running, diagonal):
    wave = math.tau * phase
    bob = (.013 if running else .009) * (-.35 - math.cos(2 * wave))
    pelvis = rig.pose.bones["pelvis"]
    pelvis.location = pelvis.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, bob))
    # The Valkyrie faces -Y in armature space. Positive X pitch leans forward.
    rotate(rig.pose.bones["spine"], X, math.radians(14 if running else 9))
    rotate(pelvis, Z, -.045 * math.sin(wave))
    rotate(rig.pose.bones["chest"], Z, math.sin(wave) * (.11 if diagonal else .08))
    rotate(rig.pose.bones["head"], Z, -math.sin(wave) * .025)

    for side, cycle in (("L", phase), ("R", phase + .5)):
        t = cycle % 1
        stance = .39 if running else .60
        stride = .34 if running else .23
        if t < stance:
            forward = stride * (2 * t / stance - 1)
            lift = 0
        else:
            swing = (t - stance) / (1 - stance)
            # Match the stance velocity at both contacts and reverse smoothly.
            travel = 2 * swing**3 - 3 * swing**2 + 1
            tangent = ((2 * swing - 3) * swing + 1) * swing + (swing - 1) * swing**2
            forward = stride * (2 * travel - 1 + 2 * (1 - stance) / stance * tangent)
            lift = (.16 if running else .085) * math.sin(math.pi * swing)**2

        thigh, shin, foot = (rig.pose.bones[f"{part}_{side}"] for part in ("thigh", "shin", "foot"))
        hip = thigh.bone.head_local
        knee = shin.bone.head_local
        ankle = foot.bone.head_local
        a, b = knee - hip, ankle - knee
        first, second = math.hypot(a.y, a.z), math.hypot(b.y, b.z)
        dy, dz = ankle.y - hip.y + forward, ankle.z - hip.z + lift - bob
        distance = min(math.hypot(dy, dz), first + second - .001)
        heading = math.atan2(dy, -dz)
        front = math.acos(max(-1, min(1, (first**2 + distance**2 - second**2) / (2 * first * distance))))
        rear = math.acos(max(-1, min(1, (second**2 + distance**2 - first**2) / (2 * second * distance))))
        thigh_pitch = heading - front - math.atan2(a.y, -a.z)
        shin_pitch = heading + rear - math.atan2(b.y, -b.z)
        rotate(thigh, X, thigh_pitch)
        rotate(shin, X, shin_pitch - thigh_pitch)
        rotate(foot, X, -shin_pitch + 1.1 * lift)

        arm_phase = math.cos(math.tau * cycle)
        swing_angle = arm_phase * (.52 if running else .34)
        upper = rig.pose.bones[f"upperarm_{side}"]
        fore = rig.pose.bones[f"forearm_{side}"]
        # Keep the hands clear of the hips while the elbows stay softly bent.
        outward = -math.copysign(.13 if running else .16, upper.bone.head_local.x)
        rest_to_local = upper.bone.matrix_local.to_quaternion().inverted()
        upper.rotation_quaternion = (
            Quaternion(rest_to_local @ Vector((0, 1, 0)), outward)
            @ Quaternion(rest_to_local @ X, swing_angle)
        )
        rotate(fore, X, -(.38 if running else .28) - (.16 if running else .09) * arm_phase)


def jump_pose(rig, phase):
    # In-place anticipation, airborne tuck and landing; gameplay owns altitude.
    crouch = .35 * (1 - smooth(0, .14, phase))
    tuck = smooth(.02, .34, phase) * (1 - smooth(.55, .88, phase))
    landing = .35 * smooth(.86, 1, phase)
    bend = .55 * crouch + .36 * tuck + .62 * landing
    rotate(rig.pose.bones["spine"], X, .20 * (crouch + landing) + .08 * tuck)
    rotate(rig.pose.bones["chest"], X, .08 * (crouch + landing))
    pelvis = rig.pose.bones["pelvis"]
    pelvis.location = pelvis.bone.matrix_local.to_3x3().inverted() @ Vector((0, 0, -.085 * (crouch + landing)))
    for side in ("L", "R"):
        rotate(rig.pose.bones[f"thigh_{side}"], X, -bend)
        rotate(rig.pose.bones[f"shin_{side}"], X, 1.65 * bend)
        rotate(rig.pose.bones[f"foot_{side}"], X, -.65 * bend)
        rotate(rig.pose.bones[f"upperarm_{side}"], X, .32 * crouch - .85 * tuck - .28 * landing)
        rotate(rig.pose.bones[f"forearm_{side}"], X, -.28 * (crouch + tuck + landing))


def side_punch(rig, phase):
    """Alternate forward head-height strikes with a guarded recovery."""
    pulses = {}
    for side, start in (("L", .10), ("R", .48)):
        pulses[side] = smooth(start, start + .15, phase) * (1 - smooth(start + .27, start + .43, phase))
    twist = .19 * (pulses["L"] - pulses["R"])
    rotate(rig.pose.bones["chest"], Z, twist)
    rotate(rig.pose.bones["spine"], X, .09 * math.sin(math.pi * phase))
    bpy.context.view_layer.update()
    for side in ("L", "R"):
        extend = pulses[side]
        upper = rig.pose.bones[f"upperarm_{side}"]
        fore = rig.pose.bones[f"forearm_{side}"]
        # Aim in armature space, toward -Y and slightly upward from shoulder.
        direction = Vector((0, -1, .48)).normalized()
        rest = (fore.bone.head_local - upper.bone.head_local).normalized()
        parent_delta = upper.parent.matrix.to_quaternion() @ upper.parent.bone.matrix_local.to_quaternion().inverted()
        target = parent_delta.inverted() @ direction
        local = upper.bone.matrix_local.to_quaternion()
        strike = local.inverted() @ rest.rotation_difference(target) @ local
        guard = Quaternion(local.inverted() @ X, -.48 * (smooth(0, .10, phase) * (1 - smooth(.90, 1, phase))))
        upper.rotation_quaternion = guard.slerp(strike, extend)
        rotate(fore, X, -.95 * (1 - extend) * smooth(0, .10, phase) * (1 - smooth(.90, 1, phase)))

def swim_pose(rig, phase):
    wave = math.tau * phase
    rotate(rig.pose.bones["pelvis"], X, math.radians(78))
    rotate(rig.pose.bones["spine"], X, .08 * math.sin(2 * wave))
    rotate(rig.pose.bones["chest"], Vector((0, 1, 0)), .23 * math.sin(wave))
    rotate(rig.pose.bones["head"], X, -.20)
    for side, offset in (("L", 0), ("R", math.pi)):
        t = wave + offset
        upper = rig.pose.bones[f"upperarm_{side}"]
        local = upper.bone.matrix_local.to_quaternion().inverted()
        upper.rotation_quaternion = Quaternion(local @ Vector((0, 1, 0)), math.copysign(.16 + .13 * math.sin(t), -upper.bone.head_local.x)) @ Quaternion(local @ X, -1.35 - 1.30 * math.cos(t))
        rotate(rig.pose.bones[f"forearm_{side}"], X, -.45 - .40 * math.sin(t))
        rotate(rig.pose.bones[f"thigh_{side}"], X, .22 * math.sin(2 * wave + offset))
        rotate(rig.pose.bones[f"shin_{side}"], X, .22 + .18 * math.cos(2 * wave + offset))
        rotate(rig.pose.bones[f"foot_{side}"], X, -.25)


def hair_pose(rig, name, phase):
    strength = {
        "Idle": .065, "Walk": .095, "Run": .165, "WalkDiagonal": .084,
        "RunDiagonal": .143, "Swim": .10, "Jump": .15, "Attack": .16, "Spin": .22,
    }[name]
    loop = name in ("Idle", "Swim") or name.startswith(("Walk", "Run"))
    envelope = 1.0 if loop else math.sin(math.pi * phase)**2
    wave = math.tau * phase
    for index, bone_name in enumerate(("hair_upper", "hair_lower")):
        bone = rig.pose.bones[bone_name]
        local = bone.bone.matrix_local.to_quaternion().inverted()
        delay = .48 * (index + 1)
        pitch = strength * envelope * math.sin(2 * wave - delay) * (.54 + .46 * index)
        sway = strength * envelope * math.sin(wave - delay) * (.34 + .40 * index)
        if name == "Idle":
            # Gentle continuous breeze, with the tips following the roots.
            pitch = strength * (.25 + .35 * math.sin(wave - delay) + .12 * math.sin(2 * wave - delay)) * (.45 + .55 * index)
            sway = strength * (.40 + .55 * math.sin(wave - delay)) * (.45 + .55 * index)
        bone.rotation_quaternion = Quaternion(local @ X, pitch) @ Quaternion(local @ Vector((0, 1, 0)), sway)


def make_clips(scene, rig, source):
    rig.animation_data_clear()
    action_data = rig.animation_data_create()
    gait_lengths = {"Walk": 52, "WalkDiagonal": 54, "Run": 40, "RunDiagonal": 42, "WalkAttack": 52, "RunAttack": 40}
    tracks = []
    for name in CLIPS:
        original = source.get(name)
        walking = name.startswith(("Walk", "Run"))
        length = 90 if name == "Swim" else gait_lengths[name] if walking else round(2 * (original.frame_range[1] - original.frame_range[0]))
        idle_source_length = length
        if name == "Idle":
            length = max(length, 480)
        poses = []
        if not walking and name not in ("Swim", "Jump", "Attack"):
            # Preserve the established action choreography, now evaluated at 60 Hz.
            action_data.action = original
            start = float(original.frame_range[0])
            for sample in range(length + 1):
                time = start + (sample % max(1, idle_source_length) if name == "Idle" else sample) / 2
                scene.frame_set(int(time), subframe=time % 1)
                poses.append({b.name: b.matrix_basis.copy() for b in rig.pose.bones})
        action_data.action = bpy.data.actions.new("Ocean_" + name)
        action = action_data.action
        for sample in range(length + 1):
            phase = sample / length
            for bone in rig.pose.bones:
                bone.rotation_mode = "QUATERNION"
                bone.rotation_quaternion = (1, 0, 0, 0)
                bone.location = (0, 0, 0)
                bone.scale = (1, 1, 1)
                if poses:
                    bone.matrix_basis = poses[sample][bone.name]
            if walking:
                gait(rig, phase, name.startswith("Run"), name.endswith("Diagonal"))
            if name == "Swim":
                swim_pose(rig, phase)
            if name.endswith("Attack"):
                side_punch(rig, phase)
            if name == "Jump":
                # The movement component supplies the jump arc.
                jump_pose(rig, phase)
                rig.pose.bones["root"].location = (0, 0, 0)
            if walking or name == "Jump":
                chest = rig.pose.bones["chest"]
                amplitude = .022 if name.startswith("Run") else .012
                bounce = amplitude * math.sin(2 * math.tau * phase - .45) if walking else .025 * math.sin(math.pi * phase)**2 * math.sin(2 * math.tau * phase)
                local = chest.bone.matrix_local.to_quaternion().inverted()
                chest.rotation_quaternion = chest.rotation_quaternion @ Quaternion(local @ X, bounce)
            if name == "Idle":
                seconds = sample / FPS
                gesture = smooth(3.0, 3.65, seconds) * (1 - smooth(4.35, 5.15, seconds))
                tug = smooth(3.65, 3.95, seconds) * (1 - smooth(4.05, 4.35, seconds))
                upper = rig.pose.bones["upperarm_R"]
                fore = rig.pose.bones["forearm_R"]
                for bone, pitch in ((upper, -.52 - .12 * tug), (fore, -1.65 - .15 * tug)):
                    local = bone.bone.matrix_local.to_quaternion().inverted()
                    target = Quaternion(local @ X, pitch)
                    if bone == upper:
                        target = Quaternion(local @ Vector((0, 1, 0)), math.copysign(.24, upper.bone.head_local.x)) @ target
                    bone.rotation_quaternion = bone.rotation_quaternion.slerp(target, gesture)
            hair_pose(rig, name.replace("Attack", "") if walking else name, phase)
            for bone in rig.pose.bones:
                bone.keyframe_insert("rotation_quaternion", frame=sample + 1, group=bone.name)
                if bone.name in ("root", "pelvis"):
                    bone.keyframe_insert("location", frame=sample + 1, group=bone.name)
        # Blender 5 removed the legacy Action.fcurves accessor.
        if not hasattr(action, "fcurves"):
            curves = action.layers[0].strips[0].channelbag(action_data.action_slot).fcurves
        else:
            curves = action.fcurves
        for curve in curves:
            for key in curve.keyframe_points:
                key.interpolation = "LINEAR"
        tracks.append((name, action))
        action_data.action = None
        print("Baked", name, length + 1, "samples")
    for name, action in tracks:
        track = action_data.nla_tracks.new()
        track.name = "Ocean_" + name
        track.strips.new(track.name, 1, action)


def audit_arm_deformation(scene, body, rig, target_path):
    """Measure actual skinned edge stretching over walking and running poses."""
    import json
    from pathlib import Path
    data = rig.animation_data
    tracks = list(data.nla_tracks)
    old_mutes = [t.mute for t in tracks]
    rest = [v.co.copy() for v in body.data.vertices]
    edges = []
    for edge in body.data.edges:
        a,b = edge.vertices
        mid = (rest[a]+rest[b])*.5
        length = (rest[a]-rest[b]).length
        if .84 < mid.z < 1.55 and abs(mid.x) > .12 and length > .0005:
            edges.append((a,b,length))
    report = []
    try:
        for track in tracks:
            track.mute = True
        for track in tracks:
            if track.name not in ("Ocean_Walk","Ocean_Run","Ocean_WalkAttack","Ocean_RunAttack"):
                continue
            data.action = track.strips[0].action
            start,end = data.action.frame_range
            for i in range(9):
                scene.frame_set(round(start+(end-start)*i/8))
                bpy.context.view_layer.update()
                obj = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
                mesh = obj.to_mesh()
                try:
                    for a,b,length in edges:
                        posed = (mesh.vertices[a].co-mesh.vertices[b].co).length
                        if posed > .025 and posed/length > 3:
                            report.append({"clip":track.name,"frame":scene.frame_current,
                                "edge":[a,b],"rest_length":length,"posed_length":posed,
                                "ratio":posed/length,"rest_midpoint":list((rest[a]+rest[b])*.5),
                                "weights":[[(body.vertex_groups[g.group].name,g.weight)
                                    for g in body.data.vertices[j].groups] for j in (a,b)]})
                finally:
                    obj.to_mesh_clear()
    finally:
        data.action = None
        for track,mute in zip(tracks,old_mutes):
            track.mute = mute
    report.sort(key=lambda item:item["posed_length"],reverse=True)
    Path(target_path).with_name("ArmDeformationAudit.json").write_text(json.dumps(
        {"flagged_samples":len(report),"worst_edges":report[:1000]},indent=2))
    print("ARM_DEFORMATION_AUDIT",len(report),"flagged edge samples",flush=True)


def export_arm_review(scene, body, rig, target_path):
    """Export evaluated mesh snapshots for direct inspection, without changing geometry."""
    from pathlib import Path
    import json
    data = rig.animation_data
    tracks = list(data.nla_tracks)
    mutes = [t.mute for t in tracks]
    folder = Path(target_path).parent / "ArmReview"
    folder.mkdir(exist_ok=True)
    try:
        for t in tracks:
            t.mute = True
        for t in tracks:
            if t.name not in ("Ocean_Idle", "Ocean_Walk", "Ocean_Run", "Ocean_Attack"):
                continue
            data.action = t.strips[0].action
            start, end = data.action.frame_range
            for index, phase in enumerate((0., .25, .5, .75)):
                scene.frame_set(round(start + (end-start)*phase))
                bpy.context.view_layer.update()
                obj = body.evaluated_get(bpy.context.evaluated_depsgraph_get())
                mesh = obj.to_mesh()
                try:
                    with (folder / (t.name + "_" + str(index) + ".obj")).open("w") as out:
                        for v in mesh.vertices:
                            out.write("v %.7f %.7f %.7f\n" % tuple(v.co))
                        for f in mesh.polygons:
                            out.write("f " + " ".join(str(i+1) for i in f.vertices) + "\n")
                finally:
                    obj.to_mesh_clear()
    finally:
        data.action = None
        for t, mute in zip(tracks, mutes):
            t.mute = mute
    print("ARM_REVIEW_EXPORTED", str(folder), flush=True)


def repair_audited_bridge_edges(scene, body, rig, target_path):
    """Remove only faces spanning measured extreme arm/torso deformation edges."""
    import json
    import bmesh
    from pathlib import Path
    report = json.loads(Path(target_path).with_name("ArmDeformationAudit.json").read_text())
    candidates = set()
    for item in report["worst_edges"]:
        if item["posed_length"] < .12 or item["ratio"] < 6:
            continue
        def region(weights):
            a=sum(w for n,w in weights if n.startswith(("upperarm_","forearm_","hand_")))
            t=sum(w for n,w in weights if n in ("spine","chest","pelvis","thigh_L","thigh_R"))
            return 1 if a>.65 else 2 if t>.65 else 0
        if {region(w) for w in item["weights"]} == {1,2}:
            candidates.add(tuple(sorted(item["edge"])))
    mesh=bmesh.new()
    mesh.from_mesh(body.data)
    faces={f for e in mesh.edges if tuple(sorted(v.index for v in e.verts)) in candidates for f in e.link_faces}
    count=len(faces)
    if faces:
        bmesh.ops.delete(mesh,geom=list(faces),context="FACES_ONLY")
        wire=[e for e in mesh.edges if e.is_wire]
        if wire: bmesh.ops.delete(mesh,geom=wire,context="EDGES")
        mesh.to_mesh(body.data)
        body.data.update()
    mesh.free()
    print("AUDITED_BRIDGE_FACE_REMOVAL",len(candidates),count,flush=True)


def close_local_repair_openings(body, rig):
    """Cap small closed boundary loops without reconnecting arm and torso."""
    import bmesh
    mesh = bmesh.new()
    mesh.from_mesh(body.data)
    deform = mesh.verts.layers.deform.active
    names = {g.index:g.name for g in body.vertex_groups}
    pending = {e for e in mesh.edges if e.is_boundary}
    repaired = 0
    while pending:
        seed = pending.pop()
        component = {seed}
        stack = [seed]
        while stack:
            edge = stack.pop()
            for v in edge.verts:
                for other in v.link_edges:
                    if other in pending:
                        pending.remove(other)
                        component.add(other)
                        stack.append(other)
        verts = {v for e in component for v in e.verts}
        if not 3 <= len(verts) <= 100:
            continue
        if any(sum(e in component for e in v.link_edges) != 2 for v in verts):
            continue
        center = sum((v.co for v in verts), Vector((0,0,0))) / len(verts)
        if not (.84 < center.z < 1.55 and abs(center.x) > .12):
            continue
        if max((v.co-center).length for v in verts) > .065:
            continue
        def region(v):
            arm = sum(w for i,w in v[deform].items() if names.get(i,"").startswith(("upperarm_","forearm_","hand_")))
            torso = sum(w for i,w in v[deform].items() if names.get(i,"") in ("spine","chest","pelvis","thigh_L","thigh_R"))
            return 1 if arm > .65 else 2 if torso > .65 else 0
        labels = {region(v) for v in verts}
        if labels != {1} and labels != {2}:
            continue
        adjacent = [f for e in component for f in e.link_faces]
        material = adjacent[0].material_index if adjacent else 0
        faces = bmesh.ops.holes_fill(mesh, edges=list(component), sides=0)["faces"]
        for f in faces:
            f.material_index = material
            f.smooth = True
        if faces:
            bmesh.ops.triangulate(mesh, faces=faces)
            repaired += 1
    bmesh.ops.recalc_face_normals(mesh, faces=list(mesh.faces))
    mesh.to_mesh(body.data)
    mesh.free()
    body.data.update()
    print("LOCAL_REPAIR_OPENINGS_CLOSED", repaired, flush=True)


def main():
    source_path, target_path = sys.argv[sys.argv.index("--") + 1:]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=source_path)
    scene = bpy.context.scene
    body, = [o for o in bpy.data.objects if o.type == "MESH"]
    rig, = [o for o in bpy.data.objects if o.type == "ARMATURE"]
    originals = {(a.name.split("|")[-1][6:] if a.name.split("|")[-1].startswith("Ocean_") else a.name.split("|")[-1]): a for a in bpy.data.actions}
    for action in originals.values():
        action.use_fake_user = True
    required = {"root", "pelvis", "spine", "chest", "head", "thigh_L", "thigh_R", "shin_L", "shin_R", "foot_L", "foot_R", "upperarm_L", "upperarm_R", "forearm_L", "forearm_R"}
    missing = sorted(required - set(rig.data.bones.keys()))
    if missing:
        raise RuntimeError("Incompatible Ocean Serenity skeleton; missing bones: " + ", ".join(missing))
    missing_clips = sorted((set(CLIPS) - {"Swim", "WalkAttack", "RunAttack"}) - set(originals))
    if missing_clips:
        raise RuntimeError("Input FBX missing required source clips: " + ", ".join(missing_clips))
    separate_hand_hip_geometry(body)
    clean_arm_fragments(body)
    repair_cross_weighted_arm_regions(body, rig)
    add_hair_rig(body, rig)
    scene.render.fps = FPS
    make_clips(scene, rig, originals)
    audit_arm_deformation(scene, body, rig, target_path)
    audit_arm_deformation(scene, body, rig, target_path)
    export_arm_review(scene, body, rig, target_path)
    scene.frame_set(1)
    bpy.ops.object.select_all(action="DESELECT")
    body.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(
        filepath=target_path, use_selection=True, add_leaf_bones=False,
        bake_anim=True, bake_anim_use_nla_strips=True, bake_anim_use_all_actions=False,
        bake_anim_step=1, bake_anim_simplify_factor=0,
        path_mode="COPY", embed_textures=True,
    )


if __name__ == "__main__":
    main()
