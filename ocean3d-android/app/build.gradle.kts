plugins { id("com.android.application"); id("org.jetbrains.kotlin.android") }
android {
 namespace = "com.oceanserenity.image3d"
 compileSdk = 35
 defaultConfig { applicationId = "com.oceanserenity.image3d"; minSdk = 28; targetSdk = 35; versionCode = 5; versionName = "0.5-offline"; ndk { abiFilters += "arm64-v8a" } }
 compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
 androidResources { noCompress += "onnx" }
 kotlinOptions { jvmTarget = "17" }
}

dependencies {
 implementation("com.microsoft.onnxruntime:onnxruntime-android:1.22.0")
 implementation("com.google.mlkit:segmentation-selfie:16.0.0-beta6")
}
