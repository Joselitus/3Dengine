// Web viewer for the engine's .scene files (see docs/ARCHITECTURE.md).
//
// It reads the same scene file and assets as the game and reproduces its
// shading (src/shaders/animatedshader.vert + src/shaders/shader.frag), so the scene can be
// inspected without building or running the engine. Served by serve.py.
//
// Query parameters:
//   scene=<path from the repo root>   default assets/scenes/desert.scene
//   view=orbit|top|player             initial camera
//   shot=1                            render once everything is loaded and
//                                     POST the PNG to /__shot (serve.py --shot)

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { OBJLoader } from 'three/addons/loaders/OBJLoader.js';
import { MTLLoader } from 'three/addons/loaders/MTLLoader.js';
import { FBXLoader } from 'three/addons/loaders/FBXLoader.js';

// The engine works with raw colour values, without sRGB conversions
THREE.ColorManagement.enabled = false;

const params = new URLSearchParams(location.search);
const ROOT = '../../';
const SCENE_PATH = params.get('scene') || 'assets/scenes/desert.scene';
const ASSETS = ROOT + 'assets/';
const SHOT = params.has('shot');

// Same constants as the engine
const FOV = 45;                  // Camera.cpp, perspective()
const SENSIVILITY = 0.005;       // Camera.h
const MAX_PITCH_PIXELS = 250;    // Controller.cpp
const PLAYER_HEIGHT = 1.8;       // AnimatedModel::computeFit
const BREATH_AMPLITUDE = 2.0;    // SceneStage::BREATH_AMPLITUDE
const LIGHT_DISTANCE = 100;      // test.cpp, the moon is a far point light
const PROP_SINK = 0.05;          // SceneStage::PROP_SINK, see desert.scene

// ------------------------------------------------------------ scene file
// Mirror of SceneFile::load (src/world/SceneFile.cpp). Keep both in sync.
function parseScene(text, path) {
  const scene = {
    moon: new THREE.Vector3(0, 1, 0), light: new THREE.Vector3(1, 1, 1),
    fog: new THREE.Vector3(0, 0, 0), sky: '', player: '', floor: null,
    playerPosition: new THREE.Vector3(), playerOnGround: false,
    cameraDistance: 4, cameraHeight: 0.8, objects: [],
  };
  const lines = text.split('\n');
  for (let n = 0; n < lines.length; n++) {
    const where = `${path}:${n + 1}`;
    const f = lines[n].replace(/#.*/, '').trim().split(/\s+/).filter(Boolean);
    if (!f.length) continue;
    const [command, ...args] = f;
    const nums = (from, count) => {
      const v = args.slice(from, from + count).map(Number);
      if (v.length < count || v.some(Number.isNaN))
        throw new Error(`${where}: wrong arguments for '${command}'`);
      return v;
    };
    // A height: a number, or "ground" (stand on the floor)
    const height = (i) => {
      if (args[i] === 'ground') return { y: 0, onGround: true };
      const y = Number(args[i]);
      if (args[i] === undefined || Number.isNaN(y))
        throw new Error(`${where}: wrong arguments for '${command}'`);
      return { y, onGround: false };
    };
    const str = (i) => {
      if (args[i] === undefined) throw new Error(`${where}: wrong arguments for '${command}'`);
      return args[i];
    };
    switch (command) {
      case 'moon': scene.moon.fromArray(nums(0, 3)).normalize(); break;
      case 'light': scene.light.fromArray(nums(0, 3)); break;
      case 'fog': scene.fog.fromArray(nums(0, 3)); break;
      case 'time_of_day': case 'day_duration': break; // the viewer has no clock
      case 'sky': scene.sky = str(0); break;
      case 'floor':
        scene.floor = { model: str(0), position: new THREE.Vector3().fromArray(nums(1, 3)),
          yaw: 0, scale: 1, effect: 'lit', onGround: false, line: n + 1 };
        break;
      case 'player': {
        scene.player = str(0);
        const h = height(2);
        scene.playerPosition.set(nums(1, 1)[0], h.y, nums(3, 1)[0]);
        scene.playerOnGround = h.onGround;
        break;
      }
      case 'camera': [scene.cameraDistance, scene.cameraHeight] = nums(0, 2); break;
      case 'object': {
        const [x] = nums(1, 1);
        const { y, onGround } = height(2);
        const [z, yaw, scale] = nums(3, 3);
        const effect = args[6] || 'lit';
        if (!['lit', 'emissive', 'breathe'].includes(effect))
          throw new Error(`${where}: unknown effect '${effect}'`);
        scene.objects.push({ model: str(0), position: new THREE.Vector3(x, y, z),
          onGround, yaw, scale, effect, line: n + 1 });
        break;
      }
      default: throw new Error(`${where}: unknown command '${command}'`);
    }
  }
  return scene;
}

// ------------------------------------------------------------ shading
// Uniforms shared by every material, like the engine's single shader.
const globals = {
  lightColor: { value: new THREE.Vector3() },
  lightPosition: { value: new THREE.Vector3() },
  fogColor: { value: new THREE.Vector3() },
  moonDir: { value: new THREE.Vector3() },
  time: { value: 0 },
  fogOn: { value: 1 },
};

// Port of src/shaders/animatedshader.vert. The skinning comes from three.js; the
// fit of AnimatedModel is applied to the player's group instead.
const vertexShader = /* glsl */`
#include <common>
#include <skinning_pars_vertex>
uniform float breathAmp;
uniform float breathTime;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec2 vUv;

vec3 breathe(vec3 p, vec3 n) {
  float w = 1.5 * breathTime;
  float s = 0.5 + 0.5 * sin(w - 0.55 * sin(w));
  float chest = smoothstep(1.8, 2.3, p.y) * (1.0 - smoothstep(2.9, 3.2, p.y));
  chest *= 1.0 - smoothstep(0.35, 0.60, abs(p.x));
  float upper = smoothstep(2.3, 3.0, p.y);
  vec3 d = n * (0.050 * chest * s);
  d.y += 0.050 * upper * s;
  d.z += -0.025 * upper * s;
  float arm = smoothstep(0.35, 0.60, abs(p.x)) * smoothstep(0.0, 2.9, p.y);
  d.z += 0.020 * arm * sin(w - 0.8);
  return p + breathAmp * d;
}

void main() {
  #include <beginnormal_vertex>
  #include <skinbase_vertex>
  #include <skinnormal_vertex>
  #include <begin_vertex>
  #include <skinning_vertex>
  if (breathAmp > 0.0) transformed = breathe(position, normal);
  vec4 world = modelMatrix * vec4(transformed, 1.0);
  vWorld = world.xyz;
  vNormal = mat3(modelMatrix) * objectNormal;
  vUv = uv;
  gl_Position = projectionMatrix * viewMatrix * world;
}`;

// Port of src/shaders/shader.frag (mode = the `unlit` uniform)
const fragmentShader = /* glsl */`
uniform sampler2D map;
uniform int mode;
uniform vec3 lightColor;
uniform vec3 lightPosition;
uniform vec3 fogColor;
uniform vec3 moonDir;
uniform float time;
uniform int fogOn;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec2 vUv;

float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
  vec3 c = texture2D(map, vUv).rgb;
  if (mode == 2) { gl_FragColor = vec4(c, 1.0); return; }
  if (mode == 1) {
    vec3 dir = normalize(vWorld - cameraPosition);
    float star = smoothstep(0.35, 0.9, max(c.r, max(c.g, c.b)));
    float away = 1.0 - smoothstep(0.985, 0.995, dot(dir, moonDir));
    float phase = hash(floor(vUv * vec2(1024.0, 512.0))) * 6.2831;
    float tw = 1.0 - 0.45 * star * away * (0.5 + 0.5 * sin(time * 2.5 + phase));
    gl_FragColor = vec4(c * tw, 1.0);
    return;
  }
  vec3 lightdir = normalize(lightPosition);
  vec3 norm = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  vec3 reflectDir = reflect(-lightdir, norm);
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);
  vec3 light = 0.3 * lightColor + max(dot(norm, lightdir), 0.0) * lightColor
             + 0.15 * spec * lightColor;
  vec3 lit = c * light;
  float fog = fogOn == 1 ? smoothstep(30.0, 70.0, distance(vWorld, cameraPosition)) : 0.0;
  gl_FragColor = vec4(mix(lit, fogColor, fog), 1.0);
}`;

const MODES = { lit: 0, sky: 1, emissive: 2, breathe: 0 };
const whiteTexture = new THREE.DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1);
whiteTexture.needsUpdate = true;

function makeMaterial(map, effect) {
  if (map) {
    map.colorSpace = THREE.NoColorSpace; // sample raw values, like the engine
    map.needsUpdate = true;
  }
  return new THREE.ShaderMaterial({
    vertexShader, fragmentShader,
    uniforms: {
      ...globals,
      map: { value: map || whiteTexture },
      mode: { value: MODES[effect] },
      breathAmp: { value: effect === 'breathe' ? BREATH_AMPLITUDE : 0 },
      breathTime: globals.time,
    },
  });
}

// ------------------------------------------------------------ loading
const objCache = new Map();

// Loads an OBJ (with its MTL) once; returns a promise of the template object.
function loadObj(path) {
  if (!objCache.has(path)) {
    const url = ASSETS + path;
    const dir = url.slice(0, url.lastIndexOf('/') + 1);
    const promise = fetch(url).then(r => {
      if (!r.ok) throw new Error(`${path}: ${r.status} ${r.statusText}`);
      return r.text();
    }).then(async text => {
      const mtl = /^mtllib\s+(.+)$/m.exec(text);
      const loader = new OBJLoader();
      if (mtl) {
        const materials = await new MTLLoader().setPath(dir).loadAsync(mtl[1].trim());
        materials.preload();
        loader.setMaterials(materials);
      }
      return loader.parse(text);
    });
    objCache.set(path, promise);
  }
  return objCache.get(path);
}

// Replaces the loader's materials (one or an array) by the engine's shading
function replaceMaterials(mesh, effect) {
  const convert = (m) => makeMaterial(m && m.map, effect);
  mesh.material = Array.isArray(mesh.material) ? mesh.material.map(convert) : convert(mesh.material);
}
const materialsOf = (mesh) => [].concat(mesh.material);

// Copy of a template whose meshes use the engine's shading with `effect`.
function instantiate(template, effect) {
  const copy = template.clone();
  copy.traverse(o => { if (o.isMesh) replaceMaterials(o, effect); });
  return copy;
}

// The player is fitted like AnimatedModel::computeFit: sample the animation,
// take the bounding box, scale its biggest side to PLAYER_HEIGHT and centre it.
async function loadPlayer(path) {
  const url = ASSETS + path;
  const fbx = await new FBXLoader().setResourcePath(url.slice(0, url.lastIndexOf('/') + 1))
    .loadAsync(url);
  const toRemove = [];
  fbx.traverse(o => {
    if (!o.isMesh) return;
    // Like AnimatedModel::processNode: drop unskinned, untextured leftovers
    if (!o.isSkinnedMesh && !materialsOf(o).some(m => m && m.map)) { toRemove.push(o); return; }
    replaceMaterials(o, 'lit');
    o.frustumCulled = false;
  });
  toRemove.forEach(o => o.removeFromParent());

  const mixer = new THREE.AnimationMixer(fbx);
  const clip = fbx.animations[0];
  if (clip) mixer.clipAction(clip).play();

  const box = new THREE.Box3();
  const samples = clip && clip.duration > 0 ? 16 : 1;
  for (let s = 0; s < samples; s++) {
    if (clip) mixer.setTime(clip.duration * s / samples);
    fbx.updateMatrixWorld(true);
    box.expandByObject(fbx, true);
  }
  mixer.setTime(0);
  const size = box.getSize(new THREE.Vector3());
  const biggest = Math.max(size.x, size.y, size.z);
  const scale = biggest > 0 ? PLAYER_HEIGHT / biggest : 1;
  const center = box.getCenter(new THREE.Vector3());
  fbx.scale.multiplyScalar(scale);
  fbx.position.sub(center.multiplyScalar(scale));

  const group = new THREE.Group();
  group.add(fbx);
  return { group, mixer };
}

// ------------------------------------------------------------ three.js setup
const renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: SHOT });
renderer.outputColorSpace = THREE.LinearSRGBColorSpace;
renderer.setPixelRatio(window.devicePixelRatio);
renderer.setSize(innerWidth, innerHeight);
document.body.prepend(renderer.domElement);

const camera = new THREE.PerspectiveCamera(FOV, innerWidth / innerHeight, 0.1, 1000);
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

const world = new THREE.Scene();
const helpers = new THREE.Group();
helpers.add(new THREE.GridHelper(160, 160, 0x4a5a8a, 0x253050));
helpers.add(new THREE.AxesHelper(5));
helpers.visible = false;
world.add(helpers);

let state = null;      // everything built from the current scene file
let selected = null;   // { entry, object, box }
let view = 'orbit';
const look = { phi: 0, theta: 0 }; // player camera, in cursor pixels like Controller

const $ = (id) => document.getElementById(id);
const status = { cursor: '', selection: '', error: '' };
function showStatus() {
  $('status').innerHTML = [status.error && `<span class="error">${status.error}</span>`,
    status.selection, status.cursor].filter(Boolean).join('\n') || 'Listo.';
}

async function build(text) {
  const desc = parseScene(text, SCENE_PATH);
  const root = new THREE.Group();
  const entries = [];

  // The floor is shown (and listed) like any other object
  const all = desc.floor ? [desc.floor, ...desc.objects] : desc.objects;
  const pending = all.map(async (o, i) => {
    const object = instantiate(await loadObj(o.model), o.effect);
    object.position.copy(o.position);
    object.rotation.y = o.yaw;
    object.scale.setScalar(o.scale);
    object.userData.entry = i;
    entries[i] = { ...o, object };
    root.add(object);
  });

  let sky = null;
  if (desc.sky) pending.push(loadObj(desc.sky).then(t => {
    sky = instantiate(t, 'sky');
    sky.traverse(m => {
      if (!m.isMesh) return;
      materialsOf(m).forEach(mat => { mat.depthTest = mat.depthWrite = false; });
      m.renderOrder = -1;
      m.frustumCulled = false;
    });
    root.add(sky);
  }));

  // The player model is reused between reloads, it is slow to fit
  let player = state && state.desc.player === desc.player ? state.player : null;
  if (desc.player && !player)
    pending.push(loadPlayer(desc.player).then(p => { player = p; }));

  await Promise.all(pending);

  // "ground" heights, like SceneStage: the floor right below (x, z)
  const floor = desc.floor ? entries[0].object : null;
  if (floor) floor.updateMatrixWorld(true);
  const groundAt = (x, z) => {
    if (!floor) return 0;
    const ray = new THREE.Raycaster(new THREE.Vector3(x, 1e4, z), new THREE.Vector3(0, -1, 0));
    const hit = ray.intersectObject(floor, true)[0];
    return hit ? hit.point.y : desc.floor.position.y;
  };
  for (const e of entries)
    if (e.onGround) {
      e.position = e.position.clone().setY(groundAt(e.position.x, e.position.z) - PROP_SINK);
      e.object.position.copy(e.position);
    }
  if (player) {
    player.group.position.copy(desc.playerPosition);
    if (desc.playerOnGround)
      player.group.position.y = groundAt(desc.playerPosition.x, desc.playerPosition.z);
    root.add(player.group);
  }
  return { desc, root, entries, sky, player, text };
}

function apply(next) {
  if (state) world.remove(state.root);
  state = next;
  world.add(state.root);
  globals.moonDir.value.copy(state.desc.moon);
  globals.lightPosition.value.copy(state.desc.moon).multiplyScalar(LIGHT_DISTANCE);
  updateLighting();
  $('title').textContent = SCENE_PATH.split('/').pop();
  fillTable();
  select(selected ? selected.index : null);
}

function updateLighting() {
  if (!state) return;
  const day = $('opt-lit').checked;
  globals.lightColor.value.copy(day ? new THREE.Vector3(1, 0.97, 0.9) : state.desc.light);
  globals.fogColor.value.copy(state.desc.fog);
  globals.fogOn.value = $('opt-fog').checked ? 1 : 0;
  const bg = day ? new THREE.Vector3(0.45, 0.55, 0.7) : state.desc.fog;
  renderer.setClearColor(new THREE.Color(bg.x, bg.y, bg.z));
  if (state.sky) state.sky.visible = $('opt-sky').checked && !day;
}

function fillTable() {
  const rows = state.entries.map((e, i) =>
    `<tr data-index="${i}"><td>${e.line}</td><td>${e.model.split('/').pop()}</td>` +
    `<td>${fmt(e.position)}</td><td>${e.effect !== 'lit' ? e.effect : ''}</td></tr>`);
  $('object-table').innerHTML = '<tr><td>línea</td><td>modelo</td><td>x y z</td><td></td></tr>' +
    rows.join('');
}

const fmt = (v) => `${v.x.toFixed(2)} ${v.y.toFixed(2)} ${v.z.toFixed(2)}`;

function select(index) {
  if (selected) world.remove(selected.box);
  selected = null;
  document.querySelectorAll('#object-table tr[data-index]').forEach(tr =>
    tr.classList.toggle('selected', Number(tr.dataset.index) === index));
  if (index === null || !state.entries[index]) { status.selection = ''; showStatus(); return; }
  const e = state.entries[index];
  const box = new THREE.BoxHelper(e.object, 0xffd25a);
  world.add(box);
  selected = { index, box };
  status.selection = `Seleccionado (línea ${e.line}): object ${e.model}  ${fmt(e.position)}  ` +
    `yaw ${e.yaw}  escala ${e.scale}  ${e.effect}`;
  showStatus();
}

function focus(index) {
  const e = state.entries[index];
  const box = new THREE.Box3().setFromObject(e.object);
  const center = box.getCenter(new THREE.Vector3());
  const radius = Math.max(box.getSize(new THREE.Vector3()).length(), 2);
  setView('orbit');
  controls.target.copy(center);
  camera.position.copy(center).add(new THREE.Vector3(radius, radius * 0.6, radius));
}

// ------------------------------------------------------------ cameras
function setView(name) {
  view = name;
  document.querySelectorAll('[data-view]').forEach(b =>
    b.classList.toggle('active', b.dataset.view === name));
  controls.enabled = name !== 'player';
  // From high above, the fog (30 to 70 units from the camera) hides everything
  $('opt-fog').checked = name !== 'top';
  updateLighting();
  if (name === 'top') {
    controls.target.set(0, 0, -14);
    camera.position.set(0, 48, -13.99);
  } else if (name === 'orbit') {
    controls.target.set(0, 0, -10);
    camera.position.set(22, 16, 14);
  } else {
    look.phi = look.theta = 0;
  }
}

// Same placement as Camera::follow, with the rotation of Camera::rotate
function updatePlayerCamera() {
  const target = state && state.player ? state.player.group.position : new THREE.Vector3();
  const { cameraDistance, cameraHeight } = state ? state.desc : { cameraDistance: 4, cameraHeight: 0.8 };
  camera.rotation.set(-SENSIVILITY * look.theta, -SENSIVILITY * look.phi, 0, 'YXZ');
  const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
  camera.position.copy(target).add(new THREE.Vector3(0, cameraHeight, 0))
    .addScaledVector(forward, -cameraDistance);
}

let dragging = null;
renderer.domElement.addEventListener('pointerdown', e => {
  dragging = { x: e.clientX, y: e.clientY, moved: false };
});
addEventListener('pointerup', e => {
  if (dragging && !dragging.moved && e.target === renderer.domElement) pick(e);
  dragging = null;
});
renderer.domElement.addEventListener('pointermove', e => {
  if (dragging) {
    const dx = e.clientX - dragging.x, dy = e.clientY - dragging.y;
    if (Math.abs(dx) + Math.abs(dy) > 3) dragging.moved = true;
    if (view === 'player') {
      look.phi += e.movementX;
      look.theta = THREE.MathUtils.clamp(look.theta + e.movementY, -MAX_PITCH_PIXELS, MAX_PITCH_PIXELS);
    }
  }
  hover(e);
});

// ------------------------------------------------------------ picking
const raycaster = new THREE.Raycaster();
function rayFrom(e) {
  const p = new THREE.Vector2(e.clientX / innerWidth * 2 - 1, -(e.clientY / innerHeight) * 2 + 1);
  raycaster.setFromCamera(p, camera);
  const targets = state ? state.entries.map(en => en.object) : [];
  return raycaster.intersectObjects(targets, true)[0];
}
function entryOf(object) {
  for (let o = object; o; o = o.parent)
    if (o.userData.entry !== undefined) return o.userData.entry;
  return null;
}
function pick(e) {
  const hit = rayFrom(e);
  select(hit ? entryOf(hit.object) : null);
}
let hoverEvent = null;
function hover(e) { hoverEvent = e; }
function updateHover() {
  if (!hoverEvent || !state) return;
  const hit = rayFrom(hoverEvent);
  hoverEvent = null;
  if (!hit) { status.cursor = ''; showStatus(); return; }
  const p = hit.point;
  const name = state.entries[entryOf(hit.object)].model;
  status.cursor = `Cursor: ${fmt(p)} sobre ${name}` +
    (name.includes('dunes') ? `   (y para un objeto aquí: ${(p.y - PROP_SINK).toFixed(2)})` : '');
  showStatus();
}

// ------------------------------------------------------------ UI
document.querySelectorAll('[data-view]').forEach(b =>
  b.addEventListener('click', () => setView(b.dataset.view)));
['opt-fog', 'opt-sky', 'opt-lit'].forEach(id => $(id).addEventListener('change', updateLighting));
$('opt-grid').addEventListener('change', () => { helpers.visible = $('opt-grid').checked; });
$('object-table').addEventListener('click', e => {
  const tr = e.target.closest('tr[data-index]');
  if (!tr) return;
  select(Number(tr.dataset.index));
  focus(Number(tr.dataset.index));
});
$('btn-reload').addEventListener('click', () => reload(true));
$('btn-shot').addEventListener('click', () => {
  renderer.render(world, camera);
  const a = document.createElement('a');
  a.download = 'escena.png';
  a.href = renderer.domElement.toDataURL('image/png');
  a.click();
});
addEventListener('resize', () => {
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(innerWidth, innerHeight);
});

async function reload(force) {
  try {
    const r = await fetch(ROOT + SCENE_PATH, { cache: 'no-store' });
    if (!r.ok) throw new Error(`${SCENE_PATH}: ${r.status} ${r.statusText}`);
    const text = await r.text();
    if (!force && state && text === state.text) return;
    apply(await build(text));
    status.error = '';
  } catch (err) {
    console.error(err);
    status.error = String(err.message || err);
  }
  showStatus();
}
setInterval(() => { if ($('opt-reload').checked && !SHOT) reload(false); }, 1000);

// ------------------------------------------------------------ main loop
const clock = new THREE.Clock();
function frame() {
  const dt = clock.getDelta();
  const animate = $('opt-anim').checked;
  if (animate) globals.time.value += dt;
  if (state && state.player && animate) state.player.mixer.update(dt);
  if (view === 'player') updatePlayerCamera(); else controls.update();
  // In first person the game doesn't draw the player (see Walker::attachCamera)
  if (state && state.player)
    state.player.group.visible = view !== 'player' || state.desc.cameraDistance > 0;
  if (state && state.sky) state.sky.position.copy(camera.position);
  if (selected) selected.box.update();
  updateHover();
  renderer.render(world, camera);
}

setView(params.get('view') || 'orbit');
await reload(true);
$('loading').remove();
showStatus();
renderer.setAnimationLoop(frame);

// Screenshot for serve.py --shot: a few frames so everything is uploaded
if (SHOT) {
  for (let i = 0; i < 10; i++) await new Promise(r => requestAnimationFrame(r));
  const blob = await new Promise(r => renderer.domElement.toBlob(r, 'image/png'));
  await fetch('/__shot', { method: 'POST', body: blob,
    headers: { 'X-Error': encodeURIComponent(status.error) } });
}
