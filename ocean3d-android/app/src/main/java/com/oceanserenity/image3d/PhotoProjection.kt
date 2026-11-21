package com.oceanserenity.image3d

import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

internal class PhotoProjection private constructor(
    private val photo: PreparedPhoto,
    private val depth: FloatArray,
    private val scale: Float,
    private val offsetX: Float,
    private val offsetY: Float,
) {
    fun enhance(p0: MeshVertex, p1: MeshVertex, p2: MeshVertex, a: Float, b: Float, color: Int): Int {
        val x = p0.x + a * (p1.x - p0.x) + b * (p2.x - p0.x)
        val y = p0.y + a * (p1.y - p0.y) + b * (p2.y - p0.y)
        val z = p0.z + a * (p1.z - p0.z) + b * (p2.z - p0.z)
        val distance = 1.9f - x
        val u = y / (.728f * distance)
        val v = -z / (.728f * distance)
        val closest = depth[depthIndex(u,v)]
        if (!closest.isFinite() || distance - closest > .038f) return color
        val ix = ((u * scale + .5f + offsetX) * SIZE).roundToInt()
        val iy = ((v * scale + .5f + offsetY) * SIZE).roundToInt()
        if (ix !in 2 until SIZE - 2 || iy !in 2 until SIZE - 2) return color
        val index = iy * SIZE + ix
        if (photo.alpha[index] < .96f || photo.alpha[index+2] < .9f || photo.alpha[index-2] < .9f ||
            photo.alpha[index+SIZE*2] < .9f || photo.alpha[index-SIZE*2] < .9f) return color
        val similarity = (1 - colorError(color,photo,index) / .23f).coerceIn(0f,1f)
        val visibility = (1 - max(0f,distance-closest) / .038f).coerceIn(0f,1f)
        val weight = .78f * similarity * visibility
        var result = -0x1000000
        for (channel in 0..2) {
            val shift = 16 - channel * 8
            val old = (color ushr shift) and 255
            val detail = photo.rgb[channel * PIXELS + index] * 255
            result = result or ((old + (detail-old)*weight).roundToInt().coerceIn(0,255) shl shift)
        }
        return result
    }

    companion object {
        private const val SIZE = 512
        private const val PIXELS = SIZE * SIZE
        private const val DEPTH_SIZE = 128

        private fun depthIndex(u: Float,v: Float): Int =
            ((v+.5f)*DEPTH_SIZE).toInt().coerceIn(0, DEPTH_SIZE-1)*DEPTH_SIZE +
                ((u+.5f)*DEPTH_SIZE).toInt().coerceIn(0, DEPTH_SIZE-1)

        private fun colorError(color: Int, photo: PreparedPhoto, index: Int): Float {
            var sum = 0f
            for (c in 0..2) sum += abs(((color ushr (16-c*8)) and 255)/255f - photo.rgb[c*PIXELS+index])
            return sum/3
        }

        fun bestMatch(mesh: LocalMesh): PhotoProjection? {
            val photo = mesh.reference ?: return null
            val depth = FloatArray(DEPTH_SIZE * DEPTH_SIZE) { Float.POSITIVE_INFINITY }
            for (p in mesh.vertices) {
                val distance=1.9f-p.x
                val key=depthIndex(p.y/(.728f*distance),-p.z/(.728f*distance))
                depth[key]=min(depth[key],distance)
            }
            var best: PhotoProjection? = null
            var bestScore = .22f
            for (scale in listOf(.86f,1f,1.14f)) for (dx in listOf(-.03f,0f,.03f)) for (dy in listOf(-.04f,0f,.04f)) {
                var total=0
                var matches=0
                var error=0f
                for (i in mesh.vertices.indices step 5) {
                    val p=mesh.vertices[i]
                    val d=1.9f-p.x
                    val u=p.y/(.728f*d)
                    val v=-p.z/(.728f*d)
                    if (d-depth[depthIndex(u,v)]>.025f) continue
                    total++
                    val x=((u*scale+.5f+dx)*SIZE).toInt()
                    val y=((v*scale+.5f+dy)*SIZE).toInt()
                    if (x !in 0 until SIZE || y !in 0 until SIZE) continue
                    val point=y*SIZE+x
                    if (photo.alpha[point]<.94f) continue
                    matches++
                    error+=colorError(mesh.colors[i],photo,point)
                }
                if (matches<150 || matches<.64f*total) continue
                val score=error/matches + .1f*(1f-matches.toFloat()/max(1,total))
                if (score<bestScore) {
                    bestScore=score
                    best=PhotoProjection(photo,depth,scale,dx,dy)
                }
            }
            return best
        }
    }
}
