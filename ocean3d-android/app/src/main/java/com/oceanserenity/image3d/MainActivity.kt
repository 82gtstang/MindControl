package com.oceanserenity.image3d

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.text.InputType
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID
import java.util.zip.ZipFile
import kotlin.concurrent.thread

class MainActivity : Activity() {
    private var image: Uri? = null
    private lateinit var server: EditText
    private lateinit var token: EditText
    private lateinit var status: TextView
    private lateinit var generate: Button
    private lateinit var save: Button
    private lateinit var preview: Button
    private val bundle by lazy { File(cacheDir, "UnrealCharacter.zip") }

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        image = state?.getString("image")?.let(Uri::parse)
        val page = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            val padding = (resources.displayMetrics.density * 24).toInt()
            setPadding(padding, padding, padding, padding)
        }
        page.addView(TextView(this).apply { text = "Image to Unreal"; textSize = 28f })
        page.addView(TextView(this).apply {
            text = "Use a clear photo of one full-body character. The worker returns an unrigged FBX and its texture for Unreal Editor."
        })
        server = EditText(this).apply {
            hint = "HTTPS worker URL"
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI
            setSingleLine(true)
            setText(getPreferences(MODE_PRIVATE).getString("server", ""))
        }
        page.addView(server)
        token = EditText(this).apply {
            hint = "Worker access token"
            inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD
            setSingleLine(true)
        }
        page.addView(token)
        page.addView(Button(this).apply {
            text = "Choose character photo"
            setOnClickListener {
                startActivityForResult(Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                    type = "image/*"
                    addCategory(Intent.CATEGORY_OPENABLE)
                }, PICK_PHOTO)
            }
        })
        generate = Button(this).apply { text = "Generate Unreal FBX"; setOnClickListener { requestModel() } }
        page.addView(generate)
        save = Button(this).apply { text = "Save Unreal ZIP"; isEnabled = bundle.isFile; setOnClickListener { chooseSaveLocation() } }
        page.addView(save)
        preview = Button(this).apply {
            text = "Preview 3D model"
            isEnabled = bundle.isFile
            setOnClickListener { showFbxPreview(bundle) }
        }
        page.addView(preview)
        status = TextView(this).apply {
            text = if (image == null) "Choose a character photo to start." else "Photo selected."
            textSize = 16f
        }
        page.addView(status)
        setContentView(ScrollView(this).apply {
            fitsSystemWindows = true
            addView(page)
        })
    }

    override fun onSaveInstanceState(state: Bundle) {
        super.onSaveInstanceState(state)
        state.putString("image", image?.toString())
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK) return
        val uri = data?.data ?: return
        if (requestCode == PICK_PHOTO) {
            try {
                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: SecurityException) {
                // The current activity can still use a temporary document grant.
            }
            image = uri
            status.text = "Photo selected."
        } else if (requestCode == SAVE_BUNDLE) {
            save.isEnabled = false
            thread {
                val message = try {
                    val output = contentResolver.openOutputStream(uri, "wt") ?: error("Cannot open destination")
                    output.use { stream -> bundle.inputStream().use { it.copyTo(stream) } }
                    "Unreal character ZIP saved."
                } catch (e: Exception) {
                    "Save failed: ${e.message}"
                }
                runOnUiThread { status.text = message; save.isEnabled = bundle.isFile }
            }
        }
    }

    private fun chooseSaveLocation() {
        startActivityForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            type = "application/zip"
            addCategory(Intent.CATEGORY_OPENABLE)
            putExtra(Intent.EXTRA_TITLE, "UnrealCharacter.zip")
        }, SAVE_BUNDLE)
    }

    private fun requestModel() {
        val uri = image ?: run { status.text = "Choose a photo first."; return }
        val base = server.text.toString().trim().trimEnd('/')
        val key = token.text.toString().trim()
        val url = runCatching { URL("$base/generate-unreal") }.getOrNull()
        if (url == null || url.protocol != "https" || url.host.isBlank() ||
            url.userInfo != null || url.query != null || url.ref != null || key.isBlank()) {
            status.text = "Enter the HTTPS worker URL and its access token."
            return
        }
        getPreferences(MODE_PRIVATE).edit().putString("server", base).apply()
        generate.isEnabled = false
        save.isEnabled = false
        preview.isEnabled = false
        status.text = "Generating the mesh and texture…"
        thread {
            val message = try {
                download(url, key, uri)
                "FBX and texture ready. Preview the model or save the Unreal ZIP."
            } catch (e: Exception) {
                "Generation failed: ${e.message}"
            }
            runOnUiThread {
                status.text = message
                generate.isEnabled = true
                save.isEnabled = bundle.isFile
                preview.isEnabled = bundle.isFile
            }
        }
    }

    private fun download(url: URL, key: String, uri: Uri) {
        val boundary = UUID.randomUUID().toString()
        val connection = (url.openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            instanceFollowRedirects = false
            connectTimeout = 30_000
            readTimeout = 620_000
            setChunkedStreamingMode(16_384)
            setRequestProperty("Authorization", "Bearer $key")
            setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
        }
        val temporary = File.createTempFile("unreal-", ".zip", cacheDir)
        try {
            connection.outputStream.use { out ->
                out.write(("--$boundary\r\nContent-Disposition: form-data; name=\"image\"; filename=\"reference\"\r\n" +
                    "Content-Type: application/octet-stream\r\n\r\n").toByteArray())
                val source = contentResolver.openInputStream(uri) ?: error("Cannot open photo")
                source.use { input ->
                    val buffer = ByteArray(16_384)
                    var total = 0
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        check(total <= 23 * 1024 * 1024) { "Choose a photo under 23 MB." }
                        out.write(buffer, 0, count)
                    }
                }
                out.write("\r\n--$boundary--\r\n".toByteArray())
            }
            val code = connection.responseCode
            if (code != 200) {
                val detail = connection.errorStream?.bufferedReader()?.use {
                    val chars = CharArray(220)
                    val count = it.read(chars)
                    if (count > 0) String(chars, 0, count) else ""
                }.orEmpty()
                error("Worker returned $code. $detail")
            }
            check(connection.contentType?.startsWith("application/zip") == true) { "Expected an Unreal ZIP from the worker." }
            connection.inputStream.use { input ->
                temporary.outputStream().use { output ->
                    val buffer = ByteArray(16_384)
                    var total = 0
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        check(total <= 100 * 1024 * 1024) { "Worker output exceeded 100 MB." }
                        output.write(buffer, 0, count)
                    }
                }
            }
            ZipFile(temporary).use {
                check(it.getEntry("Character.fbx")?.size?.let { size -> size > 0 } == true &&
                    it.getEntry("Character_BaseColor.png")?.size?.let { size -> size > 0 } == true) {
                    "The ZIP is missing the FBX or its color texture."
                }
            }
            check(temporary.renameTo(bundle)) { "Could not keep the result." }
        } finally {
            connection.disconnect()
            temporary.delete()
        }
    }

    private companion object {
        const val PICK_PHOTO = 1
        const val SAVE_BUNDLE = 2
    }
}
