# Realistic character assets

Place a rigged, textured human character at `hero.glb` and a rigged enemy at `enemy.glb` in this folder. These files are not included; the game currently falls back to procedural placeholder characters until actual models are supplied.

Export as glTF 2.0 binary (`.glb`) with embedded textures, a humanoid skeleton, and animations named with terms like `Idle`, `Walk`, `Run`, and `Attack` (case insensitive). Keep meshes under approximately 30k triangles per character for Android and use 1K–2K PBR textures. The loader automatically scales the model to the existing gameplay character height. Use assets you own or have permission to redistribute.

For realistic appearance, replace both placeholders with proper rigged models; this code change alone does not create human models or textures.
