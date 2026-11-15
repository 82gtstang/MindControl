# Mind Control — Unreal Engine 5 prototype

This is a **separate Unreal Engine 5.5 C++ starter project**, not a conversion of the existing Godot game. It is intentionally isolated on the `unreal5-prototype` branch to avoid breaking the working Godot project.

## Open and build
1. Install Unreal Engine 5.5 and Android SDK/NDK components supported by that engine release.
2. Open `unreal/MindControl.uproject` in Unreal Editor, generate C++ project files, and compile.
3. Create a level named `L_ForgottenCrypt` using modular stone meshes, staircases, rock formations, and torch fixtures.
4. Use licensed PBR assets with base color, normal, and roughness maps. Build material instances for mossy stone, damp rock, iron gates, and ancient wood.
5. Create a third-person character Blueprint with an animation Blueprint and root-motion attack montages. Bind actions for jump, attack, spin, interact, inventory and equipment.
6. Implement equipment using skeletal mesh sockets and show/hide attachments based on actual equipped slots. Do not permanently include a sword or armor in the base mesh.
7. Add three dungeon entrances with paired level portals, navigation meshes, collision and enemy spawn volumes.
8. Profile on the target Android phone; use mobile-compatible lighting and LODs, not desktop-only Nanite/Lumen assumptions.
9. Package using Unreal Editor's Android packaging workflow.

## Current limitations
No realistic dungeon models, textures, gameplay Blueprints, animation assets, enemies, or APK are included yet. The existing Godot code and content cannot be loaded directly by Unreal. This folder establishes the Unreal project and mobile rendering settings only.
