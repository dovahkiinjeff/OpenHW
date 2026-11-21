import React, { useState, useEffect, useRef } from "react";
import * as THREE from "three";
import { invoke } from "../bridge";
import { renameAssemblyMember } from "../weapon_utils";
import { HODModel, Vector3D, HODNavLight, HODDockpoint, Matrix4D } from "./Viewport";
import { parseTextureGroups, SHADER_SLOTS, KNOWN_TYPES, getExpectedSlots, findTextureForSlot } from "../texture_utils";
import { parseMtl, resolveDiffuseTexture } from "../mtl_utils";
import { Info, Move, Navigation, Layers, Radio, Activity, Shield, Flame, RefreshCw, Palette, Download, Upload, Wrench, Plus, Eye, EyeOff, Box, Image, Trash2, FlipVertical } from "lucide-react";
import { WeaponGroupInspector } from "./WeaponGroupInspector";
import {
  CoordinateSpace,
  getDisplayTransform,
  getJointGlobalMatrix,
  applyPositionEdit,
  applyRotationEdit,
  getEulerRotation,
  updateMatrixRotation,
  matrixRotationToHodRadians,
} from "../transform_utils";

export { getEulerRotation, updateMatrixRotation };

interface InspectorProps {
  model: HODModel | null;
  selectedNode: { type: string; name: string } | null;
  onPositionChange: (name: string, type: string, pos: Vector3D, localMatrix?: Matrix4D) => void;
  coordinateSpace?: CoordinateSpace;
  setCoordinateSpace?: (space: CoordinateSpace) => void;
  onModelChange?: (updatedModel: HODModel) => void;
  onSelectedNodeChange?: (node: { type: string; name: string } | null) => void;
  selectedAnimIdx: number;
  visibleMeshes?: Record<string, boolean>;
  onToggleVisibility?: (meshKey: string) => void;
  setIsLoading?: React.Dispatch<React.SetStateAction<boolean>>;
  setStatusMsg?: React.Dispatch<React.SetStateAction<string>>;
}

const quatToEulerDegrees = (q: { x: number; y: number; z: number; w: number }) => {
  const tq = new THREE.Quaternion(q.x, q.y, q.z, q.w);
  const te = new THREE.Euler().setFromQuaternion(tq, "YXZ");
  return {
    x: (te.x * 180) / Math.PI,
    y: (te.y * 180) / Math.PI,
    z: (te.z * 180) / Math.PI,
  };
};

const eulerDegreesToQuat = (x: number, y: number, z: number) => {
  const te = new THREE.Euler(
    (x * Math.PI) / 180,
    (y * Math.PI) / 180,
    (z * Math.PI) / 180,
    "YXZ"
  );
  const tq = new THREE.Quaternion().setFromEuler(te);
  return { x: tq.x, y: tq.y, z: tq.z, w: tq.w };
};

// Helpers for color conversions
const hexToRgb = (hex: string): Vector3D => {
  const r = parseInt(hex.substring(1, 3), 16) / 255;
  const g = parseInt(hex.substring(3, 5), 16) / 255;
  const b = parseInt(hex.substring(5, 7), 16) / 255;
  return { x: r, y: g, z: b };
};

const rgbToHex = (color: Vector3D): string => {
  const r = Math.round(Math.max(0, Math.min(1, color.x)) * 255).toString(16).padStart(2, "0");
  const g = Math.round(Math.max(0, Math.min(1, color.y)) * 255).toString(16).padStart(2, "0");
  const b = Math.round(Math.max(0, Math.min(1, color.z)) * 255).toString(16).padStart(2, "0");
  return `#${r}${g}${b}`;
};

const handleNumericWheel = (e: React.WheelEvent<HTMLInputElement>, onChange: (val: string) => void, step = 1) => {
  e.preventDefault();
  const direction = e.deltaY < 0 ? 1 : -1;
  const currentVal = parseFloat(e.currentTarget.value) || 0;
  const stepStr = step.toString();
  const decimals = stepStr.includes(".") ? stepStr.split(".")[1].length : 0;
  const newVal = parseFloat((currentVal + direction * step).toFixed(Math.max(decimals, 3)));
  onChange(newVal.toString());
};



// Converts 4x4 matrix representation to Euler rotation in degrees (XYZ order).
// Re-exported from transform_utils so existing inspector imports keep working.

export interface NumericInputProps {
  value: number;
  onChange: (val: string) => void;
  onWheel?: (e: React.WheelEvent<HTMLInputElement>) => void;
  step?: string;
  min?: string;
  max?: string;
  style?: React.CSSProperties;
}

export const NumericInput: React.FC<NumericInputProps> = ({
  value,
  onChange,
  onWheel,
  step = "1",
  min,
  max,
  style,
}) => {
  const [localValue, setLocalValue] = useState(value.toString());
  const isFocusedRef = useRef(false);
  const isWheelingRef = useRef(false);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (!isFocusedRef.current || isWheelingRef.current) {
      setLocalValue(value.toString());
      isWheelingRef.current = false;
    }
  }, [value]);

  useEffect(() => {
    const el = inputRef.current;
    if (!el) return;
    
    const handleNativeWheel = (e: WheelEvent) => {
      if (onWheel) {
        // Prevent browser's native default scroll which would scroll the parent container
        e.preventDefault();
        e.stopPropagation();
        isWheelingRef.current = true;
        // Construct a synthetic-like event since onWheel expects React.WheelEvent
        // We can cast it or just pass it since React.WheelEvent is very similar for our use cases
        onWheel(e as unknown as React.WheelEvent<HTMLInputElement>);
      }
    };
    
    el.addEventListener("wheel", handleNativeWheel, { passive: false });
    return () => {
      el.removeEventListener("wheel", handleNativeWheel);
    };
  }, [onWheel]);

  const handleBlur = () => {
    isFocusedRef.current = false;
    onChange(localValue);
    setLocalValue(value.toString());
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.currentTarget.blur();
    }
  };

  return (
    <input
      ref={inputRef}
      type="number"
      step={step}
      min={min}
      max={max}
      value={localValue}
      onFocus={() => { isFocusedRef.current = true; }}
      onBlur={handleBlur}
      onKeyDown={handleKeyDown}
      onChange={(e) => setLocalValue(e.target.value)}
      style={style}
    />
  );
};

interface TextInputProps {
  value: string;
  onChange: (val: string) => void;
  style?: React.CSSProperties;
  placeholder?: string;
  className?: string;
}

const TextInput: React.FC<TextInputProps> = ({
  value,
  onChange,
  style,
  placeholder,
  className
}) => {
  const [localValue, setLocalValue] = useState(value);
  const isFocusedRef = useRef(false);

  useEffect(() => {
    if (!isFocusedRef.current) {
      setLocalValue(value);
    }
  }, [value]);

  const handleBlur = () => {
    isFocusedRef.current = false;
    onChange(localValue);
    setLocalValue(value); // Resets if parent rejected the change
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.currentTarget.blur();
    }
  };

  return (
    <input
      type="text"
      value={localValue}
      onFocus={() => { isFocusedRef.current = true; }}
      onBlur={handleBlur}
      onKeyDown={handleKeyDown}
      onChange={(e) => setLocalValue(e.target.value)}
      style={style}
      placeholder={placeholder}
      className={className}
    />
  );
};



interface MeshLODInspectorProps {
  model: HODModel;
  baseName: string;
  onModelChange?: (updatedModel: HODModel) => void;
  visibleMeshes?: Record<string, boolean>;
  onToggleVisibility?: (meshKey: string) => void;
  setIsLoading?: React.Dispatch<React.SetStateAction<boolean>>;
  setStatusMsg?: React.Dispatch<React.SetStateAction<string>>;
}


interface GlowLODInspectorProps {
  model: HODModel;
  baseName: string;
  onModelChange?: (updatedModel: HODModel) => void;
  visibleMeshes?: Record<string, boolean>;
  onToggleVisibility?: (meshKey: string) => void;
  setIsLoading?: React.Dispatch<React.SetStateAction<boolean>>;
  setStatusMsg?: React.Dispatch<React.SetStateAction<string>>;
}

const GlowLODInspector: React.FC<GlowLODInspectorProps> = ({ model, baseName, onModelChange, visibleMeshes, onToggleVisibility, setIsLoading, setStatusMsg }) => {
  const [selectedLodIdx, setSelectedLodIdx] = useState(0);
  const glowLods = (model.engine_glows || []).filter(g => {
    const gBase = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
    return gBase === baseName;
  }).sort((a, b) => a.lod - b.lod);
  const selectedLodGlow = glowLods[selectedLodIdx] || glowLods[0];

  const handleImportEngineGlowOBJ = async () => {
    if (!selectedLodGlow) return;
    try {
      // Use the registered `load_text_file` command (single file-picker +
      // content read). The previous `open_file_dialog` / `read_text_file`
      // commands were never registered in the Tauri `invoke_handler`, so the
      // first invoke always rejected and the picker never opened.
      // Do not reintroduce the two-step invoke without registering both
      // commands in `src-tauri/src/lib.rs`.
      const fileContents = await invoke<string | null>("load_text_file", {
        filters: ["obj"]
      });
      if (!fileContents) return;

      setIsLoading?.(true);
      setStatusMsg?.("Parsing Engine Glow OBJ...");

      const { OBJLoader } = await import("three/examples/jsm/loaders/OBJLoader.js");
      const loader = new OBJLoader();
      const objGroup = loader.parse(fileContents);

      const newParts: any[] = [];
      let totalTris = 0;
      let totalVerts = 0;

      objGroup.traverse((child) => {
        if ((child as THREE.Mesh).isMesh) {
          const mesh = child as THREE.Mesh;
          const geom = mesh.geometry;
          if (!geom.attributes.position) return;

          const positions = geom.attributes.position.array;
          const normals = geom.attributes.normal?.array;
          const uvs = geom.attributes.uv?.array;

          const indices = geom.index ? Array.from(geom.index.array) : Array.from({ length: positions.length / 3 }, (_, i) => i);

          const vertices = [];
          for (let i = 0; i < positions.length; i += 3) {
            vertices.push({
              position: { x: positions[i], y: positions[i + 1], z: positions[i + 2] },
              normal: normals ? { x: normals[i], y: normals[i + 1], z: normals[i + 2] } : { x: 0, y: 1, z: 0 },
              uv: uvs ? { u: uvs[(i / 3) * 2], v: 1 - uvs[(i / 3) * 2 + 1] } : { u: 0, v: 0 }
            });
          }

          totalTris += indices.length / 3;
          totalVerts += vertices.length;

          newParts.push({ material_index: 0, vertices, indices });
        }
      });

      if (newParts.length === 0) {
        alert("No valid mesh found in OBJ.");
        setIsLoading?.(false);
        return;
      }

      const updatedGlows = model.engine_glows.map(g => {
        if (g.name === selectedLodGlow.name && g.lod === selectedLodGlow.lod) {
          return { ...g, mesh: { ...g.mesh, parts: newParts } };
        }
        return g;
      });

      onModelChange?.({ ...model, engine_glows: updatedGlows });
      setIsLoading?.(false);
      invoke("log_event", { level: "INFO", message: `Imported OBJ into Engine Glow: ${selectedLodGlow.name} (LOD ${selectedLodGlow.lod})` }).catch(console.error);
    } catch (e) {
      console.error(e);
      setIsLoading?.(false);
      alert("Failed to import Engine Glow OBJ.");
    }
  };

  const handleExportEngineGlowOBJ = async () => {
    if (!selectedLodGlow) return;
    try {
      setIsLoading?.(true);
      setStatusMsg?.("Exporting Glow OBJ...");
      const { OBJExporter } = await import("three/examples/jsm/exporters/OBJExporter.js");
      const exporter = new OBJExporter();
      const group = new THREE.Group();
      group.name = selectedLodGlow.name;
      selectedLodGlow.mesh.parts.forEach((part: any, pIdx: number) => {
        const geometry = new THREE.BufferGeometry();
        const vertices: number[] = [];
        part.vertices.forEach((v: any) => vertices.push(v.position.x, v.position.y, v.position.z));
        geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
        geometry.setIndex(part.indices);
        const meshObj = new THREE.Mesh(geometry, new THREE.MeshBasicMaterial());
        meshObj.name = `${selectedLodGlow.name}_part_${pIdx}`;
        group.add(meshObj);
      });
      const objResult = exporter.parse(group);
      const objFilename = `${selectedLodGlow.name}_lod${selectedLodGlow.lod}`;
      await invoke<string | null>("save_text_file", { defaultName: `${objFilename}.obj`, filters: ["obj"], contents: objResult });
      setIsLoading?.(false);
      invoke("log_event", { level: "INFO", message: `Exported Engine Glow OBJ: ${objFilename}` }).catch(console.error);
    } catch (e) {
      console.error(e);
      setIsLoading?.(false);
      alert("Failed to export Glow OBJ.");
    }
  };

  const handleAddLod = () => {
    const maxLod = Math.max(...glowLods.map(m => m.lod), -1);
    const newGlowName = `${baseName}_LOD${maxLod + 1}`;
    onModelChange?.({ ...model, engine_glows: [...model.engine_glows, { name: newGlowName, parent_name: glowLods[0]?.parent_name || "Root", lod: maxLod + 1, mesh: { name: newGlowName, parent_name: glowLods[0]?.parent_name || "Root", lod: maxLod + 1, parts: [{ material_index: 0, vertex_mask: 0x600B, vertices: [], indices: [] }] } }] });
    setSelectedLodIdx(glowLods.length);
  };

  const handleDeleteLod = (idx: number) => {
    if (glowLods.length <= 1) return;
    const target = glowLods[idx];
    onModelChange?.({ ...model, engine_glows: model.engine_glows.filter(m => !(m.name === target.name && m.lod === target.lod)) });
    setSelectedLodIdx(Math.max(0, idx - 1));
  };

  const handleMoveLod = (fromIdx: number, direction: -1 | 1) => {
    const toIdx = fromIdx + direction;
    if (toIdx < 0 || toIdx >= glowLods.length) return;
    const fromGlow = glowLods[fromIdx];
    const toGlow = glowLods[toIdx];
    onModelChange?.({ ...model, engine_glows: model.engine_glows.map(m => {
      if (m.name === fromGlow.name && m.lod === fromGlow.lod) return { ...m, lod: toGlow.lod, name: `${baseName}_LOD${toGlow.lod}` };
      if (m.name === toGlow.name && m.lod === toGlow.lod) return { ...m, lod: fromGlow.lod, name: `${baseName}_LOD${fromGlow.lod}` };
      return m;
    }) });
    setSelectedLodIdx(toIdx);
  };

  if (!selectedLodGlow) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Engine Glow not found</div>;
  const totalVerts = selectedLodGlow.mesh.parts.reduce((sum, p) => sum + p.vertices.length, 0);
  const totalTris = selectedLodGlow.mesh.parts.reduce((sum, p) => sum + p.indices.length, 0) / 3;

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
      <div>
        <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
          <span style={{ fontSize: "12px" }}>🔥</span>
          <span>Selected Engine Glow</span>
        </div>
        <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
          {baseName}
        </div>
      </div>
      <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
      <div>
        <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "4px" }}>Parent Joint</div>
        <div style={{ background: "rgba(255,255,255,0.03)", padding: "8px 10px", borderRadius: "4px", fontSize: "12px", border: "1px solid var(--border-color)" }}>
          {selectedLodGlow.parent_name}
        </div>
      </div>

      <div>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "6px" }}>
          <label style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500" }}>LOD Variants</label>
          <button onClick={handleAddLod} style={{ height: "24px", fontSize: "11px", padding: "0 8px", display: "flex", alignItems: "center", gap: "4px" }}><Plus size={12} /> Add LOD</button>
        </div>
        <div style={{ display: "flex", flexDirection: "column", gap: "4px", background: "rgba(0,0,0,0.15)", padding: "8px", borderRadius: "4px" }}>
          {glowLods.map((lm, idx) => {
            const isSelected = selectedLodIdx === idx;
            const lodKey = `engine_glow:${lm.name}`;
            const isLodVisible = visibleMeshes?.[lodKey] !== false;
            return (
              <div key={`${lm.name}_${lm.lod}`} onClick={() => setSelectedLodIdx(idx)}
                style={{ display: "flex", alignItems: "center", justifyContent: "space-between", padding: "6px 8px", borderRadius: "3px", cursor: "pointer", background: isSelected ? "rgba(22, 160, 255, 0.15)" : "transparent", border: isSelected ? "1px solid rgba(22, 160, 255, 0.3)" : "1px solid transparent" }}>
                <span style={{ fontSize: "12px", color: isSelected ? "var(--accent-cyan)" : "var(--text-primary)" }}>
                  LOD {lm.lod} <span style={{ color: "var(--text-muted)", fontSize: "10px" }}>({lm.mesh.parts.reduce((s, p) => s + p.vertices.length, 0)} verts)</span>
                </span>
                <div style={{ display: "flex", gap: "2px", alignItems: "center" }}>
                  {onToggleVisibility && (
                    <span onClick={(e) => { e.stopPropagation(); onToggleVisibility(lodKey); }}
                      style={{ padding: "2px 4px", cursor: "pointer", color: isLodVisible ? "var(--accent-cyan)" : "var(--text-muted)", display: "inline-flex", alignItems: "center" }}
                      title={isLodVisible ? "Hide LOD" : "Show LOD"}>
                      {isLodVisible ? <Eye size={12} /> : <EyeOff size={12} />}
                    </span>
                  )}
                  <button onClick={(e) => { e.stopPropagation(); handleMoveLod(idx, -1); }} disabled={idx === 0} style={{ padding: "2px 4px", fontSize: "10px" }}>▲</button>
                  <button onClick={(e) => { e.stopPropagation(); handleMoveLod(idx, 1); }} disabled={idx === glowLods.length - 1} style={{ padding: "2px 4px", fontSize: "10px" }}>▼</button>
                  <button onClick={(e) => { e.stopPropagation(); handleDeleteLod(idx); }} disabled={glowLods.length <= 1} style={{ padding: "2px 4px", fontSize: "10px", color: "#f44" }}>✕</button>
                </div>
              </div>
            );
          })}
        </div>
      </div>

      <div>
        <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "8px" }}>Mesh Statistics (LOD {selectedLodGlow.lod})</div>
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px", background: "rgba(22, 160, 255, 0.03)", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "12px" }}>
          <div>
            <div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>TRIANGLES</div>
            <div style={{ fontSize: "15px", fontWeight: "600", color: "var(--text-primary)" }}>{totalTris}</div>
          </div>
          <div>
            <div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>VERTICES</div>
            <div style={{ fontSize: "15px", fontWeight: "600", color: "var(--text-primary)" }}>{totalVerts}</div>
          </div>
        </div>
      </div>

      <div style={{ display: "flex", gap: "8px" }}>
        <button onClick={handleImportEngineGlowOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
          <Download size={14} /> Import OBJ
        </button>
        <button onClick={handleExportEngineGlowOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
          <Upload size={14} /> Export OBJ
        </button>
      </div>
    </div>
  );
};

const MeshLODInspector: React.FC<MeshLODInspectorProps> = ({ model, baseName, onModelChange, visibleMeshes, onToggleVisibility, setIsLoading, setStatusMsg }) => {
  const [selectedLodIdx, setSelectedLodIdx] = useState(0);
  const lodMeshes = model.meshes.filter(m => {
    const mBase = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
    return mBase === baseName;
  }).sort((a, b) => a.lod - b.lod);
  const selectedLodMesh = lodMeshes[selectedLodIdx] || lodMeshes[0];

  const buildThreeMeshFromState = (m: any) => {
    const group = new THREE.Group();
    group.name = m.name;
    m.parts.forEach((part: any, pIdx: number) => {
      const geometry = new THREE.BufferGeometry();
      const vertices: number[] = [];
      const normals: number[] = [];
      const uvs: number[] = [];
      part.vertices.forEach((v: any) => {
        vertices.push(v.position.x, v.position.y, v.position.z);
        normals.push(v.normal ? v.normal.x : 0, v.normal ? v.normal.y : 1, v.normal ? v.normal.z : 0);
        uvs.push(v.uv ? v.uv.u : 0, v.uv ? 1 - v.uv.v : 0);
      });
      geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
      if (normals.length > 0) geometry.setAttribute("normal", new THREE.Float32BufferAttribute(normals, 3));
      if (uvs.length > 0) geometry.setAttribute("uv", new THREE.Float32BufferAttribute(uvs, 2));
      geometry.setIndex(part.indices);
      const matName = model.materials?.[part.material_index]?.name || `material_${part.material_index}`;
      const meshObj = new THREE.Mesh(geometry, new THREE.MeshStandardMaterial({ name: matName }));
      meshObj.name = `${m.name}_part_${pIdx}`;
      group.add(meshObj);
    });
    return group;
  };

  const handleExportOBJ = async () => {
    if (!selectedLodMesh) return;
    try {
      setIsLoading?.(true);
      setStatusMsg?.("Exporting OBJ and MTL...");
      const { OBJExporter } = await import("three/examples/jsm/exporters/OBJExporter.js");
      const exporter = new OBJExporter();
      const tempGroup = buildThreeMeshFromState(selectedLodMesh);
      let objResult = exporter.parse(tempGroup);
      const objFilename = `${selectedLodMesh.name}_lod${selectedLodMesh.lod}`;
      objResult = `mtllib ${objFilename}.mtl\n` + objResult;
      const savedPath = await invoke<string | null>("save_text_file", { defaultName: `${objFilename}.obj`, filters: ["obj"], contents: objResult });
      if (savedPath) {
        setStatusMsg?.("Saving materials...");
        const mtlLines: string[] = [];
        selectedLodMesh.parts.forEach((part) => {
          const hMat = model.materials?.[part.material_index];
          const matName = hMat?.name || `material_${part.material_index}`;
          mtlLines.push(`newmtl ${matName}`, `Kd 1.0 1.0 1.0`, `Ka 1.0 1.0 1.0`, `Ks 0.1 0.1 0.1`, `Ns 32`);
          if (hMat?.texture_maps?.[0]) mtlLines.push(`map_Kd ${hMat.texture_maps[0]}.tga`);
          mtlLines.push("");
        });
        const lastSlash = Math.max(savedPath.lastIndexOf("/"), savedPath.lastIndexOf("\\"));
        const folderPath = lastSlash !== -1 ? savedPath.substring(0, lastSlash) : ".";
        await invoke("save_text_file", { defaultName: `${objFilename}.mtl`, filters: ["mtl"], contents: mtlLines.join("\n") });
        if (model.textures?.length) await invoke("export_textures_tga", { folderPath, textures: model.textures });
        alert(`Mesh exported to:\n${folderPath}`);
      }
    } catch (e: any) { console.error(e); alert(`Export failed: ${e.toString()}`); }
    finally { setIsLoading?.(false); }
  };

  const handleImportOBJ = async () => {
    if (!selectedLodMesh) return;
    try {
      const fileContent = await invoke<string | null>("load_text_file", { filters: ["obj"] });
      if (!fileContent) return;
      
      setIsLoading?.(true);
      setStatusMsg?.("Importing OBJ...");
      
      const { OBJLoader } = await import("three/examples/jsm/loaders/OBJLoader.js");
      const objGroup = new OBJLoader().parse(fileContent);
      const newParts: any[] = [];
      const nextMaterials = [...(model.materials || [])];
      const currentMaterial = model.materials?.[selectedLodMesh.parts[0]?.material_index ?? 0];
      const defaultShader = currentMaterial?.shader_name || "ship";
      const defaultSlotCount = Math.max(SHADER_SLOTS[defaultShader.toLowerCase()]?.length || 4, 4);
      let createdMaterials = 0;

      // OBJLoader exposes usemtl names on child.material. Reuse an existing HOD material
      // with the same name, or create a placeholder that a later MTL import can populate.
      const ensureMaterialIndex = (rawName: string): number => {
        const name = rawName.trim();
        if (!name) return nextMaterials.length > 0 ? 0 : ensureMaterialIndex("material_0");

        const existingIndex = nextMaterials.findIndex((m) => m.name.toLowerCase() === name.toLowerCase());
        if (existingIndex >= 0) return existingIndex;

        nextMaterials.push({
          name,
          shader_name: defaultShader,
          texture_maps: new Array(defaultSlotCount).fill(""),
          parameters: [],
          params: []
        });
        createdMaterials++;
        return nextMaterials.length - 1;
      };

      const buildPart = (geo: THREE.BufferGeometry, start: number, count: number, materialIndex: number) => {
        const posAttr = geo.attributes.position;
        const normalAttr = geo.attributes.normal;
        const uvAttr = geo.attributes.uv;
        const indexAttr = geo.index;
        const vertices: any[] = [];
        const indices: number[] = [];

        for (let i = 0; i < count; i++) {
          const sourceIndex = indexAttr ? indexAttr.getX(start + i) : start + i;
          vertices.push({
            position: { x: posAttr.getX(sourceIndex), y: posAttr.getY(sourceIndex), z: posAttr.getZ(sourceIndex) },
            normal: normalAttr ? { x: normalAttr.getX(sourceIndex), y: normalAttr.getY(sourceIndex), z: normalAttr.getZ(sourceIndex) } : { x: 0, y: 1, z: 0 },
            uv: uvAttr ? { u: uvAttr.getX(sourceIndex), v: 1 - uvAttr.getY(sourceIndex) } : { u: 0, v: 0 },
            tangent: { x: 1, y: 0, z: 0 }, binormal: { x: 0, y: 0, z: 1 }
          });
          indices.push(i);
        }

        return { material_index: materialIndex, vertex_mask: 0x600B, vertices, indices };
      };

      objGroup.traverse((child: any) => {
        if (child.isMesh) {
          const geo = (child as THREE.Mesh).geometry;
          if (!geo?.attributes.position) return;

          const materialArray = Array.isArray(child.material) ? child.material : null;
          if (materialArray && geo.groups.length > 0) {
            geo.groups.forEach((group) => {
              const sourceMaterial = materialArray[group.materialIndex ?? 0];
              const materialIndex = ensureMaterialIndex(sourceMaterial?.name || "");
              newParts.push(buildPart(geo, group.start, group.count, materialIndex));
            });
          } else {
            const sourceMaterial = materialArray ? materialArray[0] : child.material;
            const materialIndex = ensureMaterialIndex(sourceMaterial?.name || "");
            newParts.push(buildPart(geo, 0, geo.index?.count ?? geo.attributes.position.count, materialIndex));
          }
        }
      });

      if (newParts.length > 0) {
        onModelChange?.({
          ...model,
          materials: nextMaterials,
          meshes: model.meshes.map(m => m.name === selectedLodMesh.name && m.lod === selectedLodMesh.lod ? { ...m, parts: newParts } : m)
        });
        const materialNote = createdMaterials > 0 ? ` ${createdMaterials} new material(s) created from usemtl names; import the .mtl to fill their textures.` : "";
        alert(`Mesh "${selectedLodMesh.name}" LOD ${selectedLodMesh.lod} replaced by imported OBJ.${materialNote}`);
      } else { alert("No valid geometries found in the OBJ file."); }
    } catch (e: any) { console.error(e); alert(`Import failed: ${e.toString()}`); }
    finally { setIsLoading?.(false); }
  };

  const handleImportMTL = async () => {
    if (!selectedLodMesh) return;
    try {
      const fileContent = await invoke<string | null>("load_text_file", { filters: ["mtl"] });
      if (!fileContent) return;

      setIsLoading?.(true);
      setStatusMsg?.("Importing MTL materials...");

      const definitions = parseMtl(fileContent);
      if (definitions.length === 0) {
        alert("No materials were found in the selected MTL file.");
        return;
      }

      const nextMaterials = [...(model.materials || [])];
      const currentMaterial = model.materials?.[selectedLodMesh.parts[0]?.material_index ?? 0];
      const defaultShader = currentMaterial?.shader_name || "ship";
      const defaultSlotCount = Math.max(SHADER_SLOTS[defaultShader.toLowerCase()]?.length || 4, 4);
      const unresolvedTextures: string[] = [];
      let created = 0;
      let updated = 0;

      definitions.forEach((definition) => {
        let materialIndex = nextMaterials.findIndex((m) => m.name.toLowerCase() === definition.name.toLowerCase());
        let material = materialIndex >= 0 ? nextMaterials[materialIndex] : null;

        if (!material) {
          material = {
            name: definition.name,
            shader_name: defaultShader,
            texture_maps: new Array(defaultSlotCount).fill(""),
            parameters: [],
            params: []
          };
          nextMaterials.push(material);
          materialIndex = nextMaterials.length - 1;
          created++;
        } else {
          updated++;
        }

        if (definition.diffuseMap) {
          const textureName = resolveDiffuseTexture(definition.diffuseMap, model.textures || []);
          if (textureName) {
            const textureMaps = [...(material.texture_maps || [])];
            while (textureMaps.length < defaultSlotCount) textureMaps.push("");
            textureMaps[0] = textureName;
            nextMaterials[materialIndex] = { ...material, texture_maps: textureMaps };
          } else {
            unresolvedTextures.push(`${definition.name}: ${definition.diffuseMap}`);
          }
        }
      });

      onModelChange?.({ ...model, materials: nextMaterials });
      invoke("log_event", { level: "INFO", message: `Imported MTL: ${created} material(s) added, ${updated} updated.` }).catch(console.error);

      const lines = [`MTL imported: ${created} material(s) added, ${updated} updated.`];
      if (unresolvedTextures.length > 0) {
        lines.push("", "These diffuse maps were not found among the loaded HOD textures:", ...unresolvedTextures, "", "Import the matching TGA textures first, then import the MTL again.");
      }
      alert(lines.join("\n"));
    } catch (e: any) { console.error(e); alert(`MTL import failed: ${e.toString()}`); }
    finally { setIsLoading?.(false); }
  };

  const handleAddLod = () => {
    const maxLod = Math.max(...lodMeshes.map(m => m.lod), -1);
    onModelChange?.({ ...model, meshes: [...model.meshes, { name: baseName, parent_name: lodMeshes[0]?.parent_name || "Root", lod: maxLod + 1, parts: [{ material_index: 0, vertex_mask: 0x600B, vertices: [], indices: [] }] }] });
    setSelectedLodIdx(lodMeshes.length);
  };

  const handleDeleteLod = (idx: number) => {
    if (lodMeshes.length <= 1) return;
    const target = lodMeshes[idx];
    onModelChange?.({ ...model, meshes: model.meshes.filter(m => !(m.name === target.name && m.lod === target.lod)) });
    setSelectedLodIdx(Math.max(0, idx - 1));
  };

  const handleMoveLod = (fromIdx: number, direction: -1 | 1) => {
    const toIdx = fromIdx + direction;
    if (toIdx < 0 || toIdx >= lodMeshes.length) return;
    const fromMesh = lodMeshes[fromIdx];
    const toMesh = lodMeshes[toIdx];
    onModelChange?.({ ...model, meshes: model.meshes.map(m => {
      if (m.name === fromMesh.name && m.lod === fromMesh.lod) return { ...m, lod: toMesh.lod };
      if (m.name === toMesh.name && m.lod === toMesh.lod) return { ...m, lod: fromMesh.lod };
      return m;
    }) });
    setSelectedLodIdx(toIdx);
  };

  if (!selectedLodMesh) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Mesh not found</div>;

  const totalVerts = selectedLodMesh.parts.reduce((s, p) => s + p.vertices.length, 0);
  const totalIndices = selectedLodMesh.parts.reduce((s, p) => s + p.indices.length, 0);

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
      <div>
        <div style={{ fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>Mesh</div>
        <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>{baseName}</div>
      </div>
      <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
      <div>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "6px" }}>
          <label style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500" }}>LOD Variants</label>
          <button onClick={handleAddLod} style={{ height: "24px", fontSize: "11px", padding: "0 8px", display: "flex", alignItems: "center", gap: "4px" }}><Plus size={12} /> Add LOD</button>
        </div>
        <div style={{ display: "flex", flexDirection: "column", gap: "4px", background: "rgba(0,0,0,0.15)", padding: "8px", borderRadius: "4px" }}>
          {lodMeshes.map((lm, idx) => {
            const isSelected = selectedLodIdx === idx;
            const lodKey = `${lm.name}_lod_${lm.lod}`;
            const isLodVisible = visibleMeshes?.[lodKey] !== false;
            return (
              <div key={lodKey} onClick={() => setSelectedLodIdx(idx)}
                style={{ display: "flex", alignItems: "center", justifyContent: "space-between", padding: "6px 8px", borderRadius: "3px", cursor: "pointer", background: isSelected ? "rgba(22, 160, 255, 0.15)" : "transparent", border: isSelected ? "1px solid rgba(22, 160, 255, 0.3)" : "1px solid transparent" }}>
                <span style={{ fontSize: "12px", color: isSelected ? "var(--accent-cyan)" : "var(--text-primary)" }}>
                  LOD {lm.lod} <span style={{ color: "var(--text-muted)", fontSize: "10px" }}>({lm.parts.reduce((s, p) => s + p.vertices.length, 0)} verts)</span>
                </span>
                <div style={{ display: "flex", gap: "2px", alignItems: "center" }}>
                  {onToggleVisibility && (
                    <span onClick={(e) => { e.stopPropagation(); onToggleVisibility(lodKey); }}
                      style={{ padding: "2px 4px", cursor: "pointer", color: isLodVisible ? "var(--accent-cyan)" : "var(--text-muted)", display: "inline-flex", alignItems: "center" }}
                      title={isLodVisible ? "Hide LOD" : "Show LOD"}>
                      {isLodVisible ? <Eye size={12} /> : <EyeOff size={12} />}
                    </span>
                  )}
                  <button onClick={(e) => { e.stopPropagation(); handleMoveLod(idx, -1); }} disabled={idx === 0} style={{ padding: "2px 4px", fontSize: "10px" }}>▲</button>
                  <button onClick={(e) => { e.stopPropagation(); handleMoveLod(idx, 1); }} disabled={idx === lodMeshes.length - 1} style={{ padding: "2px 4px", fontSize: "10px" }}>▼</button>
                  <button onClick={(e) => { e.stopPropagation(); handleDeleteLod(idx); }} disabled={lodMeshes.length <= 1} style={{ padding: "2px 4px", fontSize: "10px", color: "#f44" }}>✕</button>
                </div>
              </div>
            );
          })}
        </div>
      </div>
      {selectedLodMesh && (
        <>
          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
          <div>
            <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>3D Model (LOD {selectedLodMesh.lod})</label>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px" }}>
              <button onClick={handleExportOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px" }}><Upload size={14} style={{ color: "var(--accent-blue)" }} /> Export OBJ</button>
              <button onClick={handleImportOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px" }}><Download size={14} style={{ color: "var(--accent-cyan)" }} /> Import OBJ</button>
              <button onClick={handleImportMTL} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", gridColumn: "1 / -1" }}><Palette size={14} style={{ color: "var(--accent-cyan)" }} /> Import MTL Materials</button>
            </div>
          </div>
          <div style={{ background: "rgba(22, 160, 255, 0.05)", border: "1px solid rgba(22, 160, 255, 0.15)", borderRadius: "4px", padding: "8px 10px", fontSize: "11px", color: "var(--text-secondary)", lineHeight: "1.4" }}>
            <span style={{ fontWeight: "600", color: "var(--accent-cyan)", marginRight: "4px" }}>Positioning Note:</span>
            Meshes inherit position from their parent joint bone (<strong>{selectedLodMesh.parent_name || "Root"}</strong>). Drag this mesh node onto a joint in the node tree to re-parent.
          </div>
          {model.materials?.length > 0 && (
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>Material Assignment</label>
              <div style={{ display: "flex", flexDirection: "column", gap: "6px", background: "rgba(0,0,0,0.15)", padding: "8px", borderRadius: "4px" }}>
                {selectedLodMesh.parts.map((part, pIdx) => (
                  <div key={pIdx} style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: "8px" }}>
                    <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>Part {pIdx}</span>
                    <select value={part.material_index} onChange={(e) => {
                      const newMatIdx = parseInt(e.target.value, 10);
                      onModelChange?.({ ...model, meshes: model.meshes.map(m => m.name === selectedLodMesh.name && m.lod === selectedLodMesh.lod ? { ...m, parts: m.parts.map((p, i) => i === pIdx ? { ...p, material_index: newMatIdx } : p) } : m) });
                    }} style={{ width: "160px", height: "26px", padding: "2px 6px", fontSize: "11px" }}>
                      {model.materials.map((mat, mIdx) => (<option key={mIdx} value={mIdx}>{mat.name}</option>))}
                    </select>
                  </div>
                ))}
              </div>
            </div>
          )}
          <div>
            <div style={{ fontSize: "11px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "6px" }}>Geometry Info</div>
            <div style={{ background: "rgba(255,255,255,0.02)", border: "1px solid var(--border-color)", padding: "10px", borderRadius: "4px", display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "10px" }}>
              <div><div style={{ fontSize: "10px", color: "var(--text-muted)" }}>PARTS</div><div style={{ fontSize: "14px", fontWeight: "600" }}>{selectedLodMesh.parts.length}</div></div>
              <div><div style={{ fontSize: "10px", color: "var(--text-muted)" }}>TRIANGLES</div><div style={{ fontSize: "14px", fontWeight: "600" }}>{totalIndices / 3}</div></div>
              <div><div style={{ fontSize: "10px", color: "var(--text-muted)" }}>VERTICES</div><div style={{ fontSize: "14px", fontWeight: "600" }}>{totalVerts}</div></div>
            </div>
          </div>
        </>
      )}
    </div>
  );
};

export const Inspector: React.FC<InspectorProps> = ({
  model,
  selectedNode,
  onPositionChange,
  onModelChange,
  onSelectedNodeChange,
  selectedAnimIdx,
  visibleMeshes,
  onToggleVisibility,
  setIsLoading,
  setStatusMsg,
  coordinateSpace = "local",
  setCoordinateSpace,
}) => {
  const [pipelines, setPipelines] = useState<string[]>([]);
  const [renameWeaponName, setRenameWeaponName] = useState("");
  const [selectedSourceMeshes, setSelectedSourceMeshes] = useState<string[]>([]);

  const collisionSourceMeshOptions = (() => {
    if (!model?.meshes) return [];
    const grouped = new Map<string, typeof model.meshes>();
    for (const mesh of model.meshes) {
      const baseName = mesh.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
      if (!grouped.has(baseName)) grouped.set(baseName, []);
      grouped.get(baseName)!.push(mesh);
    }
    return Array.from(grouped.entries()).map(([baseName, lods]) => {
      const sorted = [...lods].sort((a, b) => a.lod - b.lod);
      const primaryMesh =
        sorted.find((m) => m.parts.some((p) => p.vertices.length > 0)) || sorted[0];
      const vertexCount = primaryMesh.parts.reduce((sum, p) => sum + p.vertices.length, 0);
      return {
        baseName,
        mesh: primaryMesh,
        vertexCount,
        parentName: primaryMesh.parent_name || "Root",
      };
    });
  })();

  const activeSelectedSourceMeshes = selectedSourceMeshes.filter((name) =>
    collisionSourceMeshOptions.some((opt) => opt.baseName === name)
  );

  const loadPipelines = async () => {
    try {
      const config = await invoke<{ shader_directories: string[] }>("load_shader_config");
      if (config.shader_directories.length > 0) {
        const list = await invoke<string[]>("get_shader_pipelines", { keeperPaths: config.shader_directories });
        setPipelines(list);
      } else {
        setPipelines([]);
      }
    } catch (e) {
      console.error("Failed to load shader pipelines:", e);
    }
  };

  useEffect(() => {
    loadPipelines();
  }, []);

  // Keep shader names already assigned on loaded materials selectable even when
  // the configured shader directory does not expose that exact alias.
  const shaderOptions = (() => {
    const names: string[] = [];
    const candidates = [
      ...(model?.materials || []).map((m) => m.shader_name),
      ...pipelines,
    ];
    for (const raw of candidates) {
      const name = (raw || "").trim();
      if (!name) continue;
      if (names.some((existing) => existing.toLowerCase() === name.toLowerCase())) continue;
      names.push(name);
    }
    return names.sort((a, b) => a.localeCompare(b));
  })();


  // Generic handlers

  const handleNavLightChange = (fieldName: keyof HODNavLight, value: any) => {
    if (!model || !selectedNode || selectedNode.type !== "navlight") return;
    const updatedNavs = model.nav_lights.map((nav) => {
      if (nav.name === selectedNode.name) {
        return { ...nav, [fieldName]: value };
      }
      return nav;
    });
    if (onModelChange) {
      onModelChange({ ...model, nav_lights: updatedNavs });
    }
  };

  const handleDockpointChange = (pathName: string, ptIndex: number, fieldName: keyof HODDockpoint | "x" | "y" | "z", value: any) => {
    if (!model) return;
    const updatedPaths = model.dockpaths.map((dp) => {
      if (dp.name === pathName) {
        const updatedPoints = dp.points.map((pt, idx) => {
          if (idx === ptIndex) {
            if (fieldName === "x" || fieldName === "y" || fieldName === "z") {
              const result = applyPositionEdit(model, "dockpoint", `${pathName}:${ptIndex}`, coordinateSpace, fieldName, value);
              // Report coordinates to position change to update gizmos too if needed
              onPositionChange(`${pathName}:${ptIndex}`, "dockpoint", result.position, result.matrix);
              return { ...pt, position: result.position, rotation: result.matrix };
            } else {
              return { ...pt, [fieldName]: value };
            }
          }
          return pt;
        });
        return { ...dp, points: updatedPoints };
      }
      return dp;
    });
    if (onModelChange) {
      onModelChange({ ...model, dockpaths: updatedPaths });
    }
  };

  const handleCollisionChange = (fieldName: "min_extents" | "max_extents" | "center" | "radius" | "x" | "y" | "z", axis?: "x" | "y" | "z", value?: any) => {
    if (!model || !selectedNode || selectedNode.type !== "collision") return;
    const updatedCols = model.collision_meshes.map((col) => {
      if (col.name === selectedNode.name) {
        if (fieldName === "radius") {
          return { ...col, radius: value };
        } else if (axis) {
          const vectorKey = fieldName as "min_extents" | "max_extents" | "center";
          const newVec = {
            ...col[vectorKey],
            [axis]: value,
          };
          if (vectorKey === "center") {
            onPositionChange(col.name, "collision", newVec);
          }
          return {
            ...col,
            [vectorKey]: newVec,
          };
        }
      }
      return col;
    });
    if (onModelChange) {
      onModelChange({ ...model, collision_meshes: updatedCols });
    }
  };

  const handleGenerateCollisionFromMesh = () => {
    if (!model || !selectedNode || selectedNode.type !== "collision" || activeSelectedSourceMeshes.length === 0) return;

    const meshNames = [...activeSelectedSourceMeshes];
    const colName = selectedNode.name;

    setIsLoading?.(true);
    setStatusMsg?.("Generating Collision Convex Hull...");
    // TRAP COMMENT: Do not remove this setTimeout! Tauri IPC serializes the entire HODModel
    // synchronously on the main thread. Without this 50ms yield, React cannot paint the loading
    // spinner overlay before the UI thread blocks during convex hull generation.
    setTimeout(async () => {
      try {
        const updatedModel = await invoke<any>("auto_generate_collision_from_mesh", {
          model,
          collisionMeshName: colName,
          sourceMeshNames: meshNames,
        });

        onModelChange?.(updatedModel);

        await invoke("log_event", {
          level: "INFO",
          message: `Successfully generated convex hull collision mesh for ${colName} from visual meshes: ${meshNames.join(", ")}`,
        });

        setIsLoading?.(false);
        alert(`Collision convex hull mesh successfully generated for "${colName}" from ${meshNames.length} mesh(es) (${meshNames.join(", ")})!`);
      } catch (e: any) {
        console.error(e);
        setIsLoading?.(false);
        alert(`Failed to generate collision mesh: ${e}`);
      }
    }, 50);
  };

  const handleAutoCalculateCollision = () => {
    if (!model || !selectedNode || selectedNode.type !== "collision" || activeSelectedSourceMeshes.length === 0) return;

    const selectedEntries = collisionSourceMeshOptions.filter((opt) =>
      activeSelectedSourceMeshes.includes(opt.baseName)
    );
    if (selectedEntries.length === 0) {
      alert("Selected mesh(es) not found in model!");
      return;
    }

    let minX = Infinity;
    let minY = Infinity;
    let minZ = Infinity;
    let maxX = -Infinity;
    let maxY = -Infinity;
    let maxZ = -Infinity;
    const transformedPoints: THREE.Vector3[] = [];

    selectedEntries.forEach(({ mesh }) => {
      const parentName = mesh.parent_name || "Root";
      const jointMatrix =
        parentName.toLowerCase() === "root"
          ? new THREE.Matrix4()
          : getJointGlobalMatrix(model, parentName);

      mesh.parts.forEach((part) => {
        part.vertices.forEach((v) => {
          const pt = new THREE.Vector3(v.position.x, v.position.y, v.position.z).applyMatrix4(jointMatrix);
          if (pt.x < minX) minX = pt.x;
          if (pt.x > maxX) maxX = pt.x;
          if (pt.y < minY) minY = pt.y;
          if (pt.y > maxY) maxY = pt.y;
          if (pt.z < minZ) minZ = pt.z;
          if (pt.z > maxZ) maxZ = pt.z;
          transformedPoints.push(pt);
        });
      });
    });

    if (transformedPoints.length === 0) {
      alert("The selected mesh(es) have no vertices to calculate collision bounds from.");
      return;
    }

    const center = {
      x: (minX + maxX) / 2,
      y: (minY + maxY) / 2,
      z: (minZ + maxZ) / 2,
    };

    const min_extents = { x: minX, y: minY, z: minZ };
    const max_extents = { x: maxX, y: maxY, z: maxZ };

    let maxDistSq = 0;
    transformedPoints.forEach((pt) => {
      const dx = pt.x - center.x;
      const dy = pt.y - center.y;
      const dz = pt.z - center.z;
      const distSq = dx * dx + dy * dy + dz * dz;
      if (distSq > maxDistSq) {
        maxDistSq = distSq;
      }
    });
    const radius = Math.sqrt(maxDistSq);

    const updatedCols = model.collision_meshes.map((col) => {
      if (col.name === selectedNode.name) {
        return {
          ...col,
          min_extents,
          max_extents,
          center,
          radius,
        };
      }
      return col;
    });

    onModelChange?.({ ...model, collision_meshes: updatedCols });
    const meshSummary = selectedEntries.map((e) => `${e.baseName} (LOD ${e.mesh.lod})`).join(", ");
    invoke("log_event", {
      level: "INFO",
      message: `Auto-calculated collision bounds for ${selectedNode.name} from visual meshes: ${meshSummary}`,
    }).catch(console.error);

    alert(`Collision bounds for "${selectedNode.name}" successfully calculated from ${selectedEntries.length} mesh(es)!\n\nCenter: [${center.x.toFixed(3)}, ${center.y.toFixed(3)}, ${center.z.toFixed(3)}]\nRadius: ${radius.toFixed(3)}\nMin Extents: [${min_extents.x.toFixed(3)}, ${min_extents.y.toFixed(3)}, ${min_extents.z.toFixed(3)}]\nMax Extents: [${max_extents.x.toFixed(3)}, ${max_extents.y.toFixed(3)}, ${max_extents.z.toFixed(3)}]`);
  };

  const renderSelectedContent = () => {
    if (!model) {
      return (
        <div style={{ display: "flex", justifyContent: "center", alignItems: "center", height: "100px", color: "var(--text-muted)" }}>
          No model loaded
        </div>
      );
    }

    if (!selectedNode) {
      return (
        <div style={{ display: "flex", flexDirection: "column", justifyContent: "center", alignItems: "center", color: "var(--text-muted)", gap: "12px", textAlign: "center", padding: "10px 0" }}>
          <Info size={24} style={{ color: "var(--border-color)" }} />
          <span style={{ fontSize: "12px" }}>Select an element from the hierarchy tree to inspect and edit its properties.</span>
        </div>
      );
    }

    if (selectedNode.type === "weapon_group") {
      return (
        <WeaponGroupInspector
          model={model}
          selectedNode={selectedNode as {type: string, name: string}}
          onModelChange={onModelChange}
          onPositionChange={onPositionChange}
          onSelectedNodeChange={onSelectedNodeChange}
          coordinateSpace={coordinateSpace}
        />
      );
    }

    if (
      selectedNode.type === "hardpoint_group" ||
      selectedNode.type === "capture_point_group" ||
      selectedNode.type === "repair_point_group" ||
      selectedNode.type === "salvage_point_group"
    ) {
      const baseName = selectedNode.name;
      const groupType = selectedNode.type;

      let groupLabel = "Assembly";
      if (groupType === "hardpoint_group") groupLabel = "Hardpoint Assembly";
      else if (groupType === "capture_point_group") groupLabel = "Capture Point Assembly";
      else if (groupType === "repair_point_group") groupLabel = "Repair Point Assembly";
      else if (groupType === "salvage_point_group") groupLabel = "Salvage Point Assembly";

      let jointSpecs: {
        key: string;
        label: string;
        suffix: string;
        parentSuffix: string | null;
        defaultOffset: { x: number; y: number; z: number };
      }[] = [];

      if (groupType === "hardpoint_group") {
        jointSpecs = [
          { key: "Position", label: "1. Position (Pivot)", suffix: "_Position", parentSuffix: null, defaultOffset: { x: 0, y: 0, z: 0 } },
          { key: "Direction", label: "2. Direction (Forward)", suffix: "_Direction", parentSuffix: "_Position", defaultOffset: { x: 0, y: 5.0, z: 0 } },
          { key: "Rest", label: "3. Rest (Idle)", suffix: "_Rest", parentSuffix: "_Position", defaultOffset: { x: 0, y: 0, z: 5.0 } },
        ];
      } else if (groupType === "capture_point_group") {
        jointSpecs = [
          { key: "Base", label: "1. Base Joint", suffix: "", parentSuffix: null, defaultOffset: { x: 0, y: 0, z: 0 } },
          { key: "Heading", label: "2. Heading", suffix: "Heading", parentSuffix: "", defaultOffset: { x: 0, y: 0, z: 5.0 } },
          { key: "Left", label: "3. Left", suffix: "Left", parentSuffix: "", defaultOffset: { x: 0, y: 0, z: 5.0 } },
          { key: "Up", label: "4. Up", suffix: "Up", parentSuffix: "", defaultOffset: { x: 0, y: 5.0, z: 0 } },
        ];
      } else if (groupType === "repair_point_group") {
        jointSpecs = [
          { key: "Base", label: "1. Base Joint", suffix: "", parentSuffix: null, defaultOffset: { x: 0, y: 0, z: 0 } },
          { key: "Heading", label: "2. Heading", suffix: "Heading", parentSuffix: "", defaultOffset: { x: 0, y: 0, z: 5.0 } },
          { key: "Left", label: "3. Left", suffix: "Left", parentSuffix: "", defaultOffset: { x: 5.0, y: 0, z: 0 } },
          { key: "Up", label: "4. Up", suffix: "Up", parentSuffix: "", defaultOffset: { x: 0, y: 5.0, z: 0 } },
        ];
      } else if (groupType === "salvage_point_group") {
        jointSpecs = [
          { key: "Base", label: "1. Base Joint", suffix: "", parentSuffix: null, defaultOffset: { x: 0, y: 0, z: 0 } },
          { key: "Heading", label: "2. Heading", suffix: "Heading", parentSuffix: "", defaultOffset: { x: 0, y: 0, z: 5.0 } },
          { key: "Left", label: "3. Left", suffix: "Left", parentSuffix: "", defaultOffset: { x: 0, y: 0, z: 5.0 } },
          { key: "Up", label: "4. Up", suffix: "Up", parentSuffix: "", defaultOffset: { x: 0, y: 5.0, z: 0 } },
        ];
      }

      const foundJoints = jointSpecs.map(spec => {
        const jointName = spec.suffix ? `${baseName}${spec.suffix}` : baseName;
        let jointObj = model.joints.find(j => j.name.toLowerCase() === jointName.toLowerCase());
        if (!jointObj && spec.key === "Muzzle") {
          jointObj = model.joints.find(j => j.name.toLowerCase().startsWith(`${baseName}_Muzzle`.toLowerCase()));
        }
        return { spec, jointObj };
      });

      const missingJoints = foundJoints.filter(fj => !fj.jointObj).map(fj => fj.spec.key);

      const handleWeaponJointCoordChange = (jointName: string, axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;

        if (!model) return;
        const jointObj = model.joints.find(j => j.name === jointName);
        if (!jointObj) return;

        const result = applyPositionEdit(model, "joint", jointName, coordinateSpace, axis, val);
        onPositionChange(jointName, "joint", result.position, result.matrix);
      };

      const handleWeaponJointRotationChange = (jointName: string, axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;
        const result = applyRotationEdit(model, "joint", jointName, coordinateSpace, axis, val);
        const updatedJoints = model.joints.map((j) => {
          if (j.name === jointName) {
            return {
              ...j,
              local_transform: result.matrix,
              position: result.position,
              rotation: matrixRotationToHodRadians(result.matrix),
            };
          }
          return j;
        });
        onModelChange?.({ ...model, joints: updatedJoints });
      };

      const handleRepairStructure = () => {
        let jointsToSave = [...model.joints];
        const base = baseName;

        jointSpecs.forEach(spec => {
          const jointName = spec.suffix ? `${base}${spec.suffix}` : base;
          let jointObj = jointsToSave.find(j => j.name.toLowerCase() === jointName.toLowerCase());
          if (!jointObj && spec.key === "Muzzle") {
            jointObj = jointsToSave.find(j => j.name.toLowerCase().startsWith(`${base}_Muzzle`.toLowerCase()));
          }

          if (!jointObj) {
            let parentName = "Root";
            if (spec.parentSuffix !== null) {
              parentName = spec.parentSuffix === "" ? base : `${base}${spec.parentSuffix}`;
            }

            jointsToSave.push({
              name: jointName,
              parent_name: parentName,
              local_transform: {
                m: [
                  [1, 0, 0, 0],
                  [0, 1, 0, 0],
                  [0, 0, 1, 0],
                  [spec.defaultOffset.x, spec.defaultOffset.y, spec.defaultOffset.z, 1]
                ]
              }
            });
          }
        });

        onModelChange?.({ ...model, joints: jointsToSave });
        invoke("log_event", { level: "INFO", message: `${groupLabel} ${base} structure auto-repaired successfully.` }).catch(console.error);
        alert(`${groupLabel} structure for "${base}" successfully repaired! All core joints generated.`);
      };

      const handleRenameWeaponGroup = () => {
        const nextName = renameWeaponName.trim();
        if (!nextName || nextName === baseName) return;

        // renameAssemblyMember also covers latch children (`RepairPoint0Heading`),
        // which have no underscore before the suffix.
        const updatedJoints = model.joints.map((joint) => ({
          ...joint,
          name: renameAssemblyMember(joint.name, baseName, nextName),
          parent_name: joint.parent_name ? renameAssemblyMember(joint.parent_name, baseName, nextName) : joint.parent_name,
        }));

        const updatedMarkers = model.markers.map((marker) => ({
          ...marker,
          parent_joint: renameAssemblyMember(marker.parent_joint, baseName, nextName),
        }));

        const updatedMeshes = model.meshes.map((mesh) => ({
          ...mesh,
          parent_name: renameAssemblyMember(mesh.parent_name, baseName, nextName),
        }));

        onModelChange?.({
          ...model,
          joints: updatedJoints,
          markers: updatedMarkers,
          meshes: updatedMeshes
        });

        invoke("log_event", { level: "INFO", message: `Renamed group from ${baseName} to ${nextName}` }).catch(console.error);
        selectedNode.name = nextName;
        alert(`Successfully renamed group to "${nextName}"!`);
      };

      const renderJointCard = (label: string, _suffix: string, jointObj: any) => {
        if (!jointObj) {
          return (
            <div style={{ background: "rgba(255, 23, 68, 0.05)", border: "1px dashed rgba(255, 23, 68, 0.25)", padding: "12px", borderRadius: "8px", display: "flex", flexDirection: "column", gap: "6px" }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                <span style={{ fontSize: "12px", fontWeight: "600", color: "#ffcdd2" }}>{label} (Missing)</span>
                <span style={{ fontSize: "9px", padding: "1px 4px", background: "rgba(255,23,68,0.2)", borderRadius: "3px", color: "var(--accent-danger)" }}>Required</span>
              </div>
              <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>This joint is missing in the HOD structure. Click 'Repair' to restore.</div>
            </div>
          );
        }

        const display = getDisplayTransform(model, "joint", jointObj.name, coordinateSpace);
        const pos = display.position;
        const rot = display.rotation;

        return (
          <div style={{ background: "rgba(22, 160, 255, 0.03)", border: "1px solid var(--border-color)", padding: "12px", borderRadius: "8px", display: "flex", flexDirection: "column", gap: "10px" }}>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
              <span style={{ fontSize: "12px", fontWeight: "600", color: "var(--accent-cyan)" }}>{label}</span>
              <span style={{ fontSize: "10px", color: "var(--text-muted)", fontFamily: "var(--font-mono)" }}>{jointObj.name}</span>
            </div>

            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "6px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>{axis} Pos</label>
                  <NumericInput
                    step="1"
                    value={pos[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleWeaponJointCoordChange(jointObj.name, axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleWeaponJointCoordChange(jointObj.name, axis as "x" | "y" | "z", val), 1.0)}
                    style={{ height: "26px", fontSize: "11px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 6px", width: "100%" }}
                  />
                </div>
              ))}
            </div>

            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "6px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>{axis} Rot</label>
                  <NumericInput
                    step="1"
                    value={rot[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleWeaponJointRotationChange(jointObj.name, axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleWeaponJointRotationChange(jointObj.name, axis as "x" | "y" | "z", val), 1.0)}
                    style={{ height: "26px", fontSize: "11px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 6px", width: "100%" }}
                  />
                </div>
              ))}
            </div>
          </div>
        );
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              {groupLabel}
            </div>
            <div style={{ display: "flex", gap: "8px", alignItems: "center" }}>
              <input
                value={renameWeaponName}
                onChange={(e) => setRenameWeaponName(e.target.value)}
                style={{ fontSize: "14px", fontWeight: "600", color: "var(--accent-cyan)", background: "rgba(22, 160, 255, 0.05)", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "4px 8px", height: "30px", flex: 1 }}
              />
              <button
                onClick={handleRenameWeaponGroup}
                className="secondary"
                style={{ fontSize: "11px", height: "30px", padding: "0 10px" }}
              >
                Rename
              </button>
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          {missingJoints.length > 0 ? (
            <div style={{ background: "rgba(255, 152, 0, 0.12)", border: "1px solid #ff9800", borderRadius: "8px", padding: "12px", display: "flex", flexDirection: "column", gap: "10px" }}>
              <div style={{ fontSize: "12px", color: "#ffe0b2", fontWeight: "600" }}>
                ⚠️ Structure Incomplete: Missing {missingJoints.join(", ")} Joints!
              </div>
              <div style={{ fontSize: "11px", color: "rgba(255,255,255,0.8)" }}>
                HWRM 2.0 requires a standard skeleton structure for each assembly. Skipping this can crash the engine.
              </div>
              <button
                onClick={handleRepairStructure}
                style={{ background: "#ff9800", border: "none", padding: "6px 12px", color: "#000", fontSize: "11px", fontWeight: "700", borderRadius: "4px", cursor: "pointer", display: "flex", alignItems: "center", gap: "6px", width: "fit-content" }}
              >
                <Wrench size={12} />
                Repair {groupLabel} Structure
              </button>
            </div>
          ) : (
            <div style={{ background: "rgba(0, 230, 118, 0.1)", border: "1px solid #00e676", borderRadius: "8px", padding: "10px 14px", fontSize: "12px", color: "#b9f6ca", display: "flex", alignItems: "center", gap: "8px" }}>
              <span>✓ {groupLabel} Structure Complete & Engine-Ready!</span>
            </div>
          )}

          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            {foundJoints.map(fj => renderJointCard(fj.spec.label, fj.spec.key, fj.jointObj))}
          </div>
        </div>
      );
    }

    if (selectedNode.type === "joint" || selectedNode.type === "engine_nozzle") {
      const isNozzle = selectedNode.type === "engine_nozzle";
      const joint = model.joints.find(j => j.name === selectedNode.name);
      if (!joint) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Joint not found</div>;
      const display = getDisplayTransform(model, selectedNode.type, selectedNode.name, coordinateSpace);
      const pos = display.position;
      const rot = display.rotation;
      const parentName = joint.parent_name || "(None)";
      
      const hasBurn = model.engine_burns?.some(b => b.parent_name === joint.name);
      const hasGlow = model.engine_glows?.some(g => g.parent_name === joint.name);
      const hasShape = model.engine_shapes?.some(s => s.parent_name === joint.name);

      const handleAddSubnode = (type: string) => {
        if (type === "burn") {
          const newBurn = { name: `burn_${joint.name}`, parent_name: joint.name, num_divisions: 16, num_flames: 4, vertices: [{x:0,y:0,z:0},{x:0,y:0,z:-1},{x:0,y:0,z:-2},{x:0,y:0,z:-3}] };
          onModelChange?.({ ...model, engine_burns: [...(model.engine_burns || []), newBurn] });
        } else if (type === "glow") {
          const newGlow = { name: `glow_${joint.name}`, parent_name: joint.name, lod: 0, mesh: { name: `glow_${joint.name}`, parent_name: joint.name, lod: 0, parts: [] } };
          onModelChange?.({ ...model, engine_glows: [...(model.engine_glows || []), newGlow] });
        } else if (type === "shape") {
          const newShape = { name: `shape_${joint.name}`, parent_name: joint.name, mesh: { name: `shape_${joint.name}`, parent_name: joint.name, lod: 0, parts: [] } };
          onModelChange?.({ ...model, engine_shapes: [...(model.engine_shapes || []), newShape] });
        }
      };
      const handleRemoveSubnode = (type: string) => {
        if (type === "burn") {
          onModelChange?.({ ...model, engine_burns: (model.engine_burns || []).filter(b => b.parent_name !== joint.name) });
        } else if (type === "glow") {
          onModelChange?.({ ...model, engine_glows: (model.engine_glows || []).filter(g => g.parent_name !== joint.name) });
        } else if (type === "shape") {
          onModelChange?.({ ...model, engine_shapes: (model.engine_shapes || []).filter(s => s.parent_name !== joint.name) });
        }
      };

      const handleCoordChange = (axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;
        const result = applyPositionEdit(model, selectedNode.type, selectedNode.name, coordinateSpace, axis, val);
        onPositionChange(selectedNode.name, selectedNode.type, result.position, result.matrix);
      };

      const handleRotationChange = (axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;
        const result = applyRotationEdit(model, selectedNode.type, selectedNode.name, coordinateSpace, axis, val);
        const updatedJoints = model.joints.map((j) => {
          if (j.name === joint.name) {
            return {
              ...j,
              local_transform: result.matrix,
              position: result.position,
              rotation: matrixRotationToHodRadians(result.matrix),
            };
          }
          return j;
        });
        onModelChange?.({ ...model, joints: updatedJoints });
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px", display: "flex", alignItems: "center", gap: "6px" }}>
              {isNozzle && <span style={{fontSize: "12px"}}>🚀</span>}
              <span>{isNozzle ? "Selected Engine Nozzle" : "Selected Joint"}</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>

          
          {isNozzle && (
            <div style={{ display: "flex", flexDirection: "column", gap: "8px", background: "rgba(255,255,255,0.02)", padding: "12px", borderRadius: "4px", border: "1px solid var(--border-color)" }}>
              <div style={{ fontSize: "11px", fontWeight: "600", color: "var(--text-secondary)", marginBottom: "4px" }}>Engine Subnodes</div>
              <div style={{ display: "flex", gap: "8px", flexWrap: "wrap" }}>
                {!hasBurn ? 
                  <button onClick={() => handleAddSubnode("burn")} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Burn Plume</button> :
                  <button onClick={() => handleRemoveSubnode("burn")} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Burn</button>}
                {!hasGlow ? 
                  <button onClick={() => handleAddSubnode("glow")} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Engine Glow</button> :
                  <button onClick={() => handleRemoveSubnode("glow")} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Glow</button>}
                {!hasShape ? 
                  <button onClick={() => handleAddSubnode("shape")} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Engine Shape</button> :
                  <button onClick={() => handleRemoveSubnode("shape")} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Shape</button>}
              </div>
            </div>
          )}

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <Move size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Position Coordinates</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px", fontWeight: "500" }}>{axis.toUpperCase()} Position</label>
                  <NumericInput
                    step="1"
                    value={pos[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleCoordChange(axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleCoordChange(axis as "x" | "y" | "z", val), 1.0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "12px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <RefreshCw size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Rotation Coordinates (Degrees)</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px", fontWeight: "500" }}>{axis.toUpperCase()} Rotation</label>
                  <NumericInput
                    step="1"
                    value={rot[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleRotationChange(axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleRotationChange(axis as "x" | "y" | "z", val), 1.0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "12px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "10px" }}>
              <Navigation size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Parent attachment</span>
            </div>
            <div style={{ background: "rgba(22, 160, 255, 0.05)", border: "1px solid var(--border-color)", padding: "10px", borderRadius: "4px", fontSize: "13px" }}>
              <span style={{ color: "var(--text-muted)", marginRight: "8px" }}>Attached to:</span>
              <span style={{ fontWeight: "500", color: "var(--text-primary)" }}>{parentName}</span>
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "marker") {
      const marker = model.markers.find(m => m.name === selectedNode.name);
      if (!marker) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Marker not found</div>;
      const display = getDisplayTransform(model, "marker", selectedNode.name, coordinateSpace);
      const pos = display.position;
      const rot = display.rotation;

      const handleCoordChange = (axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;
        const result = applyPositionEdit(model, "marker", selectedNode.name, coordinateSpace, axis, val);
        onPositionChange(selectedNode.name, selectedNode.type, result.position, result.matrix);
      };

      const handleRotationChange = (axis: "x" | "y" | "z", valStr: string) => {
        const val = parseFloat(valStr);
        if (isNaN(val)) return;
        const result = applyRotationEdit(model, "marker", selectedNode.name, coordinateSpace, axis, val);
        const updatedMarkers = model.markers.map((mrk) => {
          if (mrk.name === marker.name) {
            return { ...mrk, rotation: result.matrix };
          }
          return mrk;
        });
        onModelChange?.({ ...model, markers: updatedMarkers });
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              Selected Marker
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <Move size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Position Coordinates</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px", fontWeight: "500" }}>{axis.toUpperCase()} Position</label>
                  <NumericInput
                    step="1"
                    value={pos[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleCoordChange(axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleCoordChange(axis as "x" | "y" | "z", val), 1.0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "12px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <RefreshCw size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Rotation Coordinates (Degrees)</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px", fontWeight: "500" }}>{axis.toUpperCase()} Rotation</label>
                  <NumericInput
                    step="1"
                    value={rot[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleRotationChange(axis as "x" | "y" | "z", val)}
                    onWheel={(e) => handleNumericWheel(e, (val) => handleRotationChange(axis as "x" | "y" | "z", val), 1.0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "12px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "10px" }}>
              <Layers size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Modding Guidelines</span>
            </div>
            <div style={{ fontSize: "12px", color: "var(--text-muted)", lineHeight: "1.5" }}>
              Markers act as hardpoints for weapon mounts, engine burns, subsystem docking, or UI linkages.
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "navlight") {
      const nav = model.nav_lights.find(n => n.name === selectedNode.name);
      if (!nav) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>NavLight not found</div>;

      const underlyingJoint = model.joints.find(j => j.name.toLowerCase() === nav.name.toLowerCase());
      const display = underlyingJoint
        ? getDisplayTransform(model, "joint", underlyingJoint.name, coordinateSpace)
        : { position: { x: 0, y: 0, z: 0 }, rotation: { x: 0, y: 0, z: 0 } };
      const navPosition = display.position;

      const updateNavLightPosition = (axis: "x" | "y" | "z", value: number) => {
        if (underlyingJoint) {
          const result = applyPositionEdit(model, "joint", underlyingJoint.name, coordinateSpace, axis, value);
          const newJoints = model.joints.map((joint) => {
            if (joint.name.toLowerCase() !== nav.name.toLowerCase()) return joint;
            return {
              ...joint,
              local_transform: result.matrix,
              position: result.position,
              rotation: matrixRotationToHodRadians(result.matrix),
            };
          });
          onModelChange?.({ ...model, joints: newJoints });
          return;
        }

        // No backing joint: create one at Root (global and local are identical).
        const nextPosition = { ...navPosition, [axis]: value };
        onModelChange?.({
          ...model,
          joints: [
            ...model.joints,
            {
              name: nav.name,
              parent_name: "Root",
              position: nextPosition,
              rotation: { x: 0, y: 0, z: 0 },
              scale: { x: 1, y: 1, z: 1 },
              local_transform: {
                m: [
                  [1, 0, 0, 0],
                  [0, 1, 0, 0],
                  [0, 0, 1, 0],
                  [nextPosition.x, nextPosition.y, nextPosition.z, 1]
                ]
              }
            }
          ]
        });
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Radio size={12} style={{ color: "var(--accent-cyan)" }} />
              <span>Selected NavLight</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>
                Root Coordinate {underlyingJoint ? "(Underlying Joint)" : "(Creates Backing Joint)"}
              </label>
              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
                <div>
                  <label style={{ display: "block", fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>X</label>
                  <NumericInput
                    step="0.1"
                    value={navPosition.x}
                    onChange={(val) => {
                      const v = parseFloat(val) || 0;
                      updateNavLightPosition("x", v);
                    }}
                    style={{ fontSize: "12px" }}
                  />
                </div>
                <div>
                  <label style={{ display: "block", fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>Y</label>
                  <NumericInput
                    step="0.1"
                    value={navPosition.y}
                    onChange={(val) => {
                      const v = parseFloat(val) || 0;
                      updateNavLightPosition("y", v);
                    }}
                    style={{ fontSize: "12px" }}
                  />
                </div>
                <div>
                  <label style={{ display: "block", fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>Z</label>
                  <NumericInput
                    step="0.1"
                    value={navPosition.z}
                    onChange={(val) => {
                      const v = parseFloat(val) || 0;
                      updateNavLightPosition("z", v);
                    }}
                    style={{ fontSize: "12px" }}
                  />
                </div>
              </div>
              <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: "12px 0 0 0" }} />
            </div>

          <div>
            <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Section ID</label>
            <NumericInput
              value={nav.section}
              onChange={(val) => handleNavLightChange("section", parseInt(val, 10) || 0)}
              style={{ fontSize: "12px" }}
            />
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Size</label>
              <NumericInput
                step="0.1"
                value={nav.size}
                onChange={(val) => handleNavLightChange("size", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Phase</label>
              <NumericInput
                step="0.1"
                value={nav.phase}
                onChange={(val) => handleNavLightChange("phase", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Frequency</label>
              <NumericInput
                step="0.1"
                value={nav.frequency}
                onChange={(val) => handleNavLightChange("frequency", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
          </div>

          <div>
            <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Style</label>
            <TextInput
              value={nav.style}
              onChange={(val) => handleNavLightChange("style", val)}
              style={{ fontSize: "12px", width: "100%", background: "rgba(0,0,0,0.2)", border: "1px solid rgba(255,255,255,0.1)", color: "var(--text-primary)", padding: "6px 10px", borderRadius: "4px" }}
            />
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px" }}>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Distance</label>
              <NumericInput
                value={nav.distance}
                onChange={(val) => handleNavLightChange("distance", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Color Picker</label>
              <div style={{ display: "flex", gap: "6px", alignItems: "center" }}>
                <input
                  type="color"
                  value={rgbToHex(nav.color)}
                  onChange={(e) => handleNavLightChange("color", hexToRgb(e.target.value))}
                  style={{ width: "36px", height: "24px", padding: 0, border: "none", cursor: "pointer", background: "none" }}
                />
                <span style={{ fontSize: "10px", fontFamily: "var(--font-mono)" }}>
                  {rgbToHex(nav.color).toUpperCase()}
                </span>
              </div>
            </div>
          </div>

          <div style={{ display: "flex", gap: "12px", marginTop: "4px" }}>
            <label style={{ display: "flex", alignItems: "center", gap: "6px", fontSize: "12px", cursor: "pointer" }}>
              <input
                type="checkbox"
                checked={nav.sprite_visible}
                onChange={(e) => handleNavLightChange("sprite_visible", e.target.checked)}
                style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
              />
              <span>Sprite Visible</span>
            </label>
            <label style={{ display: "flex", alignItems: "center", gap: "6px", fontSize: "12px", cursor: "pointer" }}>
              <input
                type="checkbox"
                checked={nav.high_end_only}
                onChange={(e) => handleNavLightChange("high_end_only", e.target.checked)}
                style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
              />
              <span>High End Only</span>
            </label>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "dockpoint") {
      const parts = selectedNode.name.split(":");
      const pathName = parts[0];
      const ptIndex = parseInt(parts[1], 10);
      const path = model.dockpaths.find(p => p.name === pathName);
      if (!path) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Dockpath not found</div>;
      const pt = path.points[ptIndex];
      if (!pt) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Dockpoint not found</div>;
      const display = getDisplayTransform(model, "dockpoint", selectedNode.name, coordinateSpace);
      const displayPos = display.position;

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Activity size={12} style={{ color: "var(--accent-cyan)" }} />
              <span>Selected Dockpoint</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {pathName} [Pt {ptIndex}]
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <Move size={14} style={{ color: "var(--accent-blue)" }} />
              <span>Position Coordinates</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>{axis.toUpperCase()}</label>
                  <NumericInput
                    step="1"
                    value={displayPos[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleDockpointChange(pathName, ptIndex, axis as "x" | "y" | "z", parseFloat(val) || 0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "11px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px" }}>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Tolerance</label>
              <NumericInput
                step="0.1"
                value={pt.tolerance}
                onChange={(val) => handleDockpointChange(pathName, ptIndex, "tolerance", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>Max Speed</label>
              <NumericInput
                step="0.5"
                value={pt.max_speed}
                onChange={(val) => handleDockpointChange(pathName, ptIndex, "max_speed", parseFloat(val) || 0)}
                style={{ fontSize: "12px" }}
              />
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ fontSize: "11px", fontWeight: "600", textTransform: "uppercase", color: "var(--text-muted)", marginBottom: "8px" }}>
              Points in path ({path.points.length})
            </div>
            <div style={{ display: "flex", flexDirection: "column", gap: "4px", maxHeight: "150px", overflowY: "auto", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "4px" }}>
              {path.points.map((p, idx) => (
                <div
                  key={idx}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    fontSize: "11px",
                    padding: "4px 6px",
                    borderRadius: "2px",
                    background: idx === ptIndex ? "rgba(22, 160, 255, 0.15)" : "none",
                    borderLeft: idx === ptIndex ? "2px solid var(--accent-cyan)" : "none",
                  }}
                >
                  <span style={{ color: idx === ptIndex ? "var(--accent-cyan)" : "var(--text-primary)" }}>Point {idx}</span>
                  <span style={{ color: "var(--text-muted)" }}>
                    [{p.position.x.toFixed(1)}, {p.position.y.toFixed(1)}, {p.position.z.toFixed(1)}]
                  </span>
                </div>
              ))}
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "collision") {
      const col = model.collision_meshes.find(c => c.name === selectedNode.name);
      if (!col) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Collision Mesh not found</div>;

      const handleImportCollisionOBJ = async () => {
        try {
          const fileContent = await invoke<string | null>("load_text_file", { filters: ["obj"] });
          if (!fileContent) return;
          setIsLoading?.(true); setStatusMsg?.("Importing Collision OBJ...");
          setTimeout(async () => {
            try {
              const { OBJLoader } = await import("three/examples/jsm/loaders/OBJLoader.js");
              const objGroup = new OBJLoader().parse(fileContent);
              const newParts: any[] = [];
              let totalVerts = 0;
              objGroup.traverse((child: any) => {
                if (child.isMesh) {
                  const geo = (child as THREE.Mesh).geometry;
                  if (geo?.attributes.position) {
                    const posAttr = geo.attributes.position;
                    const vertices: any[] = []; const indices: number[] = [];
                    for (let i = 0; i < posAttr.count; i++) {
                      vertices.push({
                        position: { x: posAttr.getX(i), y: posAttr.getY(i), z: posAttr.getZ(i) },
                        normal: { x: 0, y: 1, z: 0 }, tangent: { x: 1, y: 0, z: 0 }, binormal: { x: 0, y: 0, z: 1 },
                        uv: { u: 0, v: 0 }, color: 0xFFFFFFFF, skinning_data: null,
                      });
                    }
                    if (geo.index) {
                      const idxArr = geo.index.array;
                      for (let i = 0; i < idxArr.length; i++) indices.push(idxArr[i]);
                    } else {
                      for (let i = 0; i < posAttr.count; i++) indices.push(i);
                    }
                    newParts.push({ material_index: 0, vertex_mask: 0x01, vertices, indices });
                    totalVerts += vertices.length;
                  }
                }
              });
              if (newParts.length === 0) { alert("No geometry found in the OBJ file."); setIsLoading?.(false); return; }
              const updatedCols = model.collision_meshes.map(c => c.name === col.name ? { ...c, mesh: { ...c.mesh, parts: newParts } } : c);
              onModelChange?.({ ...model, collision_meshes: updatedCols });
              setIsLoading?.(false); alert(`Collision mesh imported! Parts: ${newParts.length}`);
            } catch (e: any) { console.error(e); setIsLoading?.(false); alert(`Import failed: ${e.toString()}`); }
          }, 100);
        } catch (e: any) { console.error(e); alert(`Import dialog failed: ${e.toString()}`); }
      };

      const handleExportCollisionOBJ = async () => {
        try {
          setIsLoading?.(true);
          setStatusMsg?.("Exporting Collision OBJ...");
          const { OBJExporter } = await import("three/examples/jsm/exporters/OBJExporter.js");
          const exporter = new OBJExporter();
          const group = new THREE.Group();
          group.name = col.name;
          col.mesh.parts.forEach((part: any, pIdx: number) => {
            const geometry = new THREE.BufferGeometry();
            const vertices: number[] = [];
            part.vertices.forEach((v: any) => vertices.push(v.position.x, v.position.y, v.position.z));
            geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
            if (part.indices && part.indices.length > 0) {
              geometry.setIndex(part.indices);
            }
            const meshObj = new THREE.Mesh(geometry, new THREE.MeshBasicMaterial());
            meshObj.name = `${col.name}_part_${pIdx}`;
            group.add(meshObj);
          });
          const objResult = exporter.parse(group);
          await invoke<string | null>("save_text_file", { defaultName: `${col.name}_collision.obj`, filters: ["obj"], contents: objResult });
          setIsLoading?.(false);
        } catch (e: any) {
          console.error(e);
          setIsLoading?.(false);
          alert(`Failed to export OBJ: ${e.toString()}`);
        }
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Shield size={12} style={{ color: "var(--accent-danger)" }} />
              <span>Collision Mesh</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-danger)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "11px", fontWeight: "500", textTransform: "uppercase", color: "var(--text-secondary)", marginBottom: "8px" }}>
              <span>Center</span>
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", marginBottom: "4px" }}>{axis.toUpperCase()}</label>
                  <NumericInput
                    step="0.1"
                    value={col.center[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleCollisionChange("center", axis as "x" | "y" | "z", parseFloat(val) || 0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "11px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <div>
            <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "4px" }}>Radius</label>
            <NumericInput
              step="0.1"
              value={col.radius}
              onChange={(val) => handleCollisionChange("radius", undefined, parseFloat(val) || 0)}
              style={{ fontSize: "12px" }}
            />
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ fontSize: "11px", fontWeight: "500", textTransform: "uppercase", color: "var(--text-secondary)", marginBottom: "8px" }}>
              Min Extents
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "10px", color: "var(--text-muted)", marginBottom: "4px" }}>Min {axis.toUpperCase()}</label>
                  <NumericInput
                    step="0.1"
                    value={col.min_extents[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleCollisionChange("min_extents", axis as "x" | "y" | "z", parseFloat(val) || 0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "11px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <div>
            <div style={{ fontSize: "11px", fontWeight: "500", textTransform: "uppercase", color: "var(--text-secondary)", marginBottom: "8px" }}>
              Max Extents
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis}>
                  <label style={{ display: "block", fontSize: "10px", color: "var(--text-muted)", marginBottom: "4px" }}>Max {axis.toUpperCase()}</label>
                  <NumericInput
                    step="0.1"
                    value={col.max_extents[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleCollisionChange("max_extents", axis as "x" | "y" | "z", parseFloat(val) || 0)}
                    style={{ fontFamily: "var(--font-mono)", fontSize: "11px" }}
                  />
                </div>
              ))}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
          
          <div>
            <div style={{ fontSize: "11px", fontWeight: "500", textTransform: "uppercase", color: "var(--text-secondary)", marginBottom: "8px" }}>
              Collision Hull Mesh
            </div>
            <div style={{ display: "flex", gap: "8px" }}>
              <button onClick={handleImportCollisionOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
                <Download size={14} style={{ color: "var(--accent-cyan)" }} /> Import OBJ
              </button>
              <button onClick={handleExportCollisionOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
                <Upload size={14} style={{ color: "var(--accent-blue)" }} /> Export OBJ
              </button>
            </div>
            <div style={{ marginTop: "8px", fontSize: "11px", color: "var(--text-muted)" }}>
              Hull Parts: {col.mesh.parts.length} | Triangles: {col.mesh.parts.reduce((sum, p) => sum + p.indices.length, 0) / 3}
            </div>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "12px", fontWeight: "500", textTransform: "uppercase", letterSpacing: "0.05em", color: "var(--text-secondary)", marginBottom: "12px" }}>
              <Wrench size={14} style={{ color: "var(--accent-cyan)" }} />
              <span>Auto-Calculate Bounds</span>
            </div>
            {collisionSourceMeshOptions.length > 0 ? (
              <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                <div>
                  <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "6px" }}>
                    <label style={{ fontSize: "11px", color: "var(--text-muted)" }}>
                      Included Visual Meshes ({activeSelectedSourceMeshes.length}/{collisionSourceMeshOptions.length})
                    </label>
                    <div style={{ display: "flex", gap: "6px" }}>
                      <button
                        type="button"
                        onClick={() => setSelectedSourceMeshes(collisionSourceMeshOptions.map((o) => o.baseName))}
                        style={{ height: "22px", fontSize: "10px", padding: "0 6px" }}
                      >
                        Select All
                      </button>
                      <button
                        type="button"
                        onClick={() => setSelectedSourceMeshes([])}
                        disabled={activeSelectedSourceMeshes.length === 0}
                        style={{
                          height: "22px",
                          fontSize: "10px",
                          padding: "0 6px",
                          opacity: activeSelectedSourceMeshes.length > 0 ? 1 : 0.5,
                          cursor: activeSelectedSourceMeshes.length > 0 ? "pointer" : "not-allowed",
                        }}
                      >
                        Clear
                      </button>
                    </div>
                  </div>
                  <div
                    style={{
                      display: "flex",
                      flexDirection: "column",
                      gap: "4px",
                      background: "rgba(0,0,0,0.15)",
                      padding: "6px",
                      borderRadius: "4px",
                      border: "1px solid var(--border-color)",
                      maxHeight: "180px",
                      overflowY: "auto",
                    }}
                  >
                    {collisionSourceMeshOptions.map((opt) => {
                      const isChecked = activeSelectedSourceMeshes.includes(opt.baseName);
                      const toggleMesh = () => {
                        setSelectedSourceMeshes((prev) => {
                          const validPrev = prev.filter((name) =>
                            collisionSourceMeshOptions.some((o) => o.baseName === name)
                          );
                          return validPrev.includes(opt.baseName)
                            ? validPrev.filter((name) => name !== opt.baseName)
                            : [...validPrev, opt.baseName];
                        });
                      };
                      return (
                        <div
                          key={opt.baseName}
                          onClick={toggleMesh}
                          style={{
                            display: "flex",
                            alignItems: "center",
                            justifyContent: "space-between",
                            gap: "8px",
                            padding: "6px 8px",
                            borderRadius: "3px",
                            cursor: "pointer",
                            background: isChecked ? "rgba(22, 160, 255, 0.15)" : "transparent",
                            border: isChecked ? "1px solid rgba(22, 160, 255, 0.3)" : "1px solid transparent",
                          }}
                        >
                          <div style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "8px", minWidth: 0, flex: 1 }}>
                            <input
                              type="checkbox"
                              checked={isChecked}
                              onChange={toggleMesh}
                              onClick={(e) => e.stopPropagation()}
                              style={{ width: "14px", height: "14px", flexShrink: 0, padding: 0, margin: 0, cursor: "pointer" }}
                            />
                            <span
                              style={{
                                fontSize: "12px",
                                color: isChecked ? "var(--accent-cyan)" : "var(--text-primary)",
                                paddingLeft: "4px",
                                textAlign: "left",
                                wordBreak: "break-all",
                                flex: 1,
                              }}
                              title={opt.baseName}
                            >
                              {opt.baseName}
                            </span>
                          </div>
                          <span style={{ fontSize: "10px", color: "var(--text-muted)", flexShrink: 0 }}>
                            LOD {opt.mesh.lod} ({opt.vertexCount} verts)
                          </span>
                        </div>
                      );
                    })}
                  </div>
                </div>
                <div style={{ display: "flex", gap: "8px" }}>
                  <button
                    onClick={handleGenerateCollisionFromMesh}
                    disabled={activeSelectedSourceMeshes.length === 0}
                    style={{
                      height: "32px",
                      fontSize: "12px",
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "center",
                      gap: "6px",
                      flex: 1,
                      background: "var(--accent-danger)",
                      color: "#fff",
                      opacity: activeSelectedSourceMeshes.length > 0 ? 1 : 0.5,
                      cursor: activeSelectedSourceMeshes.length > 0 ? "pointer" : "not-allowed",
                    }}
                  >
                    <Box size={14} />
                    <span>Generate Convex Hull</span>
                  </button>
                  <button
                    onClick={handleAutoCalculateCollision}
                    disabled={activeSelectedSourceMeshes.length === 0}
                    style={{
                      height: "32px",
                      fontSize: "12px",
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "center",
                      gap: "6px",
                      flex: 1,
                      opacity: activeSelectedSourceMeshes.length > 0 ? 1 : 0.5,
                      cursor: activeSelectedSourceMeshes.length > 0 ? "pointer" : "not-allowed",
                    }}
                  >
                    <RefreshCw size={14} style={{ color: "var(--accent-cyan)" }} />
                    <span>Calculate Box Bounds</span>
                  </button>
                </div>
              </div>
            ) : (
              <div style={{ fontSize: "11px", color: "var(--text-muted)" }}>
                No visual meshes available in this model to calculate from.
              </div>
            )}
          </div>
        </div>
      );
    }

    if (selectedNode.type === "mesh" || selectedNode.type === "mesh_lod") {
      const baseName = selectedNode.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
      return <MeshLODInspector model={model} baseName={baseName} onModelChange={onModelChange} visibleMeshes={visibleMeshes} onToggleVisibility={onToggleVisibility} />;
    }

    if (selectedNode.type === "engine_burn") {
      const burn = model.engine_burns?.find(b => b.name === selectedNode.name);
      if (!burn) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Engine Burn not found</div>;

      const handleBurnFieldChange = (fieldName: "num_divisions" | "num_flames", valueStr: string) => {
        const val = parseInt(valueStr, 10);
        if (isNaN(val)) return;
        const updatedBurns = model.engine_burns.map((b) => {
          if (b.name === burn.name) {
            return { ...b, [fieldName]: val };
          }
          return b;
        });
        onModelChange?.({ ...model, engine_burns: updatedBurns });
      };

      const handleAddBurnVertex = () => {
        const newVertex = { x: 0, y: 0, z: -1.0 }; // default backward vertex
        const updatedBurns = model.engine_burns.map((b) => {
          if (b.name === burn.name) {
            return { ...b, vertices: [...(b.vertices || []), newVertex] };
          }
          return b;
        });
        onModelChange?.({ ...model, engine_burns: updatedBurns });
        invoke("log_event", { level: "INFO", message: `Added new vertex to engine burn ${burn.name}` }).catch(console.error);
      };

      const handleRemoveBurnVertex = (ptIdx: number) => {
        const updatedBurns = model.engine_burns.map((b) => {
          if (b.name === burn.name) {
            return { ...b, vertices: b.vertices.filter((_, idx) => idx !== ptIdx) };
          }
          return b;
        });
        onModelChange?.({ ...model, engine_burns: updatedBurns });
        invoke("log_event", { level: "INFO", message: `Removed vertex index ${ptIdx} from engine burn ${burn.name}` }).catch(console.error);
      };

      const handleBurnVertexCoordinateChange = (ptIdx: number, axis: "x" | "y" | "z", valueStr: string) => {
        const val = parseFloat(valueStr);
        if (isNaN(val)) return;

        const updatedBurns = model.engine_burns.map((b) => {
          if (b.name === burn.name) {
            const updatedVerts = b.vertices.map((v, idx) => {
              if (idx === ptIdx) {
                return { ...v, [axis]: val };
              }
              return v;
            });
            return { ...b, vertices: updatedVerts };
          }
          return b;
        });
        onModelChange?.({ ...model, engine_burns: updatedBurns });
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Flame size={12} style={{ color: "var(--accent-cyan)" }} />
              <span>Selected Engine Burn</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>
          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
          <div>
            <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "4px" }}>Parent Joint</div>
            <div style={{ background: "rgba(255,255,255,0.03)", padding: "8px 10px", borderRadius: "4px", fontSize: "12px", border: "1px solid var(--border-color)" }}>
              {burn.parent_name}
            </div>
          </div>
          <div>
            <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>Divisions / Flames</div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px" }}>
              <div>
                <label style={{ display: "block", fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "2px" }}>Divisions</label>
                <NumericInput
                  value={burn.num_divisions}
                  onChange={(val) => handleBurnFieldChange("num_divisions", val)}
                  onWheel={(e) => handleNumericWheel(e, (val) => handleBurnFieldChange("num_divisions", val), 1)}
                  style={{ height: "28px", fontSize: "12px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px", width: "100%" }}
                />
              </div>
              <div>
                <label style={{ display: "block", fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "2px" }}>Flames</label>
                <NumericInput
                  value={burn.num_flames}
                  onChange={(val) => handleBurnFieldChange("num_flames", val)}
                  onWheel={(e) => handleNumericWheel(e, (val) => handleBurnFieldChange("num_flames", val), 1)}
                  style={{ height: "28px", fontSize: "12px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px", width: "100%" }}
                />
              </div>
            </div>
          </div>
          <div>
            <div style={{ padding: "8px 12px", background: "rgba(22, 160, 255, 0.05)", borderLeft: "3px solid var(--accent-cyan)", fontSize: "11px", color: "var(--text-secondary)", marginBottom: "12px", borderRadius: "0 4px 4px 0", lineHeight: "1.4" }}>
              💡 <b>Note:</b> The base position of this Engine Burn is governed by its <b>Parent Joint</b>. The <b>Burn Vertices</b> below represent the individual flame coordinates relative to that parent joint.
            </div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "8px" }}>
              <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500" }}>Burn Vertices ({burn.vertices.length})</div>
              <button
                onClick={handleAddBurnVertex}
                style={{
                  fontSize: "10px",
                  padding: "2px 8px",
                  background: "rgba(22, 160, 255, 0.15)",
                  color: "var(--accent-cyan)",
                  border: "1px solid rgba(22, 160, 255, 0.3)",
                  borderRadius: "4px",
                  cursor: "pointer",
                  fontWeight: "600"
                }}
              >
                + Add Vertex
              </button>
            </div>
            <div style={{ display: "flex", flexDirection: "column", gap: "6px", maxHeight: "250px", overflowY: "auto", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "6px" }}>
              {burn.vertices && burn.vertices.length > 0 ? (
                burn.vertices.map((v, idx) => (
                  <div key={idx} style={{ padding: "8px", fontSize: "11px", background: "rgba(255,255,255,0.02)", borderRadius: "6px", display: "flex", flexDirection: "column", gap: "6px" }}>
                    <div style={{ display: "flex", justifyContent: "space-between", fontWeight: "600" }}>
                      <span style={{ color: "var(--accent-cyan)" }}>Vertex {idx}</span>
                      <button
                        onClick={() => handleRemoveBurnVertex(idx)}
                        style={{
                          background: "transparent",
                          border: "none",
                          color: "var(--accent-danger)",
                          cursor: "pointer",
                          fontSize: "11px",
                          padding: "0 2px"
                        }}
                        title="Remove Vertex"
                      >
                        ✕
                      </button>
                    </div>
                    {/* XYZ Editable fields */}
                    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "4px" }}>
                      {["x", "y", "z"].map((axis) => (
                        <div key={axis}>
                          <label style={{ fontSize: "8px", color: "var(--text-muted)", textTransform: "uppercase" }}>{axis}</label>
                          <NumericInput
                            step="1"
                            value={v[axis as "x" | "y" | "z"]}
                            onChange={(val) => handleBurnVertexCoordinateChange(idx, axis as "x" | "y" | "z", val)}
                            onWheel={(e) => handleNumericWheel(e, (val) => handleBurnVertexCoordinateChange(idx, axis as "x" | "y" | "z", val), 1.0)}
                            style={{ height: "24px", fontSize: "11px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 6px", width: "100%" }}
                          />
                        </div>
                      ))}
                    </div>
                  </div>
                ))
              ) : (
                <div style={{ padding: "12px", color: "var(--text-muted)", fontSize: "11px", textAlign: "center" }}>
                  No vertices defined in this engine burn.
                </div>
              )}
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "engine_glow") {
      return <GlowLODInspector model={model} baseName={selectedNode.name} onModelChange={onModelChange} visibleMeshes={visibleMeshes} onToggleVisibility={onToggleVisibility} setIsLoading={setIsLoading} setStatusMsg={setStatusMsg} />;
    }

    if (selectedNode.type === "engine_shape") {
      const shape = model.engine_shapes?.find(s => s.name === selectedNode.name);
      if (!shape) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Engine Shape not found</div>;
      const totalVerts = shape.mesh.parts.reduce((sum, p) => sum + p.vertices.length, 0);

      const handleImportEngineShapeOBJ = async () => {
        try {
          const fileContent = await invoke<string | null>("load_text_file", { filters: ["obj"] });
          if (!fileContent) return;
          setIsLoading?.(true); setStatusMsg?.("Importing Engine Shape OBJ...");
          setTimeout(async () => {
            try {
              const { OBJLoader } = await import("three/examples/jsm/loaders/OBJLoader.js");
              const objGroup = new OBJLoader().parse(fileContent);
              const newParts: any[] = [];
              objGroup.traverse((child: any) => {
                if (child.isMesh) {
                  const geo = (child as THREE.Mesh).geometry;
                  if (geo?.attributes.position) {
                    const posAttr = geo.attributes.position;
                    const vertices: any[] = []; const indices: number[] = [];
                    for (let i = 0; i < posAttr.count; i++) {
                      vertices.push({
                        position: { x: posAttr.getX(i), y: posAttr.getY(i), z: posAttr.getZ(i) },
                        normal: geo.attributes.normal ? { x: geo.attributes.normal.getX(i), y: geo.attributes.normal.getY(i), z: geo.attributes.normal.getZ(i) } : { x: 0, y: 1, z: 0 },
                        tangent: { x: 1, y: 0, z: 0 }, binormal: { x: 0, y: 0, z: 1 },
                        uv: geo.attributes.uv ? { u: geo.attributes.uv.getX(i), v: 1 - geo.attributes.uv.getY(i) } : { u: 0, v: 0 },
                        color: 0xFFFFFFFF, skinning_data: null,
                      });
                    }
                    if (geo.index) {
                      const idxArr = geo.index.array;
                      for (let i = 0; i < idxArr.length; i++) indices.push(idxArr[i]);
                    } else {
                      for (let i = 0; i < posAttr.count; i++) indices.push(i);
                    }
                    newParts.push({ material_index: 0, vertex_mask: 0x01, vertices, indices });
                  }
                }
              });
              if (newParts.length === 0) { alert("No geometry found in the OBJ file."); setIsLoading?.(false); return; }
              const updatedShapes = model.engine_shapes.map(s => s.name === shape.name ? { ...s, mesh: { ...s.mesh, parts: newParts } } : s);
              onModelChange?.({ ...model, engine_shapes: updatedShapes });
              setIsLoading?.(false); alert(`Engine Shape mesh imported! Parts: ${newParts.length}`);
            } catch (e: any) { console.error(e); setIsLoading?.(false); alert(`Import failed: ${e.toString()}`); }
          }, 100);
        } catch (e: any) { console.error(e); alert(`Import dialog failed: ${e.toString()}`); }
      };

      const handleExportEngineShapeOBJ = async () => {
        try {
          setIsLoading?.(true);
          setStatusMsg?.("Exporting Shape OBJ...");
          const { OBJExporter } = await import("three/examples/jsm/exporters/OBJExporter.js");
          const exporter = new OBJExporter();
          const group = new THREE.Group();
          group.name = shape.name;
          shape.mesh.parts.forEach((part: any, pIdx: number) => {
            const geometry = new THREE.BufferGeometry();
            const vertices: number[] = [];
            part.vertices.forEach((v: any) => vertices.push(v.position.x, v.position.y, v.position.z));
            geometry.setAttribute("position", new THREE.Float32BufferAttribute(vertices, 3));
            geometry.setIndex(part.indices);
            const meshObj = new THREE.Mesh(geometry, new THREE.MeshBasicMaterial());
            meshObj.name = `${shape.name}_part_${pIdx}`;
            group.add(meshObj);
          });
          const objResult = exporter.parse(group);
          await invoke<string | null>("save_text_file", { defaultName: `${shape.name}.obj`, filters: ["obj"], contents: objResult });
          setIsLoading?.(false);
          invoke("log_event", { level: "INFO", message: `Exported Engine Shape OBJ: ${shape.name}` }).catch(console.error);
        } catch (e) {
          console.error(e);
          setIsLoading?.(false);
          alert("Failed to export Shape OBJ.");
        }
      };

      const totalTris = shape.mesh.parts.reduce((sum, p) => sum + p.indices.length, 0) / 3;
      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Flame size={12} style={{ color: "var(--accent-cyan)" }} />
              <span>Selected Engine Shape</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>
          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
          <div>
            <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "4px" }}>Parent Joint</div>
            <div style={{ background: "rgba(255,255,255,0.03)", padding: "8px 10px", borderRadius: "4px", fontSize: "12px", border: "1px solid var(--border-color)" }}>
              {shape.parent_name}
            </div>
          </div>
          <div>
            <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "8px" }}>Mesh Statistics</div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px", background: "rgba(22, 160, 255, 0.03)", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "12px" }}>
              <div>
                <div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>TRIANGLES</div>
                <div style={{ fontSize: "15px", fontWeight: "600", color: "var(--text-primary)" }}>{totalTris}</div>
              </div>
              <div>
                <div style={{ fontSize: "10px", color: "var(--text-muted)", marginBottom: "2px" }}>VERTICES</div>
                <div style={{ fontSize: "15px", fontWeight: "600", color: "var(--text-primary)" }}>{totalVerts}</div>
              </div>
            </div>
          </div>
          
                  <div style={{ display: "flex", gap: "8px" }}>
          <button onClick={handleImportEngineShapeOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
            <Download size={14} style={{ color: "var(--accent-cyan)" }} /> Import OBJ
          </button>
          <button onClick={handleExportEngineShapeOBJ} style={{ height: "32px", fontSize: "12px", display: "flex", alignItems: "center", justifyContent: "center", gap: "6px", flex: 1 }}>
            <Upload size={14} style={{ color: "var(--accent-cyan)" }} /> Export OBJ
          </button>
        </div>

          {model.materials?.length > 0 && shape.mesh?.parts?.length > 0 && (
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>Material Assignment</label>
              <div style={{ display: "flex", flexDirection: "column", gap: "6px", background: "rgba(0,0,0,0.15)", padding: "8px", borderRadius: "4px" }}>
                {shape.mesh.parts.map((part, pIdx) => (
                  <div key={pIdx} style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: "8px" }}>
                    <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>Part {pIdx}</span>
                    <select value={part.material_index} onChange={(e) => {
                      const newMatIdx = parseInt(e.target.value, 10);
                      onModelChange?.({ ...model, engine_shapes: model.engine_shapes.map(s => s.name === shape.name ? { ...s, mesh: { ...s.mesh, parts: s.mesh.parts.map((p, i) => i === pIdx ? { ...p, material_index: newMatIdx } : p) } } : s) });
                    }} style={{ width: "160px", height: "26px", padding: "2px 6px", fontSize: "11px", background: "#050a12", border: "1px solid var(--border-color)", color: "white" }}>
                      {model.materials.map((mat, mIdx) => (<option key={mIdx} value={mIdx}>{mat.name}</option>))}
                    </select>
                  </div>
                ))}
              </div>
            </div>
          )}

        </div>
      );
    }

    if (selectedNode.type === "dockpath") {
      const path = model.dockpaths?.find(p => p.name === selectedNode.name);
      if (!path) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Dockpath not found</div>;

      const handleAddPoint = () => {
        const newPt: HODDockpoint = {
          position: { x: 0, y: 0, z: 0 },
          rotation: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          },
          tolerance: 10.0,
          max_speed: 150.0,
          extra1: 0,
          extra2: 0
        };

        const updatedPaths = model.dockpaths.map((dp) => {
          if (dp.name === path.name) {
            return { ...dp, points: [...(dp.points || []), newPt] };
          }
          return dp;
        });

        onModelChange?.({ ...model, dockpaths: updatedPaths });
        invoke("log_event", { level: "INFO", message: `Added new dockpoint to path ${path.name}` }).catch(console.error);
      };

      const handleRemovePoint = (ptIdx: number) => {
        const updatedPaths = model.dockpaths.map((dp) => {
          if (dp.name === path.name) {
            return { ...dp, points: dp.points.filter((_, pIdx) => pIdx !== ptIdx) };
          }
          return dp;
        });

        onModelChange?.({ ...model, dockpaths: updatedPaths });
        invoke("log_event", { level: "INFO", message: `Removed dockpoint index ${ptIdx} from path ${path.name}` }).catch(console.error);
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
              <Activity size={12} style={{ color: "var(--accent-cyan)" }} />
              <span>Selected Dockpath</span>
            </div>
            <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
              {selectedNode.name}
            </div>
          </div>
          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />
          <div>
            <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "4px" }}>Parent Joint</div>
            <div style={{ background: "rgba(255,255,255,0.03)", padding: "8px 10px", borderRadius: "4px", fontSize: "12px", border: "1px solid var(--border-color)" }}>
              {path.parent_name}
            </div>
          </div>
          <div>
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "8px" }}>
              <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500" }}>Dockpoints ({path.points?.length || 0})</div>
              <button
                onClick={handleAddPoint}
                style={{
                  fontSize: "10px",
                  padding: "2px 8px",
                  background: "rgba(22, 160, 255, 0.15)",
                  color: "var(--accent-cyan)",
                  border: "1px solid rgba(22, 160, 255, 0.3)",
                  borderRadius: "4px",
                  cursor: "pointer",
                  fontWeight: "600"
                }}
              >
                + Add Point
              </button>
            </div>
            <div style={{ display: "flex", flexDirection: "column", gap: "6px", maxHeight: "250px", overflowY: "auto", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "6px" }}>
              {path.points && path.points.length > 0 ? (
                path.points.map((pt, idx) => (
                  <div key={idx} style={{ padding: "6px 8px", fontSize: "11px", background: "rgba(255,255,255,0.02)", borderRadius: "3px", display: "flex", flexDirection: "column", gap: "2px" }}>
                    <div style={{ display: "flex", justifyContent: "space-between", fontWeight: "500" }}>
                      <span style={{ color: "var(--accent-cyan)" }}>Pt {idx}</span>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                        <span style={{ color: "var(--text-muted)" }}>Tol: {pt.tolerance} | Spd: {pt.max_speed}</span>
                        <button
                          onClick={() => handleRemovePoint(idx)}
                          style={{
                            background: "transparent",
                            border: "none",
                            color: "var(--accent-danger)",
                            cursor: "pointer",
                            fontSize: "11px",
                            padding: "0 2px"
                          }}
                          title="Remove Point"
                        >
                          ✕
                        </button>
                      </div>
                    </div>
                    <div style={{ fontFamily: "var(--font-mono)", color: "var(--text-secondary)", fontSize: "10px" }}>
                      Pos: [{pt.position.x.toFixed(2)}, {pt.position.y.toFixed(2)}, {pt.position.z.toFixed(2)}]
                    </div>
                  </div>
                ))
              ) : (
                <div style={{ padding: "12px", color: "var(--text-muted)", fontSize: "11px", textAlign: "center" }}>
                  No dockpoints defined in this path.
                </div>
              )}
            </div>
          </div>
        </div>
      );
    }


    if (selectedNode.type === "texture_group") {
      const groups = parseTextureGroups(model.textures || []);
      const group = groups.find(g => g.baseName === selectedNode.name);
      if (!group) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Texture Group not found</div>;

      const handleGroupNameChange = (newName: string) => {
        if (!newName || newName === group.baseName) return;
        
        let updatedTextures = [...model.textures];
        let updatedMaterials = [...(model.materials || [])];

        group.textures.forEach(item => {
          const newTexName = newName + item.type + item.compression;
          
          // Update textures array
          updatedTextures = updatedTextures.map(t => {
            if (t === item.texture) {
              return { ...t, name: newTexName };
            }
            return t;
          });

          // Update materials referencing this texture
          updatedMaterials = updatedMaterials.map(m => {
            let maps = [...(m.texture_maps || [])];
            let changed = false;
            maps.forEach((name, idx) => {
              if (name === item.originalName) {
                maps[idx] = newTexName;
                changed = true;
              }
            });
            return changed ? { ...m, texture_maps: maps } : m;
          });
        });

        onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
        onSelectedNodeChange?.({ type: "texture_group", name: newName });
      };

      const handleToggleTextureFlip = (item: any) => {
        const updatedTextures = model.textures.map(t => {
          if (t === item.texture) {
            return { ...t, legacy_storage_y_flipped: !t.legacy_storage_y_flipped };
          }
          return t;
        });
        onModelChange?.({ ...model, textures: updatedTextures, textures_modified: true });
      };

      const handleRemoveTexture = (item: any) => {
        if (!window.confirm(`Are you sure you want to remove the texture "${item.originalName}"?`)) return;
        const updatedTextures = model.textures.filter(t => t !== item.texture);
        
        // Remove from materials
        const updatedMaterials = model.materials?.map(m => {
           let maps = [...(m.texture_maps || [])];
           let changed = false;
           maps.forEach((name, idx) => {
              if (name === item.originalName) {
                 maps[idx] = "";
                 changed = true;
              }
           });
           return changed ? { ...m, texture_maps: maps } : m;
        }) || [];

        onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
      };

      
      const handleTypeChange = (item: any, newType: string) => {
        const newTexName = group.baseName + newType + item.compression;
        
        let updatedTextures = model.textures.map(t => {
          if (t === item.texture) {
            return { ...t, name: newTexName };
          }
          return t;
        });

        let updatedMaterials = (model.materials || []).map(m => {
          const newMaps = m.texture_maps.map(mapName => mapName === item.originalName ? newTexName : mapName);
          return { ...m, texture_maps: newMaps };
        });

        onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
      };

      const handleCompressionChange = (item: any, newCompression: string) => {
        const newTexName = group.baseName + item.type + newCompression;
        
        let updatedTextures = model.textures.map(t => {
          if (t === item.texture) {
            return { ...t, name: newTexName, format: newCompression };
          }
          return t;
        });

        let updatedMaterials = (model.materials || []).map(m => {
          const newMaps = m.texture_maps.map(mapName => mapName === item.originalName ? newTexName : mapName);
          return { ...m, texture_maps: newMaps };
        });

        onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
      };

      const handleDeleteTexture = (item: any) => {
        if (!window.confirm(`Are you sure you want to delete ${item.type || "this"} texture from the group?`)) return;
        
        let updatedTextures = model.textures.filter(t => t !== item.texture);
        let updatedMaterials = (model.materials || []).map(m => {
          const newMaps = m.texture_maps.map(mapName => mapName === item.originalName ? "" : mapName);
          return { ...m, texture_maps: newMaps };
        });

        onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", height: "100%" }}>
          <div style={{ padding: "16px", borderBottom: "1px solid rgba(255,255,255,0.05)" }}>
            <div style={{ display: "flex", alignItems: "center", gap: "10px", marginBottom: "8px" }}>
              <div style={{ width: "32px", height: "32px", borderRadius: "6px", background: "rgba(255,255,255,0.1)", display: "flex", alignItems: "center", justifyContent: "center" }}>
                <Image size={18} style={{ color: "var(--accent-cyan)" }} />
              </div>
              <div style={{ flex: 1 }}>
                <h3 style={{ margin: 0, fontSize: "14px", fontWeight: "600", color: "var(--text-primary)" }}>Texture Group</h3>
                <span style={{ fontSize: "11px", color: "var(--text-muted)" }}>Manage sub-textures and formats</span>
              </div>
            </div>
          </div>
          
          <div style={{ padding: "16px", overflowY: "auto", flex: 1, display: "flex", flexDirection: "column", gap: "16px" }}>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>
                Group Base Name
              </label>
              <TextInput
                value={group.baseName}
                onChange={(val) => handleGroupNameChange(val)}
                style={{ width: "100%", background: "rgba(0,0,0,0.2)", border: "1px solid rgba(255,255,255,0.1)", color: "var(--text-primary)", padding: "6px 10px", borderRadius: "4px", fontSize: "12px", fontFamily: "var(--font-mono)" }}
              />
            </div>

            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>
                Sub-Textures
              </label>
              <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
                {group.textures.map((item, idx) => (
                  <div key={idx} style={{ background: "rgba(0,0,0,0.15)", borderRadius: "4px", padding: "10px", border: "1px solid rgba(255,255,255,0.03)" }}>
                    <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "8px" }}>
                      <select
                        value={item.type}
                        onChange={(e) => handleTypeChange(item, e.target.value)}
                        style={{ fontSize: "12px", fontWeight: "600", color: "var(--accent-cyan)", background: "transparent", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "3px", padding: "2px 4px", outline: "none", cursor: "pointer" }}
                      >
                        <option value="" style={{ color: "black" }}>(Base Texture)</option>
                        {KNOWN_TYPES.map((t: string) => (
                          <option key={t} value={t} style={{ color: "black" }}>{t}</option>
                        ))}
                      </select>
                      <Trash2 size={14} style={{ cursor: "pointer", color: "var(--text-muted)" }} onClick={() => handleDeleteTexture(item)} />
                    </div>
                    <div style={{ display: "flex", gap: "10px" }}>
                      {item.texture.png_preview ? (
                        <img
                          src={item.texture.png_preview.startsWith("data:") ? item.texture.png_preview : `data:image/png;base64,${item.texture.png_preview}`}
                          alt={item.originalName}
                          style={{ width: "48px", height: "48px", objectFit: "cover", borderRadius: "4px", border: "1px solid var(--border-color)", background: "#000", flexShrink: 0 }}
                        />
                      ) : (
                        <div style={{ width: "48px", height: "48px", borderRadius: "4px", background: "rgba(255,255,255,0.05)", display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0 }}>
                          <Image size={16} style={{ color: "var(--text-muted)" }} />
                        </div>
                      )}
                      <div style={{ flex: 1, display: "flex", flexDirection: "column", gap: "6px" }}>
                        <div style={{ display: "flex", justifyContent: "space-between", fontSize: "11px", color: "var(--text-muted)" }}>
                          <span>Size: {item.texture.width}x{item.texture.height}</span>
                          <div style={{ display: "flex", gap: "4px" }}>
                            <button
                              className="icon-button"
                              onClick={() => handleToggleTextureFlip(item)}
                              title="Toggle Y-Flip (legacy storage)"
                              style={{ 
                                padding: "2px 4px", 
                                background: item.texture.legacy_storage_y_flipped ? "var(--accent-cyan)" : "rgba(255,255,255,0.05)",
                                border: "1px solid rgba(255,255,255,0.1)", 
                                borderRadius: "3px", 
                                color: item.texture.legacy_storage_y_flipped ? "black" : "var(--text-primary)"
                              }}
                            >
                              <FlipVertical size={12} />
                            </button>
                            <button
                              className="icon-button"
                              onClick={() => handleRemoveTexture(item)}
                              title="Remove Texture"
                              style={{ padding: "2px 4px", background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "3px", color: "#ff1744" }}
                            >
                              <Trash2 size={12} />
                            </button>
                          </div>
                        </div>
                        <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                          <span style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Format:</span>
                          <select
                            value={item.compression}
                            onChange={(e) => handleCompressionChange(item, e.target.value)}
                            style={{ flex: 1, padding: "4px 8px", fontSize: "11px", background: "rgba(0,0,0,0.3)", border: "1px solid var(--border-color)", color: "white", borderRadius: "3px" }}
                          >
                            <option value="DXT1">DXT1</option>
                            <option value="DXT3">DXT3</option>
                            <option value="DXT5">DXT5</option>
                            <option value="RGBA">RGBA</option>
                          </select>
                        </div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "material") {
      const material = model.materials?.find(m => m.name === selectedNode.name);
      if (!material) return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Material not found</div>;

      const handleMaterialNameChange = (newName: string) => {
        if (!newName) return;
        const updatedMaterials = model.materials.map(m => {
          if (m.name === material.name) {
            return { ...m, name: newName };
          }
          return m;
        });
        onModelChange?.({ ...model, materials: updatedMaterials });
        // Also update the selection node name so it stays active!
        selectedNode.name = newName;
      };

      const handleShaderChange = (newShader: string) => {
        const updatedMaterials = model.materials.map(m => {
          if (m.name === material.name) {
            return { ...m, shader_name: newShader };
          }
          return m;
        });
        onModelChange?.({ ...model, materials: updatedMaterials });
      };

      const handleTextureGroupChange = (groupName: string) => {
        const groups = parseTextureGroups(model.textures || []);
        const group = groups.find(g => g.baseName === groupName);
        
        const updatedMaterials = model.materials.map(m => {
          if (m.name === material.name) {
            const expectedSlots = getExpectedSlots(m.shader_name || "", m.texture_maps || []);
            
            const newMaps = expectedSlots.map(slotLabel => {
              if (!group) return "";
              
              let expectedType = "";
              const match = slotLabel.match(/\((.*?)\)/);
              if (match) {
                expectedType = "_" + match[1];
              }
              
              const matchedTex = expectedType ? findTextureForSlot(group, expectedType) : undefined;
              if (matchedTex) return matchedTex.originalName;
              
              // Fallback to empty string if missing in group
              return "";
            });
            
            return { ...m, texture_maps: newMaps };
          }
          return m;
        });
        onModelChange?.({ ...model, materials: updatedMaterials });
      };

      const handleRemoveMaterial = (name: string) => {
        const matIndex = model.materials.findIndex(m => m.name === name);
        const updatedMaterials = model.materials.filter(m => m.name !== name);
        let updatedMeshes = model.meshes;
        if (matIndex !== -1) {
          updatedMeshes = model.meshes.map((mesh) => {
            const updatedParts = mesh.parts.map((part) => {
              if (part.material_index === matIndex) {
                return { ...part, material_index: 0 };
              } else if (part.material_index > matIndex) {
                return { ...part, material_index: part.material_index - 1 };
              }
              return part;
            });
            return { ...mesh, parts: updatedParts };
          });
        }
        onModelChange?.({ ...model, materials: updatedMaterials, meshes: updatedMeshes });
        onSelectedNodeChange?.(null);
        invoke("log_event", { level: "INFO", message: `Removed material: ${name} from Inspector.` }).catch(console.error);
      };

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <div>
              <div style={{ display: "flex", alignItems: "center", gap: "8px", fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px" }}>
                <Palette size={12} style={{ color: "var(--accent-cyan)" }} />
                <span>Material Configuration</span>
              </div>
              <div style={{ fontSize: "16px", fontWeight: "600", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
                {material.name}
              </div>
            </div>
            <button
              onClick={() => {
                if (window.confirm(`Are you sure you want to remove the material "${material.name}"?`)) {
                  handleRemoveMaterial(material.name);
                }
              }}
              style={{
                padding: "4px 10px",
                background: "rgba(255, 23, 68, 0.15)",
                border: "1px solid rgba(255, 23, 68, 0.35)",
                color: "#ff1744",
                fontSize: "11px",
                fontWeight: "600",
                cursor: "pointer",
                borderRadius: "4px",
                height: "28px"
              }}
              title="Remove this material"
            >
              Remove Material
            </button>
          </div>

          <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

          <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>
                Material Name
              </label>
              <TextInput
                value={material.name}
                onChange={(val) => handleMaterialNameChange(val)}
                style={{ width: "100%", background: "rgba(0,0,0,0.2)", border: "1px solid rgba(255,255,255,0.1)", color: "var(--text-primary)", padding: "6px 10px", borderRadius: "4px", fontSize: "12px", fontFamily: "var(--font-mono)" }}
              />
            </div>

            <div>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: "6px" }}>
                <label style={{ fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500" }}>
                  Shader / Pipeline Name
                </label>
                <div style={{ display: "flex", gap: "4px" }}>
                  <button
                    onClick={loadPipelines}
                    style={{ fontSize: "10px", padding: "2px 8px", background: "var(--accent-cyan)", color: "#000", border: "none", borderRadius: "3px", cursor: "pointer", fontWeight: "600" }}
                    title="Reload shaders from configured directories"
                  >
                    Refresh
                  </button>
                </div>
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "6px" }}>
                <select
                  value={material.shader_name || ""}
                  onChange={(e) => handleShaderChange(e.target.value)}
                  style={{ width: "100%" }}
                >
                  <option value="">-- Select Shader --</option>
                  {shaderOptions.map((p) => (
                    <option key={p} value={p}>{p}</option>
                  ))}
                </select>
                {shaderOptions.length === 0 && (
                  <div style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic" }}>
                    No shaders loaded. Configure shader directories in Settings.
                  </div>
                )}
              </div>
            </div>

            <div>
              <label style={{ display: "block", fontSize: "11px", color: "var(--text-secondary)", fontWeight: "500", marginBottom: "6px" }}>
                Texture Mapping
              </label>
              <div style={{ display: "flex", flexDirection: "column", gap: "8px", background: "rgba(0,0,0,0.15)", padding: "10px", borderRadius: "4px" }}>
                {(() => {
                  const groups = parseTextureGroups(model.textures || []);
                  
                  // Guess the currently assigned group based on the first slot (Diffuse usually)
                  let currentGroup = "";
                  if (material.texture_maps && material.texture_maps[0]) {
                    const firstTex = material.texture_maps[0];
                    const matchedGroup = groups.find(g => g.textures.some(t => t.originalName === firstTex));
                    if (matchedGroup) {
                      currentGroup = matchedGroup.baseName;
                    }
                  }

                  const expectedSlots = getExpectedSlots(material.shader_name || "", material.texture_maps || []);

                  return (
                    <div style={{ display: "flex", flexDirection: "column", gap: "10px" }}>
                      <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                        <span style={{ fontSize: "11px", color: "var(--text-muted)", flexShrink: 0 }}>Texture Group:</span>
                        <select
                          value={currentGroup}
                          onChange={(e) => handleTextureGroupChange(e.target.value)}
                          style={{ flex: 1, padding: "6px 8px", fontSize: "12px", background: "rgba(0,0,0,0.3)", border: "1px solid var(--border-color)", color: "white", borderRadius: "3px" }}
                        >
                          <option value="">(None)</option>
                          {groups.map(g => (
                            <option key={g.baseName} value={g.baseName}>{g.baseName}</option>
                          ))}
                        </select>
                      </div>
                      
                      {currentGroup && (
                        <div style={{ padding: "8px", background: "rgba(255,255,255,0.03)", borderRadius: "4px", display: "flex", flexDirection: "column", gap: "6px" }}>
                          {expectedSlots.map((slotLabel, idx) => {
                            const mapName = material.texture_maps?.[idx] || "";
                            return (
                              <div key={idx} style={{ display: "flex", justifyContent: "space-between", fontSize: "11px" }}>
                                <span style={{ color: "var(--text-secondary)" }}>{slotLabel}</span>
                                {mapName ? (
                                  <span style={{ color: "var(--accent-cyan)", fontFamily: "var(--font-mono)" }}>{mapName}</span>
                                ) : (
                                  <span style={{ color: "var(--text-muted)" }}>-- Missing --</span>
                                )}
                              </div>
                            );
                          })}
                        </div>
                      )}
                    </div>
                  );
                })()}


              </div>
            </div>
          </div>
        </div>
      );
    }

    if (selectedNode.type === "keyframe") {
      const parts = selectedNode.name.split(":");
      const jointName = parts[0];
      const kfIdx = parseInt(parts[1] || "0", 10);

      const activeAnimIdx = selectedAnimIdx ?? 0;
      const activeAnim = model?.animations?.[activeAnimIdx];
      if (!activeAnim) {
        return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>No active animation found</div>;
      }

      const track = activeAnim.tracks.find(t => t.joint_name.toLowerCase() === jointName.toLowerCase());
      if (!track) {
        return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Track not found</div>;
      }

      const kf = track.keyframes[kfIdx];
      if (!kf) {
        return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Keyframe not found</div>;
      }

      const p = kf.position || { x: 0, y: 0, z: 0 };
      const q = kf.rotation || { x: 0, y: 0, z: 0, w: 1 };

      const handlePositionChange = (axis: "x" | "y" | "z", valueStr: string) => {
        const val = parseFloat(valueStr) || 0;
        const updatedTracks = activeAnim.tracks.map(t => {
          if (t.joint_name.toLowerCase() !== jointName.toLowerCase()) return t;
          const updatedKfs = t.keyframes.map((keyf, idx) => {
            if (idx !== kfIdx) return keyf;
            const newPos = { ...(keyf.position || { x: 0, y: 0, z: 0 }), [axis]: val };
            return { ...keyf, position: newPos };
          });
          return { ...t, keyframes: updatedKfs };
        });
        const updatedAnim = { ...activeAnim, tracks: updatedTracks };
        const updatedAnims = (model.animations ?? []).map((anim, idx) => idx === activeAnimIdx ? updatedAnim : anim);
        onModelChange?.({ ...model, animations: updatedAnims });
      };

      const handleTimeChange = (valueStr: string) => {
        const val = parseFloat(valueStr) || 0;
        const updatedTracks = activeAnim.tracks.map(t => {
          if (t.joint_name.toLowerCase() !== jointName.toLowerCase()) return t;
          const updatedKfs = t.keyframes.map((keyf, idx) => {
            if (idx !== kfIdx) return keyf;
            return { ...keyf, time: Math.max(0, Math.min(activeAnim.duration, val)) };
          });
          updatedKfs.sort((a, b) => a.time - b.time);
          return { ...t, keyframes: updatedKfs };
        });
        const updatedAnim = { ...activeAnim, tracks: updatedTracks };
        const updatedAnims = (model.animations ?? []).map((anim, idx) => idx === activeAnimIdx ? updatedAnim : anim);
        onModelChange?.({ ...model, animations: updatedAnims });
      };

      const handleRotationChange = (axis: "x" | "y" | "z", valueStr: string) => {
        const val = parseFloat(valueStr) || 0;
        const currentEuler = kf.rotation_euler ? {
          x: kf.rotation_euler.x * 180 / Math.PI,
          y: kf.rotation_euler.y * 180 / Math.PI,
          z: kf.rotation_euler.z * 180 / Math.PI
        } : quatToEulerDegrees(q);
        const newEuler = { ...currentEuler, [axis]: val };
        const newQuat = eulerDegreesToQuat(newEuler.x, newEuler.y, newEuler.z);
        const newEulerRad = { x: newEuler.x * Math.PI / 180, y: newEuler.y * Math.PI / 180, z: newEuler.z * Math.PI / 180 };
        const updatedTracks = activeAnim.tracks.map(t => {
          if (t.joint_name.toLowerCase() !== jointName.toLowerCase()) return t;
          const updatedKfs = t.keyframes.map((keyf, idx) => {
            if (idx !== kfIdx) return keyf;
            return { ...keyf, rotation: newQuat, rotation_euler: newEulerRad };
          });
          return { ...t, keyframes: updatedKfs };
        });
        const updatedAnim = { ...activeAnim, tracks: updatedTracks };
        const updatedAnims = (model.animations ?? []).map((anim, idx) => idx === activeAnimIdx ? updatedAnim : anim);
        onModelChange?.({ ...model, animations: updatedAnims });
      };

      const euler = kf.rotation_euler ? {
        x: kf.rotation_euler.x * 180 / Math.PI,
        y: kf.rotation_euler.y * 180 / Math.PI,
        z: kf.rotation_euler.z * 180 / Math.PI
      } : quatToEulerDegrees(q);

      return (
        <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
          <div style={{ display: "flex", alignItems: "center", gap: "10px", background: "rgba(255, 171, 0, 0.08)", border: "1px solid rgba(255, 171, 0, 0.25)", borderRadius: "6px", padding: "10px 12px" }}>
            <Activity size={18} style={{ color: "#ffd600" }} />
            <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
              <span style={{ fontWeight: "700", fontSize: "13px", color: "var(--accent-cyan)", textTransform: "uppercase" }}>Keyframe Editor</span>
              <span style={{ fontSize: "10px", color: "var(--text-secondary)" }}>Track: {jointName}</span>
            </div>
          </div>

          <div style={{ fontSize: "11px", color: "var(--text-muted)", lineHeight: "1.5", background: "rgba(255,255,255,0.02)", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "8px 10px" }}>
            Keyframes store animation deltas relative to the joint's rest pose. They are always displayed in local/animation space and are not affected by the Local/Global toggle.
          </div>

          <div className="card-bg" style={{ display: "flex", flexDirection: "column", gap: "8px", padding: "12px", borderRadius: "6px", border: "1px solid var(--border-color)", background: "rgba(0,0,0,0.15)" }}>
            <div style={{ fontSize: "10px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)" }}>
              Timeline Position
            </div>            <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
              <span style={{ fontSize: "9px", color: "var(--text-muted)" }}>Time (seconds)</span>
              <NumericInput
                step="0.05"
                min="0"
                max={activeAnim.duration.toString()}
                value={kf.time}
                onChange={handleTimeChange}
                onWheel={(e) => handleNumericWheel(e, handleTimeChange, 0.05)}
                style={{ height: "28px", fontSize: "12px" }}
              />
            </div>
          </div>

          <div className="card-bg" style={{ display: "flex", flexDirection: "column", gap: "8px", padding: "12px", borderRadius: "6px", border: "1px solid var(--border-color)", background: "rgba(0,0,0,0.15)" }}>
            <div style={{ fontSize: "10px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)" }}>
              Position Delta
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-danger)", fontWeight: "600" }}>X</span>
                <NumericInput
                  step="0.05"
                  value={p.x}
                  onChange={(v) => handlePositionChange("x", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handlePositionChange("x", v), 0.05)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-success)", fontWeight: "600" }}>Y</span>
                <NumericInput
                  step="0.05"
                  value={p.y}
                  onChange={(v) => handlePositionChange("y", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handlePositionChange("y", v), 0.05)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-blue)", fontWeight: "600" }}>Z</span>
                <NumericInput
                  step="0.05"
                  value={p.z}
                  onChange={(v) => handlePositionChange("z", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handlePositionChange("z", v), 0.05)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
            </div>
          </div>

          <div className="card-bg" style={{ display: "flex", flexDirection: "column", gap: "8px", padding: "12px", borderRadius: "6px", border: "1px solid var(--border-color)", background: "rgba(0,0,0,0.15)" }}>
            <div style={{ fontSize: "10px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)" }}>
              Rotation Angles (Euler Degrees)
            </div>
            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "8px" }}>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-danger)", fontWeight: "600" }}>Pitch</span>
                <NumericInput
                  step="1.0"
                  value={euler.x}
                  onChange={(v) => handleRotationChange("x", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handleRotationChange("x", v), 1.0)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-success)", fontWeight: "600" }}>Yaw</span>
                <NumericInput
                  step="1.0"
                  value={euler.y}
                  onChange={(v) => handleRotationChange("y", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handleRotationChange("y", v), 1.0)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
              <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                <span style={{ fontSize: "9px", color: "var(--accent-blue)", fontWeight: "600" }}>Roll</span>
                <NumericInput
                  step="1.0"
                  value={euler.z}
                  onChange={(v) => handleRotationChange("z", v)}
                  onWheel={(e) => handleNumericWheel(e, (v) => handleRotationChange("z", v), 1.0)}
                  style={{ height: "28px", fontSize: "12px" }}
                />
              </div>
            </div>
          </div>

          <div className="card-bg" style={{ display: "flex", flexDirection: "column", gap: "6px", padding: "10px 12px", borderRadius: "6px", background: "rgba(255,255,255,0.02)", border: "1px solid rgba(255,255,255,0.05)" }}>
            <span style={{ fontSize: "8px", color: "var(--text-muted)", textTransform: "uppercase" }}>Raw Quaternion Value</span>
            <span style={{ fontFamily: "var(--font-mono)", fontSize: "9px", color: "var(--text-secondary)", wordBreak: "break-all" }}>
              [{q.x.toFixed(4)}, {q.y.toFixed(4)}, {q.z.toFixed(4)}, {q.w.toFixed(4)}]
            </span>
          </div>
        </div>
      );
    }

    return <div style={{ color: "var(--text-muted)", textAlign: "center" }}>Unsupported node selection</div>;
  };

  return (
    <div className="panel panel-right" style={{ height: "100%", display: "flex", flexDirection: "column" }}>
      <div className="panel-header" style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: "8px" }}>
        <span>Inspector</span>
        {setCoordinateSpace && (
          <div style={{ display: "flex", gap: "2px" }} title="Coordinate space for position/rotation fields">
            <button
              onClick={() => setCoordinateSpace("local")}
              className={coordinateSpace === "local" ? "primary" : ""}
              style={{ fontSize: "9px", height: "20px", padding: "0 6px" }}
            >
              Local
            </button>
            <button
              onClick={() => setCoordinateSpace("global")}
              className={coordinateSpace === "global" ? "primary" : ""}
              style={{ fontSize: "9px", height: "20px", padding: "0 6px" }}
            >
              Global
            </button>
          </div>
        )}
      </div>
      <div className="panel-content" style={{ display: "flex", flexDirection: "column", gap: "16px", overflowY: "auto", flex: 1, padding: "15px" }}>
        {/* Selected element form details */}
        {renderSelectedContent()}
      </div>
    </div>
  );
};
