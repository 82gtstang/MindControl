import * as THREE from 'three';
import { FBXLoader } from 'three/addons/loaders/FBXLoader.js';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const status = document.getElementById('status');
const reset = document.getElementById('reset');
const turn = document.getElementById('turn');

async function preview() {
    const scene = new THREE.Scene();
    scene.background = new THREE.Color(0x151a23);
    const renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
    document.body.prepend(renderer.domElement);

    const camera = new THREE.PerspectiveCamera(35, 1, 0.01, 100);
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enablePan = false;
    controls.enableDamping = true;
    controls.touches.TWO = THREE.TOUCH.DOLLY_ROTATE;
    scene.add(new THREE.HemisphereLight(0xffffff, 0x9ba7bd, 1.65));
    const key = new THREE.DirectionalLight(0xfff6ec, 1.45);
    key.position.set(2, 4, 3);
    scene.add(key);
    const fill = new THREE.DirectionalLight(0xe3eaff, 0.65);
    fill.position.set(-3, 1, -2);
    scene.add(fill);

    const model = await new FBXLoader().loadAsync('/model/Character.fbx');
    const bounds = new THREE.Box3().setFromObject(model);
    const size = bounds.getSize(new THREE.Vector3());
    const center = bounds.getCenter(new THREE.Vector3());
    const extent = Math.max(size.x, size.y, size.z);
    if (!Number.isFinite(extent) || extent <= 0) throw new Error('The FBX has no visible geometry.');
    const stage = new THREE.Group();
    stage.scale.setScalar(2 / extent);
    model.position.sub(center);
    stage.add(model);
    scene.add(stage);

    const untextured = new Set();
    const matteMaterials = new Map();
    const matte = (original) => {
        if (!original) return original;
        if (!matteMaterials.has(original)) {
            const material = new THREE.MeshStandardMaterial({
                color: original.map ? 0xffffff : original.color ?? 0xffffff,
                map: original.map ?? null,
                roughness: 0.86,
                metalness: 0,
                side: original.side,
                vertexColors: original.vertexColors,
            });
            matteMaterials.set(original, material);
        }
        return matteMaterials.get(original);
    };
    model.traverse((node) => {
        if (node.isMesh) {
            node.material = Array.isArray(node.material) ? node.material.map(matte) : matte(node.material);
            for (const material of [node.material].flat()) {
                if (material && !material.map) untextured.add(material);
            }
        }
    });
    if (untextured.size) {
        try {
            const texture = await new THREE.TextureLoader().loadAsync('/model/Character_BaseColor.png');
            texture.colorSpace = THREE.SRGBColorSpace;
            for (const material of untextured) {
                material.map = texture;
                material.color?.set(0xffffff);
                material.needsUpdate = true;
            }
        } catch {
            // Geometry is still viewable if the optional fallback texture cannot load.
        }
    }

    const resize = () => {
        camera.aspect = innerWidth / Math.max(1, innerHeight);
        camera.updateProjectionMatrix();
        renderer.setSize(innerWidth, innerHeight, false);
    };
    resize();
    window.addEventListener('resize', resize);
    const frame = () => {
        const vertical = THREE.MathUtils.degToRad(camera.fov) / 2;
        const horizontal = Math.atan(Math.tan(vertical) * camera.aspect);
        const distance = 1.2 / Math.sin(Math.min(vertical, horizontal));
        controls.target.set(0, 0, 0);
        camera.position.set(0.18, 0.07, 1).normalize().multiplyScalar(distance);
        controls.minDistance = 0.35;
        controls.maxDistance = distance * 3;
        controls.update();
    };
    frame();
    reset.addEventListener('click', frame);
    reset.hidden = false;
    turn.hidden = false;
    turn.addEventListener('click', () => {
        stage.rotation.z -= Math.PI / 2;
        frame();
    });
    status.textContent = 'Drag to rotate · Pinch to zoom';
    renderer.setAnimationLoop(() => {
        controls.update();
        renderer.render(scene, camera);
    });
}

preview().catch((error) => {
    console.error(error);
    status.textContent = 'Could not preview this FBX. You can still save the Unreal ZIP.';
});
