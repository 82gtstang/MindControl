"""Open3D geometry diagnostics for Ocean Serenity. Never rewrites textured source OBJ."""
import json, sys
from pathlib import Path
import numpy as np
import open3d as o3d

source=Path(sys.argv[1])
output=Path(sys.argv[2])
output.parent.mkdir(parents=True,exist_ok=True)
mesh=o3d.io.read_triangle_mesh(str(source),enable_post_processing=False)
if mesh.is_empty() or not mesh.has_triangles():
    raise RuntimeError("Open3D could not read a triangle mesh from "+str(source))
vertices=np.asarray(mesh.vertices)
triangles=np.asarray(mesh.triangles)
if not np.isfinite(vertices).all():
    raise RuntimeError("Source has non-finite vertices")
if len(vertices)<1000:
    raise RuntimeError("Source mesh unexpectedly small")
bbox=mesh.get_axis_aligned_bounding_box()
bounds=np.asarray(bbox.get_extent())
if np.min(bounds)<=0:
    raise RuntimeError("Degenerate mesh bounding box")
# Analyze connected triangle components without changing original UVs/materials.
clusters, counts, areas=mesh.cluster_connected_triangles()
counts=np.asarray(counts)
areas=np.asarray(areas)
order=np.argsort(counts)[::-1]
report={
 "source":str(source),
 "open3d_version":o3d.__version__,
 "vertices":int(len(vertices)),
 "triangles":int(len(triangles)),
 "bounds_xyz":bounds.tolist(),
 "connected_components":int(len(counts)),
 "largest_components":[{"triangles":int(counts[i]),"area":float(areas[i])} for i in order[:20]],
 "watertight":bool(mesh.is_watertight()),
 "edge_manifold":bool(mesh.is_edge_manifold()),
 "vertex_manifold":bool(mesh.is_vertex_manifold()),
 "self_intersecting":bool(mesh.is_self_intersecting()),
 "note":"Diagnostic only: preserving source OBJ, UVs, and textures. Open3D cannot infer anatomical rigging or fingers."
}
output.write_text(json.dumps(report,indent=2))
print("OPEN3D_MESH_REPORT "+json.dumps(report),flush=True)
