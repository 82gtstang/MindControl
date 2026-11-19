package com.oceanserenity.image3d

import android.app.Activity
import android.os.Bundle
import android.content.Intent
import android.net.Uri
import android.graphics.BitmapFactory
import android.widget.*
import android.view.ViewGroup
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID
import kotlin.concurrent.thread

class MainActivity : Activity() {
 private var image: Uri? = null
 private lateinit var status: TextView
 private lateinit var preview: ImageView
 private lateinit var endpoint: EditText
 private var pendingFile: ByteArray? = null
 override fun onCreate(savedInstanceState: Bundle?) {
  super.onCreate(savedInstanceState)
  val layout = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(28,28,28,28) }
  layout.addView(TextView(this).apply { text="Ocean 3D • Image to Blender"; textSize=24f })
  endpoint = EditText(this).apply { hint="HTTPS AI server URL"; setSingleLine(true) }
  layout.addView(endpoint)
  layout.addView(Button(this).apply { text="Choose character image"; setOnClickListener {
   startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT).apply { type="image/*"; addCategory(Intent.CATEGORY_OPENABLE) }, 1)
  } })
  preview = ImageView(this).apply { adjustViewBounds=true; maxHeight=550 }
  layout.addView(preview, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
  layout.addView(Button(this).apply { text="Generate Blender project"; setOnClickListener { generate() } })
  status = TextView(this).apply { text="Select an image and enter your HTTPS AI server URL." }
  layout.addView(status)
  setContentView(layout)
 }
 override fun onActivityResult(requestCode:Int, resultCode:Int, data:Intent?) {
  super.onActivityResult(requestCode,resultCode,data)
  if (requestCode==2 && resultCode==RESULT_OK) { data?.data?.let { saveResult(it) }; return }
  if (requestCode==1 && resultCode==RESULT_OK) {
   image=data?.data
   image?.let { uri -> contentResolver.openInputStream(uri)?.use { preview.setImageBitmap(BitmapFactory.decodeStream(it)) } }
   status.text="Image selected"
  }
 }
 private fun generate() {
  val uri=image ?: run { status.text="Select an image first"; return }
  val base=endpoint.text.toString().trim().trimEnd('/')
  if (!base.startsWith("https://")) { status.text="Use an HTTPS server URL"; return }
  status.text="Uploading and processing image…"
  thread {
   try {
    val boundary="Ocean3D"+UUID.randomUUID().toString().replace("-","")
    val connection=(URL("$base/generate").openConnection() as HttpURLConnection).apply {
     requestMethod="POST"; doOutput=true; connectTimeout=30000; readTimeout=600000
     setRequestProperty("Content-Type","multipart/form-data; boundary=$boundary")
    }
    connection.outputStream.use { out ->
     out.write("--$boundary\r\nContent-Disposition: form-data; name=\"image\"; filename=\"reference.png\"\r\nContent-Type: image/png\r\n\r\n".toByteArray())
     contentResolver.openInputStream(uri)!!.use { it.copyTo(out) }
     out.write("\r\n--$boundary--\r\n".toByteArray())
    }
    if (connection.responseCode!=200) {
     val error=connection.errorStream?.bufferedReader()?.readText()?.take(500) ?: connection.responseMessage
     throw Exception("Server ${connection.responseCode}: $error")
    }
    val bytes=connection.inputStream.use { it.readBytes() }
    connection.disconnect()
    if (bytes.size<100) throw Exception("Empty Blender output")
    runOnUiThread {
     status.text="Blender file ready (${bytes.size/1024} KB). Choose where to save."
     pendingFile=bytes
     startActivityForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply { type="application/octet-stream"; addCategory(Intent.CATEGORY_OPENABLE); putExtra(Intent.EXTRA_TITLE,"OceanSerenity.blend") },2)
    }
   } catch(e:Exception) { runOnUiThread { status.text="Generation failed: ${e.message}" } }
  }
 }
 private fun saveResult(uri:Uri) {
  val bytes=pendingFile ?: return
  try { contentResolver.openOutputStream(uri)?.use { it.write(bytes) }; status.text="Saved Blender project" }
  catch(e:Exception) { status.text="Save failed: ${e.message}" }
  finally { pendingFile=null }
 }
}
