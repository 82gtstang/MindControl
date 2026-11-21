package com.oceanserenity.image3d

import android.graphics.Bitmap
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlin.math.ceil
import kotlin.math.max
import kotlin.math.sqrt

internal fun saveOfflineBundle(mesh: LocalMesh, destination: File) {
    val faces = mesh.indices.size / 3
    val columnCount = ceil(sqrt(faces.toDouble())).toInt()
    val tile = 6
    val atlasSize = columnCount * tile
    check(atlasSize <= 4096) { "This model exceeds the texture atlas limit." }
    val bitmap = Bitmap.createBitmap(atlasSize, atlasSize, Bitmap.Config.ARGB_8888)
    val uv = DoubleArray(faces * 6)
    val texels = IntArray(tile * tile)
    val projection = PhotoProjection.bestMatch(mesh)
    for (face in 0 until faces) {
        val tx = (face % columnCount) * tile
        val ty = (face / columnCount) * tile
        val p0 = mesh.vertices[mesh.indices[face * 3]]
        val p1 = mesh.vertices[mesh.indices[face * 3 + 1]]
        val p2 = mesh.vertices[mesh.indices[face * 3 + 2]]
        val c0 = mesh.colors[mesh.indices[face * 3]]
        val c1 = mesh.colors[mesh.indices[face * 3 + 1]]
        val c2 = mesh.colors[mesh.indices[face * 3 + 2]]
        for (y in 0 until tile) for (x in 0 until tile) {
            val a = ((x - 1f) / 3).coerceIn(0f,1f)
            val b = ((y - 1f) / 3).coerceIn(0f,1f)
            val sum = max(1f, a + b)
            var color = -0x1000000
            for (shift in intArrayOf(0,8,16)) {
                val value = (((c0 ushr shift and 255) * (1 - (a + b) / sum)) +
                    (c1 ushr shift and 255) * a / sum + (c2 ushr shift and 255) * b / sum).toInt()
                color = color or (value.coerceIn(0,255) shl shift)
            }
            texels[y * tile + x] = projection?.enhance(p0, p1, p2, a / sum, b / sum, color) ?: color
        }
        bitmap.setPixels(texels,0,tile,tx,ty,tile,tile)
        val start = face * 6
        for (i in 0..2) {
            uv[start + i*2] = (tx + if (i==1) 4.5 else 1.5) / atlasSize
            uv[start + i*2+1] = 1 - (ty + if (i==2) 4.5 else 1.5) / atlasSize
        }
    }
    val work = File(destination.parentFile, "offline-character.partial")
    try {
        ZipOutputStream(work.outputStream().buffered()).use { zip ->
            zip.putNextEntry(ZipEntry("Character_BaseColor.png"))
            check(bitmap.compress(Bitmap.CompressFormat.PNG, 100, zip))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("Character.fbx"))
            zip.write(writeFbx(mesh,uv))
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("IMPORT.txt"))
            zip.write("Import Character.fbx as an unrigged Static Mesh in Unreal. If the texture does not link automatically, import Character_BaseColor.png and connect it to the material's Base Color.\n".toByteArray())
            zip.closeEntry()
        }
        check(work.renameTo(destination)) { "Could not keep the generated ZIP." }
    } finally {
        bitmap.recycle()
        work.delete()
    }
}

private fun writeFbx(mesh: LocalMesh, uv: DoubleArray): ByteArray {
    val points = DoubleArray(mesh.vertices.size * 3)
    val low = mesh.vertices.minOf { it.z }
    val high = mesh.vertices.maxOf { it.z }
    val scale = 175.0 / max(.01f,high-low)
    for ((i, p) in mesh.vertices.withIndex()) {
        points[3*i] = p.y * scale
        points[3*i+1] = (p.z-low) * scale
        points[3*i+2] = p.x * scale
    }
    val normals = DoubleArray(points.size)
    for (i in mesh.indices.indices step 3) {
        val a=mesh.indices[i]*3; val b=mesh.indices[i+1]*3; val c=mesh.indices[i+2]*3
        val ux=points[b]-points[a]; val uy=points[b+1]-points[a+1]; val uz=points[b+2]-points[a+2]
        val vx=points[c]-points[a]; val vy=points[c+1]-points[a+1]; val vz=points[c+2]-points[a+2]
        for (v in intArrayOf(a,b,c)) {
            normals[v]+=uy*vz-uz*vy; normals[v+1]+=uz*vx-ux*vz; normals[v+2]+=ux*vy-uy*vx
        }
    }
    for (i in normals.indices step 3) {
        val length = sqrt(normals[i]*normals[i]+normals[i+1]*normals[i+1]+normals[i+2]*normals[i+2])
        if(length>0.0) for (j in 0..2) normals[i+j]/=length
    }
    val polygon = IntArray(mesh.indices.size) { if (it%3==2) -mesh.indices[it]-1 else mesh.indices[it] }
    val filename = "Character_BaseColor.png"
    fun element(type: String) = node("LayerElement", children=listOf(node("Type",type),node("TypedIndex",0)))
    fun setting(name: String,value: Any) = node("P",name,if(value is Double) "double" else "int",if(value is Double) "Number" else "Integer", "",value)
    val nodes = listOf(
        node("FBXHeaderExtension", children=listOf(node("FBXHeaderVersion",1003),node("FBXVersion",7400))),
        node("GlobalSettings", children=listOf(node("Version",1000),node("Properties70", children=listOf(
            setting("UpAxis",1),setting("UpAxisSign",1),setting("FrontAxis",2),setting("FrontAxisSign",1),
            setting("CoordAxis",0),setting("CoordAxisSign",1),setting("UnitScaleFactor",1.0))))),
        node("Objects", children=listOf(
            node("Geometry",1L,"Character\u0000\u0001Geometry","Mesh",children=listOf(
                node("Vertices",points),node("PolygonVertexIndex",polygon),node("GeometryVersion",124),
                node("LayerElementNormal",0,children=listOf(node("Version",101),node("Name",""),
                    node("MappingInformationType","ByVertice"),node("ReferenceInformationType","Direct"),node("Normals",normals))),
                node("LayerElementUV",0,children=listOf(node("Version",101),node("Name","UVMap"),
                    node("MappingInformationType","ByPolygonVertex"),node("ReferenceInformationType","IndexToDirect"),
                    node("UV",uv),node("UVIndex",IntArray(mesh.indices.size){it}))),
                node("LayerElementMaterial",0,children=listOf(node("Version",101),node("Name",""),
                    node("MappingInformationType","AllSame"),node("ReferenceInformationType","IndexToDirect"),node("Materials",intArrayOf(0)))),
                node("Layer",0,children=listOf(node("Version",100),element("LayerElementNormal"),element("LayerElementUV"),element("LayerElementMaterial")))
            )),
            node("Model",2L,"Character\u0000\u0001Model","Mesh",children=listOf(node("Version",232),node("Shading",true),node("Culling","CullingOff"))),
            node("Material",3L,"Character_Material\u0000\u0001Material","",children=listOf(node("Version",102),node("ShadingModel","Lambert"),node("MultiLayer",0),
                node("Properties70",children=listOf(node("P","DiffuseColor","Color","","A",1.0,1.0,1.0))))),
            node("Texture",4L,"Character_BaseColor\u0000\u0001Texture","",children=listOf(node("Type","TextureVideoClip"),node("Version",202),node("FileName",filename),node("RelativeFilename",filename))),
            node("Video",5L,"Character_BaseColor\u0000\u0001Video","Clip",children=listOf(
                node("Type","Clip"),node("Filename",filename),node("RelativeFilename",filename)))
        )),
        node("Connections",children=listOf(node("C","OO",1L,2L),node("C","OO",2L,0L),node("C","OO",3L,2L),node("C","OP",4L,3L,"DiffuseColor"),node("C","OO",5L,4L)))
    )
    val output = ByteBuffer.allocate(256 + nodes.sumOf { it.size }).order(ByteOrder.LITTLE_ENDIAN)
    output.put("Kaydara FBX Binary  \u0000\u001a\u0000".toByteArray())
    output.putInt(7400)
    nodes.forEach { it.write(output) }
    output.put(ByteArray(13))
    output.put(byteArrayOf(0xFA.toByte(),0xBC.toByte(),0xAB.toByte(),0x09,0xD0.toByte(),0xC8.toByte(),0xD4.toByte(),0x66,0xB1.toByte(),0x76,0xFB.toByte(),0x83.toByte(),0x1C,0xF7.toByte(),0x26,0x7E))
    while (output.position() % 16 != 0) output.put(0)
    output.putInt(7400)
    output.put(ByteArray(120))
    output.put(byteArrayOf(0xF8.toByte(),0x5A,0x8C.toByte(),0x6A,0xDE.toByte(),0xF5.toByte(),0xD9.toByte(),0x7E,0xEC.toByte(),0xE9.toByte(),0x0C,0xE3.toByte(),0x75,0x8F.toByte(),0x29,0x0B))
    return output.array().copyOf(output.position())
}

private fun node(name: String, vararg props: Any, children: List<FbxNode> = emptyList()) =
    FbxNode(name.toByteArray(), props.map(::property), children)

private data class FbxNode(val name: ByteArray,val properties: List<ByteArray>,val children: List<FbxNode>) {
    private val propertyBytes = properties.sumOf { it.size }
    val size: Int = 13 + name.size + propertyBytes + children.sumOf { it.size } + if (children.isEmpty()) 0 else 13
    fun write(output: ByteBuffer) {
        output.putInt(output.position()+size).putInt(properties.size).putInt(propertyBytes).put(name.size.toByte()).put(name)
        properties.forEach { output.put(it) }
        children.forEach { it.write(output) }
        if (children.isNotEmpty()) output.put(ByteArray(13))
    }
}

private fun property(value: Any): ByteArray {
    val size = when(value) {
        is Int -> 5
        is Long, is Double -> 9
        is Boolean -> 2
        is String -> 5 + value.toByteArray().size
        is DoubleArray -> 13 + value.size * 8
        is IntArray -> 13 + value.size * 4
        else -> error("Unknown FBX property type")
    }
    val bytes = ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN)
    fun tag(c: Char) { bytes.put(c.code.toByte()) }
    when(value) {
        is Int -> { tag('I'); bytes.putInt(value) }
        is Long -> { tag('L'); bytes.putLong(value) }
        is Double -> { tag('D'); bytes.putDouble(value) }
        is Boolean -> { tag('C'); bytes.put(if (value) 1.toByte() else 0.toByte()) }
        is String -> { tag('S'); val text=value.toByteArray(); bytes.putInt(text.size).put(text) }
        is DoubleArray -> { tag('d'); bytes.putInt(value.size).putInt(0).putInt(value.size*8); value.forEach { bytes.putDouble(it) } }
        is IntArray -> { tag('i'); bytes.putInt(value.size).putInt(0).putInt(value.size*4); value.forEach { bytes.putInt(it) } }
    }
    return bytes.array()
}
