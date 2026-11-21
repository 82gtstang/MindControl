"""Authenticated, single-job TripoSR worker for the Ocean 3D Android app."""

import hmac
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import zipfile

from flask import Flask, request, send_file
from PIL import Image, ImageOps, UnidentifiedImageError


app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 24 * 1024 * 1024
job_lock = threading.Lock()
Image.MAX_IMAGE_PIXELS = 36_000_000


@app.post("/generate-unreal")
def generate():
    expected = os.environ["OCEAN3D_TOKEN"]
    if not hmac.compare_digest(
        request.headers.get("Authorization", "").encode(), f"Bearer {expected}".encode()
    ):
        return "Invalid server access token.", 401
    if not job_lock.acquire(blocking=False):
        return "The generator is working on another image. Try again shortly.", 429
    try:
        upload = request.files.get("image")
        if upload is None:
            return "Choose one image.", 400
        with tempfile.TemporaryDirectory(prefix="ocean3d-") as temporary:
            root = Path(temporary)
            photo = root / "reference.png"
            try:
                with Image.open(upload.stream) as raw:
                    if raw.width * raw.height > Image.MAX_IMAGE_PIXELS:
                        return "Image exceeds the 36-megapixel limit.", 400
                    image = ImageOps.exif_transpose(raw).convert("RGBA")
                    image.thumbnail((2048, 2048))
                    image.save(photo)
            except (OSError, UnidentifiedImageError, Image.DecompressionBombError):
                return "That image could not be decoded. Try JPEG or PNG.", 400

            triposr = Path(os.environ["OCEAN3D_TRIPOSR"]).resolve()
            out = root / "model"
            subprocess.run(
                [sys.executable, str(Path(__file__).with_name("run_triposr.py").resolve()), str(photo),
                 "--output-dir", str(out), "--model-save-format", "obj",
                 "--bake-texture", "--texture-resolution", "1024"],
                cwd=triposr, check=True, timeout=480,
            )
            model = out / "0"
            fbx = root / "Character.fbx"
            subprocess.run(
                [os.environ["OCEAN3D_BLENDER"], "--background", "--factory-startup",
                 "--python-exit-code", "1", "--python", str(Path(__file__).with_name("export_unreal.py").resolve()),
                 "--", str(model), str(fbx)],
                check=True, timeout=90,
            )
            bundle = io.BytesIO()
            with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
                archive.write(fbx, "Character.fbx")
                archive.write(model / "texture.png", "Character_BaseColor.png")
                archive.writestr("IMPORT.txt", "In Unreal Editor, import Character.fbx as a Static Mesh with materials and textures enabled. If needed, import Character_BaseColor.png and connect it to Base Color. This reconstruction is unrigged. Check its scale, orientation and topology before rigging or animating it.\n")
            bundle.seek(0)
            return send_file(bundle, mimetype="application/zip", as_attachment=True, download_name="UnrealCharacter.zip")
    except subprocess.TimeoutExpired:
        return "Generation timed out on the worker.", 504
    except (OSError, subprocess.CalledProcessError):
        app.logger.exception("Character generation failed")
        return "Generation failed. Check the worker log.", 502
    finally:
        job_lock.release()


if __name__ == "__main__":
    from waitress import serve

    for variable in ("OCEAN3D_TOKEN", "OCEAN3D_TRIPOSR", "OCEAN3D_BLENDER"):
        if not os.environ.get(variable):
            raise SystemExit(f"Set {variable} before starting the worker.")
    if len(os.environ["OCEAN3D_TOKEN"]) < 24:
        raise SystemExit("Use a random access token at least 24 characters long.")
    if not (Path(os.environ["OCEAN3D_TRIPOSR"]) / "run.py").is_file():
        raise SystemExit("OCEAN3D_TRIPOSR must point to the installed TripoSR directory.")
    if not shutil.which(os.environ["OCEAN3D_BLENDER"]):
        raise SystemExit("OCEAN3D_BLENDER must identify a Blender executable.")
    serve(app, host="127.0.0.1", port=8787, threads=4, max_request_body_size=24 * 1024 * 1024, channel_timeout=660)
