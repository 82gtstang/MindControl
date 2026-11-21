package com.oceanserenity.image3d

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import kotlin.concurrent.thread

class MainActivity : Activity() {
    private var image: Uri? = null
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
            text = "Choose one subject. This app generates a textured, unrigged FBX on your phone."
        })
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
        generate.isEnabled = false
        save.isEnabled = false
        preview.isEnabled = false
        status.text = "Generating the mesh and texture…"
        thread {
            val message = try {
                val mesh = LocalReconstruction(applicationContext).generate(uri) { stage ->
                    runOnUiThread { status.text = stage }
                }
                saveOfflineBundle(mesh, bundle)
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

    private companion object {
        const val PICK_PHOTO = 1
        const val SAVE_BUNDLE = 2
    }
}
