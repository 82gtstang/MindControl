"""Run TripoSR with texture queries on the same device as the scene code."""

from pathlib import Path
import runpy
import sys

import numpy as np
import torch


root = Path.cwd()
sys.path.insert(0, str(root))
from tsr import bake_texture


def positions_to_colors(model, scene_code, positions_texture, texture_resolution):
    pixels = positions_texture.reshape(-1, 4)
    positions = torch.tensor(pixels[:, :3], device=scene_code.device)
    with torch.no_grad():
        result = model.renderer.query_triplane(model.decoder, positions, scene_code)
    rgb = result["color"].detach().cpu().numpy().reshape(-1, 3)
    rgba = np.concatenate((rgb, pixels[:, 3:4]), axis=1)
    rgba[pixels[:, 3] == 0] = 0
    return rgba.reshape(texture_resolution, texture_resolution, 4)


bake_texture.positions_to_colors = positions_to_colors
runpy.run_path(str(root / "run.py"), run_name="__main__")
