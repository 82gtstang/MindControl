"""Apply the approved Ocean Serenity run-pose contour to the game mesh."""
from mathutils import Vector


def add_run_contour(mesh):
    world = mesh.matrix_world
    positions = [world @ v.co for v in mesh.data.vertices]
    bottom = min(p.z for p in positions)
    top = max(p.z for p in positions)
    middle = (min(p.x for p in positions) + max(p.x for p in positions)) / 2
    neighbors = [set() for _ in mesh.data.vertices]
    for edge in mesh.data.edges:
        a, b = edge.vertices
        neighbors[a].add(b)
        neighbors[b].add(a)
    def ramp(value, start, end):
        t = min(1, max(0, (value-start)/(end-start)))
        return t*t*(3-2*t)
    span = top-bottom
    strengths = []
    for pos in positions:
        z = (pos.z-bottom)/span
        x = abs(pos.x-middle)/span
        # Fade the edit at the soles, hips, and outer edges near the hands.
        strengths.append(ramp(z,.025,.075)*(1-ramp(z,.44,.51))*
                         (1-ramp(x,.101,.125)))
    rest = [vertex.co.copy() for vertex in mesh.data.vertices]
    for factor in (.58,-.31)*7:
        updated = []
        for index, (point, adjacent, strength) in enumerate(zip(rest, neighbors, strengths)):
            average = sum((rest[j] for j in adjacent), Vector())/len(adjacent) if adjacent else point
            updated.append(point + (average-point)*(factor*strength))
        rest = updated
    world = mesh.matrix_world
    local = world.inverted()
    for vertex, point in zip(mesh.data.vertices, rest):
        pos = world @ point
        z = (pos.z-bottom)/span
        x = (pos.x-middle)/span
        # Front camera looks along +Y, so positive X is her anatomical left.
        if x > 0 and .06 < z < .49:
            amount = ramp(z,.06,.16)*(1-ramp(z,.37,.49))*ramp(x,.012,.025)*(1-ramp(x,.105,.122))
            band = [p for p in positions if abs(p.z-pos.z)<span*.018 and middle<p.x<middle+span*.105]
            if band:
                axis_x = (min(p.x for p in band)+max(p.x for p in band))/2
                axis_y = (min(p.y for p in band)+max(p.y for p in band))/2
                pos.x += (pos.x-axis_x)*.21*amount
                pos.y += (pos.y-axis_y)*.15*amount
                point = local @ pos
        vertex.co = point
    mesh.data.update()
    baseline = [v.co.copy() for v in mesh.data.vertices]
    for vertex, rest_position in zip(mesh.data.vertices, baseline):
        pos = world @ rest_position
        z = (pos.z-bottom)/span
        x = (pos.x-middle)/span
        strength = ramp(z,.258,.352)*(1-ramp(z,.405,.492))*ramp(x,.009,.028)*(1-ramp(x,.110,.135))
        if strength:
            # Use a gently tapering axis along the thigh to avoid a bulge at the strap.
            center_x = middle + span*(.050 + .106*(z-.29))
            offset = pos.x - center_x
            pos.x += offset*.29*strength
            vertex.co = local @ pos
    softened = [v.co.copy() for v in mesh.data.vertices]
    weights = []
    for rest_vertex in baseline:
        pos = world @ rest_vertex
        z = (pos.z-bottom)/span
        x = (pos.x-middle)/span
        weights.append(ramp(z,.19,.257)*(1-ramp(z,.407,.475))*ramp(x,.008,.025)*(1-ramp(x,.11,.135)))
    for factor in (.48,-.36)*6:
        softened = [
            p.lerp(sum((softened[j] for j in adjacent), Vector())/len(adjacent), factor*weight)
            if adjacent and weight else p
            for p, adjacent, weight in zip(softened, neighbors, weights)
        ]
    for vertex, position in zip(mesh.data.vertices, softened):
        pos = world @ position
        z = (pos.z-bottom)/span
        x = (pos.x-middle)/span
        hip = ramp(z,.388,.451)*(1-ramp(z,.482,.534))*ramp(x,.035,.095)*(1-ramp(x,.112,.135))
        pos.x += span*.010*hip
        vertex.co = local @ pos
    mesh.data.update()
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh.data)
    def on_thigh(v):
        p = world @ v.co
        return .016 < (p.x-middle)/span < .122 and .235 < (p.z-bottom)/span < .515
    boundary = {edge for edge in bm.edges if edge.is_boundary and all(on_thigh(v) for v in edge.verts)}
    while boundary:
        seed = boundary.pop()
        ring = [seed]
        vertices = set(seed.verts)
        pending = list(seed.verts)
        while pending:
            v = pending.pop()
            for edge in v.link_edges:
                if edge in boundary:
                    boundary.remove(edge)
                    ring.append(edge)
                    for endpoint in edge.verts:
                        if endpoint not in vertices:
                            vertices.add(endpoint)
                            pending.append(endpoint)
        if 3 <= len(ring) <= 25 and all(sum(e in ring for e in v.link_edges) == 2 for v in vertices):
            filled = bmesh.ops.holes_fill(bm, edges=ring, sides=25)['faces']
            for face in filled:
                face.smooth = True
    patch = {v for v in bm.verts if on_thigh(v)}
    for _ in range(9):
        offsets = {}
        for v in patch:
            p = world @ v.co
            z = (p.z-bottom)/span
            x = (p.x-middle)/span
            amount = .35*ramp(z,.235,.295)*(1-ramp(z,.455,.515))*ramp(x,.016,.032)*(1-ramp(x,.108,.122))
            adjacent = [e.other_vert(v).co for e in v.link_edges]
            if adjacent and amount:
                offsets[v] = v.co.lerp(sum(adjacent, Vector())/len(adjacent),amount)
        for v, co in offsets.items():
            v.co = co
    bm.normal_update()
    bm.to_mesh(mesh.data)
    bm.free()
    mesh.data.update()
    corrected = [v.co.copy() for v in mesh.data.vertices]
    if len(corrected) != len(baseline):
        raise RuntimeError("Run contour repair changed the vertex count")
    for vertex, coordinate in zip(mesh.data.vertices, baseline):
        vertex.co = coordinate
    mesh.data.update()
    mesh.shape_key_add(name="Basis")
    key = mesh.shape_key_add(name="OceanRunLeftLeg")
    for vertex, coordinate in zip(key.data, corrected):
        vertex.co = coordinate
