<script setup lang="ts">
import { ref, watch, nextTick, onMounted, onUnmounted } from "vue";
import { withBase } from "vitepress";
import BoardPhoto from "./BoardPhoto.vue";
import {
  RotateCcw,
  RotateCw,
  ZoomIn,
  ZoomOut,
  Scan,
  MousePointer2,
} from "lucide-vue-next";
type Marker = { name: string; pin: string; x: number; y: number };
const props = withDefaults(
  defineProps<{
    en: boolean;
    active?: boolean;
    markers: Marker[];
    selected: string;
    ledColor?: string;
  }>(),
  { active: true },
);
const emit = defineEmits<{ select: [name: string] }>();
const host = ref<HTMLElement>();
const failed = ref(false),
  loading = ref(true),
  progress = ref(0);
const markerLayer = ref<HTMLElement>();
const side = ref("front");
let flip = (value: string) => {};
let cleanup = () => {},
  reset = () => {},
  zoom = (factor: number) => {},
  rotate = (angle: number) => {},
  activate = () => {};
let setLed = (color: string) => {};
watch(
  () => props.ledColor,
  (value) => setLed(value || "#171d24"),
);
let disposed = false;
watch(
  () => props.active,
  () => nextTick(activate),
);
onMounted(async () => {
  try {
    const started = performance.now();
    const [
      THREE,
      { OrbitControls },
      { GLTFLoader },
      { MeshoptDecoder },
      { RoomEnvironment },
    ] = await Promise.all([
      import("three"),
      import("three/addons/controls/OrbitControls.js"),
      import("three/addons/loaders/GLTFLoader.js"),
      import("meshoptimizer"),
      import("three/addons/environments/RoomEnvironment.js"),
    ]);
    if (disposed) return;
    const scene = new THREE.Scene();
    const camera = new THREE.OrthographicCamera(-55, 55, 55, -55, 0.5, 500);
    const renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: "high-performance",
    });
    renderer.setPixelRatio(Math.min(devicePixelRatio, 1.5));
    renderer.setClearColor(0xffffff, 0);
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 0.9;
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.VSMShadowMap;
    renderer.shadowMap.autoUpdate = false;
    const pmrem = new THREE.PMREMGenerator(renderer);
    const room = new RoomEnvironment();
    const environment = pmrem.fromScene(room, 0.04);
    scene.environment = environment.texture;
    room.dispose();
    pmrem.dispose();
    host.value!.appendChild(renderer.domElement);
    renderer.domElement.setAttribute(
      "aria-label",
      props.en
        ? "OSRCORE 3D board. Drag to pan, scroll to zoom."
        : "OSRCORE 三维板卡。拖动平移，滚轮缩放。",
    );
    const controls = new OrbitControls(camera, renderer.domElement);
    controls.enableDamping = !matchMedia("(prefers-reduced-motion: reduce)")
      .matches;
    controls.dampingFactor = 0.12;
    controls.rotateSpeed = 0.45;
    controls.minPolarAngle = 0.15;
    controls.maxPolarAngle = Math.PI * 0.88;
    controls.zoomSpeed = 0.7;
    controls.enablePan = true;
    controls.screenSpacePanning = true;
    controls.enableRotate = false;
    controls.mouseButtons.LEFT = THREE.MOUSE.PAN;
    controls.mouseButtons.RIGHT = THREE.MOUSE.PAN;
    controls.touches.ONE = THREE.TOUCH.PAN;
    controls.touches.TWO = THREE.TOUCH.DOLLY_PAN;
    controls.minZoom = 0.55;
    controls.maxZoom = 2.8;
    scene.add(new THREE.HemisphereLight(0xffffff, 0x8296a3, 0.4));
    const light = new THREE.DirectionalLight(0xfff8ef, 2.2);
    light.position.set(-30, 20, 100);
    light.castShadow = true;
    light.shadow.mapSize.set(1024, 1024);
    Object.assign(light.shadow.camera, { left: -55, right: 55, top: 55, bottom: -55, near: 1, far: 220 });
    light.shadow.bias = -0.0003;
    light.shadow.normalBias = 0.12;
    light.shadow.radius = 7;
    light.shadow.blurSamples = 8;
    scene.add(light);
    const fill = new THREE.DirectionalLight(0xffffff, 0.45);
    fill.position.set(40, -60, 30);
    scene.add(fill);
    let raf = 0;
    let center = new THREE.Vector3();
    let loaded = false;
    const point = new THREE.Vector3();
    let width = 0,
      height = 0;
    const updateMarkers = () => {
      if (!loaded || !markerLayer.value) return;
      const buttons = markerLayer.value.children;
      props.markers.forEach((m, i) => {
        const button = buttons[i] as HTMLElement;
        if (!button) return;
        point
          .set((m.x / 100 - 0.5) * 41, (0.5 - m.y / 100) * 60, 4)
          .sub(center)
          .project(camera);
        button.style.transform = `translate3d(${(point.x * 0.5 + 0.5) * width - 22}px, ${(-point.y * 0.5 + 0.5) * height - 22}px, 0)`;
        button.style.visibility =
          point.z < 1 && Math.abs(point.x) < 0.95 && Math.abs(point.y) < 0.94
            ? "visible"
            : "hidden";
      });
    };
    let lastFrame = 0;
    const frameTimes: number[] = [];
    const render = () => {
      const frameStart = performance.now();
      if (lastFrame && frameStart - lastFrame < 100)
        frameTimes.push(frameStart - lastFrame);
      lastFrame = frameStart;
      if (frameTimes.length > 120) frameTimes.shift();
      raf = 0;
      if (disposed || !props.active || document.hidden) return;
      controls.update();
      renderer.render(scene, camera);
      updateMarkers();
      if (host.value) {
        host.value.dataset.frameSamples = JSON.stringify(
          frameTimes.map((v) => Math.round(v * 10) / 10),
        );
        host.value.dataset.renderMs = String(
          Math.round((performance.now() - frameStart) * 10) / 10,
        );
        host.value.dataset.drawCalls = String(renderer.info.render.calls);
        host.value.dataset.triangles = String(renderer.info.render.triangles);
      }
    };
    const requestRender = () => {
      if (!raf && !disposed && props.active && !document.hidden)
        raf = requestAnimationFrame(render);
    };
    const resize = () => {
      if (!host.value || !props.active) return;
      const w = host.value.clientWidth,
        h = host.value.clientHeight;
      if (!w || !h) return;
      width = w;
      height = h;
      renderer.setSize(w, h);
      const aspect = w / h;
      const span = aspect < 1 ? 46 / aspect : 46;
      camera.left = -span * aspect;
      camera.right = span * aspect;
      camera.top = span;
      camera.bottom = -span;
      camera.updateProjectionMatrix();
      requestRender();
    };
    activate = () => {
      resize();
      requestRender();
    };
    const visibility = () => requestRender();
    document.addEventListener("visibilitychange", visibility);
    controls.addEventListener("change", requestRender);
    const obs = new ResizeObserver(resize);
    obs.observe(host.value!);
    cleanup = () => {
      if (raf) cancelAnimationFrame(raf);
      obs.disconnect();
      document.removeEventListener("visibilitychange", visibility);
      controls.dispose();
      scene.traverse((o: any) => {
        o.geometry?.dispose();
        if (o.material)
          (Array.isArray(o.material) ? o.material : [o.material]).forEach(
            (m: any) => m.dispose(),
          );
      });
      environment.dispose();
      renderer.dispose();
      renderer.domElement.remove();
    };
    const loader = new GLTFLoader().setMeshoptDecoder(MeshoptDecoder);
    const gltf = await loader.loadAsync(
      withBase("/hardware/osrcore.glb"),
      (e) => {
        if (e.total) progress.value = Math.round((e.loaded / e.total) * 100);
      },
    );
    if (disposed) {
      gltf.scene.traverse((o: any) => {
        o.geometry?.dispose();
        if (o.material)
          (Array.isArray(o.material) ? o.material : [o.material]).forEach(
            (m: any) => m.dispose(),
          );
      });
      return;
    }
    gltf.scene.scale.setScalar(1000);
    const box = new THREE.Box3().setFromObject(gltf.scene);
    if (box.isEmpty()) throw Error("Empty model");
    center = box.getCenter(new THREE.Vector3());
    gltf.scene.position.sub(center);
    const board = new THREE.Group();
    board.add(gltf.scene);
    scene.add(board);
    gltf.scene.traverse((o: any) => {
      if (o.isMesh) {
        o.castShadow = true;
        o.receiveShadow = true;
        // STEP exports colors but no physical materials; glTF defaults all surfaces to metal.
        const materials = Array.isArray(o.material) ? o.material : [o.material];
        materials.forEach((material: any) => {
          material.envMapIntensity = 0.65;
          if (!material.name.startsWith("mat_")) return;
          const id = Number(material.name.replace("mat_", ""));
          const metals = [0, 2, 6, 8, 10, 14, 16, 19, 21, 22, 23, 25, 28, 32, 33, 35, 37];
          material.metalness = metals.includes(id) ? 0.78 : 0;
          material.roughness = metals.includes(id) ? 0.38 : 0.72;
          material.envMapIntensity = 0.65;
          if (!metals.includes(id) && material.color.r < 0.1 && material.color.g < 0.1 && material.color.b < 0.1) material.color.multiplyScalar(0.4);
        });
        o.matrixAutoUpdate = false;
        o.updateMatrix();
      }
    });
    const floor = new THREE.Mesh(new THREE.PlaneGeometry(220, 220), new THREE.ShadowMaterial({ opacity: 0.18 }));
    floor.position.z = box.min.z - center.z - 0.6;
    floor.receiveShadow = true;
    scene.add(floor);
    renderer.shadowMap.needsUpdate = true;
    const ledMaterial = new THREE.MeshBasicMaterial({ color: 0x171d24 });
    const led = new THREE.Mesh(new THREE.SphereGeometry(1, 12, 8), ledMaterial);
    led.position.set((0.93 - 0.5) * 41, (0.5 - 0.88) * 60, 4.4).sub(center);
    board.add(led);
    setLed = (color) => {
      ledMaterial.color.set(color);
      requestRender();
    };
    setLed(props.ledColor || "#171d24");
    loaded = true;
    reset = () => {
      camera.up.set(0, 0, 1);
      camera.position.set(72, -64, 108);
      camera.zoom = 1;
      camera.updateProjectionMatrix();
      controls.target.set(0, 0, 0);
      controls.update();
      resize();
    };
    zoom = (f) => {
      camera.zoom = THREE.MathUtils.clamp(camera.zoom / f, 0.55, 2.8);
      camera.updateProjectionMatrix();
      controls.update();
      requestRender();
    };
    rotate = (angle) => {
      camera.position.sub(controls.target).applyAxisAngle(camera.up, angle).add(controls.target);
      controls.update();
      requestRender();
    };
    flip = (value) => {
      side.value = value;
      board.rotation.x = value === "back" ? Math.PI : 0;
      renderer.shadowMap.needsUpdate = true;
      requestRender();
    };
    reset();
    loading.value = false;
    nextTick(requestRender);
    host.value!.dataset.modelLoadMs = String(
      Math.round(performance.now() - started),
    );
  } catch {
    if (!disposed) {
      failed.value = true;
      loading.value = false;
      cleanup();
    }
  }
});
onUnmounted(() => {
  disposed = true;
  cleanup();
});
</script>
<template>
  <div class="model-view">
    <div v-if="!failed" ref="host" class="model-canvas"></div>
    <div v-if="loading || failed" class="model-fallback"><BoardPhoto angle="angle" :en="en" /></div>
    <div ref="markerLayer" v-show="!loading && !failed && side === 'front'" class="model-markers">
      <button
        v-for="m in markers"
        :key="m.name"
        class="model-marker"
        :class="{ 'is-selected': selected === m.name }"
        :aria-label="m.name + ' · ' + m.pin"
        :aria-pressed="selected === m.name"
        @click="emit('select', m.name)"
      >
        <span
          class="marker-dot"
          :style="
            m.name === 'RGB LED' && ledColor
              ? { backgroundColor: ledColor, boxShadow: '0 0 12px ' + ledColor }
              : {}
          "
        ></span
        ><span class="marker-label">{{ m.name }}</span>
      </button>
    </div>
    <div v-if="loading || failed" class="model-message" role="status">
      {{
        failed
          ? en
            ? "3D unavailable · board image shown"
            : "三维暂不可用 · 已显示板卡图片"
          : en
            ? "Loading 3D model"
            : "正在加载三维模型"
      }}<span v-if="loading">{{ progress }}%</span>
    </div>
    <div v-if="!failed && !loading" class="model-side-switch" :aria-label="en ? 'Board side' : '板卡面向'">
      <button @click="flip('front')" :aria-pressed="side === 'front'">{{ en ? 'Front' : '正面' }}</button>
      <button @click="flip('back')" :aria-pressed="side === 'back'">{{ en ? 'Back' : '背面' }}</button>
    </div>
    <div
      v-if="!failed && !loading"
      class="model-tools"
      :aria-label="en ? '3D controls' : '三维控制'"
    >
      <button
        @click="rotate(-0.25)"
        :aria-label="en ? 'Rotate left' : '向左旋转'"
      >
        <RotateCcw /></button
      ><button
        @click="rotate(0.25)"
        :aria-label="en ? 'Rotate right' : '向右旋转'"
      >
        <RotateCw /></button
      ><span class="tool-separator"></span
      ><button @click="zoom(1.15)" :aria-label="en ? 'Zoom out' : '缩小'">
        <ZoomOut /></button
      ><button @click="zoom(0.87)" :aria-label="en ? 'Zoom in' : '放大'">
        <ZoomIn /></button
      ><span class="tool-separator"></span
      ><button @click="reset()" :aria-label="en ? 'Reset view' : '复位视角'">
        <Scan />
      </button>
    </div>
    <div class="stage-hint">
      <MousePointer2 /><span>{{
        en ? "Drag to pan · Buttons to rotate · Scroll to zoom" : "拖动平移 · 按钮旋转 · 滚轮缩放"
      }}</span>
    </div>
  </div>
</template>
