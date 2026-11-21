import React, { useEffect, useRef, useState } from "react";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { TransformControls } from "three/examples/jsm/controls/TransformControls.js";
import { invoke } from "../bridge";

// HOD Types matching Rust backend
export interface Vector3D {
  x: number;
  y: number;
  z: number;
}

export interface Matrix4D {
  m: number[][];
}

export interface HODVertex {
  position: Vector3D;
  normal?: Vector3D;
  color?: number;
  uv?: { u: number; v: number };
  tangent?: Vector3D;
  binormal?: Vector3D;
}

export interface HODMeshPart {
  material_index: number;
  vertex_mask: number;
  vertices: HODVertex[];
  indices: number[];
}

export interface HODMesh {
  name: string;
  parent_name: string;
  lod: number;
  has_mult_tags?: boolean;
  parts: HODMeshPart[];
}

export interface HODJoint {
  name: string;
  parent_name?: string;
  local_transform: Matrix4D;
  position?: Vector3D;
  rotation?: Vector3D;
  scale?: Vector3D;
}

export interface HODMarker {
  name: string;
  parent_joint: string;
  position: Vector3D;
  rotation: Matrix4D;
}

export interface HODNavLight {
  name: string;
  section: number;
  size: number;
  phase: number;
  frequency: number;
  style: string;
  color: Vector3D;
  distance: number;
  sprite_visible: boolean;
  high_end_only: boolean;
}

export interface HODEngineBurn {
  name: string;
  parent_name: string;
  num_divisions: number;
  num_flames: number;
  vertices: Vector3D[];
}

export interface HODEngineGlow {
  name: string;
  parent_name: string;
  lod: number;
  mesh: HODMesh;
}

export interface HODEngineShape {
  name: string;
  parent_name: string;
  mesh: HODMesh;
}

export interface HODCollisionMesh {
  name: string;
  min_extents: Vector3D;
  max_extents: Vector3D;
  center: Vector3D;
  radius: number;
  mesh: HODMesh;
}

export interface HODDockpoint {
  position: Vector3D;
  rotation: Matrix4D;
  tolerance: number;
  max_speed: number;
  extra1?: number;
  extra2?: number;
}

export interface HODDockpath {
  name: string;
  parent_name: string;
  points: HODDockpoint[];
  val1?: number;
  val2?: number;
  val3?: number;
  val4?: number;
  val5?: number;
  compatible_ships?: string;
  padding1?: number;
  padding2?: string;
}

export interface HODTexture {
  name: string;
  width: number;
  height: number;
  format: string;
  png_preview?: string;
  png_data?: string;
  source_path?: string;
  legacy_storage_y_flipped?: boolean;
}

export interface HODKeyframe {
  time: number;
  position?: Vector3D;
  rotation?: { x: number; y: number; z: number; w: number };
  rotation_euler?: Vector3D;
  scale?: Vector3D;
}

export interface HODAnimationTrack {
  joint_name: string;
  keyframes: HODKeyframe[];
}

export interface HODAnimation {
  name: string;
  duration: number; // in seconds
  tracks: HODAnimationTrack[];
}
export interface HODMaterialParam {
  param_type: number;
  value: number[];
  name: string;
}
export interface HODMaterial {
  name: string;
  shader_name: string;
  texture_maps: string[];
  parameters: number[];
  params?: HODMaterialParam[];
}

export interface HODModel {
  version: number;
  is_v2: boolean;
  name: string;
  textures: HODTexture[];
  materials: HODMaterial[];
  meshes: HODMesh[];
  joints: HODJoint[];
  markers: HODMarker[];
  nav_lights: HODNavLight[];
  engine_burns: HODEngineBurn[];
  engine_glows: HODEngineGlow[];
  engine_shapes: HODEngineShape[];
  collision_meshes: HODCollisionMesh[];
  dockpaths: HODDockpath[];
  animations?: HODAnimation[];
  textures_modified: boolean;
}

const isFiniteVector = (v: any): boolean => {
  return (
    v &&
    typeof v.x === "number" && Number.isFinite(v.x) &&
    typeof v.y === "number" && Number.isFinite(v.y) &&
    typeof v.z === "number" && Number.isFinite(v.z)
  );
};

interface ViewportProps {
  model: HODModel | null;
  selectedNode: { type: string; name: string } | null;
  setSelectedNode: (node: { type: string; name: string } | null) => void;
  transformMode: "translate" | "rotate" | "scale";
  setTransformMode: (mode: "translate" | "rotate" | "scale") => void;
  coordinateSpace: "local" | "global";
  setCoordinateSpace: (space: "local" | "global") => void;
  onNodeTransform: (name: string, type: string, position: Vector3D, localMatrix?: Matrix4D, localQuat?: { x: number, y: number, z: number, w: number }, localEuler?: Vector3D) => void;
  visibleMeshes: Record<string, boolean>;
  showBoneLines: boolean;
  renderMode: "untextured" | "textured" | "shaded" | "wireframe" | "shaded_team" | "textured_team";
  teamColor?: string;
  stripeColor?: string;
  onModelChange?: (updatedModel: HODModel) => void;
  selectedAnimIdx: number;
  setSelectedAnimIdx: (idx: number) => void;
  isPlaying: boolean;
  setIsPlaying: (v: boolean) => void;
  currentTime: number;
  setCurrentTime: (v: number) => void;
  playbackSpeed: number;
  loopPlayback: boolean;
  targetBoxes?: any[];
}

export const Viewport: React.FC<ViewportProps> = ({
  model,
  selectedNode,
  setSelectedNode: _setSelectedNode,
  transformMode,
  setTransformMode,
  coordinateSpace,
  setCoordinateSpace,
  onNodeTransform,
  visibleMeshes,
  showBoneLines,
  renderMode,
  teamColor = "#4278a3",
  stripeColor = "#e5d94c",
  onModelChange,
  selectedAnimIdx,
  setSelectedAnimIdx: _setSelectedAnimIdx,
  isPlaying,
  setIsPlaying: _setIsPlaying,
  currentTime,
  setCurrentTime,
  playbackSpeed,
  loopPlayback,
  targetBoxes,
}) => {
  const mountRef = useRef<HTMLDivElement>(null);
  const transformControlsRef = useRef<TransformControls | null>(null);
  const orbitControlsRef = useRef<OrbitControls | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.PerspectiveCamera | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const markersGroupRef = useRef<THREE.Group | null>(null);
  const jointsGroupRef = useRef<THREE.Group | null>(null);
  const meshesGroupRef = useRef<THREE.Group | null>(null);
  
  const navLightsGroupRef = useRef<THREE.Group | null>(null);
  const collisionGroupRef = useRef<THREE.Group | null>(null);
  const dockpathsGroupRef = useRef<THREE.Group | null>(null);
  const engineBurnsGroupRef = useRef<THREE.Group | null>(null);
  const engineGlowsGroupRef = useRef<THREE.Group | null>(null);
  const engineShapesGroupRef = useRef<THREE.Group | null>(null);
  const targetBoxesGroupRef = useRef<THREE.Group | null>(null);

  const clockRef = useRef(new THREE.Clock());
  const navLightsAnimateRef = useRef<{ mesh: THREE.Mesh; material: THREE.MeshBasicMaterial; frequency: number; phase: number }[]>([]);
  
  const [isInitialized, setIsInitialized] = useState(false);

  // Animation timeline player state — now driven by props from App.tsx
  // Local state only for the create-anim modal inside Viewport (legacy, now removed)

  const isPlayingRef = useRef(isPlaying);
  const currentTimeRef = useRef(currentTime);
  const selectedAnimIdxRef = useRef(selectedAnimIdx);
  const playbackSpeedRef = useRef(playbackSpeed);
  const loopPlaybackRef = useRef(loopPlayback);

  useEffect(() => { isPlayingRef.current = isPlaying; }, [isPlaying]);
  useEffect(() => { currentTimeRef.current = currentTime; }, [currentTime]);
  useEffect(() => { selectedAnimIdxRef.current = selectedAnimIdx; }, [selectedAnimIdx]);
  useEffect(() => { playbackSpeedRef.current = playbackSpeed; }, [playbackSpeed]);
  useEffect(() => { loopPlaybackRef.current = loopPlayback; }, [loopPlayback]);

  const scaleFactorRef = useRef<number>(1.0);
  const selectionAxesHelperRef = useRef<THREE.AxesHelper | null>(null);
  const lastFocusedModelRef = useRef<string | null>(null);
  // Textures survive scene rebuilds: re-decoding every base64 PNG and
  // re-uploading it to the GPU dominated each visibility toggle / transform
  // commit. Keyed by texture name + flip; `src` detects replaced image data.
  const textureCacheRef = useRef<Map<string, { src: string; tex: THREE.Texture }>>(new Map());
  // Bumped after every scene rebuild so the render loop re-evaluates poses once.
  const sceneVersionRef = useRef(0);
  const lastAnimEvalRef = useRef<{ model: HODModel | null; key: string }>({ model: null, key: "" });

  const modelRef = useRef(model);
  // Read by the scene rebuild for initial visibility. Visibility changes are
  // applied by flipping `.visible` (see the visibility effect), never by
  // rebuilding the scene.
  const visibleMeshesRef = useRef(visibleMeshes);
  visibleMeshesRef.current = visibleMeshes;
  const onModelChangeRef = useRef(onModelChange);
  useEffect(() => {
    modelRef.current = model;
    onModelChangeRef.current = onModelChange;
  }, [model, onModelChange]);

  const selectedNodeRef = useRef(selectedNode);
  useEffect(() => {
    selectedNodeRef.current = selectedNode;
  }, [selectedNode]);

  const onNodeTransformRef = useRef(onNodeTransform);
  useEffect(() => {
    onNodeTransformRef.current = onNodeTransform;
  }, [onNodeTransform]);

  // Name -> joint index, static global matrices and per-animation track index,
  // rebuilt whenever App replaces the model object (it updates immutably).
  // Scene rebuilds, gizmo commits and animation frames all resolve joints
  // through here; the previous joints.find() per ancestor made them O(n^2).
  const jointCacheRef = useRef<{
    model: HODModel | null;
    byName: Map<string, HODJoint>;
    globals: Map<string, THREE.Matrix4>;
    tracks: WeakMap<object, Map<string, any>>;
  }>({ model: null, byName: new Map(), globals: new Map(), tracks: new WeakMap() });

  const getJointCache = () => {
    const currentModel = modelRef.current;
    if (jointCacheRef.current.model !== currentModel) {
      const byName = new Map<string, HODJoint>();
      currentModel?.joints.forEach(j => {
        const key = j.name.toLowerCase();
        if (!byName.has(key)) byName.set(key, j);
      });
      jointCacheRef.current = { model: currentModel, byName, globals: new Map(), tracks: new WeakMap() };
    }
    return jointCacheRef.current;
  };

  const findTrack = (anim: any, jointName: string) => {
    const cache = getJointCache();
    let byJoint = cache.tracks.get(anim);
    if (!byJoint) {
      byJoint = new Map();
      for (const t of anim.tracks || []) {
        const key = t.joint_name.toLowerCase();
        if (!byJoint.has(key)) byJoint.set(key, t);
      }
      cache.tracks.set(anim, byJoint);
    }
    return byJoint.get(jointName.toLowerCase());
  };

  const getAnimatedJointGlobalMatrix = (jointName: string, time: number, visited = new Set<string>(), memo?: Map<string, THREE.Matrix4>): THREE.Matrix4 => {
    const currentModel = modelRef.current;
    const memoKey = jointName.toLowerCase();
    const memoHit = memo?.get(memoKey);
    if (memoHit) return memoHit.clone();
    if (!currentModel || visited.has(memoKey)) {
      return new THREE.Matrix4();
    }
    const joint = getJointCache().byName.get(memoKey);
    if (!joint) return new THREE.Matrix4();

    const nextVisited = new Set(visited);
    nextVisited.add(jointName.toLowerCase());

    // If this joint is currently being actively dragged by the user, bypass keyframe 
    // interpolation and just use its current live visual transform as the global matrix.
    // This stops evaluateAnimation from snapping it back and allows children to follow it smoothly.
    const tc = transformControlsRef.current;
    if (tc && tc.dragging && tc.object) {
      const tcName = tc.object.name.toLowerCase();
      // Allow it if it's the matching joint, navlight, or marker
      if (tcName === `joint:${joint.name}`.toLowerCase() || 
          tcName === `navlight:${joint.name}`.toLowerCase()) {
        tc.object.updateMatrix();
        const dragged = tc.object.matrix.clone();
        if (memo && visited.size === 0) memo.set(memoKey, dragged.clone());
        return dragged;
      }
    }

    let parentMatrix = new THREE.Matrix4();
    if (joint.parent_name && joint.parent_name.toLowerCase() !== jointName.toLowerCase()) {
      parentMatrix = getAnimatedJointGlobalMatrix(joint.parent_name, time, nextVisited, memo);
    }

    let localMatrix = toThreeMatrix(joint.local_transform);

    const anims = currentModel?.animations || [];
    const activeAnim = anims[selectedAnimIdxRef.current];
    const isPlay = isPlayingRef.current;
    const selNode = selectedNodeRef.current;
    const isKeyframeActive = selNode?.type === "keyframe";

    // Only apply animation if playing or actively inspecting a keyframe.
    // This allows joints to reset to their natural coordinates when not animating/editing keyframes.
    if (activeAnim && (isPlay || isKeyframeActive)) {
      const track = findTrack(activeAnim, joint.name);
      if (track && track.keyframes.length > 0) {
        const keys = track.keyframes;
        let t1 = keys[0];
        let t2 = keys[keys.length - 1];

        for (let i = 0; i < keys.length - 1; i++) {
          if (time >= keys[i].time && time <= keys[i+1].time) {
            t1 = keys[i];
            t2 = keys[i+1];
            break;
          }
        }

        let alpha = 0;
        if (t2.time !== t1.time) {
          alpha = (time - t1.time) / (t2.time - t1.time);
        }
        alpha = Math.max(0, Math.min(1, alpha));

        const basePos = new THREE.Vector3();
        const baseQuat = new THREE.Quaternion();
        const baseScale = new THREE.Vector3();
        localMatrix.decompose(basePos, baseQuat, baseScale);

        const pos = basePos.clone();
        if (t1.position && t2.position) {
          pos.set(
            THREE.MathUtils.lerp(t1.position.x, t2.position.x, alpha),
            THREE.MathUtils.lerp(t1.position.y, t2.position.y, alpha),
            THREE.MathUtils.lerp(t1.position.z, t2.position.z, alpha)
          );
        }

        const quat = baseQuat.clone();
        if (t1.rotation_euler && t2.rotation_euler) {
          const rx = THREE.MathUtils.lerp(t1.rotation_euler.x, t2.rotation_euler.x, alpha);
          const ry = THREE.MathUtils.lerp(t1.rotation_euler.y, t2.rotation_euler.y, alpha);
          const rz = THREE.MathUtils.lerp(t1.rotation_euler.z, t2.rotation_euler.z, alpha);
          const euler = new THREE.Euler(rx, ry, rz, "YXZ");
          quat.setFromEuler(euler);
        } else if (t1.rotation && t2.rotation) {
          const q1 = new THREE.Quaternion(t1.rotation.x, t1.rotation.y, t1.rotation.z, t1.rotation.w).normalize();
          const q2 = new THREE.Quaternion(t2.rotation.x, t2.rotation.y, t2.rotation.z, t2.rotation.w).normalize();
          q1.slerp(q2, alpha).normalize();
          quat.copy(q1);
        }

        const scale = baseScale.clone();
        if (t1.scale && t2.scale) {
          scale.set(
            THREE.MathUtils.lerp(t1.scale.x, t2.scale.x, alpha),
            THREE.MathUtils.lerp(t1.scale.y, t2.scale.y, alpha),
            THREE.MathUtils.lerp(t1.scale.z, t2.scale.z, alpha)
          );
        }

        localMatrix = new THREE.Matrix4().compose(pos, quat, scale);
      }
    }

    const result = parentMatrix.clone().multiply(localMatrix);
    if (memo && visited.size === 0) memo.set(memoKey, result.clone());
    return result;
  };

  const evaluateAnimation = (mModel: HODModel, animIdx: number, time: number) => {
    const anims = mModel.animations || [];
    let activeAnim = anims[animIdx];

    if (!activeAnim) {
      return 4.0;
    }

    // One frame's worth of animated globals, shared by joints, meshes and helpers.
    const memo = new Map<string, THREE.Matrix4>();
    const jointMeshes = new Map<string, THREE.Object3D>();
    jointsGroupRef.current?.children.forEach(c => {
      const key = c.name.toLowerCase();
      if (!jointMeshes.has(key)) jointMeshes.set(key, c);
    });

    if (jointsGroupRef.current) {
      mModel.joints.forEach(joint => {
        const jointMesh = jointMeshes.get(`joint:${joint.name}`.toLowerCase()) as THREE.Mesh | undefined;
        if (jointMesh) {
          const tc = transformControlsRef.current;
          const isBeingDragged = tc && tc.dragging && tc.object === jointMesh;

          if (!isBeingDragged) {
            const animatedWorldMatrix = getAnimatedJointGlobalMatrix(joint.name, time, undefined, memo);
            const pos = new THREE.Vector3();
            const quat = new THREE.Quaternion();
            const scale = new THREE.Vector3();
            animatedWorldMatrix.decompose(pos, quat, scale);
            jointMesh.position.copy(pos);
            jointMesh.quaternion.copy(quat);
          }

          const activeNode = selectedNodeRef.current;
          const isSelected = activeNode && activeNode.type === "joint" && activeNode.name.toLowerCase() === joint.name.toLowerCase();
          if (isSelected) {
            jointMesh.scale.setScalar(1.8);
            (jointMesh.material as THREE.MeshBasicMaterial).color.set("#ffd600");
          } else {
            jointMesh.scale.setScalar(1.0);
            (jointMesh.material as THREE.MeshBasicMaterial).color.set("#00e676");
          }
        }
      });

      jointsGroupRef.current.children.forEach(child => {
        if (child.name.startsWith("bone:")) {
          const boneLine = child as THREE.Line;
          const parentName = boneLine.userData.parentJointName as string | undefined;
          const childName = boneLine.userData.childJointName as string | undefined;
          if (parentName && childName) {
            const parentMesh = jointMeshes.get(`joint:${parentName}`.toLowerCase()) as THREE.Mesh | undefined;
            const childMesh = jointMeshes.get(`joint:${childName}`.toLowerCase()) as THREE.Mesh | undefined;
            if (parentMesh && childMesh) {
              const positions = boneLine.geometry.attributes.position.array as Float32Array;
              positions[0] = childMesh.position.x;
              positions[1] = childMesh.position.y;
              positions[2] = childMesh.position.z;
              positions[3] = parentMesh.position.x;
              positions[4] = parentMesh.position.y;
              positions[5] = parentMesh.position.z;
              boneLine.geometry.attributes.position.needsUpdate = true;
            }
          }
        }
      });
    }

    if (meshesGroupRef.current) {
      meshesGroupRef.current.children.forEach(child => {
        const parentJointName = child.userData.parentJointName as string | undefined;
        if (!parentJointName) return;

        const animatedWorldMatrix = getAnimatedJointGlobalMatrix(parentJointName, time, undefined, memo);
        child.matrix.copy(animatedWorldMatrix);
        child.matrix.decompose(child.position, child.quaternion, child.scale);
        child.matrixWorldNeedsUpdate = true;
      });
    }

    const updateGroupChildren = (group: THREE.Group | null) => {
      if (!group) return;
      group.children.forEach(child => {
        const parentJointName = child.userData.parentJointName as string | undefined;
        const baseMatrix = child.userData.baseMatrix as THREE.Matrix4 | undefined;
        if (!parentJointName || !baseMatrix) return;

        const tc = transformControlsRef.current;
        const isBeingDragged = tc && tc.dragging && tc.object === child;

        if (!isBeingDragged) {
          const animatedWorldMatrix = getAnimatedJointGlobalMatrix(parentJointName, time, undefined, memo);
          const newMatrix = animatedWorldMatrix.clone().multiply(baseMatrix);
          child.matrix.copy(newMatrix);
          child.matrix.decompose(child.position, child.quaternion, child.scale);
        }
        
        // Prevent visual helper gizmos from becoming massive due to joint scale
        if (child.name.startsWith("dockpoint:") || 
            child.name.startsWith("marker:") || 
            child.name.startsWith("navlight:") || 
            child.name.startsWith("engine_burn:")) {
          child.scale.set(1, 1, 1);
        }

        child.matrixWorldNeedsUpdate = true;
      });
    };

    updateGroupChildren(markersGroupRef.current);
    updateGroupChildren(navLightsGroupRef.current);
    updateGroupChildren(dockpathsGroupRef.current);
    updateGroupChildren(engineBurnsGroupRef.current);
    updateGroupChildren(engineGlowsGroupRef.current);
    updateGroupChildren(engineShapesGroupRef.current);
    updateGroupChildren(collisionGroupRef.current);

    if (dockpathsGroupRef.current) {
      const dockPoints = new Map<string, THREE.Object3D>();
      dockpathsGroupRef.current.children.forEach(c => {
        if (c.name.startsWith("dockpoint:") && !dockPoints.has(c.name)) dockPoints.set(c.name, c);
      });
      dockpathsGroupRef.current.children.forEach(child => {
        if (child.name.startsWith("dockpath_line:")) {
          const pathLine = child as THREE.Line;
          const pathName = pathLine.userData.pathName as string | undefined;
          const pointCount = pathLine.userData.pointCount as number | undefined;
          if (pathName && pointCount) {
            const pointsInWorld: THREE.Vector3[] = [];
            for (let i = 0; i < pointCount; i++) {
              const ptMesh = dockPoints.get(`dockpoint:${pathName}:${i}`) as THREE.Mesh | undefined;
              if (ptMesh) {
                pointsInWorld.push(ptMesh.position);
              }
            }
            if (pointsInWorld.length > 1) {
              const positions = pathLine.geometry.attributes.position.array as Float32Array;
              pointsInWorld.forEach((pt, idx) => {
                positions[idx * 3] = pt.x;
                positions[idx * 3 + 1] = pt.y;
                positions[idx * 3 + 2] = pt.z;
              });
              pathLine.geometry.attributes.position.needsUpdate = true;
            }
          }
        }
      });
    }

    return activeAnim.duration;
  };

  // Helper to convert Matrix4D to THREE.Matrix4 with singular scale sanitization
  const toThreeMatrix = (matrix: Matrix4D): THREE.Matrix4 => {
    const m = matrix?.m;
    if (!m || m.length < 4 || m.some(row => !row || row.length < 4)) {
      invoke("log_event", { level: "ERROR", message: `toThreeMatrix check FAILED. matrix: ${JSON.stringify(matrix)}` }).catch(console.error);
      return new THREE.Matrix4();
    }

    // Check if the scale/basis vectors are zero (singular matrix), and restore them to identity if so
    let s0 = Math.sqrt(m[0][0]*m[0][0] + m[0][1]*m[0][1] + m[0][2]*m[0][2]);
    let s1 = Math.sqrt(m[1][0]*m[1][0] + m[1][1]*m[1][1] + m[1][2]*m[1][2]);
    let s2 = Math.sqrt(m[2][0]*m[2][0] + m[2][1]*m[2][1] + m[2][2]*m[2][2]);

    let r00 = m[0][0], r01 = m[0][1], r02 = m[0][2];
    let r10 = m[1][0], r11 = m[1][1], r12 = m[1][2];
    let r20 = m[2][0], r21 = m[2][1], r22 = m[2][2];

    if (s0 < 0.0001) { r00 = 1.0; r01 = 0.0; r02 = 0.0; }
    if (s1 < 0.0001) { r10 = 0.0; r11 = 1.0; r12 = 0.0; }
    if (s2 < 0.0001) { r20 = 0.0; r21 = 0.0; r22 = 1.0; }

    return new THREE.Matrix4().set(
      r00, r10, r20, m[3][0],
      r01, r11, r21, m[3][1],
      r02, r12, r22, m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    );
  };

  // Helper to decompose matrices safely with NaN/Infinity recovery
  const safeDecompose = (matrix: THREE.Matrix4, pos: THREE.Vector3, q: THREE.Quaternion, s: THREE.Vector3) => {
    matrix.decompose(pos, q, s);
    if (!Number.isFinite(pos.x) || !Number.isFinite(pos.y) || !Number.isFinite(pos.z)) {
      const te = matrix.elements;
      pos.set(te[12], te[13], te[14]);
    }
    if (!Number.isFinite(q.x) || !Number.isFinite(q.y) || !Number.isFinite(q.z) || !Number.isFinite(q.w)) {
      q.set(0, 0, 0, 1);
    }
    if (!Number.isFinite(s.x) || !Number.isFinite(s.y) || !Number.isFinite(s.z)) {
      s.set(1, 1, 1);
    }
  };

  // Helper to compute joint global transform matrix recursively (with stack-overflow protection!)
  const getJointGlobalMatrix = (jointName: string, visited = new Set<string>()): THREE.Matrix4 => {
    const currentModel = modelRef.current;
    if (!currentModel || visited.has(jointName.toLowerCase())) {
      return new THREE.Matrix4();
    }

    const cache = getJointCache();
    const key = jointName.toLowerCase();
    const cached = cache.globals.get(key);
    if (cached) return cached.clone();

    const joint = cache.byName.get(key);
    if (!joint) return new THREE.Matrix4();

    const localMatrix = toThreeMatrix(joint.local_transform);

    let result: THREE.Matrix4;
    if (joint.parent_name && joint.parent_name.toLowerCase() !== key) {
      const nextVisited = new Set(visited);
      nextVisited.add(key);
      const parentGlobalMatrix = getJointGlobalMatrix(joint.parent_name, nextVisited);
      result = parentGlobalMatrix.multiply(localMatrix);
    } else {
      result = localMatrix;
    }
    // Only cache top-level results so cycle-cut walks never pollute the cache.
    if (visited.size === 0) cache.globals.set(key, result.clone());
    return result;
  };

  // Helper to resolve helper world position to local position of parent joint
  const getLocalPosition = (obj: THREE.Object3D, selectedType: string, selectedName: string): Vector3D => {
    const currentModel = modelRef.current;
    let parentName = "";
    if (selectedType === "marker") {
      const marker = currentModel?.markers.find(m => m.name.toLowerCase() === selectedName.toLowerCase());
      parentName = marker?.parent_joint || "";
    } else if (selectedType === "joint" || selectedType === "engine_nozzle") {
      const joint = currentModel?.joints.find(j => j.name.toLowerCase() === selectedName.toLowerCase());
      parentName = joint?.parent_name || "";
    } else if (selectedType === "navlight") {
      const joint = currentModel?.joints.find(j => j.name.toLowerCase() === selectedName.toLowerCase());
      parentName = joint?.parent_name || "";
    } else if (selectedType === "weapon_group") {
      const joint = currentModel?.joints.find(j => j.name.toLowerCase() === `${selectedName}_Position`.toLowerCase());
      parentName = joint?.parent_name || "";
    } else if (selectedType === "dockpoint") {
      const [pathName] = selectedName.split(":");
      const path = currentModel?.dockpaths.find(p => p.name.toLowerCase() === pathName.toLowerCase());
      parentName = path?.parent_name || "";
    } else if (selectedType === "collision") {
      parentName = selectedName;
    } else if (selectedType === "engine_burn") {
      const burn = currentModel?.engine_burns.find(b => b.name.toLowerCase() === selectedName.toLowerCase());
      parentName = burn?.parent_name || "";
    } else if (selectedType === "engine_glow") {
      const glow = currentModel?.engine_glows.find(g => g.name.toLowerCase() === selectedName.toLowerCase());
      parentName = glow?.parent_name || "";
    } else if (selectedType === "engine_shape") {
      const shape = currentModel?.engine_shapes.find(s => s.name.toLowerCase() === selectedName.toLowerCase());
      parentName = shape?.parent_name || "";
    } else if (selectedType === "keyframe") {
      const jointName = selectedName.split(":")[0];
      const joint = currentModel?.joints.find(j => j.name.toLowerCase() === jointName.toLowerCase());
      parentName = joint?.parent_name || "";
    }

    if (parentName && parentName.toLowerCase() !== "root") {
      const parentMatrix = getJointGlobalMatrix(parentName);
      const invParent = parentMatrix.clone().invert();
      const localPos = obj.position.clone().applyMatrix4(invParent);
      return { x: localPos.x, y: localPos.y, z: localPos.z };
    }

    return { x: obj.position.x, y: obj.position.y, z: obj.position.z };
  };

  useEffect(() => {
    if (!mountRef.current) return;

    try {
      // 1. Initialize Scene, Camera, WebGLRenderer
      const width = mountRef.current.clientWidth || 800;
      const height = mountRef.current.clientHeight || 600;

      // Clean out any stale, duplicated, or frozen canvas elements
      mountRef.current.innerHTML = "";

      const scene = new THREE.Scene();
      scene.background = new THREE.Color("#03060a");
      sceneRef.current = scene;

      const camera = new THREE.PerspectiveCamera(50, width / height, 0.05, 100000);
      camera.position.set(20, 15, 30);
      cameraRef.current = camera;

      const renderer = new THREE.WebGLRenderer({ antialias: true });
      renderer.setSize(width, height);
      renderer.setPixelRatio(window.devicePixelRatio);
      rendererRef.current = renderer;
      mountRef.current.appendChild(renderer.domElement);

      const handlePrevent = (e: Event) => e.preventDefault();
      const container = mountRef.current;
      if (container) {
        container.addEventListener("dragstart", handlePrevent);
        container.addEventListener("selectstart", handlePrevent);
      }
      renderer.domElement.addEventListener("dragstart", handlePrevent);
      renderer.domElement.addEventListener("selectstart", handlePrevent);

      // 2. Add Cinematic Stellar Space Lighting (Distant Sun + Nebula Bounce/Fill)
      // Deep Space Ambient Fill
      const ambientLight = new THREE.AmbientLight("#080c14", 0.18);
      scene.add(ambientLight);

      // The Distant Sun (Bright, crisp neutral white stellar light)
      const sunLight = new THREE.DirectionalLight("#ffffff", 1.6);
      sunLight.position.set(150, 80, 100);
      scene.add(sunLight);

      // Cool Nebula Glow (Opposite fill light to softly illuminate shadowed areas)
      const nebulaFill = new THREE.DirectionalLight("#355c7d", 0.7);
      nebulaFill.position.set(-150, -40, -100);
      scene.add(nebulaFill);

      // Warm Dust Reflection (Bottom-up bounce light simulating stellar dust reflections)
      const dustBounce = new THREE.DirectionalLight("#ffd1a9", 0.3);
      dustBounce.position.set(0, -100, 0);
      scene.add(dustBounce);

      // 3. Add Grid and Axis Helpers (Adaptive Infinite Grids with Gray Slate lines)
      const gridFine = new THREE.GridHelper(200, 200, "#16a0ff", "#384e6e");
      // Safely duplicate the material properties without breaking the internal shader
      const fineMat = new THREE.LineBasicMaterial({
        color: 0xffffff,
        vertexColors: true,
        transparent: true,
        opacity: 0.45,
        depthTest: true
      });
      gridFine.material = fineMat;
      gridFine.renderOrder = 0;
      gridFine.position.y = 0;
      scene.add(gridFine);

      const gridCoarse = new THREE.GridHelper(2000, 200, "#16a0ff", "#25344a");
      const coarseMat = new THREE.LineBasicMaterial({
        vertexColors: true,
        transparent: true,
        opacity: 0.5,
        depthTest: true
      });
      gridCoarse.material = coarseMat;
      gridCoarse.renderOrder = 0;
      gridCoarse.position.y = 0;
      scene.add(gridCoarse);

      const gridMega = new THREE.GridHelper(20000, 200, "#16a0ff", "#182333");
      const megaMat = new THREE.LineBasicMaterial({
        vertexColors: true,
        transparent: true,
        opacity: 0.0,
        depthTest: true
      });
      gridMega.material = megaMat;
      gridMega.renderOrder = 0;
      gridMega.position.y = 0;
      scene.add(gridMega);

      const axes = new THREE.AxesHelper(5);
      scene.add(axes);

      // 4. Set up Camera Orbit Controls with Right-Click Rotation!
      const orbitControls = new OrbitControls(camera, renderer.domElement);
      orbitControls.enableDamping = false;
      orbitControls.mouseButtons = {
        MIDDLE: THREE.MOUSE.DOLLY,
        RIGHT: THREE.MOUSE.ROTATE, // Allow right click to rotate camera around the origin!
      } as any;
      orbitControlsRef.current = orbitControls;

      // 5. Set up Transform Controls for Visually Moving Joints/Markers
      const transformControls = new TransformControls(camera, renderer.domElement);
      transformControlsRef.current = transformControls;
      scene.add(transformControls.getHelper());

      // Prevent orbit rotation while transforming nodes
      transformControls.addEventListener("dragging-changed", (event) => {
        orbitControls.enabled = !event.value;
        if (!event.value) {
          // Drag finished. Commit transform to the global React state.
          commitTransform();
        }
      });

      const commitTransform = () => {
        const activeNode = selectedNodeRef.current;
        if (transformControls.object && activeNode) {
          const obj = transformControls.object;
          obj.updateMatrix();
          
          const localPos = getLocalPosition(obj, activeNode.type, activeNode.name);
          
          // Compute local matrix for rotation/scale
          const currentModel = modelRef.current;
          let parentName = "";
          if (activeNode.type === "marker") {
            const marker = currentModel?.markers.find(m => m.name.toLowerCase() === activeNode.name.toLowerCase());
            parentName = marker?.parent_joint || "";
          } else if (activeNode.type === "joint" || activeNode.type === "navlight" || activeNode.type === "engine_nozzle") {
            const joint = currentModel?.joints.find(j => j.name.toLowerCase() === activeNode.name.toLowerCase());
            parentName = joint?.parent_name || "";
          } else if (activeNode.type === "weapon_group") {
            const joint = currentModel?.joints.find(j => j.name.toLowerCase() === `${activeNode.name}_Position`.toLowerCase());
            parentName = joint?.parent_name || "";
          } else if (activeNode.type === "dockpoint") {
            const [pathName] = activeNode.name.split(":");
            const path = currentModel?.dockpaths.find(p => p.name.toLowerCase() === pathName.toLowerCase());
            parentName = path?.parent_name || "";
          } else if (activeNode.type === "engine_burn") {
            const burn = currentModel?.engine_burns.find(b => b.name.toLowerCase() === activeNode.name.toLowerCase());
            parentName = burn?.parent_name || "";
          } else if (activeNode.type === "keyframe") {
            const jointName = activeNode.name.split(":")[0];
            const joint = currentModel?.joints.find(j => j.name.toLowerCase() === jointName.toLowerCase());
            parentName = joint?.parent_name || "";
          }
          
          let localMat = obj.matrix.clone();
          if (parentName && parentName.toLowerCase() !== "root") {
            const parentMatrix = getJointGlobalMatrix(parentName);
            const invParent = parentMatrix.clone().invert();
            localMat = invParent.multiply(obj.matrix);
          }
          
          const dPos = new THREE.Vector3();
          const dQuat = new THREE.Quaternion();
          const dScale = new THREE.Vector3();
          safeDecompose(localMat, dPos, dQuat, dScale);
          const dEuler = new THREE.Euler().setFromQuaternion(dQuat, "YXZ");
          
          const localMatrix: Matrix4D = {
            m: [
              [localMat.elements[0], localMat.elements[1], localMat.elements[2], localMat.elements[3]],
              [localMat.elements[4], localMat.elements[5], localMat.elements[6], localMat.elements[7]],
              [localMat.elements[8], localMat.elements[9], localMat.elements[10], localMat.elements[11]],
              [localMat.elements[12], localMat.elements[13], localMat.elements[14], localMat.elements[15]],
            ]
          };

          if (onNodeTransformRef.current) {
            onNodeTransformRef.current(
              activeNode.name, 
              activeNode.type, 
              localPos, 
              localMatrix,
              { x: dQuat.x, y: dQuat.y, z: dQuat.z, w: dQuat.w },
              { x: dEuler.x, y: dEuler.y, z: dEuler.z }
            );
          }
        }
      };

      // We no longer trigger re-renders during active drag, because it rebuilds 
      // the scene and destroys the target object causing TransformControls to crash.
      transformControls.addEventListener("objectChange", () => {
        // Optional: you could update local React state for Inspector preview here 
        // without causing a global model change, but for now we just let the 3D 
        // viewport update itself until the drag ends.
      });

      const meshesGroup = new THREE.Group();
      const jointsGroup = new THREE.Group();
      const markersGroup = new THREE.Group();
      const navLightsGroup = new THREE.Group();
      const collisionGroup = new THREE.Group();
      const dockpathsGroup = new THREE.Group();
      const engineBurnsGroup = new THREE.Group();
      const engineGlowsGroup = new THREE.Group();
      const engineShapesGroup = new THREE.Group();
      const targetBoxesGroup = new THREE.Group();
      
      scene.add(meshesGroup);
      scene.add(jointsGroup);
      scene.add(markersGroup);
      scene.add(navLightsGroup);
      scene.add(collisionGroup);
      scene.add(dockpathsGroup);
      scene.add(engineBurnsGroup);
      scene.add(engineGlowsGroup);
      scene.add(engineShapesGroup);
      scene.add(targetBoxesGroup);

      meshesGroupRef.current = meshesGroup;
      jointsGroupRef.current = jointsGroup;
      markersGroupRef.current = markersGroup;
      navLightsGroupRef.current = navLightsGroup;
      collisionGroupRef.current = collisionGroup;
      dockpathsGroupRef.current = dockpathsGroup;
      engineBurnsGroupRef.current = engineBurnsGroup;
      engineGlowsGroupRef.current = engineGlowsGroup;
      engineShapesGroupRef.current = engineShapesGroup;
      targetBoxesGroupRef.current = targetBoxesGroup;

      // 7. Render Loop with NavLight Pulsing Animate Update
      const clock = clockRef.current;
      const playbackClock = new THREE.Clock();
      let animationId: number;
      const animate = () => {
        animationId = requestAnimationFrame(animate);
        orbitControls.update();

        // Adaptive grid toggling based on camera distance (zoom)
        // Ensure only one grid is active at a time to prevent clipping / z-fighting
        if (camera && fineMat && coarseMat && megaMat) {
          const dist = camera.position.distanceTo(orbitControls.target);
          
          if (dist < 150) {
            gridFine.visible = true;
            gridCoarse.visible = false;
            gridMega.visible = false;
            fineMat.opacity = 0.45;
          } else if (dist < 1500) {
            gridFine.visible = false;
            gridCoarse.visible = true;
            gridMega.visible = false;
            coarseMat.opacity = 0.5;
          } else {
            gridFine.visible = false;
            gridCoarse.visible = false;
            gridMega.visible = true;
            megaMat.opacity = 0.6;
          }
        }

        // Pulsing NavLights
        const time = clock.getElapsedTime();
        navLightsAnimateRef.current.forEach(({ mesh, material, frequency, phase }) => {
          if (frequency > 0) {
            const pulse = Math.sin(time * frequency * Math.PI * 2 + phase) * 0.4 + 0.6;
            material.opacity = pulse * 0.8;
          } else {
            material.opacity = 0.8;
          }
          mesh.scale.setScalar(1.0);
        });

        // Evaluate LERP/SLERP keyframe tracks
        if (modelRef.current) {
          const isPlay = isPlayingRef.current;
          const speed = playbackSpeedRef.current;
          const loop = loopPlaybackRef.current;
          let currTime = currentTimeRef.current;

          if (isPlay) {
            currTime += playbackClock.getDelta() * speed;
          } else {
            playbackClock.getDelta(); // keep delta clean
          }

          // Poses only change while playing, scrubbing, dragging, inspecting a
          // keyframe, or after a rebuild/selection change; otherwise skip the
          // whole-skeleton solve that used to run every frame.
          const tc = transformControlsRef.current;
          const sel = selectedNodeRef.current;
          const evalKey = `${sceneVersionRef.current}|${selectedAnimIdxRef.current}|${currTime}|${isPlay}|${sel ? `${sel.type}:${sel.name}` : ""}`;
          const last = lastAnimEvalRef.current;
          let duration: number;
          if (isPlay || (tc && tc.dragging) || last.model !== modelRef.current || last.key !== evalKey) {
            duration = evaluateAnimation(modelRef.current, selectedAnimIdxRef.current, currTime);
            lastAnimEvalRef.current = { model: modelRef.current, key: evalKey };
          } else {
            duration = modelRef.current.animations?.[selectedAnimIdxRef.current]?.duration ?? 4.0;
          }

          if (isPlay) {
            if (currTime >= duration) {
              currTime = loop ? 0 : duration;
            }
            currentTimeRef.current = currTime;
            setCurrentTime(currTime);
          }
        }

        renderer.render(scene, camera);
      };
      animate();

      // 8. Window Resize Handler
      const handleResize = () => {
        if (!mountRef.current) return;
        const w = mountRef.current.clientWidth;
        const h = mountRef.current.clientHeight;
        camera.aspect = w / h;
        camera.updateProjectionMatrix();
        renderer.setSize(w, h);
      };
      window.addEventListener("resize", handleResize);

      setIsInitialized(true);

      const handleExportGLTFEvent = async () => {
        const curModel = modelRef.current;
        if (!meshesGroupRef.current || !curModel) return;
        
        invoke("log_event", { level: "INFO", message: "Starting GLTF model export..." }).catch(console.error);
        
        try {
          const { GLTFExporter } = await import("three/examples/jsm/exporters/GLTFExporter.js");
          const exporter = new GLTFExporter();
          
          exporter.parse(
            meshesGroupRef.current,
            async (gltf) => {
              try {
                const jsonStr = JSON.stringify(gltf, null, 2);
                const defaultName = `${curModel.name}.gltf`;
                
                const savedPath = await invoke<string | null>("save_text_file", {
                  defaultName,
                  filters: ["gltf"],
                  contents: jsonStr
                });
                
                if (savedPath) {
                  invoke("log_event", { level: "INFO", message: `HOD 3D meshes exported successfully to GLTF: ${savedPath}` }).catch(console.error);
                }
              } catch (e: any) {
                console.error(e);
                invoke("log_event", { level: "ERROR", message: `Failed to write exported GLTF file: ${e.toString()}` }).catch(console.error);
              }
            },
            (error) => {
              console.error("GLTF export error:", error);
              invoke("log_event", { level: "ERROR", message: `GLTF compilation failed: ${error.toString()}` }).catch(console.error);
            },
            {
              binary: false,
              animations: [],
              includeCustomExtensions: false
            }
          );
        } catch (e: any) {
          console.error(e);
          invoke("log_event", { level: "ERROR", message: `GLTFExporter dynamic import failed: ${e.toString()}` }).catch(console.error);
        }
      };

      const handleImportGLTFEvent = async () => {
        const curModel = modelRef.current;
        if (!curModel) return;
        try {
          const fileContent = await invoke<string | null>("load_text_file", {
            filters: ["gltf"]
          });
          if (!fileContent) return;

          invoke("log_event", { level: "INFO", message: "Parsing imported GLTF file..." }).catch(console.error);

          const { GLTFLoader } = await import("three/examples/jsm/loaders/GLTFLoader.js");
          const loader = new GLTFLoader();

          const blob = new Blob([fileContent], { type: "application/json" });
          const url = URL.createObjectURL(blob);

          loader.load(url, (gltf) => {
            URL.revokeObjectURL(url);
            
            const newMeshes: HODMesh[] = [];
            gltf.scene.traverse((child) => {
              if ((child as any).isMesh) {
                const mesh = child as THREE.Mesh;
                const geo = mesh.geometry;
                
                if (geo) {
                  const posAttr = geo.attributes.position;
                  if (posAttr) {
                    const vertices: any[] = [];
                    const indices: number[] = [];
                    
                    for (let i = 0; i < posAttr.count; i++) {
                      vertices.push({
                        position: { x: posAttr.getX(i), y: posAttr.getY(i), z: posAttr.getZ(i) },
                        normal: geo.attributes.normal ? { x: geo.attributes.normal.getX(i), y: geo.attributes.normal.getY(i), z: geo.attributes.normal.getZ(i) } : { x: 0, y: 1, z: 0 },
                        uv: geo.attributes.uv ? { u: geo.attributes.uv.getX(i), v: geo.attributes.uv.getY(i) } : { u: 0, v: 0 },
                        tangent: { x: 1, y: 0, z: 0 },
                        binormal: { x: 0, y: 0, z: 1 }
                      });
                    }
                    
                    const indexAttr = geo.index;
                    if (indexAttr) {
                      for (let i = 0; i < indexAttr.count; i++) {
                        indices.push(indexAttr.getX(i));
                      }
                    } else {
                      for (let i = 0; i < posAttr.count; i++) {
                        indices.push(i);
                      }
                    }

                    newMeshes.push({
                      name: mesh.name || `Imported_Mesh_${Date.now()}`,
                      lod: 0,
                      parent_name: "Root",
                      parts: [{
                        material_index: 0,
                        vertex_mask: 0x600B,
                        vertices,
                        indices
                      }]
                    });
                  }
                }
              }
            });

            if (newMeshes.length > 0) {
              onModelChangeRef.current?.({
                ...curModel,
                meshes: [...curModel.meshes, ...newMeshes]
              });
              invoke("log_event", { level: "INFO", message: `Successfully imported ${newMeshes.length} meshes from GLTF.` }).catch(console.error);
            } else {
              alert("No valid meshes found in the GLTF file.");
            }
          });
        } catch (e: any) {
          console.error(e);
          invoke("log_event", { level: "ERROR", message: `Failed to import GLTF: ${e.toString()}` }).catch(console.error);
          alert(`Import Failed: ${e.toString()}`);
        }
      };

      window.addEventListener("export-gltf-model", handleExportGLTFEvent);
      window.addEventListener("import-gltf-mesh", handleImportGLTFEvent);

      setIsInitialized(true);

      return () => {
        if (container) {
          container.removeEventListener("dragstart", handlePrevent);
          container.removeEventListener("selectstart", handlePrevent);
        }
        renderer.domElement.removeEventListener("dragstart", handlePrevent);
        renderer.domElement.removeEventListener("selectstart", handlePrevent);
        window.removeEventListener("resize", handleResize);
        window.removeEventListener("export-gltf-model", handleExportGLTFEvent);
        window.removeEventListener("import-gltf-mesh", handleImportGLTFEvent);
        cancelAnimationFrame(animationId);
        orbitControls.dispose();
        transformControls.dispose();
        renderer.dispose();
        rendererRef.current = null;
        if (mountRef.current && renderer.domElement) {
          mountRef.current.removeChild(renderer.domElement);
        }
        setIsInitialized(false);
      };
    } catch (e: any) {
      invoke("log_event", {
        level: "ERROR",
        message: `Exception thrown inside Viewport mount useEffect: ${e.stack || e.toString()}`,
      }).catch(console.error);
    }
  }, []); // Mounts only once! Persistent WebGL context prevents reset on node selection change.

  // Update transform mode (translate/rotate/scale)
  useEffect(() => {
    if (transformControlsRef.current) {
      transformControlsRef.current.setMode(transformMode);
    }
  }, [transformMode]);

  // Update gizmo coordinate space (local = node axes, global = world axes)
  useEffect(() => {
    if (transformControlsRef.current) {
      transformControlsRef.current.setSpace(coordinateSpace === "global" ? "world" : "local");
    }
  }, [coordinateSpace]);

  // Update Scene when new model is loaded or visibility settings change
  useEffect(() => {
    try {
      const scene = sceneRef.current;
      const meshesGroup = meshesGroupRef.current;
      const jointsGroup = jointsGroupRef.current;
      const markersGroup = markersGroupRef.current;
      const navLightsGroup = navLightsGroupRef.current;
      const collisionGroup = collisionGroupRef.current;
      const dockpathsGroup = dockpathsGroupRef.current;
      const engineBurnsGroup = engineBurnsGroupRef.current;
      const engineGlowsGroup = engineGlowsGroupRef.current;
      const engineShapesGroup = engineShapesGroupRef.current;
      const targetBoxesGroup = targetBoxesGroupRef.current;
      const camera = cameraRef.current;
      const orbitControls = orbitControlsRef.current;

      const rebuildStart = performance.now();

      if (!scene || !model || !meshesGroup || !jointsGroup || !markersGroup || !navLightsGroup || !collisionGroup || !dockpathsGroup || !engineBurnsGroup || !engineGlowsGroup || !engineShapesGroup || !targetBoxesGroup) {
        return;
      }

      // Clear existing meshes, joints, markers, and helpers with full recursive GPU resource disposal!
      // Cached textures are owned by textureCacheRef and swept after the
      // rebuild; disposing them here forced a full GPU re-upload every time.
      const cachedTextures = new Set<THREE.Texture>();
      textureCacheRef.current.forEach(entry => cachedTextures.add(entry.tex));
      const disposeTexture = (tex: THREE.Texture | null | undefined) => {
        if (tex && !cachedTextures.has(tex)) tex.dispose();
      };
      const disposeNode = (node: THREE.Object3D) => {
        if ((node as any).geometry) {
          (node as any).geometry.dispose();
        }
        if ((node as any).material) {
          const mats = Array.isArray((node as any).material) ? (node as any).material : [(node as any).material];
          mats.forEach((mat: any) => {
            disposeTexture(mat.map);
            disposeTexture(mat.normalMap);
            disposeTexture(mat.emissiveMap);
            disposeTexture(mat.roughnessMap);
            disposeTexture(mat.metalnessMap);
            mat.dispose();
          });
        }
      };

      const disposeHierarchy = (group: THREE.Group) => {
        group.traverse(disposeNode);
      };

      disposeHierarchy(meshesGroup);
      disposeHierarchy(jointsGroup);
      disposeHierarchy(markersGroup);
      disposeHierarchy(navLightsGroup);
      disposeHierarchy(collisionGroup);
      disposeHierarchy(dockpathsGroup);
      disposeHierarchy(engineBurnsGroup);
      disposeHierarchy(engineGlowsGroup);
      disposeHierarchy(engineShapesGroup);
      disposeHierarchy(targetBoxesGroup);
      const usedTextureKeys = new Set<string>();

      while (meshesGroup.children.length > 0) meshesGroup.remove(meshesGroup.children[0]);
      while (jointsGroup.children.length > 0) jointsGroup.remove(jointsGroup.children[0]);
      while (markersGroup.children.length > 0) markersGroup.remove(markersGroup.children[0]);
      while (navLightsGroup.children.length > 0) navLightsGroup.remove(navLightsGroup.children[0]);
      while (collisionGroup.children.length > 0) collisionGroup.remove(collisionGroup.children[0]);
      while (dockpathsGroup.children.length > 0) dockpathsGroup.remove(dockpathsGroup.children[0]);
      while (engineBurnsGroup.children.length > 0) engineBurnsGroup.remove(engineBurnsGroup.children[0]);
      while (engineGlowsGroup.children.length > 0) engineGlowsGroup.remove(engineGlowsGroup.children[0]);
      while (engineShapesGroup.children.length > 0) engineShapesGroup.remove(engineShapesGroup.children[0]);
      while (targetBoxesGroup.children.length > 0) targetBoxesGroup.remove(targetBoxesGroup.children[0]);
      
      navLightsAnimateRef.current = [];


      const isKeyVisible = (key: string) => visibleMeshesRef.current[key] !== false;

      const bbox = new THREE.Box3();
      let hasValidBounds = false;

      // 1. Render ship meshes
      model.meshes.forEach((mesh) => {
        const isVisible = isKeyVisible(`${mesh.name}_lod_${mesh.lod}`);

        mesh.parts.forEach((part, index) => {
          const geometry = new THREE.BufferGeometry();
          
          // Compile position array
          const positions = new Float32Array(part.vertices.length * 3);
          const normals = new Float32Array(part.vertices.length * 3);
          const uvs = new Float32Array(part.vertices.length * 2);

          // Identify valid vs uninitialized/sentinel/skeletal outlier vertices
          const isValid = part.vertices.map((v) => {
            return (
              v.position &&
              isFiniteVector(v.position) &&
              Math.abs(v.position.x) < 10000.0 &&
              Math.abs(v.position.y) < 10000.0 &&
              Math.abs(v.position.z) < 10000.0
            );
          });

          part.vertices.forEach((v, i) => {
            if (isValid[i]) {
              positions[i * 3] = v.position.x;
              positions[i * 3 + 1] = v.position.y;
              positions[i * 3 + 2] = v.position.z;
            } else {
              positions[i * 3] = 0;
              positions[i * 3 + 1] = 0;
              positions[i * 3 + 2] = 0;
            }

            if (v.normal && isFiniteVector(v.normal)) {
              normals[i * 3] = v.normal.x;
              normals[i * 3 + 1] = v.normal.y;
              normals[i * 3 + 2] = v.normal.z;
            } else {
              normals[i * 3] = 0;
              normals[i * 3 + 1] = 1;
              normals[i * 3 + 2] = 0;
            }

            if (v.uv) {
              uvs[i * 2] = Number.isFinite(v.uv.u) ? v.uv.u : 0;
              uvs[i * 2 + 1] = Number.isFinite(v.uv.v) ? v.uv.v : 0;
            }
          });

          // Gracefully collapse triangles containing outlier/invalid vertices to zero area
          // so WebGL skips rendering them entirely, preventing glitchy spikes to infinity.
          for (let t = 0; t < part.indices.length; t += 3) {
            const iA = part.indices[t];
            const iB = part.indices[t + 1];
            const iC = part.indices[t + 2];

            const valA = isValid[iA];
            const valB = isValid[iB];
            const valC = isValid[iC];

            const validCount = (valA ? 1 : 0) + (valB ? 1 : 0) + (valC ? 1 : 0);

            if (validCount === 1 || validCount === 2) {
              // Mix of valid and invalid vertices in the same triangle.
              // Copy the position of a valid vertex to the invalid ones.
              const targetValidIdx = valA ? iA : (valB ? iB : iC);
              const targetX = positions[targetValidIdx * 3];
              const targetY = positions[targetValidIdx * 3 + 1];
              const targetZ = positions[targetValidIdx * 3 + 2];

              if (!valA) {
                positions[iA * 3] = targetX;
                positions[iA * 3 + 1] = targetY;
                positions[iA * 3 + 2] = targetZ;
              }
              if (!valB) {
                positions[iB * 3] = targetX;
                positions[iB * 3 + 1] = targetY;
                positions[iB * 3 + 2] = targetZ;
              }
              if (!valC) {
                positions[iC * 3] = targetX;
                positions[iC * 3 + 1] = targetY;
                positions[iC * 3 + 2] = targetZ;
              }
            }
          }

          geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
          if (part.vertex_mask & 2) { // Normal is bit 2 (0x2)
            geometry.setAttribute("normal", new THREE.BufferAttribute(normals, 3));
          } else {
            geometry.computeVertexNormals();
          }

          if (part.vertex_mask & 8) { // Texture0 is bit 8 (0x8)
            geometry.setAttribute("uv", new THREE.BufferAttribute(uvs, 2));
          }

          // Use Uint32Array to support large meshes with > 65535 vertices
          geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(part.indices), 1));

          // Add to bounding box using only active referenced vertices to avoid uninitialized padding garbage!
          let meshJointMatrix = new THREE.Matrix4();
          if (mesh.parent_name && mesh.parent_name !== "Root") {
            meshJointMatrix = getJointGlobalMatrix(mesh.parent_name);
          }

          part.indices.forEach((idx) => {
            if (isValid[idx]) {
              const v = part.vertices[idx];
              const pt = new THREE.Vector3(v.position.x, v.position.y, v.position.z);
              pt.applyMatrix4(meshJointMatrix); // Bring to world space
              bbox.expandByPoint(pt);
              hasValidBounds = true;
            }
          });

          // Helper to get or create a texture from base64Png
          const getCachedTexture = (base64Png: string, name: string, yFlipped: boolean = false): THREE.Texture => {
            const cache = textureCacheRef.current;
            const cacheKey = `${name}:${yFlipped}`;
            usedTextureKeys.add(cacheKey);
            const existing = cache.get(cacheKey);
            // Unchanged textures keep the same string object across immutable
            // model updates, so this comparison is normally a pointer check.
            if (existing && existing.src === base64Png) {
              return existing.tex;
            }
            if (existing) existing.tex.dispose();
            const img = new Image();
            img.src = base64Png.startsWith("data:") ? base64Png : `data:image/png;base64,${base64Png}`;
            const tex = new THREE.Texture(img);
            tex.wrapS = THREE.RepeatWrapping;
            tex.wrapT = THREE.RepeatWrapping;
            tex.flipY = !yFlipped; // True by default for standard HOD pipeline, false if y-flipped flag is set
            tex.colorSpace = THREE.SRGBColorSpace;
            tex.anisotropy = rendererRef.current?.capabilities.getMaxAnisotropy() ?? 1;
            tex.generateMipmaps = false;
            tex.minFilter = THREE.LinearFilter;
            tex.magFilter = THREE.LinearFilter;
            img.onload = () => {
              tex.needsUpdate = true;
            };
            cache.set(cacheKey, { src: base64Png, tex });
            return tex;
          };

          // Find texture mapping
          let textureMap: THREE.Texture | null = null;
          let normalMap: THREE.Texture | null = null;
          let glowMap: THREE.Texture | null = null;
          let teamMap: THREE.Texture | null = null;

          if (model.materials && part.material_index < model.materials.length) {
            const hMaterial = model.materials[part.material_index];
            if (hMaterial && hMaterial.texture_maps) {
              const cleanTexName = (name: string) => name.toLowerCase().replace(/\.(tga|png|dds|bmp|jpg|jpeg)$/, "").trim();
              const findTexture = (texName: string) => {
                const mName = cleanTexName(texName);
                if (!mName) return undefined;
                const exact = model.textures?.find(t => cleanTexName(t.name) === mName);
                if (exact) return exact;
                return model.textures?.find(t => {
                  const tName = cleanTexName(t.name);
                  return tName.includes(mName) || mName.includes(tName);
                });
              };

              hMaterial.texture_maps.forEach((texName, tIdx) => {
                if (!texName?.trim()) return;
                const hTexture = findTexture(texName);
                if (hTexture) {
                  const b64 = hTexture.png_data || hTexture.png_preview;
                  if (b64) {
                    const tex = getCachedTexture(b64, hTexture.name, !!hTexture.legacy_storage_y_flipped);
                    const lowerName = texName.toLowerCase();
                    // Suffix roles mirror SHADERS.MAP channel roles. REFL/SPEC on
                    // `ship` are channels packed inside the GLOW texture, not
                    // separate maps; shipglow-family $spec has no direct
                    // three.js binding without an envmap. Thruster DIFX/GLOX
                    // are off-state alternates stored after the ON textures,
                    // so first-wins keeps the ON state visible.
                    const hasKnownSuffix = lowerName.includes("_team") || lowerName.includes("_temx")
                      || lowerName.includes("_diff") || lowerName.includes("_difx")
                      || lowerName.includes("_norm") || lowerName.includes("_normal")
                      || lowerName.includes("_glow") || lowerName.includes("_glox")
                      || lowerName.includes("_spec") || lowerName.includes("_spex");
                    if (lowerName.includes("_team") || lowerName.includes("_temx")) {
                      teamMap = tex;
                    } else if (lowerName.includes("_norm") || lowerName.includes("_normal")) {
                      normalMap = tex;
                    } else if (lowerName.includes("_glow") || lowerName.includes("_glox")) {
                      if (!glowMap) glowMap = tex;
                    } else if (lowerName.includes("_diff") || lowerName.includes("_difx")) {
                      if (!textureMap) textureMap = tex;
                    } else if (tIdx === 0 && !hasKnownSuffix) {
                      textureMap = tex;
                    }
                  }
                }
              });
            }
          }

          // Harmonized tech color palette (cobalt, indigo, cyan, violet) to color-code different parts beautifully!
          const partColors = ["#4361ee", "#3f37c9", "#4cc9f0", "#7209b7", "#f72585"];
          const colorHex = partColors[part.material_index % partColors.length] || "#4361ee";

          let material: THREE.Material;

          const hMaterial = model.materials && part.material_index < model.materials.length ? model.materials[part.material_index] : null;
          const isBadgeShader = !!(hMaterial && hMaterial.shader_name.toLowerCase().includes("badge"));

          if (renderMode === "wireframe") {
            material = new THREE.MeshBasicMaterial({
              color: new THREE.Color(colorHex),
              wireframe: true,
              side: THREE.DoubleSide,
            });
          } else if (renderMode === "untextured") {
            material = new THREE.MeshStandardMaterial({
              color: new THREE.Color("#8da6c4"),
              roughness: 0.6,
              metalness: 0.1,
              side: THREE.DoubleSide,
              flatShading: true,
            });
          } else if (renderMode === "textured" || renderMode === "textured_team") {
            material = new THREE.MeshBasicMaterial({
              color: textureMap ? new THREE.Color("#ffffff") : new THREE.Color(colorHex),
              map: textureMap,
              transparent: isBadgeShader,
              side: THREE.DoubleSide,
            });
          } else { // "shaded" or "shaded_team"
            material = new THREE.MeshStandardMaterial({
              color: textureMap ? new THREE.Color("#ffffff") : new THREE.Color(colorHex),
              map: textureMap,
              normalMap: normalMap,
              normalScale: new THREE.Vector2(1.0, 1.0),
              emissiveMap: glowMap,
              emissive: glowMap ? new THREE.Color("#ffffff") : new THREE.Color("#000000"),
              roughness: 0.5,
              metalness: 0.15,
              side: THREE.DoubleSide,
            });
          }

          const hasTeam = teamMap && (renderMode === "textured_team" || renderMode === "shaded_team");
          const hasGlow = glowMap && (renderMode === "shaded" || renderMode === "shaded_team");

          if (hasTeam || hasGlow) {
            const uTeam = new THREE.Color(teamColor);
            const uStripe = new THREE.Color(stripeColor);
            
            let isGlowRGB = false;
            if (hMaterial) {
              const sName = hMaterial.shader_name.toLowerCase();
              if (sName.includes("glow") || sName.includes("burn") || sName === "ore" || sName.startsWith("bg_")) {
                 isGlowRGB = true;
              }
            }

            material.onBeforeCompile = (shader, _renderer) => {
              if (hasTeam) {
                shader.uniforms.teamMap = { value: teamMap };
                shader.uniforms.uColTeam = { value: uTeam };
                shader.uniforms.uColStripe = { value: uStripe };

                shader.fragmentShader = `
                  uniform sampler2D teamMap;
                  uniform vec3 uColTeam;
                  uniform vec3 uColStripe;
                ` + shader.fragmentShader;

                shader.fragmentShader = shader.fragmentShader.replace(
                  '#include <map_fragment>',
                  `
                  #include <map_fragment>
                  #ifdef USE_MAP
                    vec4 texTeam = texture2D( teamMap, vMapUv );
                    vec3 paint = mix(uColTeam, diffuseColor.rgb, texTeam.r);
                    paint = mix(uColStripe, paint, texTeam.g);
                    diffuseColor.rgb = paint;
                  #else
                    diffuseColor.rgb *= uColTeam;
                  #endif
                  `
                );
              }

              if (hasGlow) {
                shader.fragmentShader = shader.fragmentShader.replace(
                  '#include <emissivemap_fragment>',
                  `
                  #include <emissivemap_fragment>
                  #ifdef USE_EMISSIVEMAP
                    ${isGlowRGB 
                      ? '' 
                      : 'totalEmissiveRadiance = diffuseColor.rgb * emissiveColor.g * emissive;'
                    }
                  #endif
                  `
                );
              }
            };
          }

          const threeMesh = new THREE.Mesh(geometry, material);
          threeMesh.name = `${mesh.name}_part_${index}`;
          threeMesh.visible = isVisible;
          threeMesh.userData.visKey = `${mesh.name}_lod_${mesh.lod}`;

          const parentName = mesh.parent_name || "Root";
          const jointMatrix = getJointGlobalMatrix(parentName);
          threeMesh.applyMatrix4(jointMatrix);
          threeMesh.userData.parentJointName = parentName;
          threeMesh.userData.baseJointMatrix = jointMatrix.clone();

          meshesGroup.add(threeMesh);

          // Stunning wireframe overlay for an advanced sci-tech engineering blueprint aesthetic!
          if (renderMode === "untextured") {
            const wireframeGeo = new THREE.WireframeGeometry(geometry);
            const wireframeMat = new THREE.LineBasicMaterial({
              color: new THREE.Color("#16a0ff"),
              transparent: true,
              opacity: 0.15,
            });
            const wireframe = new THREE.LineSegments(wireframeGeo, wireframeMat);
            threeMesh.add(wireframe);
          }
        });
      });

      // 2. Pre-calculate joint positions to build bounding box and connections safely
      const jointPositions = new Map<string, THREE.Vector3>();

      model.joints.forEach((joint) => {
        // Find global position from transform matrix recursively
        const matrix = getJointGlobalMatrix(joint.name);
        
        const pos = new THREE.Vector3();
        const q = new THREE.Quaternion();
        const s = new THREE.Vector3();
        safeDecompose(matrix, pos, q, s);

        if (isFiniteVector(pos) && Math.abs(pos.x) < 100000 && Math.abs(pos.y) < 100000 && Math.abs(pos.z) < 100000) {
          jointPositions.set(joint.name.toLowerCase(), pos);
          bbox.expandByPoint(pos);
          hasValidBounds = true;
        } else {
          jointPositions.set(joint.name.toLowerCase(), new THREE.Vector3(0, 0, 0));
        }
      });

      // Process markers to expand bounds:
      model.markers.forEach((marker) => {
        const pos = new THREE.Vector3(marker.position.x, marker.position.y, marker.position.z);
        bbox.expandByPoint(pos);
        hasValidBounds = true;
      });

      // Calculate max dimension and dynamic scale factor
      const size = new THREE.Vector3();
      bbox.getSize(size);
      const maxDim = Math.max(size.x, size.y, size.z);
      // Scale factor: scale of joints/markers relative to the ship's longest dimension
      const scaleFactor = hasValidBounds && maxDim > 0 ? Math.max(0.15, maxDim * 0.025) : 1.0;
      scaleFactorRef.current = scaleFactor;


      // 3. Render joint skeletons using scaleFactor
      const sphereGeo = new THREE.SphereGeometry(Math.max(0.12, Math.min(2.5, 0.08 * scaleFactor)), 16, 16);
      const jointMat = new THREE.MeshBasicMaterial({ 
        color: "#00e676", 
        depthTest: false, 
        transparent: true, 
        opacity: 0.9 
      });

      model.joints.forEach((joint) => {
        const isNavLight = model.nav_lights?.some(n => n.name.toLowerCase() === joint.name.toLowerCase());
        if (isNavLight) {
          const isEngineParent =
            model.engine_burns?.some(b => b.parent_name === joint.name) ||
            model.engine_glows?.some(g => g.parent_name === joint.name) ||
            model.engine_shapes?.some(s => s.parent_name === joint.name);
          if (!isEngineParent) return;
        }

        const pos = jointPositions.get(joint.name.toLowerCase());
        if (!pos) return;

        // Decompose global orientation matrix
        const matrix = getJointGlobalMatrix(joint.name);
        const q = new THREE.Quaternion();
        const dummyPos = new THREE.Vector3();
        const dummyScale = new THREE.Vector3();
        safeDecompose(matrix, dummyPos, q, dummyScale);

        const jointMesh = new THREE.Mesh(sphereGeo, jointMat);
        jointMesh.position.copy(pos);
        jointMesh.quaternion.copy(q);
        jointMesh.name = `joint:${joint.name}`;
        jointMesh.userData.visKey = `joint:${joint.name}`;
        jointMesh.visible = isKeyVisible(`joint:${joint.name}`);
        jointsGroup.add(jointMesh);

        // Draw bone line if there is a parent joint, parent is visible, and showBoneLines is enabled
        if (showBoneLines && joint.parent_name) {
          {
            const parentPos = jointPositions.get(joint.parent_name.toLowerCase());
            if (parentPos) {
              const boneGeo = new THREE.BufferGeometry().setFromPoints([pos, parentPos]);
              const boneMat = new THREE.LineBasicMaterial({ 
                color: "#00b0ff", 
                depthTest: false, 
                transparent: true, 
                opacity: 0.6 
              });
              const boneLine = new THREE.Line(boneGeo, boneMat);
              boneLine.name = `bone:${joint.parent_name}->${joint.name}`;
              boneLine.userData = {
                parentJointName: joint.parent_name,
                childJointName: joint.name,
                visKeys: [`joint:${joint.name}`, `joint:${joint.parent_name}`],
              };
              boneLine.visible = isKeyVisible(`joint:${joint.name}`) && isKeyVisible(`joint:${joint.parent_name}`);
              jointsGroup.add(boneLine);
            }
          }
        }
      });


      // 4. Render markers using scaleFactor
      const mr = Math.max(0.1, Math.min(2.5, 0.05 * scaleFactor));
      const mh = Math.max(0.4, Math.min(6.0, 0.2 * scaleFactor));
      const markerGeo = new THREE.ConeGeometry(mr, mh, 8);
      markerGeo.rotateX(Math.PI / 2); // Point forward along Z
      const markerMat = new THREE.MeshBasicMaterial({ 
        color: "#00d2ff", 
        depthTest: false, 
        transparent: true, 
        opacity: 0.9 
      });

      model.markers.forEach((marker) => {
        const markerMesh = new THREE.Mesh(markerGeo, markerMat);
        
        // Calculate global transform recursively from parent joint
        const parentMatrix = getJointGlobalMatrix(marker.parent_joint);
        const localPos = new THREE.Vector3(marker.position.x, marker.position.y, marker.position.z);
        const localRotMatrix = toThreeMatrix(marker.rotation);

        const localMatrix = localRotMatrix.clone();
        localMatrix.setPosition(localPos);

        const globalMatrix = parentMatrix.clone().multiply(localMatrix);

        const globalPos = new THREE.Vector3();
        const globalQuat = new THREE.Quaternion();
        const globalScale = new THREE.Vector3();
        safeDecompose(globalMatrix, globalPos, globalQuat, globalScale);

        markerMesh.position.copy(globalPos);
        markerMesh.quaternion.copy(globalQuat);
        
        markerMesh.name = `marker:${marker.name}`;
        markerMesh.userData = {
          parentJointName: marker.parent_joint,
          baseMatrix: localMatrix.clone(),
          visKey: `marker:${marker.name}`,
        };
        markerMesh.visible = isKeyVisible(`marker:${marker.name}`);
        markersGroup.add(markerMesh);
        
        const markerAxes = new THREE.AxesHelper(0.4 * scaleFactor);
        markerMesh.add(markerAxes);
      });

      // 5. Render NavLights (Pulsing Previews)
      if (model.nav_lights) {
        model.nav_lights.forEach((nav) => {
          const color = new THREE.Color(nav.color.x, nav.color.y, nav.color.z);
          
          // Render the actual sprite size as a small solid pulsing sphere
          const spriteRadius = (nav.size && nav.size > 0) ? (nav.size * 0.25) : 0.15;
          const navGeo = new THREE.SphereGeometry(spriteRadius, 16, 16);
          const navMat = new THREE.MeshBasicMaterial({
            color: color,
            transparent: true,
            opacity: 0.8,
            depthTest: false,
            depthWrite: false,
          });
           const navMesh = new THREE.Mesh(navGeo, navMat);
          navMesh.name = `navlight:${nav.name}`;
          navMesh.renderOrder = 20;
          navMesh.userData = {
            parentJointName: nav.name,
            baseMatrix: new THREE.Matrix4(),
            visKey: `navlight:${nav.name}`,
          };
          navMesh.visible = isKeyVisible(`navlight:${nav.name}`);

          const jointPos = jointPositions.get(nav.name.toLowerCase());
          if (jointPos) {
            navMesh.position.copy(jointPos);
          } else {
            navMesh.position.set(0, 0, 0);
          }

          navLightsGroup.add(navMesh);

          if (nav.distance && nav.distance > 0) {
            const rangeGeo = new THREE.SphereGeometry(nav.distance, 16, 16);
            const rangeMat = new THREE.MeshBasicMaterial({
              color: color,
              wireframe: true,
              transparent: true,
              opacity: 0.12,
              depthTest: false,
              depthWrite: false,
            });
            const rangeMesh = new THREE.Mesh(rangeGeo, rangeMat);
            rangeMesh.renderOrder = 19;
            navMesh.add(rangeMesh);
          }

          navLightsAnimateRef.current.push({
            mesh: navMesh,
            material: navMat,
            frequency: typeof nav.frequency === "number" ? nav.frequency : 0.0,
            phase: typeof nav.phase === "number" ? nav.phase : 0.0,
          });
        });
      }

      // 6. Render Dockpaths (connected paths with splines)
      if (model.dockpaths) {
        model.dockpaths.forEach((path) => {
          const pathVisKey = `dockpath:${path.name}`;
          const isPathVisible = isKeyVisible(pathVisKey);

          const parentMatrix = getJointGlobalMatrix(path.parent_name);
          const pointsInWorld: THREE.Vector3[] = [];

          path.points.forEach((pt, ptIndex) => {
            const r = pt.rotation.m;
            const localMatrix = new THREE.Matrix4().set(
              r[0][0], r[1][0], r[2][0], r[3][0],
              r[0][1], r[1][1], r[2][1], r[3][1],
              r[0][2], r[1][2], r[2][2], r[3][2],
              r[0][3], r[1][3], r[2][3], r[3][3]
            );
            localMatrix.setPosition(pt.position.x, pt.position.y, pt.position.z);

            const globalMatrix = parentMatrix.clone().multiply(localMatrix);

            const ptPos = new THREE.Vector3();
            const ptQuat = new THREE.Quaternion();
            const ptScale = new THREE.Vector3();
            safeDecompose(globalMatrix, ptPos, ptQuat, ptScale);

            pointsInWorld.push(ptPos);

            const pr = Math.max(0.2, Math.min(4.0, 0.06 * scaleFactor));
            const ph = Math.max(0.5, Math.min(10.0, 0.15 * scaleFactor));
            const pyramidGeo = new THREE.ConeGeometry(pr, ph, 4);
            pyramidGeo.rotateX(Math.PI / 2);

            const pyramidMat = new THREE.MeshBasicMaterial({
              color: "#ffaa00",
              depthTest: false,
              transparent: true,
              opacity: 0.9,
            });
            const pyramidMesh = new THREE.Mesh(pyramidGeo, pyramidMat);
            pyramidMesh.name = `dockpoint:${path.name}:${ptIndex}`;
            pyramidMesh.position.copy(ptPos);
            pyramidMesh.quaternion.copy(ptQuat);
            pyramidMesh.userData = {
              parentJointName: path.parent_name,
              baseMatrix: localMatrix.clone(),
              visKey: pathVisKey,
            };
            pyramidMesh.visible = isPathVisible;

            dockpathsGroup.add(pyramidMesh);
          });

          if (pointsInWorld.length > 1) {
            const lineGeo = new THREE.BufferGeometry().setFromPoints(pointsInWorld);
            const lineMat = new THREE.LineBasicMaterial({
              color: "#ffd54f",
              linewidth: 2,
              transparent: true,
              opacity: 0.8,
            });
            const pathLine = new THREE.Line(lineGeo, lineMat);
            pathLine.name = `dockpath_line:${path.name}`;
            pathLine.userData = {
              pathName: path.name,
              pointCount: path.points.length,
              visKey: pathVisKey,
            };
            pathLine.visible = isPathVisible;
            dockpathsGroup.add(pathLine);
          }
        });
      }

      const createThreeMeshGroup = (mesh: HODMesh, defaultColor: string, opacity: number): THREE.Group => {
        const group = new THREE.Group();
        mesh.parts.forEach((part) => {
          const geometry = new THREE.BufferGeometry();
          const positions = new Float32Array(part.vertices.length * 3);
          const normals = new Float32Array(part.vertices.length * 3);
          const uvs = new Float32Array(part.vertices.length * 2);

          const isValid = part.vertices.map((v) => {
            return (
              v.position &&
              isFiniteVector(v.position) &&
              Math.abs(v.position.x) < 10000.0 &&
              Math.abs(v.position.y) < 10000.0 &&
              Math.abs(v.position.z) < 10000.0
            );
          });

          part.vertices.forEach((v, i) => {
            if (isValid[i]) {
              positions[i * 3] = v.position.x;
              positions[i * 3 + 1] = v.position.y;
              positions[i * 3 + 2] = v.position.z;
            } else {
              positions[i * 3] = 0;
              positions[i * 3 + 1] = 0;
              positions[i * 3 + 2] = 0;
            }

            if (v.normal && isFiniteVector(v.normal)) {
              normals[i * 3] = v.normal.x;
              normals[i * 3 + 1] = v.normal.y;
              normals[i * 3 + 2] = v.normal.z;
            } else {
              normals[i * 3] = 0;
              normals[i * 3 + 1] = 1;
              normals[i * 3 + 2] = 0;
            }

            if (v.uv) {
              uvs[i * 2] = Number.isFinite(v.uv.u) ? v.uv.u : 0;
              uvs[i * 2 + 1] = Number.isFinite(v.uv.v) ? v.uv.v : 0;
            }
          });

          for (let t = 0; t < part.indices.length; t += 3) {
            const iA = part.indices[t];
            const iB = part.indices[t + 1];
            const iC = part.indices[t + 2];

            const valA = isValid[iA];
            const valB = isValid[iB];
            const valC = isValid[iC];

            const validCount = (valA ? 1 : 0) + (valB ? 1 : 0) + (valC ? 1 : 0);

            if (validCount === 1 || validCount === 2) {
              const targetValidIdx = valA ? iA : (valB ? iB : iC);
              const targetX = positions[targetValidIdx * 3];
              const targetY = positions[targetValidIdx * 3 + 1];
              const targetZ = positions[targetValidIdx * 3 + 2];

              if (!valA) {
                positions[iA * 3] = targetX;
                positions[iA * 3 + 1] = targetY;
                positions[iA * 3 + 2] = targetZ;
              }
              if (!valB) {
                positions[iB * 3] = targetX;
                positions[iB * 3 + 1] = targetY;
                positions[iB * 3 + 2] = targetZ;
              }
              if (!valC) {
                positions[iC * 3] = targetX;
                positions[iC * 3 + 1] = targetY;
                positions[iC * 3 + 2] = targetZ;
              }
            }
          }

          geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
          if (part.vertex_mask & 2) {
            geometry.setAttribute("normal", new THREE.BufferAttribute(normals, 3));
          } else {
            geometry.computeVertexNormals();
          }

          if (part.vertex_mask & 8) {
            geometry.setAttribute("uv", new THREE.BufferAttribute(uvs, 2));
          }

          geometry.setIndex(new THREE.BufferAttribute(new Uint32Array(part.indices), 1));

          const material = new THREE.MeshBasicMaterial({
            color: new THREE.Color(defaultColor),
            transparent: opacity < 1.0,
            opacity: opacity,
            side: THREE.DoubleSide,
            depthWrite: false, // Prevents transparent parts from clipping out each other
            blending: THREE.AdditiveBlending,
          });

          const partMesh = new THREE.Mesh(geometry, material);
          group.add(partMesh);
        });
        return group;
      };

      // 7. Render Engine Burns (exhaust trails)
      if (model.engine_burns) {
        model.engine_burns.forEach((burn) => {
          const parentMatrix = getJointGlobalMatrix(burn.parent_name);
          const parentPos = new THREE.Vector3();
          const parentQuat = new THREE.Quaternion();
          const parentScale = new THREE.Vector3();
          safeDecompose(parentMatrix, parentPos, parentQuat, parentScale);

          const burnPoints: THREE.Vector3[] = [];
          burn.vertices.forEach((v) => {
            const pt = new THREE.Vector3(v.x, v.y, v.z);
            burnPoints.push(pt);
          });

          if (burnPoints.length > 0) {
            const burnGroup = new THREE.Group();
            burnGroup.name = `engine_burn:${burn.name}`;
            burnGroup.position.copy(parentPos);
            burnGroup.quaternion.copy(parentQuat);
            burnGroup.userData = {
              parentJointName: burn.parent_name,
              baseMatrix: new THREE.Matrix4(),
              visKey: `engine_burn:${burn.name}`,
            };
            burnGroup.visible = isKeyVisible(`engine_burn:${burn.name}`);

            const burnGeo = new THREE.BufferGeometry().setFromPoints(burnPoints);
            const burnMat = new THREE.LineBasicMaterial({
              color: "#ff3d00",
              transparent: true,
              opacity: 0.8,
            });
            const burnLine = new THREE.Line(burnGeo, burnMat);
            burnGroup.add(burnLine);

            // Add glowing particle spawn indicator spheres at each burn vertex coordinate
            const sphereGeo = new THREE.SphereGeometry(0.12 * scaleFactor, 8, 8);
            const dotMat = new THREE.MeshBasicMaterial({
              color: "#ff9100",
              depthTest: false,
              transparent: true,
              opacity: 0.95,
            });

            burnPoints.forEach((pt) => {
              const dotMesh = new THREE.Mesh(sphereGeo, dotMat);
              dotMesh.position.copy(pt);
              burnGroup.add(dotMesh);
            });

            engineBurnsGroup.add(burnGroup);
          }
        });
      }

      // 8. Render Engine Glows
      if (model.engine_glows) {
        model.engine_glows.forEach((glow) => {
          const parentMatrix = getJointGlobalMatrix(glow.parent_name);
          const parentPos = new THREE.Vector3();
          const parentQuat = new THREE.Quaternion();
          const parentScale = new THREE.Vector3();
          safeDecompose(parentMatrix, parentPos, parentQuat, parentScale);

          const glowGroup = createThreeMeshGroup(glow.mesh, "#ffd600", 0.5);
          glowGroup.name = `engine_glow:${glow.name}`;
          glowGroup.position.copy(parentPos);
          glowGroup.quaternion.copy(parentQuat);
          glowGroup.userData = {
            parentJointName: glow.parent_name,
            baseMatrix: new THREE.Matrix4(),
            visKey: `engine_glow:${glow.name}`,
          };
          glowGroup.visible = isKeyVisible(`engine_glow:${glow.name}`);

          engineGlowsGroup.add(glowGroup);
        });
      }

      // 9. Render Engine Shapes
      if (model.engine_shapes) {
        model.engine_shapes.forEach((shape) => {
          const parentMatrix = getJointGlobalMatrix(shape.parent_name);
          const parentPos = new THREE.Vector3();
          const parentQuat = new THREE.Quaternion();
          const parentScale = new THREE.Vector3();
          safeDecompose(parentMatrix, parentPos, parentQuat, parentScale);

          const shapeGroup = createThreeMeshGroup(shape.mesh, "#7209b7", 0.4);
          shapeGroup.name = `engine_shape:${shape.name}`;
          shapeGroup.position.copy(parentPos);
          shapeGroup.quaternion.copy(parentQuat);
          shapeGroup.userData = {
            parentJointName: shape.parent_name,
            baseMatrix: new THREE.Matrix4(),
            visKey: `engine_shape:${shape.name}`,
          };
          shapeGroup.visible = isKeyVisible(`engine_shape:${shape.name}`);

          engineShapesGroup.add(shapeGroup);
        });
      }

      // 10. Render Collision Meshes (Wireframe)
      if (model.collision_meshes) {
        model.collision_meshes.forEach((col) => {
          const jointMatrix = getJointGlobalMatrix(col.name);

          const colGroup = new THREE.Group();
          colGroup.name = `collision:${col.name}`;
          colGroup.renderOrder = 1;

          const hasCollisionGeometry = !!(
            col.mesh &&
            col.mesh.parts &&
            col.mesh.parts.some(part => part.vertices.length > 0 && part.indices.length > 0)
          );

          // Render actual mesh if it exists
          if (hasCollisionGeometry) {
            const meshMat = new THREE.MeshBasicMaterial({
              color: "#ff1744",
              transparent: true,
              opacity: 0.35,
              wireframe: false,
              depthWrite: false,
              side: THREE.DoubleSide
            });
            const wireMat = new THREE.LineBasicMaterial({
              color: "#ff1744",
              transparent: true,
              opacity: 0.8,
              depthWrite: false
            });

            col.mesh.parts.forEach(part => {
              const geo = new THREE.BufferGeometry();
              const vertices = new Float32Array(part.vertices.length * 3);
              part.vertices.forEach((v, i) => {
                vertices[i * 3] = v.position.x;
                vertices[i * 3 + 1] = v.position.y;
                vertices[i * 3 + 2] = v.position.z;
              });
              geo.setAttribute("position", new THREE.BufferAttribute(vertices, 3));
              geo.setIndex(part.indices);
              
              const m = new THREE.Mesh(geo, meshMat);
              m.renderOrder = 1;
              const w = new THREE.LineSegments(new THREE.WireframeGeometry(geo), wireMat);
              w.renderOrder = 2;
              m.add(w);
              colGroup.add(m);
            });
          }

          const sizeX = col.max_extents.x - col.min_extents.x;
          const sizeY = col.max_extents.y - col.min_extents.y;
          const sizeZ = col.max_extents.z - col.min_extents.z;
          const hasValidBox = [
            col.min_extents.x,
            col.min_extents.y,
            col.min_extents.z,
            col.max_extents.x,
            col.max_extents.y,
            col.max_extents.z,
            sizeX,
            sizeY,
            sizeZ,
          ].every(Number.isFinite) && (Math.abs(sizeX) > 0.0001 || Math.abs(sizeY) > 0.0001 || Math.abs(sizeZ) > 0.0001);

          if (hasValidBox) {
            // BBOX helper remains visible even when KDOP/TRIS geometry is present.
            const boxGeo = new THREE.BoxGeometry(Math.abs(sizeX), Math.abs(sizeY), Math.abs(sizeZ));
            const boxMat = new THREE.MeshBasicMaterial({
              color: "#ff1744",
              transparent: true,
              opacity: hasCollisionGeometry ? 0.08 : 0.15,
              wireframe: false,
              depthWrite: false,
            });
            const boxMesh = new THREE.Mesh(boxGeo, boxMat);
            boxMesh.renderOrder = 1;

            const boxWire = new THREE.LineSegments(
              new THREE.EdgesGeometry(boxGeo),
              new THREE.LineBasicMaterial({ color: "#ff1744", transparent: true, opacity: 0.6, depthWrite: false })
            );
            boxWire.renderOrder = 2;
            boxMesh.add(boxWire);

            const boxCenter = new THREE.Vector3(
              (col.min_extents.x + col.max_extents.x) / 2,
              (col.min_extents.y + col.max_extents.y) / 2,
              (col.min_extents.z + col.max_extents.z) / 2
            );
            boxMesh.position.copy(boxCenter);
            colGroup.add(boxMesh);
          } else if (!hasCollisionGeometry) {
            const markerGeo = new THREE.BoxGeometry(0.1, 0.1, 0.1);
            const markerMat = new THREE.MeshBasicMaterial({ color: "#ff1744", transparent: true, opacity: 0.35, depthWrite: false });
            const markerMesh = new THREE.Mesh(markerGeo, markerMat);
            markerMesh.renderOrder = 1;
            colGroup.add(markerMesh);
          }

          const hasValidSphere = Number.isFinite(col.radius) && col.radius > 0.0001 && isFiniteVector(col.center);
          if (hasValidSphere) {
            // BSPH helper remains visible even when KDOP/TRIS geometry is present.
            const sphereGeo = new THREE.SphereGeometry(col.radius, 16, 16);
            const sphereMat = new THREE.MeshBasicMaterial({
              color: "#ff1744",
              wireframe: true,
              transparent: true,
              opacity: 0.25,
              depthWrite: false,
            });
            const sphereMesh = new THREE.Mesh(sphereGeo, sphereMat);
            sphereMesh.renderOrder = 1;
            sphereMesh.position.set(col.center.x, col.center.y, col.center.z);
            colGroup.add(sphereMesh);
          }

          colGroup.applyMatrix4(jointMatrix);
          colGroup.userData = {
            parentJointName: col.name,
            baseMatrix: new THREE.Matrix4(),
            visKey: `collision:${col.name}`,
          };
          colGroup.visible = isKeyVisible(`collision:${col.name}`);
          collisionGroup.add(colGroup);
        });
      }

      if (targetBoxes) {
        targetBoxes.forEach((box) => {
          if (box.visible === false) return;

          const center = bbox.getCenter(new THREE.Vector3());
          const size = bbox.getSize(new THREE.Vector3());
          const halfSize = size.clone().multiplyScalar(0.5);

          const minX = center.x + halfSize.x * box.min.x;
          const minY = center.y + halfSize.y * box.min.y;
          const minZ = center.z + halfSize.z * box.min.z;

          const maxX = center.x + halfSize.x * box.max.x;
          const maxY = center.y + halfSize.y * box.max.y;
          const maxZ = center.z + halfSize.z * box.max.z;

          const width = Math.abs(maxX - minX);
          const height = Math.abs(maxY - minY);
          const length = Math.abs(maxZ - minZ);

          const boxGeo = new THREE.BoxGeometry(width || 0.1, height || 0.1, length || 0.1);
          const boxMat = new THREE.MeshBasicMaterial({
            color: "#ffab00",
            transparent: true,
            opacity: 0.15,
            depthTest: false,
          });
          const boxMesh = new THREE.Mesh(boxGeo, boxMat);

          const boxWire = new THREE.LineSegments(
            new THREE.EdgesGeometry(boxGeo),
            new THREE.LineBasicMaterial({ color: "#ffab00", transparent: true, opacity: 0.6, depthTest: false })
          );
          boxMesh.add(boxWire);

          const boxCenter = new THREE.Vector3(
            (minX + maxX) / 2,
            (minY + maxY) / 2,
            (minZ + maxZ) / 2
          );
          boxMesh.position.copy(boxCenter);
          
          if (targetBoxesGroupRef.current) {
            targetBoxesGroupRef.current.add(boxMesh);
          }
        });
      }

      // 9. Auto-Focus and Camera Fitting (Only run once when a new HOD model is loaded!)
      if (camera && orbitControls && hasValidBounds && maxDim > 0 && lastFocusedModelRef.current !== model.name) {
        const center = new THREE.Vector3();
        bbox.getCenter(center);

        const fovRad = (camera.fov * Math.PI) / 180;
        let cameraDistance = maxDim / (2 * Math.tan(fovRad / 2));
        cameraDistance *= 1.5; // Padding

         camera.near = Math.max(0.05, cameraDistance * 0.001);
        camera.far = Math.max(50000, cameraDistance * 20);
        camera.updateProjectionMatrix();

        const camX = cameraDistance * 0.5;
        const camY = cameraDistance * 0.4;
        const camZ = cameraDistance * 1.0;
        camera.position.set(camX, camY, camZ);
        orbitControls.target.set(0, 0, 0);
        orbitControls.update();

        // Save this model name so we don't refocus on sub-component toggles or parameter edits!
        lastFocusedModelRef.current = model.name;

        invoke("log_event", {
          level: "INFO",
          message: `Camera auto-focused for loaded HOD model "${model.name}": far=${camera.far.toFixed(0)} pos=[${camX.toFixed(1)},${camY.toFixed(1)},${camZ.toFixed(1)}]`,
        }).catch(console.error);
      }

      textureCacheRef.current.forEach((entry, key) => {
        if (!usedTextureKeys.has(key)) {
          entry.tex.dispose();
          textureCacheRef.current.delete(key);
        }
      });
      sceneVersionRef.current++;

      // One summary line per rebuild. Per-part/per-joint log_event calls used to
      // fire hundreds of synchronous IPC + log-file writes on every edit; do not
      // reintroduce logging inside the rebuild loops.
      invoke("log_event", {
        level: "INFO",
        message: `Viewport scene rebuilt in ${(performance.now() - rebuildStart).toFixed(1)}ms (meshes: ${model.meshes.length}, joints: ${model.joints.length}, textures cached: ${textureCacheRef.current.size})`,
      }).catch(console.error);
    } catch (e: any) {
      invoke("log_event", {
        level: "ERROR",
        message: `Exception thrown inside Viewport scene update useEffect: ${e.stack || e.toString()}`,
      }).catch(console.error);
    }
    // visibleMeshes is intentionally NOT a dependency: visibility toggles are
    // applied by the effect below without rebuilding geometry or textures.
  }, [model, isInitialized, showBoneLines, renderMode, teamColor, stripeColor, targetBoxes]);

  // Apply node/layer visibility by flipping `.visible` on already-built objects.
  useEffect(() => {
    const visible = visibleMeshes;
    const apply = (obj: THREE.Object3D) => {
      const keys: string[] | undefined = obj.userData.visKeys ?? (obj.userData.visKey ? [obj.userData.visKey] : undefined);
      if (keys) obj.visible = keys.every(key => visible[key] !== false);
    };
    [
      meshesGroupRef, jointsGroupRef, markersGroupRef, navLightsGroupRef, collisionGroupRef,
      dockpathsGroupRef, engineBurnsGroupRef, engineGlowsGroupRef, engineShapesGroupRef,
    ].forEach(ref => ref.current?.children.forEach(apply));
  }, [visibleMeshes, model, isInitialized, showBoneLines, renderMode, teamColor, stripeColor, targetBoxes]);

  // Hook visual gizmo to the selected node
  useEffect(() => {
    const transformControls = transformControlsRef.current;
    const scene = sceneRef.current;
    const jointsGroup = jointsGroupRef.current;
    const markersGroup = markersGroupRef.current;

    if (!transformControls || !scene || !jointsGroup || !markersGroup) return;

    transformControls.detach();

    // Force Three.js to immediately compute absolute world positions/matrices
    scene.updateMatrixWorld(true);

    // Remove old axes helper if attached to anything
    if (selectionAxesHelperRef.current && selectionAxesHelperRef.current.parent) {
      selectionAxesHelperRef.current.parent.remove(selectionAxesHelperRef.current);
    }

    if (selectedNode) {
      if (selectedNode.type === "joint" && selectedNode.name.toLowerCase() === "root") {
        transformControls.detach();
        return;
      }

      let targetObj: THREE.Object3D | undefined;

      if (selectedNode.type === "joint" || selectedNode.type === "engine_nozzle") {
        targetObj = jointsGroup.children.find(c => c.name.toLowerCase() === `joint:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "weapon_group") {
        targetObj = jointsGroup.children.find(c => c.name.toLowerCase() === `joint:${selectedNode.name}_position`.toLowerCase());
      } else if (selectedNode.type === "marker") {
        targetObj = markersGroup.children.find(c => c.name.toLowerCase() === `marker:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "navlight") {
        targetObj = navLightsGroupRef.current?.children.find(c => c.name.toLowerCase() === `navlight:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "dockpoint") {
        targetObj = dockpathsGroupRef.current?.children.find(c => c.name.toLowerCase() === `dockpoint:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "collision") {
        targetObj = collisionGroupRef.current?.children.find(c => c.name.toLowerCase() === `collision:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "engine_burn") {
        targetObj = engineBurnsGroupRef.current?.children.find(c => c.name.toLowerCase() === `engine_burn:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "engine_glow") {
        targetObj = engineGlowsGroupRef.current?.children.find(c => c.name.toLowerCase() === `engine_glow:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "engine_shape") {
        targetObj = engineShapesGroupRef.current?.children.find(c => c.name.toLowerCase() === `engine_shape:${selectedNode.name}`.toLowerCase());
      } else if (selectedNode.type === "keyframe") {
        const jointName = selectedNode.name.split(":")[0];
        targetObj = jointsGroup.children.find(c => c.name.toLowerCase() === `joint:${jointName}`.toLowerCase());
      }

      invoke("log_event", {
        level: "INFO",
        message: `Selection changed: type=${selectedNode.type}, name=${selectedNode.name}, foundTarget=${!!targetObj}, targetPos=${targetObj ? JSON.stringify(targetObj.position) : "none"}`
      }).catch(console.error);

      // Hidden nodes used to be absent from the scene; keep them un-pickable.
      if (targetObj && !targetObj.visible) targetObj = undefined;

      if (targetObj) {
        transformControls.attach(targetObj);

        // Highlight selected node by showing a smaller axes helper on its local orientation!
        const targetScale = 2.2 * (scaleFactorRef.current || 1.0);
        if (!selectionAxesHelperRef.current) {
          selectionAxesHelperRef.current = new THREE.AxesHelper(1.0);
        }
        selectionAxesHelperRef.current.position.set(0, 0, 0);
        selectionAxesHelperRef.current.rotation.set(0, 0, 0);
        selectionAxesHelperRef.current.scale.setScalar(targetScale);

        // Apply depth test bypass so it draws on top clearly
        const helperMaterial = selectionAxesHelperRef.current.material;
        if (Array.isArray(helperMaterial)) {
          helperMaterial.forEach(mat => {
            mat.depthTest = false;
            mat.transparent = true;
            mat.opacity = 0.95;
          });
        } else if (helperMaterial) {
          helperMaterial.depthTest = false;
          helperMaterial.transparent = true;
          helperMaterial.opacity = 0.95;
        }

        targetObj.add(selectionAxesHelperRef.current);
      }
    }
  }, [selectedNode, model, visibleMeshes]);

  return (
    <div className="viewport-container" style={{ position: "relative" }}>
      {/* ── Three.js exclusive container ── */}
      <div ref={mountRef} style={{ position: "absolute", top: 0, left: 0, right: 0, bottom: 0 }} />
      
      {/* ── React UI Elements ── */}
      <div
        style={{
          position: "absolute",
          top: "12px",
          left: "12px",
          display: "flex",
          gap: "8px",
          zIndex: 10,
        }}
      >
        <button
          className={transformMode === "translate" ? "primary" : ""}
          onClick={() => setTransformMode("translate")}
          title="Translate Gizmo"
        >
          Move
        </button>
        <button
          className={transformMode === "rotate" ? "primary" : ""}
          onClick={() => setTransformMode("rotate")}
          title="Rotate Gizmo"
        >
          Rotate
        </button>
        <div style={{ width: "1px", background: "rgba(255,255,255,0.15)", margin: "2px 2px" }} />
        <button
          className={coordinateSpace === "local" ? "primary" : ""}
          onClick={() => setCoordinateSpace("local")}
          title="Gizmo axes follow the selected node's local orientation"
        >
          Local
        </button>
        <button
          className={coordinateSpace === "global" ? "primary" : ""}
          onClick={() => setCoordinateSpace("global")}
          title="Gizmo axes stay aligned with world axes"
        >
          Global
        </button>
      </div>

      {model && model.animations && model.animations.length > 0 && (
        <div
          style={{
            position: "absolute",
            top: "12px",
            left: "50%",
            transform: "translateX(-50%)",
            background: "rgba(0, 230, 118, 0.10)",
            border: "1px solid rgba(0, 230, 118, 0.35)",
            borderRadius: "20px",
            padding: "3px 12px",
            display: "flex",
            alignItems: "center",
            gap: "6px",
            zIndex: 10,
            fontSize: "9px",
            fontWeight: "700",
            color: "#00e676",
            textTransform: "uppercase",
            letterSpacing: "0.1em",
            pointerEvents: "none",
          }}
        >
          <span style={{ display: "inline-block", width: "5px", height: "5px", borderRadius: "50%", background: "#00e676" }} />
          {model.animations.length} Anim{model.animations.length !== 1 ? "s" : ""} — See Dock Below
        </div>
      )}
    </div>
  );
};
