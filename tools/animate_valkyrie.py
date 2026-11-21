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
CLIPS = ("Idle", "Walk", "Run", "WalkDiagonal", "RunDiagonal", "Jump", "Attack", "Spin")


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
    rotate(rig.pose.bones["spine"], X, math.radians(-11 if running else -9))
    rotate(rig.pose.bones["chest"], Z, math.sin(wave) * (.09 if diagonal else .05))
    rotate(rig.pose.bones["head"], Z, -math.sin(wave) * .025)

    for side, cycle in (("L", phase), ("R", phase + .5)):
        t = cycle % 1
        stance = .39 if running else .60
        stride = .39 if running else .26
        if t < stance:
            forward = stride * (2 * t / stance - 1)
            lift = 0
        else:
            swing = (t - stance) / (1 - stance)
            # Match the stance velocity at both contacts and reverse smoothly.
            travel = 2 * swing**3 - 3 * swing**2 + 1
            tangent = ((2 * swing - 3) * swing + 1) * swing + (swing - 1) * swing**2
            forward = stride * (2 * travel - 1 + 2 * (1 - stance) / stance * tangent)
            lift = (.19 if running else .105) * math.sin(math.pi * swing)**2

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
        swing_angle = arm_phase * (.38 if running else .23)
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


def side_punch(rig, phase):
    """Alternating outward punches with a guarded recovery and mild torso twist."""
    # Left hand extends first, right hand follows; no root translation.
    for side, start in (("L", .10), ("R", .48)):
        extend = smooth(start, start + .15, phase) * (1 - smooth(start + .27, start + .43, phase))
        upper = rig.pose.bones[f"upperarm_{side}"]
        fore = rig.pose.bones[f"forearm_{side}"]
        # Arm spreads laterally, elbow straightens on impact, then returns.
        rotate(upper, Z, (1 if side == "L" else -1) * (.20 + 1.10 * extend))
        rotate(fore, X, -.78 * (1 - extend) - .10 * extend)
    twist = .16 * (smooth(.10, .25, phase) * (1 - smooth(.37, .53, phase))
                   - smooth(.48, .63, phase) * (1 - smooth(.75, .91, phase)))
    rotate(rig.pose.bones["chest"], Z, twist)
    rotate(rig.pose.bones["spine"], X, -.07 * math.sin(math.pi * phase))


def hair_pose(rig, name, phase):
    strength = {
        "Idle": .017, "Walk": .076, "Run": .129, "WalkDiagonal": .084,
        "RunDiagonal": .143, "Jump": .15, "Attack": .16, "Spin": .22,
    }[name]
    loop = name == "Idle" or name.startswith(("Walk", "Run"))
    envelope = 1.0 if loop else math.sin(math.pi * phase)**2
    wave = math.tau * phase
    for index, bone_name in enumerate(("hair_upper", "hair_lower")):
        bone = rig.pose.bones[bone_name]
        local = bone.bone.matrix_local.to_quaternion().inverted()
        delay = .48 * (index + 1)
        pitch = strength * envelope * math.sin(2 * wave - delay) * (.54 + .46 * index)
        sway = strength * envelope * math.sin(wave - delay) * (.34 + .40 * index)
        bone.rotation_quaternion = Quaternion(local @ X, pitch) @ Quaternion(local @ Vector((0, 1, 0)), sway)


def make_clips(scene, rig, source):
    rig.animation_data_clear()
    action_data = rig.animation_data_create()
    gait_lengths = {"Walk": 52, "WalkDiagonal": 54, "Run": 40, "RunDiagonal": 42}
    tracks = []
    for name in CLIPS:
        original = source.get(name)
        walking = name.startswith(("Walk", "Run"))
        length = gait_lengths[name] if walking else round(2 * (original.frame_range[1] - original.frame_range[0]))
        poses = []
        if not walking:
            # Preserve the established action choreography, now evaluated at 60 Hz.
            action_data.action = original
            start = float(original.frame_range[0])
            for sample in range(length + 1):
                time = start + sample / 2
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
            if name == "Attack":
                side_punch(rig, phase)
            if name == "Jump":
                # The movement component supplies the jump arc.
                rig.pose.bones["root"].location = (0, 0, 0)
            hair_pose(rig, name, phase)
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
    missing_clips = sorted(set(CLIPS) - set(originals))
    if missing_clips:
        raise RuntimeError("Input FBX missing required source clips: " + ", ".join(missing_clips))
    separate_hand_hip_geometry(body)
    add_hair_rig(body, rig)
    scene.render.fps = FPS
    make_clips(scene, rig, originals)
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
