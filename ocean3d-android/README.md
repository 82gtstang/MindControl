# Image to Unreal for Android

The APK sends a character photo to a private HTTPS worker and saves an Unreal
import ZIP containing a textured FBX. The result is an unrigged static mesh.
Single-image reconstruction estimates details on the hidden sides.

## Run the worker on the Windows GPU PC

Use the existing TripoSR installation created by
`tools/ocean3d/install-triposr.ps1`. From the repository root on that PC:

```powershell
$workerRoot = Join-Path $env:LOCALAPPDATA 'Ocean3D'
$workerPython = Join-Path $workerRoot 'venv\Scripts\python.exe'
& $workerPython -m pip install "Flask>=3.1,<4" "waitress>=3,<4"
$env:OCEAN3D_TRIPOSR = Join-Path $workerRoot 'TripoSR-main'
$env:OCEAN3D_BLENDER = 'C:\Program Files\Blender Foundation\Blender 4.3\blender.exe'
$env:OCEAN3D_TOKEN = & $workerPython -c "import secrets; print(secrets.token_urlsafe(32))"
Write-Host "Enter this token only in your Android app: $env:OCEAN3D_TOKEN"
& $workerPython tools/ocean3d/unreal_server.py
```

Adjust the Blender path to match your installation. Route your HTTPS reverse
proxy to `http://127.0.0.1:8787` on the worker PC. Forward the Authorization
header and allow a POST of up to 24 MiB and a response timeout of 620 seconds.
Use an HTTPS hostname with a certificate trusted by Android. Keep the token
private and stable for subsequent starts.

In the APK, enter the HTTPS base URL and the token, choose one picture, and
select **Generate Unreal FBX**. Save the ZIP. Extract it on the Unreal PC and
import `Character.fbx` as a Static Mesh. The ZIP also contains the color
texture and import instructions. Rig the mesh separately for character
animation.

## Preview and local builds

After generation, tap **Preview 3D model** to open the FBX on the device. Drag
to rotate, pinch to zoom, or select **Reset view** to frame the model again.
The preview uses files already present in the cached ZIP.

Before a local Gradle build, run `npm --prefix viewer ci` and
`npm --prefix viewer run build` from the `ocean3d-android` directory.
