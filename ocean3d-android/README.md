# Image to Unreal for Android

This ARM64 APK bundles the TripoSR model. Select a photo, tap **Generate Unreal
FBX**, and use **Preview 3D model** or **Save Unreal ZIP**. The app runs on the
phone. First use extracts the bundled encoder to private app storage and
requires about 2 GB of extra space beyond the APK installation. The result is
an unrigged static mesh, and hidden surfaces are estimated from the image.

Unzip the output and import Character.fbx as a Static Mesh in Unreal. Import
Character_BaseColor.png separately if the material does not automatically find it.

The Android workflow downloads the ONNX exports from the model repository and
verifies SHA-256 before packaging. The model license ships in the APK. Build the
viewer with npm before running Gradle locally, and place the same verified ONNX
files in app/src/main/assets/models.
