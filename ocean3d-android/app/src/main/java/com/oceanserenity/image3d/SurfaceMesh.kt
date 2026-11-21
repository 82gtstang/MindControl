package com.oceanserenity.image3d

import kotlin.math.max
import kotlin.math.min

internal const val GRID_SIZE = 80
internal const val GRID_RADIUS = .87f
private val cornerOffsets = arrayOf(
    intArrayOf(0,0,0), intArrayOf(1,0,0), intArrayOf(1,1,0), intArrayOf(0,1,0),
    intArrayOf(0,0,1), intArrayOf(1,0,1), intArrayOf(1,1,1), intArrayOf(0,1,1)
)
private val tetrahedra = arrayOf(
    intArrayOf(0,5,1,6), intArrayOf(0,1,2,6), intArrayOf(0,2,3,6),
    intArrayOf(0,3,7,6), intArrayOf(0,7,4,6), intArrayOf(0,4,5,6)
)
private fun index(x: Int,y: Int,z: Int) = (z * GRID_SIZE + y) * GRID_SIZE + x
internal fun gridPoint(index: Int): MeshVertex {
    fun component(i: Int) = -GRID_RADIUS + 2 * GRID_RADIUS * i / (GRID_SIZE - 1)
    return MeshVertex(component(index % GRID_SIZE), component(index / GRID_SIZE % GRID_SIZE), component(index / (GRID_SIZE * GRID_SIZE)))
}

internal fun extractSurface(density: FloatArray): Pair<List<MeshVertex>, IntArray> {
    val vertices = ArrayList<MeshVertex>()
    val triangles = ArrayList<Int>()
    val edges = HashMap<Long, Int>()
    val offsets = cornerOffsets.map { (x,y,z) -> index(x,y,z) }
    val corners = IntArray(8)
    val inside = IntArray(4)
    val outside = IntArray(4)
    fun edge(a: Int, b: Int): Int {
        val key = (min(a,b).toLong() shl 32) or max(a,b).toLong()
        return edges.getOrPut(key) {
            val p = gridPoint(a)
            val q = gridPoint(b)
            val t = (density[a] / (density[a] - density[b])).coerceIn(.0001f,.9999f)
            vertices.add(MeshVertex(p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t))
            vertices.lastIndex
        }
    }
    fun face(a: Int, b: Int, c: Int, interior: MeshVertex) {
        val p = vertices[a]
        val q = vertices[b]
        val r = vertices[c]
        val abx=q.x-p.x; val aby=q.y-p.y; val abz=q.z-p.z
        val acx=r.x-p.x; val acy=r.y-p.y; val acz=r.z-p.z
        val inward = (aby*acz-abz*acy)*(interior.x-p.x) +
            (abz*acx-abx*acz)*(interior.y-p.y) + (abx*acy-aby*acx)*(interior.z-p.z)
        triangles.add(a)
        triangles.add(if (inward > 0) c else b)
        triangles.add(if (inward > 0) b else c)
    }
    for (z in 0 until GRID_SIZE-1) for (y in 0 until GRID_SIZE-1) for (x in 0 until GRID_SIZE-1) {
        val base = index(x,y,z)
        var occupied = 0
        for (i in corners.indices) {
            corners[i] = base + offsets[i]
            if (density[corners[i]] > 0) occupied++
        }
        if (occupied == 0 || occupied == 8) continue
        for (tetra in tetrahedra) {
            var a = 0
            var b = 0
            for (corner in tetra) {
                val v = corners[corner]
                if (density[v] > 0) inside[a++] = v else outside[b++] = v
            }
            if (a == 0 || a == 4) continue
            val tip = gridPoint(inside[0])
            when (a) {
                1 -> face(edge(inside[0],outside[0]),edge(inside[0],outside[1]),edge(inside[0],outside[2]),tip)
                2 -> {
                    val ac = edge(inside[0],outside[0]); val ad = edge(inside[0],outside[1])
                    val bc = edge(inside[1],outside[0]); val bd = edge(inside[1],outside[1])
                    face(ac,ad,bd,tip)
                    face(ac,bd,bc,tip)
                }
                3 -> face(edge(inside[0],outside[0]),edge(inside[1],outside[0]),edge(inside[2],outside[0]),tip)
            }
        }
    }
    return vertices to triangles.toIntArray()
}
