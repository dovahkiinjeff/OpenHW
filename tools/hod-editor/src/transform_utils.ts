import * as THREE from "three";
import type { HODModel, Matrix4D, Vector3D } from "./components/Viewport";

export type CoordinateSpace = "local" | "global";

const IDENTITY_MATRIX = new THREE.Matrix4();

export const IDENTITY_MATRIX4D: Matrix4D = {
  m: [
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 1, 0],
    [0, 0, 0, 1],
  ],
};

/**
 * HOD stores row-vector matrices (translation in m[3][0..2]). THREE uses
 * column-vector matrices, so the conversion is a transpose. This mirrors the
 * long-standing Viewport conversion, including recovery from singular basis
 * vectors produced by zeroed HOD 2.0 gimbal-limit fields.
 */
export function toThreeMatrix(matrix: Matrix4D): THREE.Matrix4 {
  const m = matrix?.m;
  if (!m || m.length < 4 || m.some((row) => !row || row.length < 4)) {
    return new THREE.Matrix4();
  }

  const s0 = Math.sqrt(m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]);
  const s1 = Math.sqrt(m[1][0] * m[1][0] + m[1][1] * m[1][1] + m[1][2] * m[1][2]);
  const s2 = Math.sqrt(m[2][0] * m[2][0] + m[2][1] * m[2][1] + m[2][2] * m[2][2]);

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
}

export function toHodMatrix(matrix: THREE.Matrix4): Matrix4D {
  const e = matrix.elements;
  return {
    m: [
      [e[0], e[1], e[2], e[3]],
      [e[4], e[5], e[6], e[7]],
      [e[8], e[9], e[10], e[11]],
      [e[12], e[13], e[14], e[15]],
    ],
  };
}

// Converts a HOD matrix to Euler rotation in degrees (XYZ order).
export const getEulerRotation = (m: number[][]): { x: number; y: number; z: number } => {
  if (!m || m.length < 4 || m.some((row) => !row || row.length < 4)) {
    return { x: 0, y: 0, z: 0 };
  }
  const matrix = new THREE.Matrix4().set(
    m[0][0], m[1][0], m[2][0], m[3][0],
    m[0][1], m[1][1], m[2][1], m[3][1],
    m[0][2], m[1][2], m[2][2], m[3][2],
    m[0][3], m[1][3], m[2][3], m[3][3]
  );
  const euler = new THREE.Euler().setFromRotationMatrix(matrix, "XYZ");
  return {
    x: Math.round(THREE.MathUtils.radToDeg(euler.x) * 100) / 100,
    y: Math.round(THREE.MathUtils.radToDeg(euler.y) * 100) / 100,
    z: Math.round(THREE.MathUtils.radToDeg(euler.z) * 100) / 100,
  };
};

// Builds a new 4x4 rotation matrix using new Euler angles in degrees and merges it with position.
export const updateMatrixRotation = (m: number[][], rot: { x: number; y: number; z: number }): number[][] => {
  const euler = new THREE.Euler(
    THREE.MathUtils.degToRad(rot.x),
    THREE.MathUtils.degToRad(rot.y),
    THREE.MathUtils.degToRad(rot.z),
    "XYZ"
  );
  const rotationMatrix = new THREE.Matrix4().makeRotationFromEuler(euler);
  const elements = rotationMatrix.elements;

  const next = m.map((row) => [...row]);
  next[0][0] = elements[0]; next[0][1] = elements[1]; next[0][2] = elements[2];
  next[1][0] = elements[4]; next[1][1] = elements[5]; next[1][2] = elements[6];
  next[2][0] = elements[8]; next[2][1] = elements[9]; next[2][2] = elements[10];
  return next;
};

export function eulerDegreesToRadians(rot: { x: number; y: number; z: number }): { x: number; y: number; z: number } {
  return {
    x: THREE.MathUtils.degToRad(rot.x),
    y: THREE.MathUtils.degToRad(rot.y),
    z: THREE.MathUtils.degToRad(rot.z),
  };
}

function findJoint(model: HODModel, name: string) {
  return model.joints.find((j) => j.name.toLowerCase() === name.toLowerCase());
}

/** Composes a joint's world matrix from its ancestors (root is identity). */
export function getJointGlobalMatrix(
  model: HODModel,
  jointName: string,
  visited = new Set<string>()
): THREE.Matrix4 {
  const key = jointName.toLowerCase();
  if (visited.has(key)) return new THREE.Matrix4();

  const joint = findJoint(model, jointName);
  if (!joint) return new THREE.Matrix4();

  const local = toThreeMatrix(joint.local_transform);
  const parentName = joint.parent_name;
  if (!parentName || parentName.toLowerCase() === "root" || parentName.toLowerCase() === key) {
    return local;
  }

  const nextVisited = new Set(visited);
  nextVisited.add(key);
  return getJointGlobalMatrix(model, parentName, nextVisited).multiply(local);
}

/** Local (parent-relative) matrix for any transform-bearing editor node. */
export function getNodeLocalMatrix(model: HODModel, type: string, name: string): THREE.Matrix4 {
  if (type === "joint" || type === "engine_nozzle" || type === "navlight") {
    const joint = findJoint(model, name);
    return joint ? toThreeMatrix(joint.local_transform) : new THREE.Matrix4();
  }

  if (type === "marker") {
    const marker = model.markers.find((m) => m.name.toLowerCase() === name.toLowerCase());
    if (!marker) return new THREE.Matrix4();
    const matrix = toThreeMatrix(marker.rotation);
    matrix.setPosition(marker.position.x, marker.position.y, marker.position.z);
    return matrix;
  }

  if (type === "weapon_group" || type === "turret_group") {
    const joint = findJoint(model, `${name}_Position`);
    return joint ? toThreeMatrix(joint.local_transform) : new THREE.Matrix4();
  }

  if (type === "dockpoint") {
    const [pathName, pointIndexRaw] = name.split(":");
    const pointIndex = parseInt(pointIndexRaw, 10);
    const path = model.dockpaths.find((p) => p.name.toLowerCase() === pathName.toLowerCase());
    const point = path?.points[pointIndex];
    if (!point) return new THREE.Matrix4();
    const matrix = toThreeMatrix(point.rotation);
    matrix.setPosition(point.position.x, point.position.y, point.position.z);
    return matrix;
  }

  return new THREE.Matrix4();
}

/** World matrix of the parent that a node's local transform is relative to. */
export function getNodeParentGlobalMatrix(model: HODModel, type: string, name: string): THREE.Matrix4 {
  let parentName = "";

  if (type === "joint" || type === "engine_nozzle") {
    parentName = findJoint(model, name)?.parent_name || "";
  } else if (type === "navlight") {
    parentName = findJoint(model, name)?.parent_name || "";
  } else if (type === "marker") {
    parentName = model.markers.find((m) => m.name.toLowerCase() === name.toLowerCase())?.parent_joint || "";
  } else if (type === "weapon_group" || type === "turret_group") {
    parentName = findJoint(model, `${name}_Position`)?.parent_name || "";
  } else if (type === "dockpoint") {
    const [pathName] = name.split(":");
    parentName = model.dockpaths.find((p) => p.name.toLowerCase() === pathName.toLowerCase())?.parent_name || "";
  } else if (type === "engine_burn") {
    parentName = model.engine_burns?.find((b) => b.name.toLowerCase() === name.toLowerCase())?.parent_name || "";
  } else if (type === "engine_glow") {
    parentName = model.engine_glows?.find((g) => g.name.toLowerCase() === name.toLowerCase())?.parent_name || "";
  } else if (type === "engine_shape") {
    parentName = model.engine_shapes?.find((s) => s.name.toLowerCase() === name.toLowerCase())?.parent_name || "";
  }

  if (!parentName || parentName.toLowerCase() === "root") {
    return new THREE.Matrix4();
  }
  return getJointGlobalMatrix(model, parentName);
}

export function getNodeGlobalMatrix(model: HODModel, type: string, name: string): THREE.Matrix4 {
  return getNodeParentGlobalMatrix(model, type, name).multiply(getNodeLocalMatrix(model, type, name));
}

export interface DisplayTransform {
  matrix: THREE.Matrix4;
  position: Vector3D;
  rotation: { x: number; y: number; z: number };
}

export function getDisplayTransform(
  model: HODModel,
  type: string,
  name: string,
  space: CoordinateSpace
): DisplayTransform {
  const local = getNodeLocalMatrix(model, type, name);
  const matrix = space === "global" ? getNodeParentGlobalMatrix(model, type, name).multiply(local) : local;
  const position = new THREE.Vector3().setFromMatrixPosition(matrix);
  return {
    matrix,
    position: { x: position.x, y: position.y, z: position.z },
    rotation: getEulerRotation(toHodMatrix(matrix).m),
  };
}

export interface TransformEditResult {
  position: Vector3D;
  matrix: Matrix4D;
}

function resultFromLocalMatrix(local: THREE.Matrix4): TransformEditResult {
  const position = new THREE.Vector3().setFromMatrixPosition(local);
  return {
    position: { x: position.x, y: position.y, z: position.z },
    matrix: toHodMatrix(local),
  };
}

/** Applies a single-axis position edit in the requested display space. */
export function applyPositionEdit(
  model: HODModel,
  type: string,
  name: string,
  space: CoordinateSpace,
  axis: "x" | "y" | "z",
  value: number
): TransformEditResult {
  const local = getNodeLocalMatrix(model, type, name);
  const localPosition = new THREE.Vector3().setFromMatrixPosition(local);

  if (space === "global") {
    const globalPosition = new THREE.Vector3()
      .setFromMatrixPosition(getNodeGlobalMatrix(model, type, name));
    globalPosition[axis] = value;
    const inverseParent = getNodeParentGlobalMatrix(model, type, name).clone().invert();
    localPosition.copy(globalPosition).applyMatrix4(inverseParent);
  } else {
    localPosition[axis] = value;
  }

  local.setPosition(localPosition);
  return resultFromLocalMatrix(local);
}

/** Applies a single-axis Euler rotation edit (degrees) in the requested display space. */
export function applyRotationEdit(
  model: HODModel,
  type: string,
  name: string,
  space: CoordinateSpace,
  axis: "x" | "y" | "z",
  value: number
): TransformEditResult {
  const local = getNodeLocalMatrix(model, type, name);
  const localHod = toHodMatrix(local);
  const displayRotation = space === "global"
    ? getEulerRotation(toHodMatrix(getNodeGlobalMatrix(model, type, name)).m)
    : getEulerRotation(localHod.m);
  const nextRotation = { ...displayRotation, [axis]: value };

  if (space === "local") {
    const next = updateMatrixRotation(localHod.m, nextRotation);
    const matrix = toThreeMatrix({ m: next });
    return resultFromLocalMatrix(matrix);
  }

  const globalPosition = new THREE.Vector3()
    .setFromMatrixPosition(getNodeGlobalMatrix(model, type, name));
  const worldRotation = new THREE.Matrix4().makeRotationFromEuler(
    new THREE.Euler(
      THREE.MathUtils.degToRad(nextRotation.x),
      THREE.MathUtils.degToRad(nextRotation.y),
      THREE.MathUtils.degToRad(nextRotation.z),
      "XYZ"
    )
  );
  worldRotation.setPosition(globalPosition);
  const inverseParent = getNodeParentGlobalMatrix(model, type, name).clone().invert();
  return resultFromLocalMatrix(inverseParent.multiply(worldRotation));
}

/** Converts a matrix rotation into the radians XYZ values stored in HODJoint.rotation. */
export function matrixRotationToHodRadians(matrix: Matrix4D): { x: number; y: number; z: number } {
  return eulerDegreesToRadians(getEulerRotation(matrix.m));
}

export { IDENTITY_MATRIX };
