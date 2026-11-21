package com.oceanserenity.image3d

import android.annotation.SuppressLint
import android.app.Activity
import android.app.Dialog
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.LinearLayout
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FilterInputStream
import java.io.InputStream
import java.util.zip.ZipFile

private const val PREVIEW_HOST = "appassets.androidplatform.net"

@SuppressLint("SetJavaScriptEnabled")
internal fun Activity.showFbxPreview(bundle: File) {
    val dialog = Dialog(this, android.R.style.Theme_Material_Light_NoActionBar)
    val browser = WebView(this).apply {
        settings.javaScriptEnabled = true
        settings.allowFileAccess = false
        settings.allowContentAccess = false
        webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest) = true

            override fun shouldInterceptRequest(view: WebView, request: WebResourceRequest): WebResourceResponse {
                val url = request.url
                if (request.method != "GET" || url.scheme != "https" || url.host != PREVIEW_HOST) {
                    return missing()
                }
                return try {
                    when (url.path) {
                        "/viewer/index.html" -> WebResourceResponse("text/html", "UTF-8", assets.open("viewer/index.html"))
                        "/viewer/viewer.js" -> WebResourceResponse("text/javascript", "UTF-8", assets.open("viewer/viewer.js"))
                        "/model/Character.fbx" -> responseFromZip(bundle, "Character.fbx", "application/octet-stream")
                        "/model/Character_BaseColor.png" -> responseFromZip(bundle, "Character_BaseColor.png", "image/png")
                        else -> missing()
                    }
                } catch (_: Exception) {
                    missing()
                }
            }
        }
    }
    val panel = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        addView(Button(context).apply {
            text = "Close preview"
            setOnClickListener { dialog.dismiss() }
        })
        addView(browser, LinearLayout.LayoutParams(-1, 0, 1f))
    }
    dialog.setContentView(panel)
    dialog.setOnDismissListener {
        browser.stopLoading()
        panel.removeView(browser)
        browser.destroy()
    }
    dialog.show()
    dialog.window?.setLayout(-1, -1)
    browser.loadUrl("https://$PREVIEW_HOST/viewer/index.html")
}

private fun responseFromZip(file: File, name: String, mime: String): WebResourceResponse {
    val zip = ZipFile(file)
    val input: InputStream
    try {
        val entry = zip.getEntry(name) ?: error("Missing model file")
        input = zip.getInputStream(entry)
    } catch (exception: Exception) {
        zip.close()
        throw exception
    }
    val stream = object : FilterInputStream(input) {
        override fun close() {
            try {
                super.close()
            } finally {
                zip.close()
            }
        }
    }
    return WebResourceResponse(mime, null, stream)
}

private fun missing() = WebResourceResponse(
    "text/plain", "UTF-8", 404, "Not Found", emptyMap(), ByteArrayInputStream(byteArrayOf())
)
