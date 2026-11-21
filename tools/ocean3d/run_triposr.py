"""Run TripoSR with CUDA texture baking and conservative source-photo detail."""

from pathlib import Path
import runpy
import sys

import numpy as np
from PIL import ImageFilter
import torch


root = Path.cwd()
sys.path.insert(0, str(root))
from tsr import bake_texture, utils

source_photo = None
original_resize = utils.resize_foreground


def capture_foreground(image, ratio):
    global source_photo
    source_photo = original_resize(image, ratio)
    return source_photo



def restore_face(locations, colors, reference):
    try:
        import cv2
    except ImportError:
        print("Face refinement skipped: face detector unavailable.", flush=True)
        return
    height, width = reference.shape[:2]
    occupied = reference[:, :, 3] > .85
    rows = np.flatnonzero(occupied.sum(axis=1) > width * .025)
    if len(rows) < height * .35:
        return
    crown, bottom = int(rows[0]), int(rows[-1])
    body_height = bottom - crown
    gray = cv2.cvtColor(np.uint8(reference[:, :, :3] * 255), cv2.COLOR_RGB2GRAY)
    detector = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    if detector.empty():
        return
    face_limit = crown + int(body_height * .24)
    try:
        faces = detector.detectMultiScale(gray[max(0, crown-10):face_limit], 1.08, 5, minSize=(30, 30))
    except cv2.error:
        print("Face refinement skipped: face detector rejected this crop.", flush=True)
        return
    if len(faces) != 1:
        print("Face refinement skipped: expected one frontal face.", flush=True)
        return
    fx, fy, fw, fh = (int(value) for value in faces[0])
    fy += max(0, crown - 10)
    if not .052 < fh / body_height < .21:
        return

    # Use crown-to-foot scale and the head's local silhouette rather than
    # the correspondence estimated from the entire outfit.
    depth = 1.9 - locations[:, 0]
    u = locations[:, 1] / (depth * .728)
    v = -locations[:, 2] / (depth * .728)
    top, foot = np.percentile(v, [.15, 99.85])
    if foot <= top:
        return
    qy = crown + (v - top) * body_height / (foot - top)
    band = (qy > fy + .28 * fh) & (qy < fy + .58 * fh)
    if np.count_nonzero(band) < 300:
        return
    head_left, head_right = np.percentile(u[band], [4, 96])
    photo_row = int(fy + .44 * fh)
    silhouette = np.flatnonzero(occupied[photo_row])
    if len(silhouette) < fw or head_right <= head_left:
        return
    image_left, image_right = np.percentile(silhouette, [3, 97])
    head_center = .5 * (head_left + head_right)
    image_center = .5 * (image_left + image_right)
    x_scale = np.clip((image_right - image_left) / (head_right - head_left),
                      .72 * body_height / (foot - top), 1.35 * body_height / (foot - top))
    qx = image_center + (u - head_center) * x_scale
    cx, cy = fx + .5 * fw, fy + .51 * fh
    ellipse = ((qx - cx) / (.52 * fw))**2 + ((qy - cy) / (.57 * fh))**2
    face_ids = np.flatnonzero(ellipse < 1)
    if not face_ids.size:
        return

    bx = np.clip(((u + .5) * 511).astype(np.int32), 0, 511)
    by = np.clip(((v + .5) * 511).astype(np.int32), 0, 511)
    front = np.full((512, 512), np.inf, np.float32)
    np.minimum.at(front, (by, bx), depth)
    x = np.clip(np.rint(qx[face_ids]).astype(np.int32), 0, width-1)
    y = np.clip(np.rint(qy[face_ids]).astype(np.int32), 0, height-1)
    surface = np.clip(1 - (depth[face_ids] - front[by[face_ids], bx[face_ids]]) / .023, 0, 1)
    edge = np.clip((1 - ellipse[face_ids]) / .46, 0, 1)
    alpha = (.96 * surface * edge * (reference[y, x, 3] > .98))[:, None]
    colors[face_ids, :3] += alpha * (reference[y, x, :3] - colors[face_ids, :3])
    print(f"Frontal face reference restored on {np.count_nonzero(alpha > .35)} texels.", flush=True)


def add_photo_detail(pixels, colors):
    if source_photo is None or min(source_photo.size) < 160:
        return
    photo = source_photo.copy()
    photo.thumbnail((1536, 1536))
    reference = np.asarray(photo, dtype=np.float32) / 255
    blurred = np.asarray(photo.convert("RGB").filter(ImageFilter.GaussianBlur(3)), dtype=np.float32) / 255
    height, width = reference.shape[:2]
    visible_indices = np.flatnonzero(pixels[:, 3] > .9)
    if not visible_indices.size:
        return

    # Test the reconstruction's principal views against the source photo.
    # A depth buffer excludes surfaces hidden behind the matching front view.
    locations = pixels[visible_indices, :3]
    expected = colors[visible_indices, :3]
    sample = slice(None, None, max(1, len(locations) // 18000))
    best = None
    for axis, sign in [(0, 1), (0, -1), (1, 1), (1, -1)]:
        depth = 1.9 - sign * locations[:, axis]
        lateral = locations[:, 1 - axis] * (sign if axis == 0 else -sign)
        u = lateral / (depth * .728)
        v = -locations[:, 2] / (depth * .728)
        bx = np.clip(((u + .5) * 255).astype(np.int32), 0, 255)
        by = np.clip(((v + .5) * 255).astype(np.int32), 0, 255)
        front_depth = np.full((256, 256), np.inf, np.float32)
        np.minimum.at(front_depth, (by, bx), depth)
        visible = (depth - front_depth[by, bx] < .034) & (abs(u) < .5) & (abs(v) < .5)
        for scale in (.86, 1., 1.14):
            for offset_y in (-.045, 0., .045):
                for offset_x in (-.035, 0., .035):
                    x = np.clip(((u[sample] * scale + .5 + offset_x) * width).astype(np.int32), 0, width - 1)
                    y = np.clip(((v[sample] * scale + .5 + offset_y) * height).astype(np.int32), 0, height - 1)
                    mask = visible[sample] & (reference[y, x, 3] > .97)
                    count = np.count_nonzero(mask)
                    if count < 450:
                        continue
                    e = expected[sample][mask]
                    p = blurred[y[mask], x[mask]]
                    lum_e = e.mean(axis=1)
                    lum_p = p.mean(axis=1)
                    if lum_e.std() < .035 or lum_p.std() < .035:
                        continue
                    correlation = float(np.corrcoef(lum_e, lum_p)[0, 1])
                    error = float(np.abs(e - p).mean())
                    coverage = count / max(1, np.count_nonzero(visible[sample]))
                    score = correlation - error + .30 * coverage
                    if coverage >= .64 and correlation >= .53 and error <= .24 and (best is None or score > best[0]):
                        best = (score, axis, sign, scale, offset_x, offset_y, correlation)
    if best is None:
        print("Photo detail skipped: no reliable source-view match.", flush=True)
        return

    _, axis, sign, scale, offset_x, offset_y, correlation = best
    depth = 1.9 - sign * locations[:, axis]
    u = locations[:, 1 - axis] * (sign if axis == 0 else -sign) / (depth * .728)
    v = -locations[:, 2] / (depth * .728)
    bx = np.clip(((u + .5) * 255).astype(np.int32), 0, 255)
    by = np.clip(((v + .5) * 255).astype(np.int32), 0, 255)
    zbuffer = np.full((256, 256), np.inf, np.float32)
    np.minimum.at(zbuffer, (by, bx), depth)
    qx = (u * scale + .5 + offset_x) * (width - 1)
    qy = (v * scale + .5 + offset_y) * (height - 1)
    x = np.clip(qx.astype(np.int32), 2, width - 3)
    y = np.clip(qy.astype(np.int32), 2, height - 3)
    photo_pixel = reference[y, x, :3]
    base = blurred[y, x]
    difference = np.abs(base - expected).mean(axis=1)
    mask = (
        (qx >= 2) & (qx < width - 2) & (qy >= 2) & (qy < height - 2)
        & (reference[y, x, 3] > .99) & (reference[y-2, x, 3] > .99)
        & (reference[y+2, x, 3] > .99) & (reference[y, x-2, 3] > .99)
        & (reference[y, x+2, 3] > .99)
    )
    confidence = np.clip((.23 - difference) / .18, 0, 1)
    surface = np.clip(1 - (depth - zbuffer[by, bx]) / .032, 0, 1)
    blend = (confidence * surface * mask)[:, None]
    # Recover small surface details while keeping the model's broad color and
    # leaving occluded surfaces and uncertain alignment entirely model-derived.
    detail = np.clip(photo_pixel - base, -.13, .13)
    # Increase fine contrast only within the lowest-error part of a match.
    # Keep the stronger transfer away from seams and uncertain boundaries.
    strong_match = np.clip((.145 - difference) / .105, 0, 1)[:, None]
    recovered = (.78 + .42 * strong_match) * detail
    colors[visible_indices, :3] = np.clip(expected + blend * (recovered + .10 * (base - expected)), 0, 1)
    if axis == 0 and sign == 1:
        refined = colors[visible_indices].copy()
        try:
            restore_face(locations, refined, reference)
        except (ValueError, RuntimeError, IndexError) as error:
            print(f"Face refinement skipped: {type(error).__name__}.", flush=True)
        else:
            colors[visible_indices] = refined
    else:
        print("Face refinement skipped: source-view orientation is uncertain.", flush=True)
    print(f"Source photo details transferred to {np.count_nonzero(blend > .1)} texels (match {correlation:.2f}).", flush=True)


def positions_to_colors(model, scene_code, positions_texture, texture_resolution):
    pixels = positions_texture.reshape(-1, 4)
    positions = torch.tensor(pixels[:, :3], device=scene_code.device)
    with torch.no_grad():
        result = model.renderer.query_triplane(model.decoder, positions, scene_code)
    rgb = result["color"].detach().cpu().numpy().reshape(-1, 3)
    rgba = np.concatenate((rgb, pixels[:, 3:4]), axis=1)
    rgba[pixels[:, 3] == 0] = 0
    add_photo_detail(pixels, rgba)
    return rgba.reshape(texture_resolution, texture_resolution, 4)


utils.resize_foreground = capture_foreground
bake_texture.positions_to_colors = positions_to_colors
runpy.run_path(str(root / "run.py"), run_name="__main__")
