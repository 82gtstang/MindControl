package com.oceanserenity.image3d

import ai.onnxruntime.OnnxTensor
import ai.onnxruntime.OrtEnvironment
import ai.onnxruntime.OrtSession
import android.content.Context
import android.graphics.Bitmap
import android.graphics.ImageDecoder
import android.net.Uri
import com.google.android.gms.tasks.Tasks
import com.google.mlkit.vision.common.InputImage
import com.google.mlkit.vision.segmentation.Segmentation
import com.google.mlkit.vision.segmentation.selfie.SelfieSegmenterOptions
import java.io.File
import java.nio.FloatBuffer
import java.util.concurrent.TimeUnit
import kotlin.math.*

internal data class MeshVertex(val x: Float, val y: Float, val z: Float)
internal data class LocalMesh(val vertices: List<MeshVertex>, val indices: IntArray, val colors: IntArray, val reference: PreparedPhoto? = null)

internal data class PreparedPhoto(val rgb: FloatArray, val alpha: FloatArray)

internal class LocalReconstruction(private val context: Context) {
    private val environment = OrtEnvironment.getEnvironment()

    fun generate(uri: Uri, progress: (String) -> Unit): LocalMesh {
        progress("Preparing the image…")
        val image = prepare(uri)
        progress("Loading the bundled model…")
        val encoder = unpackEncoder()
        val planes: FloatArray
        val side: Int
        OrtSession.SessionOptions().use { options ->
            options.setOptimizationLevel(OrtSession.SessionOptions.OptLevel.NO_OPT)
            options.setIntraOpNumThreads(4)
            environment.createSession(encoder.path, options).use { session ->
                progress("Reconstructing the shape on your phone…")
                OnnxTensor.createTensor(environment, FloatBuffer.wrap(image.rgb), longArrayOf(1, 3, 512, 512)).use { tensor ->
                    session.run(mapOf("image" to tensor)).use { result ->
                        val output = result[0] as OnnxTensor
                        val shape = output.info.shape
                        check(shape.size == 5 && shape[1] == 3L && shape[2] == 40L && shape[3] == shape[4]) {
                            "Unexpected reconstruction model output."
                        }
                        side = shape[3].toInt()
                        val values = output.floatBuffer
                        planes = FloatArray(values.remaining()).also { values.get(it) }
                    }
                }
            }
        }
        val decoder = context.assets.open("models/mlp.onnx").use { it.readBytes() }
        OrtSession.SessionOptions().use { options ->
            options.setIntraOpNumThreads(4)
            environment.createSession(decoder, options).use { session ->
                progress("Building the surface…")
                val query = PlaneDecoder(environment, session, planes, side)
                val count = GRID_SIZE * GRID_SIZE * GRID_SIZE
                val density = FloatArray(count)
                for (start in 0 until count step BATCH) {
                    val length = min(BATCH, count - start)
                    val points = List(length) { gridPoint(start + it) }
                    query.run(points) { item, value, _, _, _ -> density[start + item] = value }
                }
                val (vertices, triangles) = extractSurface(density)
                check(vertices.isNotEmpty() && triangles.isNotEmpty()) { "No 3D surface found. Try a clearer photo of one subject." }
                progress("Adding color and exporting FBX…")
                val colors = IntArray(vertices.size)
                for (start in vertices.indices step BATCH) {
                    val points = vertices.subList(start, min(vertices.size, start + BATCH))
                    query.run(points) { item, _, r, g, b ->
                        colors[start + item] = (channel(r) shl 16) or (channel(g) shl 8) or channel(b)
                    }
                }
                return LocalMesh(vertices, triangles, colors, image)
            }
        }
    }

    private fun unpackEncoder(): File {
        val model = File(context.noBackupFilesDir, "triposr-9e030d7d.onnx")
        if (model.length() == ENCODER_BYTES) return model
        check(context.noBackupFilesDir.usableSpace > ENCODER_BYTES + 250_000_000) {
            "Free at least 2 GB of additional storage for the local model."
        }
        val partial = File(model.parentFile, "triposr.partial")
        try {
            context.assets.open("models/image_to_triplanes.onnx").use { input ->
                partial.outputStream().buffered().use { input.copyTo(it, 262144) }
            }
            check(partial.length() == ENCODER_BYTES && partial.renameTo(model)) { "Bundled model could not be unpacked." }
        } finally {
            partial.delete()
        }
        return model
    }

    private fun prepare(uri: Uri): PreparedPhoto {
        val bitmap = ImageDecoder.decodeBitmap(ImageDecoder.createSource(context.contentResolver, uri)) { decoder, info, _ ->
            decoder.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
            val longest = max(info.size.width, info.size.height)
            check(longest > 0) { "Invalid image dimensions." }
            val scale = min(1f, 768f / longest)
            decoder.setTargetSize(max(1, (info.size.width * scale).roundToInt()), max(1, (info.size.height * scale).roundToInt()))
        }
        val width = bitmap.width
        val height = bitmap.height
        val pixels = IntArray(width * height)
        bitmap.getPixels(pixels, 0, width, 0, 0, width, height)
        val transparency = pixels.count { it ushr 24 < 24 } > pixels.size / 25
        val mask = FloatArray(pixels.size) { (pixels[it] ushr 24) / 255f }
        if (!transparency) {
            val segmenter = Segmentation.getClient(
                SelfieSegmenterOptions.Builder().setDetectorMode(SelfieSegmenterOptions.SINGLE_IMAGE_MODE).build()
            )
            try {
                val result = Tasks.await(segmenter.process(InputImage.fromBitmap(bitmap, 0)), 25, TimeUnit.SECONDS)
                val buffer = result.buffer
                val values = FloatArray(result.width * result.height) { buffer.float }
                if (values.count { it > .65f } > values.size / 40) {
                    for (y in 0 until height) for (x in 0 until width) {
                        val confidence = values[(y * result.height / height) * result.width + x * result.width / width]
                        mask[y * width + x] = ((confidence - .10f) / .80f).coerceIn(0f, 1f)
                    }
                }
            } catch (_: Exception) {
                // A pre-cut subject or a clean background can still be reconstructed.
            } finally {
                segmenter.close()
            }
        }
        bitmap.recycle()
        var left = width
        var right = 0
        var top = height
        var bottom = 0
        for (y in 0 until height) for (x in 0 until width) {
            if (mask[y * width + x] > .45f) {
                left = min(left,x); right = max(right,x); top = min(top,y); bottom = max(bottom,y)
            }
        }
        check(right > left && bottom > top) { "No visible subject found." }
        val centerX = (left + right) / 2f
        val centerY = (top + bottom) / 2f
        val extent = max(right-left+1,bottom-top+1) / .85f
        val plane = 512 * 512
        val input = FloatArray(plane * 3) { .5f }
        val alpha = FloatArray(plane)
        for (y in 0 until 512) for (x in 0 until 512) {
            val sx = (centerX + ((x + .5f) / 512 - .5f) * extent).roundToInt()
            val sy = (centerY + ((y + .5f) / 512 - .5f) * extent).roundToInt()
            if (sx !in 0 until width || sy !in 0 until height) continue
            val index = sy * width + sx
            val a = mask[index]
            alpha[y * 512 + x] = a
            for (c in 0..2) {
                val color = (pixels[index] ushr (16 - c * 8)) and 255
                input[c * plane + y * 512 + x] = .5f * (1 - a) + a * color / 255f
            }
        }
        return PreparedPhoto(input, alpha)
    }

    private fun channel(logit: Float) = (255f / (1f + exp(-logit))).roundToInt().coerceIn(0, 255)

    companion object {
        private const val ENCODER_BYTES = 1678054860L
        private const val BATCH = 2048
    }
}

private class PlaneDecoder(
    private val environment: OrtEnvironment,
    private val session: OrtSession,
    private val planes: FloatArray,
    private val side: Int
) {
    fun run(points: List<MeshVertex>, read: (Int, Float, Float, Float, Float) -> Unit) {
        val features = FloatArray(points.size * 120)
        for ((i, point) in points.withIndex()) {
            sample(0, point.x, point.y, features, i * 120)
            sample(1, point.x, point.z, features, i * 120 + 40)
            sample(2, point.y, point.z, features, i * 120 + 80)
        }
        OnnxTensor.createTensor(environment, FloatBuffer.wrap(features), longArrayOf(points.size.toLong(), 120)).use { input ->
            session.run(mapOf("features" to input)).use { result ->
                val density = (result.get("density").get() as OnnxTensor).floatBuffer
                val rgb = (result.get("colors").get() as OnnxTensor).floatBuffer
                for (i in points.indices) read(i, density.get() - DENSITY_THRESHOLD, rgb.get(), rgb.get(), rgb.get())
            }
        }
    }

    private fun sample(plane: Int, x: Float, y: Float, output: FloatArray, offset: Int) {
        val u = ((x / GRID_RADIUS + 1) * side - 1) / 2
        val v = ((y / GRID_RADIUS + 1) * side - 1) / 2
        val ix = floor(u).toInt()
        val iy = floor(v).toInt()
        val fx = u - ix
        val fy = v - iy
        for (channel in 0 until 40) {
            val base = (plane * 40 + channel) * side * side
            fun at(a: Int, b: Int) = if (a in 0 until side && b in 0 until side) planes[base + b * side + a] else 0f
            output[offset + channel] =
                (1 - fy) * ((1 - fx) * at(ix, iy) + fx * at(ix + 1, iy)) +
                fy * ((1 - fx) * at(ix, iy + 1) + fx * at(ix + 1, iy + 1))
        }
    }
}

private const val DENSITY_THRESHOLD = 4.218876f // exp(raw - 1) = 25
