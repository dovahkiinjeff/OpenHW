import React, { useState, useRef } from "react";
import { createPortal } from "react-dom";
import { invoke } from "../bridge";
import { HODModel, HODAnimation, HODDockpath } from "./Viewport";
import { parseTextureGroups, KNOWN_TYPES, getExpectedTextureType } from "../texture_utils";
import { Folder, FolderOpen, Tag, ChevronDown, ChevronRight, Search, Box, Eye, EyeOff, Radio, Activity, Shield, Flame, Palette, Crosshair, Plus, Trash2, AlertTriangle, Info , FlipVertical, Download, Upload, Wand2, Pencil } from "lucide-react";
import {
  getSlaveBases,
  isJointInWeaponFamily,
  isJointInWeaponBase,
  nextSlaveBase,
  cloneWeaponFamilyJoints,
  renameWeaponFamilyName,
  muzzleIndex,
  muzzleName,
  nextMuzzleIndex,
} from "../weapon_utils";

type TreeJoint = HODModel["joints"][number];
type WeaponGroupInfo = { baseName: string; suffix: string; type: string } | null;

/** Per-model lookups for the tree render; built lazily by getTreeIndex. */
interface TreeIndex {
  jointsByName: Map<string, TreeJoint>;
  childrenByParent: Map<string, TreeJoint[]>;
  groupInfoByName: Map<string, WeaponGroupInfo>;
  groupJointsByBase: Map<string, TreeJoint[]>;
  /** Assembly bases whose root joint(s) hang off the keyed parent, in group order. */
  weaponGroupsByParent: Map<string, string[]>;
  navLightsByParent: Map<string, NonNullable<HODModel["nav_lights"]>[number][]>;
  markersByParent: Map<string, HODModel["markers"][number][]>;
  meshesByParent: Map<string, HODModel["meshes"][number][]>;
  navLightNamesLower: Set<string>;
  engineParents: Set<string>;
}

interface HierarchyTreeProps {
  model: HODModel | null;
  selectedNode: { type: string; name: string } | null;
  setSelectedNode: (node: { type: string; name: string } | null) => void;
  visibleMeshes: Record<string, boolean>;
  setVisibleMeshes: React.Dispatch<React.SetStateAction<Record<string, boolean>>>;
  onReParentNode?: (nodeName: string, nodeType: string, newParentName: string) => void;
  onModelChange?: (updatedModel: HODModel) => void;
  selectedAnimIdx: number;
  setSelectedAnimIdx: (idx: number) => void;
  targetBoxes?: any[];
  setTargetBoxes?: (boxes: any[]) => void;
  onTabChange?: (tab: string) => void;
  setIsLoading?: (loading: boolean) => void;
  setStatusMsg?: (msg: string) => void;
}

const handleNumericWheel = (e: React.WheelEvent<HTMLInputElement>, onChange: (val: string) => void, step = 1) => {
  e.preventDefault();
  const direction = e.deltaY < 0 ? 1 : -1;
  const currentVal = parseFloat(e.currentTarget.value) || 0;
  const stepStr = step.toString();
  const decimals = stepStr.includes(".") ? stepStr.split(".")[1].length : 0;
  const newVal = parseFloat((currentVal + direction * step).toFixed(Math.max(decimals, 3)));
  onChange(newVal.toString());
};

interface NumericInputProps {
  value: number;
  onChange: (val: string) => void;
  onWheel?: (e: React.WheelEvent<HTMLInputElement>) => void;
  step?: string;
  min?: string;
  max?: string;
  style?: React.CSSProperties;
}

const NumericInput: React.FC<NumericInputProps> = ({
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

  React.useEffect(() => {
    if (!isFocusedRef.current || isWheelingRef.current) {
      setLocalValue(value.toString());
      isWheelingRef.current = false;
    }
  }, [value]);

  React.useEffect(() => {
    const el = inputRef.current;
    if (!el) return;
    
    const handleNativeWheel = (e: WheelEvent) => {
      if (onWheel) {
        // Prevent browser's native default scroll which would scroll the parent container
        e.preventDefault();
        e.stopPropagation();
        isWheelingRef.current = true;
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

export const HierarchyTree: React.FC<HierarchyTreeProps> = ({
  model,
  selectedNode,
  setSelectedNode,
  visibleMeshes,
  setVisibleMeshes,
  onReParentNode,
  onModelChange,
  selectedAnimIdx,
  setSelectedAnimIdx,
  targetBoxes = [],
  setTargetBoxes,
  onTabChange,
  setIsLoading,
  setStatusMsg,
}) => {
  const [searchTerm, setSearchTerm] = useState("");
  const [activeTab, setActiveTab] = useState<"hierarchy" | "materials" | "animations" | "targetboxes">("hierarchy");
  const [selectedBoxIdx, setSelectedBoxIdx] = useState<number | null>(null);
  const [showLuaCode, setShowLuaCode] = useState(false);

  
  const handleAutoFixTextures = () => {
    if (!model) return;
    
    let updatedTextures = [...model.textures];
    let updatedMaterials = (model.materials || []).map(m => ({ ...m, texture_maps: [...(m.texture_maps || [])] }));
    let renameCount = 0;
    
    // Create a mapping of old texture names to their updated info
    for (let mIdx = 0; mIdx < updatedMaterials.length; mIdx++) {
      const mat = updatedMaterials[mIdx];
      
      for (let i = 0; i < mat.texture_maps.length; i++) {
        const oldName = mat.texture_maps[i];
        if (!oldName) continue;
        
        const expectedSuffix = getExpectedTextureType(mat.shader_name, i);
        if (!expectedSuffix) continue;
        
        const texIdx = updatedTextures.findIndex(t => t.name === oldName);
        if (texIdx === -1) continue;
        
        const tex = updatedTextures[texIdx];
        
        // Strip compression
        let baseName = tex.name;
        if (baseName.toUpperCase().endsWith(tex.format.toUpperCase())) {
          baseName = baseName.substring(0, baseName.length - tex.format.length);
        }
        
        // If it doesn't already end with the expected suffix
        if (!baseName.toUpperCase().endsWith(expectedSuffix.toUpperCase())) {
          const newName = baseName + expectedSuffix + tex.format;
          
          // Apply rename to the texture
          updatedTextures[texIdx] = { ...tex, name: newName };
          
          // Apply rename to the material
          mat.texture_maps[i] = newName;
          
          // Also rename in other materials using this oldName
          for (let omIdx = 0; omIdx < updatedMaterials.length; omIdx++) {
            const oMat = updatedMaterials[omIdx];
            for (let j = 0; j < oMat.texture_maps.length; j++) {
              if (oMat.texture_maps[j] === oldName) {
                oMat.texture_maps[j] = newName;
              }
            }
          }
          
          renameCount++;
        }
      }
    }
    
    if (renameCount > 0) {
      onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
      alert(`Auto-fixed ${renameCount} texture names to match their assigned shader slots.`);
    } else {
      alert("All textures are already correctly named according to their slots!");
    }
  };

  const handleImportTGA = async () => {
    try {
      setIsLoading?.(true);
      setStatusMsg?.("Importing TGA textures...");
      
      setTimeout(async () => {
        try {
          const importedTexs = await invoke<any[] | null>("import_tga_textures");
          if (!importedTexs || importedTexs.length === 0) {
            setIsLoading?.(false);
            return;
          }

          let updatedTextures = [...(model?.textures || [])];
          let importedNames = [];
          for (const importedTex of importedTexs) {
            let nameNoFormat = importedTex.name;
            if (nameNoFormat.toUpperCase().endsWith(importedTex.format.toUpperCase())) {
                nameNoFormat = nameNoFormat.substring(0, nameNoFormat.length - importedTex.format.length);
            }
            const hasKnownType = KNOWN_TYPES.some(t => nameNoFormat.toUpperCase().endsWith(t));

            if (!hasKnownType) {
                let userType = window.prompt(`The imported texture "${importedTex.name}" is missing a recognized type suffix.\n\nPlease enter the texture type suffix (e.g. _DIFF, _GLOW, _SPEC, _TEAM, _NORM, _DIFX, _GLOX):`, "_DIFF");
                if (userType === null) {
                    alert(`Skipping import of ${importedTex.name}`);
                    continue;
                }
                if (!userType.startsWith("_")) userType = "_" + userType;
                importedTex.name = nameNoFormat + userType.toUpperCase() + importedTex.format;
            }

            importedNames.push(importedTex.name);
            const existsIdx = updatedTextures.findIndex(t => t.name.toLowerCase() === importedTex.name.toLowerCase());
            if (existsIdx !== -1) {
              updatedTextures[existsIdx] = importedTex;
            } else {
              updatedTextures.push(importedTex);
            }
          }

          if (importedNames.length > 0 && model) {
            onModelChange?.({ ...model, textures: updatedTextures, textures_modified: true });
            const names = importedNames.join(", ");
            invoke("log_event", { level: "INFO", message: `Imported and bound TGA textures: ${names}` }).catch(console.error);
            alert(`Successfully imported TGA textures "${names}"!\\n\\nThey are now available as a Texture Group.`);
          }
        } catch (e: any) {
          console.error(e);
          alert(`Failed to import TGA texture: ${e.toString()}`);
        } finally {
          setIsLoading?.(false);
        }
      }, 50);
    } catch (e: any) {
      console.error(e);
      setIsLoading?.(false);
    }
  };

  // Animation CRUD
  const [isCreateAnimOpen, setIsCreateAnimOpen] = useState(false);
  const [newAnimName, setNewAnimName] = useState("");
  const [newAnimDuration, setNewAnimDuration] = useState(4.0);

  const handleCreateAnimation = () => {
    if (!model || !newAnimName.trim()) return;
    const newAnim: HODAnimation = {
      name: newAnimName.trim(),
      duration: newAnimDuration,
      tracks: [],
    };
    const updatedAnims = [...(model.animations ?? []), newAnim];
    onModelChange?.({ ...model, animations: updatedAnims });
    setSelectedAnimIdx(updatedAnims.length - 1);
    setIsCreateAnimOpen(false);
    setNewAnimName("");
    setNewAnimDuration(4.0);
  };

  const handleDeleteAnimation = (idxToDelete: number, animName: string, e: React.MouseEvent) => {
    e.stopPropagation();
    if (!model) return;
    const confirmed = window.confirm(`Delete animation "${animName}"? This cannot be undone.`);
    if (!confirmed) return;
    const updatedAnims = (model.animations ?? []).filter((_, i) => i !== idxToDelete);
    onModelChange?.({ ...model, animations: updatedAnims });
    setSelectedAnimIdx(Math.max(0, selectedAnimIdx > idxToDelete ? selectedAnimIdx - 1 : (selectedAnimIdx === idxToDelete ? 0 : selectedAnimIdx)));
  };

  // Duration editing (minimum mirrors the create-animation dialog: 0.1s).
  const [editingDurationIdx, setEditingDurationIdx] = useState<number | null>(null);
  const [editDurationText, setEditDurationText] = useState("");

  const handleDurationCommit = (animIdx: number) => {
    if (!model) {
      setEditingDurationIdx(null);
      return;
    }
    const anim = (model.animations ?? [])[animIdx];
    if (!anim) {
      setEditingDurationIdx(null);
      return;
    }
    const parsed = parseFloat(editDurationText);
    if (!Number.isFinite(parsed) || parsed < 0.1) {
      alert(`Invalid duration "${editDurationText}". Duration must be a number of at least 0.1 seconds.`);
      setEditingDurationIdx(null);
      return;
    }
    const newDuration = Math.round(parsed * 1000) / 1000;
    if (newDuration === anim.duration) {
      setEditingDurationIdx(null);
      return;
    }
    // Shortening below placed keyframes would strand them past the end of
    // the timeline (playback clamps at duration, so they would never play).
    // Clamp them to the new duration after explicit confirmation; keyframe
    // order is restored by re-sorting. Interpolation stays safe: the
    // viewport guards zero-length spans (t2.time === t1.time → alpha 0).
    if (newDuration < anim.duration) {
      const stranded = anim.tracks.reduce(
        (count, t) => count + t.keyframes.filter((kf) => kf.time > newDuration + 1e-9).length,
        0
      );
      if (stranded > 0) {
        const confirmed = window.confirm(
          `Shortening "${anim.name}" to ${newDuration}s strands ${stranded} keyframe(s) placed past the new end.\n\nOK = clamp them to ${newDuration}s.\nCancel = keep the current duration.`
        );
        if (!confirmed) {
          setEditingDurationIdx(null);
          return;
        }
      }
    }
    const updatedAnims = (model.animations ?? []).map((a, i) => {
      if (i !== animIdx) return a;
      return {
        ...a,
        duration: newDuration,
        tracks: a.tracks.map((t) => ({
          ...t,
          keyframes: t.keyframes
            .map((kf) => (kf.time > newDuration ? { ...kf, time: newDuration } : kf))
            .sort((x, y) => x.time - y.time),
        })),
      };
    });
    onModelChange?.({ ...model, animations: updatedAnims });
    setEditingDurationIdx(null);
    invoke("log_event", { level: "INFO", message: `Animation "${anim.name}" duration changed: ${anim.duration}s -> ${newDuration}s` }).catch(console.error);
  };

  const handleExportNodes = async () => {
    if (!model) return;
    const exportData = {
      joints: model.joints,
      markers: model.markers,
      nav_lights: model.nav_lights,
      engine_burns: model.engine_burns,
      engine_glows: model.engine_glows,
      engine_shapes: model.engine_shapes,
      collision_meshes: model.collision_meshes,
      dockpaths: model.dockpaths,
    };
    try {
      const savedPath = await invoke<string | null>("save_text_file", {
        defaultName: `${model.name || "model"}_nodes.json`,
        filters: ["json"],
        contents: JSON.stringify(exportData, null, 2),
      });
      if (savedPath && setStatusMsg) {
        setStatusMsg("Successfully exported node tree!");
      }
    } catch (e) {
      console.error("Failed to export node JSON", e);
      if (setStatusMsg) setStatusMsg("Failed to export node tree");
    }
  };

  const handleImportNodes = () => {
    const input = document.createElement("input");
    input.type = "file";
    input.accept = ".json";
    input.onchange = (e: any) => {
      const file = e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = (ev) => {
        try {
          const data = JSON.parse(ev.target?.result as string);
          if (onModelChange && model) {
            onModelChange({
              ...model,
              joints: data.joints || model.joints,
              markers: data.markers || model.markers,
              nav_lights: data.nav_lights || model.nav_lights,
              engine_burns: data.engine_burns || model.engine_burns,
              engine_glows: data.engine_glows || model.engine_glows,
              engine_shapes: data.engine_shapes || model.engine_shapes,
              collision_meshes: data.collision_meshes || model.collision_meshes,
              dockpaths: data.dockpaths || model.dockpaths,
            });
            if (setStatusMsg) setStatusMsg("Successfully imported node tree!");
          }
        } catch (err) {
          console.error("Failed to parse node JSON", err);
          if (setStatusMsg) setStatusMsg("Failed to parse node JSON");
        }
      };
      reader.readAsText(file);
    };
    input.click();
  };

  const getShipBounds = () => {
    let minX = 0, minY = 0, minZ = 0;
    let maxX = 0, maxY = 0, maxZ = 0;
    let first = true;

    if (model && model.meshes) {
      model.meshes.forEach(mesh => {
        mesh.parts.forEach(part => {
          part.vertices.forEach(v => {
            if (v.position) {
              const x = v.position.x;
              const y = v.position.y;
              const z = v.position.z;
              if (first) {
                minX = x; minY = y; minZ = z;
                maxX = x; maxY = y; maxZ = z;
                first = false;
              } else {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (z < minZ) minZ = z;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
                if (z > maxZ) maxZ = z;
              }
            }
          });
        });
      });
    }

    const width = Math.abs(maxX - minX);
    const height = Math.abs(maxY - minY);
    const length = Math.abs(maxZ - minZ);

    return {
      min: { x: minX, y: minY, z: minZ },
      max: { x: maxX, y: maxY, z: maxZ },
      center: { x: (minX + maxX) / 2, y: (minY + maxY) / 2, z: (minZ + maxZ) / 2 },
      size: { x: width, y: height, z: length }
    };
  };
  const [collapsedJoints, setCollapsedJoints] = useState<Record<string, boolean>>({});
  const [dragOverJoint, setDragOverJoint] = useState<string | null>(null);
  const listContainerRef = useRef<HTMLDivElement>(null);
  const treeIndexRef = useRef<{ model: HODModel; index: TreeIndex } | null>(null);
  const [diagnosticsHeight, setDiagnosticsHeight] = useState(140);

  const handleDiagnosticsDragStart = (e: React.MouseEvent) => {
    e.preventDefault();
    const startY = e.clientY;
    const startHeight = diagnosticsHeight;

    const handleMouseMove = (moveEvent: MouseEvent) => {
      const deltaY = moveEvent.clientY - startY;
      const newHeight = Math.max(60, Math.min(500, startHeight - deltaY));
      setDiagnosticsHeight(newHeight);
    };

    const handleMouseUp = () => {
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
    };

    window.addEventListener("mousemove", handleMouseMove);
    window.addEventListener("mouseup", handleMouseUp);
  };

  // Add Node Form State
  const [isAddNodeOpen, setIsAddNodeOpen] = useState(false);
  const [addNodeType, setAddNodeType] = useState<"joint" | "marker" | "navlight" | "dockpath" | "collision" | "weapon_template" | "turret_template" | "engine_nozzle" | "mesh" | "repair_point_template" | "capture_point_template" | "hardpoint_template" | "salvage_point_template">("joint");
  const [newNodeName, setNewNodeName] = useState("");
  const [newNodeParent, setNewNodeParent] = useState("Root");

  // These node types generate their name in handleAddNode, so the hidden name field must not gate submission.
  const isAddNodeNameAutoGenerated = ["collision", "engine_nozzle", "repair_point_template", "capture_point_template", "salvage_point_template"].includes(addNodeType);
  const canSubmitAddNode = isAddNodeNameAutoGenerated || newNodeName.trim().length > 0;

  // NavLight option states
  const [navLightSection, setNavLightSection] = useState(0);
  const [navLightSize, setNavLightSize] = useState(1.0);
  const [navLightPhase, setNavLightPhase] = useState(0.0);
  const [navLightFreq, setNavLightFreq] = useState(1.0);
  const [navLightStyle, setNavLightStyle] = useState("Default");
  const [navLightColor, setNavLightColor] = useState("#ffffff");

  // Material Addition State
  const [isAddMatOpen, setIsAddMatOpen] = useState(false);
  const [newMatName, setNewMatName] = useState("");
  const [newMatShader, setNewMatShader] = useState("");
  const [pipelines, setPipelines] = useState<string[]>([]);

  const loadPipelines = async () => {
    try {
      const config = await invoke<{ shader_directories: string[] }>("load_shader_config");
      if (config.shader_directories.length > 0) {
        const list = await invoke<string[]>("get_shader_pipelines", { keeperPaths: config.shader_directories });
        setPipelines(list);
        if (!newMatShader && list.length > 0) {
          setNewMatShader(list[0]);
        }
      } else {
        setPipelines([]);
      }
    } catch (e) {
      console.error("Failed to load shader pipelines:", e);
    }
  };

  React.useEffect(() => {
    loadPipelines();
  }, []);

  // Weapon Grouping helpers
  const getWeaponGroupInfo = (name: string) => {
    // Check weapon assemblies
    const weaponMatch = name.match(/^(Weapon_[A-Za-z0-9_]+)_(Position|Direction|Heading|Muzzle\d*|Rest|Latitude|Pitch|Yaw|Barrel\d*)$/i);
    if (weaponMatch) {
      const baseName = weaponMatch[1];
      return { baseName, suffix: weaponMatch[2], type: 'weapon_group' };
    }
    
    // Check point groups (Capture, Repair, Salvage, Hardpoint)
    // Latch points (loadLatchPointList in .ship) are looked up by the engine
    // as `<base>Heading` / `<base>Left` / `<base>Up` with NO underscore; every
    // vanilla HOD uses that form. `_?` still groups files saved by older
    // editor builds, which wrote `<base>_Heading`. Hardpoints use `_Position`.
    const pointMatch =
      name.match(/^(CapturePoint\d+|RepairPoint\d+|SalvagePoint\d+)_?(Heading|Left|Up)$/i) ||
      name.match(/^(Hardpoint[A-Za-z0-9_]*)_(Position|Direction|Rest)$/i);
    if (pointMatch) {
      let type = "point_group";
      if (pointMatch[1].startsWith("Hardpoint")) type = "hardpoint_group";
      else if (pointMatch[1].startsWith("Capture")) type = "capture_point_group";
      else if (pointMatch[1].startsWith("Repair")) type = "repair_point_group";
      else if (pointMatch[1].startsWith("Salvage")) type = "salvage_point_group";
      return { baseName: pointMatch[1], suffix: pointMatch[2], type };
    }
    
    // Check base nodes for point groups
    const exactPointMatch = name.match(/^(CapturePoint\d+|RepairPoint\d+|SalvagePoint\d+|Hardpoint[A-Za-z0-9_]*)$/i);
    if (exactPointMatch) {
      let type = "point_group";
      if (exactPointMatch[1].startsWith("Hardpoint")) type = "hardpoint_group";
      else if (exactPointMatch[1].startsWith("Capture")) type = "capture_point_group";
      else if (exactPointMatch[1].startsWith("Repair")) type = "repair_point_group";
      else if (exactPointMatch[1].startsWith("Salvage")) type = "salvage_point_group";
      return { baseName: exactPointMatch[1], suffix: "", type };
    }

    return null;
  };

  const getUniqueAssemblyGroups = (): string[] => {
    if (!model) return [];
    const groups = new Set<string>();
    model.joints.forEach(j => {
      const info = getWeaponGroupInfo(j.name);
      if (info) {
        groups.add(info.baseName);
      }
    });
    return Array.from(groups);
  };

  // PERF TRAP: renderJointNode runs once per joint on every tree render
  // (selection, visibility, collapse). Scanning model.joints / groups inside it
  // was O(J^2 * G) and cost ~100ms per click on large ships. Everything it needs
  // is indexed here once per model object; edits always produce a new model.
  const getTreeIndex = (m: HODModel): TreeIndex => {
    const cached = treeIndexRef.current;
    if (cached && cached.model === m) return cached.index;

    const push = <T,>(map: Map<string, T[]>, key: string, value: T) => {
      const list = map.get(key);
      if (list) list.push(value); else map.set(key, [value]);
    };

    const jointsByName = new Map<string, HODModel["joints"][number]>();
    const childrenByParent = new Map<string, HODModel["joints"][number][]>();
    m.joints.forEach(j => {
      if (!jointsByName.has(j.name)) jointsByName.set(j.name, j);
      if (j.parent_name) push(childrenByParent, j.parent_name, j);
    });

    const groupInfoByName = new Map<string, ReturnType<typeof getWeaponGroupInfo>>();
    m.joints.forEach(j => groupInfoByName.set(j.name, getWeaponGroupInfo(j.name)));

    const groupJointsByBase = new Map<string, HODModel["joints"][number][]>();
    const weaponGroupsByParent = new Map<string, string[]>();
    getUniqueAssemblyGroups().forEach(baseName => {
      const groupJoints = m.joints.filter(j => isJointInWeaponBase(j.name, baseName));
      groupJointsByBase.set(baseName, groupJoints);
      const memberNames = new Set(groupJoints.map(j => j.name));
      const rootParents = new Set<string>();
      groupJoints.forEach(j => {
        if (j.parent_name && !memberNames.has(j.parent_name)) rootParents.add(j.parent_name);
      });
      rootParents.forEach(parent => push(weaponGroupsByParent, parent, baseName));
    });

    const navLightsByParent = new Map<string, NonNullable<HODModel["nav_lights"]>[number][]>();
    m.nav_lights?.forEach(n => {
      const joint = jointsByName.get(n.name);
      const key = joint ? joint.parent_name : n.name;
      if (key) push(navLightsByParent, key, n);
    });

    const markersByParent = new Map<string, HODModel["markers"][number][]>();
    m.markers.forEach(mk => push(markersByParent, mk.parent_joint, mk));
    const meshesByParent = new Map<string, HODModel["meshes"][number][]>();
    m.meshes?.forEach(mesh => push(meshesByParent, mesh.parent_name, mesh));

    const engineParents = new Set<string>();
    m.engine_burns?.forEach(b => engineParents.add(b.parent_name));
    m.engine_glows?.forEach(g => engineParents.add(g.parent_name));
    m.engine_shapes?.forEach(sh => engineParents.add(sh.parent_name));

    const index: TreeIndex = {
      jointsByName,
      childrenByParent,
      groupInfoByName,
      groupJointsByBase,
      weaponGroupsByParent,
      navLightsByParent,
      markersByParent,
      meshesByParent,
      navLightNamesLower: new Set((m.nav_lights || []).map(n => n.name.toLowerCase())),
      engineParents,
    };
    treeIndexRef.current = { model: m, index };
    return index;
  };

  const handleAddNode = () => {
    if (!model) return;
    let autoName = newNodeName.trim();
    const isAutoNumbered = ["engine_nozzle", "repair_point_template", "capture_point_template", "salvage_point_template"].includes(addNodeType);
    
    if (isAutoNumbered) {
        let maxNum = -1;
        let prefix = "";
        if (addNodeType === "engine_nozzle") prefix = "EngineNozzle";
        else if (addNodeType === "repair_point_template") prefix = "RepairPoint";
        else if (addNodeType === "capture_point_template") prefix = "CapturePoint";
        else if (addNodeType === "salvage_point_template") prefix = "SalvagePoint";

        model.joints.forEach(j => {
            const match = j.name.match(new RegExp(`^${prefix}(\\d+)$`, "i"));
            if (match) {
                maxNum = Math.max(maxNum, parseInt(match[1]));
            }
        });
        autoName = `${prefix}${maxNum + 1}`;
    } else if (addNodeType === "collision") {
        autoName = "Root";
    } else if (!autoName) {
        return; // Empty name not allowed for non-auto nodes
    }

    const name = autoName;
    const parent = newNodeParent === "Root" ? "Root" : newNodeParent;

    if (addNodeType === "collision") {
      if (model.collision_meshes.length > 0) {
        window.alert("Only one COL node is allowed per HOD. Delete the existing COL node before adding a new one.");
        return;
      }

      const newCol = {
        name: "Root",
        min_extents: { x: -5, y: -5, z: -5 },
        max_extents: { x: 5, y: 5, z: 5 },
        center: { x: 0, y: 0, z: 0 },
        radius: 5.0,
        mesh: {
          name: "CollisionMesh",
          parent_name: "Root",
          lod: 0,
          parts: []
        }
      };

      onModelChange?.({ ...model, collision_meshes: [newCol] });
      invoke("log_event", { level: "INFO", message: "Added new collision mesh: Root" }).catch(console.error);
      setIsAddNodeOpen(false);
      setNewNodeName("");
      return;
    }

    
    const checkDuplicate = (n: string) => {
      if (model.joints.some(j => j.name === n)) return true;
      if (model.meshes.some(m => `${m.name}_lod_${m.lod}` === n || m.name === n)) return true;
      if (model.nav_lights.some(nv => nv.name === n)) return true;
      if (model.markers.some(m => m.name === n)) return true;
      if (model.engine_burns?.some(b => b.name === n)) return true;
      if (model.engine_glows?.some(g => g.name === n)) return true;
      if (model.engine_shapes?.some(s => s.name === n)) return true;
      if (model.collision_meshes?.some(c => c.name === n)) return true;
      if (model.dockpaths?.some(d => d.name === n)) return true;
      return false;
    };

    let finalNodeName = name;
    if (addNodeType === "weapon_template" || addNodeType === "turret_template") finalNodeName = name.startsWith("Weapon_") || name.startsWith("weapon_") ? name : `Weapon_${name}`;
    else if (addNodeType === "repair_point_template" || addNodeType === "capture_point_template" || addNodeType === "salvage_point_template") finalNodeName = name; // already computed autoName
    else if (addNodeType === "engine_nozzle") finalNodeName = name;

    // Validate every generated template joint for name collisions before creating anything.
    if (addNodeType === "weapon_template" || addNodeType === "turret_template") {
      const suffixes = addNodeType === "turret_template"
        ? ["_Position", "_Direction", "_Latitude", "_Muzzle", "_Rest"]
        : ["_Position", "_Direction", "_Muzzle", "_Rest"];
      const duplicate = suffixes
        .map((suffix) => `${finalNodeName}${suffix}`)
        .find((jointName) => checkDuplicate(jointName));
      if (duplicate) {
        window.alert(`A node with the name "${duplicate}" already exists! Please choose a unique base name.`);
        return;
      }
    }

    if (checkDuplicate(finalNodeName)) {
      window.alert(`A node with the name "${finalNodeName}" already exists! Please choose a unique name.`);
      return;
    }
    
    let updatedModel = { ...model };

    if (addNodeType === "joint") {
      const newJoint = {
        name,
        parent_name: parent === "(None)" ? undefined : parent,
        local_transform: {
          m: [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [0, 0, 0, 1]
          ]
        }
      };
      updatedModel.joints = [...model.joints, newJoint];
      invoke("log_event", { level: "INFO", message: `Added new joint: ${name} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "marker") {
      const newMarker = {
        name,
        parent_joint: parent,
        position: { x: 0, y: 0, z: 0 },
        rotation: {
          m: [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [0, 0, 0, 1]
          ]
        }
      };
      updatedModel.markers = [...model.markers, newMarker];
      invoke("log_event", { level: "INFO", message: `Added new marker: ${name} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "navlight") {
      const r = parseInt(navLightColor.substring(1, 3), 16) / 255;
      const g = parseInt(navLightColor.substring(3, 5), 16) / 255;
      const b = parseInt(navLightColor.substring(5, 7), 16) / 255;
      const newNav = {
        name,
        section: navLightSection,
        size: navLightSize,
        phase: navLightPhase,
        frequency: navLightFreq,
        style: navLightStyle,
        color: { x: r, y: g, z: b },
        distance: 100.0,
        sprite_visible: true,
        high_end_only: false
      };
      updatedModel.nav_lights = [...model.nav_lights, newNav];

      // Also ensure joint of same name exists in hierarchy
      if (!model.joints.some(j => j.name === name)) {
        updatedModel.joints = [...model.joints, {
          name,
          parent_name: parent === "(None)" ? undefined : parent,
          position: { x: 0, y: 0, z: 0 },
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 0, y: 0, z: 0 },
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        }];
      }
      invoke("log_event", { level: "INFO", message: `Added new navlight: ${name} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "dockpath") {
      const newPath: HODDockpath = {
        name,
        parent_name: parent,
        points: [
          {
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
          }
        ],
        val1: 0,
        val2: 0,
        val3: 0,
        val4: 0,
        val5: 0,
        compatible_ships: "",
        padding1: 0,
        padding2: ""
      };
      updatedModel.dockpaths = [...model.dockpaths, newPath];
      invoke("log_event", { level: "INFO", message: `Added new dockpath: ${name} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "weapon_template" || addNodeType === "turret_template") {
      const base = finalNodeName;
      const isTurret = addNodeType === "turret_template";
      const parentName = parent === "(None)" ? undefined : parent;

      const makeJoint = (
        suffix: string,
        parentSuffix: string | null,
        offset: { x: number; y: number; z: number }
      ) => ({
        name: `${base}${suffix}`,
        parent_name: parentSuffix === null ? parentName : `${base}${parentSuffix}`,
        local_transform: {
          m: [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [offset.x, offset.y, offset.z, 1]
          ]
        },
        rotation: { x: 0, y: 0, z: 0 },
      });

      const templateJoints = isTurret
        ? [
            // Vanilla HWRM turret chain: Position -> Latitude -> Muzzle, with
            // Direction/Rest used as up and idle vectors. Barrel is optional
            // and unverified, so it is not created by default.
            makeJoint("_Position", null, { x: 0, y: 0, z: 0 }),
            makeJoint("_Direction", "_Position", { x: 0, y: 5.0, z: 0 }),
            makeJoint("_Latitude", "_Position", { x: 0, y: 0, z: 0 }),
            makeJoint("_Muzzle", "_Latitude", { x: 0, y: 0, z: 7.64 }),
            makeJoint("_Rest", "_Position", { x: 0, y: 0, z: 10.0 }),
          ]
        : [
            makeJoint("_Position", null, { x: 0, y: 0, z: 0 }),
            makeJoint("_Direction", "_Position", { x: 0, y: 5.0, z: 0 }),
            makeJoint("_Muzzle", "_Position", { x: 0, y: 0, z: 5.0 }),
            makeJoint("_Rest", "_Position", { x: 0, y: 0, z: 5.0 }),
          ];

      updatedModel.joints = [...model.joints, ...templateJoints];
      invoke("log_event", {
        level: "INFO",
        message: `Added new ${isTurret ? "Turret" : "Weapon"} Assembly: ${base} (${templateJoints.length} joints)`,
      }).catch(console.error);
    } else if (addNodeType === "engine_nozzle") {
      const baseJointName = finalNodeName;
      const newJoint = {
        name: baseJointName,
        parent_name: parent === "(None)" ? undefined : parent,
        local_transform: {
          m: [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [0, 0, 0, 1]
          ]
        }
      };
      // Don't auto-add engine_burn here, let the user add it via the inspector
      updatedModel.joints = [...model.joints, newJoint];
      invoke("log_event", { level: "INFO", message: `Added new Engine Nozzle joint ${baseJointName} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "mesh") {
      const newMesh = {
        name,
        parent_name: parent,
        lod: 0,
        parts: []
      };
      updatedModel.meshes = [...model.meshes, newMesh];
      invoke("log_event", { level: "INFO", message: `Added new empty HODMesh: ${name} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "repair_point_template") {
      const base = finalNodeName;
      const headingName = `${base}Heading`;
      const leftName = `${base}Left`;
      const upName = `${base}Up`;

      const templateJoints = [
        {
          name: base,
          parent_name: parent === "(None)" ? undefined : parent,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        },
        {
          name: headingName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 5.0, 1]
            ]
          }
        },
        {
          name: leftName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [5.0, 0, 0, 1]
            ]
          }
        },
        {
          name: upName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 5.0, 0, 1]
            ]
          }
        }
      ];
      updatedModel.joints = [...model.joints, ...templateJoints];
      invoke("log_event", { level: "INFO", message: `Added new repair point template: ${base} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "capture_point_template") {
      const base = finalNodeName;
      const headingName = `${base}Heading`;
      const leftName = `${base}Left`;
      const upName = `${base}Up`;

      const templateJoints = [
        {
          name: base,
          parent_name: parent === "(None)" ? undefined : parent,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        },
        {
          name: headingName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 5.0, 1]
            ]
          }
        },
        {
          name: leftName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [5.0, 0, 0, 1]
            ]
          }
        },
        {
          name: upName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 5.0, 0, 1]
            ]
          }
        }
      ];
      updatedModel.joints = [...model.joints, ...templateJoints];
      invoke("log_event", { level: "INFO", message: `Added new capture point template: ${base} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "hardpoint_template") {
      const base = name.startsWith("Hardpoint_") ? name : `Hardpoint_${name}`;
      const posName = `${base}_Position`;
      const dirName = `${base}_Direction`;
      const restName = `${base}_Rest`;

      const templateJoints = [
        {
          name: posName,
          parent_name: parent === "(None)" ? undefined : parent,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        },
        {
          name: dirName,
          parent_name: posName,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 5.0, 0, 1]
            ]
          }
        },
        {
          name: restName,
          parent_name: posName,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 5.0, 1]
            ]
          }
        }
      ];
      updatedModel.joints = [...model.joints, ...templateJoints];
      invoke("log_event", { level: "INFO", message: `Added new hardpoint template: ${base} parented under ${parent}` }).catch(console.error);
    } else if (addNodeType === "salvage_point_template") {
      const base = finalNodeName;
      const headingName = `${base}Heading`;
      const leftName = `${base}Left`;
      const upName = `${base}Up`;

      const templateJoints = [
        {
          name: base,
          parent_name: parent === "(None)" ? undefined : parent,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        },
        {
          name: headingName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 5.0, 1]
            ]
          }
        },
        {
          name: leftName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 5.0, 1]
            ]
          }
        },
        {
          name: upName,
          parent_name: base,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 5.0, 0, 1]
            ]
          }
        }
      ];
      updatedModel.joints = [...model.joints, ...templateJoints];
      invoke("log_event", { level: "INFO", message: `Added new salvage point template: ${base} parented under ${parent}` }).catch(console.error);
    }

    onModelChange?.(updatedModel);
    setNewNodeName("");
    setIsAddNodeOpen(false);
  };

  const handleAddMaterial = () => {
    if (!model || !newMatName.trim()) return;
    const name = newMatName.trim();
    
    // Do not pre-fill texture slots per user request
    const textureSlots = ["", "", "", ""];

    const newMaterial = {
      name,
      shader_name: newMatShader,
      texture_maps: textureSlots,
      parameters: [],
      params: []
    };

    const updatedModel = {
      ...model,
      materials: [...(model.materials || []), newMaterial]
    };

    onModelChange?.(updatedModel);
    setNewMatName("");
    setIsAddMatOpen(false);
    invoke("log_event", { level: "INFO", message: `Added new material: ${name} with shader: ${newMatShader}` }).catch(console.error);
    alert(`Material "${name}" successfully added to the library!`);
  };

  const isNodeDeletable = (name: string, type: string): boolean => {
    if (type === "joint" && name.toLowerCase() === "root") return false;
    if (type === "joint") {
      if (getWeaponGroupInfo(name) !== null) return false;
    }
    return true;
  };

  const renderDeleteButton = (_name: string, _type: string) => {
    return null;
  };

  
  const [contextMenu, setContextMenu] = useState<{ x: number, y: number, name: string, type: string } | null>(null);

  const handleAddMuzzleToGroup = (baseName: string) => {
    if (!model) return;
    const index = nextMuzzleIndex(model, baseName);
    const newMuzzleName = muzzleName(baseName, index);

    const hasLatitude = model.joints.some(j => j.name.toLowerCase() === `${baseName}_Latitude`.toLowerCase());
    const parentName = hasLatitude ? `${baseName}_Latitude` : `${baseName}_Position`;

    onModelChange?.({
      ...model,
      joints: [
        ...model.joints,
        {
          name: newMuzzleName,
          parent_name: parentName,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, hasLatitude ? 7.64 : 5.0, 1]
            ]
          },
          rotation: { x: 0, y: 0, z: 0 },
        },
      ],
    });
    invoke("log_event", { level: "INFO", message: `Added muzzle joint ${newMuzzleName} to ${baseName}` }).catch(console.error);
  };

  const handleAddSlaveToGroup = (baseName: string) => {
    if (!model) return;
    if (/Slave\d*$/i.test(baseName)) return;
    const slaveBase = nextSlaveBase(model, baseName);
    const newJoints = cloneWeaponFamilyJoints(model, baseName, slaveBase);
    if (newJoints.length === 0) return;

    onModelChange?.({ ...model, joints: [...model.joints, ...newJoints] });
    setSelectedNode({ type: "weapon_group", name: slaveBase });
    invoke("log_event", {
      level: "INFO",
      message: `Added slave barrel assembly ${slaveBase} to ${baseName}`,
    }).catch(console.error);
  };

  const handleContextMenu = (e: React.MouseEvent, name: string, type: string) => {
    if (type === "joint") {
      const wInfo = getWeaponGroupInfo(name);
      if (wInfo && name !== wInfo.baseName) {
        // It's a subnode of an assembly, prevent context menu
        e.preventDefault();
        e.stopPropagation();
        return;
      }
    }
    e.preventDefault();
    e.stopPropagation();
    setSelectedNode({ type, name });
    setContextMenu({ x: e.clientX, y: e.clientY, name, type });
  };

  const handleRenameNode = (oldName: string, type: string) => {
    if (!model) return;
    if (type === "collision") {
      alert("COL nodes are engine-defined and must remain named Root.");
      return;
    }
    if (oldName === "Root") {
      alert("The 'Root' node cannot be renamed as it is required by the engine.");
      return;
    }
    if (oldName.toLowerCase().startsWith("enginenozzle") || oldName.toLowerCase().startsWith("repairpoint") || oldName.toLowerCase().startsWith("capturepoint") || oldName.toLowerCase().startsWith("salvagepoint")) {
      alert("Auto-generated point and nozzle nodes cannot be manually renamed.");
      return;
    }
    if (type === "engine_burn" || type === "engine_glow" || type === "engine_shape") {
      let parentName = "";
      if (type === "engine_burn") parentName = model.engine_burns?.find(b => b.name === oldName)?.parent_name || "";
      if (type === "engine_glow") parentName = model.engine_glows?.find(g => g.name === oldName)?.parent_name || "";
      if (type === "engine_shape") parentName = model.engine_shapes?.find(s => s.name === oldName)?.parent_name || "";
      if (parentName.toLowerCase().startsWith("enginenozzle")) {
        alert("EngineNozzle subnodes cannot be renamed.");
        return;
      }
    }

    let cleanOldName = oldName;
    let prefix = "";
    let suffix = "";
    
    if (type === "navlight" && oldName.startsWith("NAVL[")) {
      prefix = "NAVL["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    } else if (type === "marker" && oldName.startsWith("MARK[")) {
      prefix = "MARK["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    } else if (type === "engine_burn" && oldName.startsWith("BURN[")) {
      prefix = "BURN["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    } else if (type === "mesh" && oldName.startsWith("MULT[")) {
      prefix = "MULT["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    } else if (type === "collision" && oldName.startsWith("COL[")) {
      prefix = "COL["; suffix = "]"; cleanOldName = oldName.substring(4, oldName.length - 1);
    } else if (type === "engine_glow" && oldName.startsWith("GLOW[")) {
      prefix = "GLOW["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    } else if (type === "engine_shape" && oldName.startsWith("SHAP[")) {
      prefix = "SHAP["; suffix = "]"; cleanOldName = oldName.substring(5, oldName.length - 1);
    }

    const input = window.prompt(`Rename ${type}:`, cleanOldName);
    if (!input || input.trim() === "" || input === cleanOldName) return;
    
    const newName = `${prefix}${input.trim()}${suffix}`;
    
    const checkDuplicate = (n: string) => {
      if (model.joints.some(j => j.name === n)) return true;
      if (model.meshes.some(m => `${m.name}_lod_${m.lod}` === n || m.name === n)) return true;
      if (model.nav_lights.some(nv => nv.name === n)) return true;
      if (model.markers.some(m => m.name === n)) return true;
      if (model.engine_burns?.some(b => b.name === n)) return true;
      if (model.engine_glows?.some(g => g.name === n)) return true;
      if (model.engine_shapes?.some(s => s.name === n)) return true;
      if (model.collision_meshes?.some(c => c.name === n)) return true;
      if (model.dockpaths?.some(d => d.name === n)) return true;
      return false;
    };
    
    let updatedModel = { ...model };

    if (type.endsWith("_group")) {
      const familyBases = [oldName, ...getSlaveBases(model, oldName)];
      const familyJoints = model.joints.filter(j => isJointInWeaponFamily(j.name, familyBases));

      for (const j of familyJoints) {
         const renamed = renameWeaponFamilyName(j.name, familyBases, oldName, newName);
         if (renamed !== j.name && checkDuplicate(renamed)) {
            window.alert(`Cannot rename weapon assembly: Subnode "${renamed}" would conflict with an existing node!`);
            return;
         }
      }

      updatedModel.joints = model.joints.map(j => ({
        ...j,
        name: isJointInWeaponFamily(j.name, familyBases)
          ? renameWeaponFamilyName(j.name, familyBases, oldName, newName)
          : j.name,
        parent_name: j.parent_name
          ? renameWeaponFamilyName(j.parent_name, familyBases, oldName, newName)
          : j.parent_name,
      }));
      updatedModel.meshes = model.meshes.map(m => ({
         ...m,
         parent_name: renameWeaponFamilyName(m.parent_name, familyBases, oldName, newName),
      }));
      updatedModel.markers = model.markers.map(m => ({
         ...m,
         parent_joint: renameWeaponFamilyName(m.parent_joint, familyBases, oldName, newName),
      }));
      
      onModelChange?.(updatedModel);
      if (selectedNode && selectedNode.name === oldName && selectedNode.type === type) {
        setSelectedNode({ type, name: newName });
      }
      invoke("log_event", { level: "INFO", message: `Renamed weapon_group assembly from ${oldName} to ${newName}` }).catch(console.error);
      return;
    }

    if (checkDuplicate(newName)) {
      window.alert(`A node with the name "${newName}" already exists!`);
      return;
    }

    if (type === "joint") {
      updatedModel.joints = model.joints.map(j => {
        if (j.name === oldName) return { ...j, name: newName };
        if (j.parent_name === oldName) return { ...j, parent_name: newName };
        return j;
      });
      updatedModel.markers = model.markers.map(m => {
        if (m.parent_joint === oldName) return { ...m, parent_joint: newName };
        return m;
      });
      updatedModel.meshes = model.meshes.map(m => {
        if (m.parent_name === oldName) return { ...m, parent_name: newName };
        return m;
      });
      updatedModel.dockpaths = model.dockpaths.map(dp => {
        if (dp.parent_name === oldName) return { ...dp, parent_name: newName };
        return dp;
      });
      updatedModel.engine_burns = model.engine_burns.map(eb => {
        if (eb.parent_name === oldName) return { ...eb, parent_name: newName };
        return eb;
      });
      updatedModel.engine_glows = model.engine_glows.map(eg => {
        if (eg.parent_name === oldName) return { ...eg, parent_name: newName };
        return eg;
      });
      updatedModel.engine_shapes = model.engine_shapes.map(es => {
        if (es.parent_name === oldName) return { ...es, parent_name: newName };
        return es;
      });
      updatedModel.collision_meshes = model.collision_meshes.map(c => {
        if (c.mesh && c.mesh.parent_name === oldName) {
           return { ...c, mesh: { ...c.mesh, parent_name: newName } };
        }
        return c;
      });
    } else if (type === "marker") {
      updatedModel.markers = model.markers.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "navlight") {
      updatedModel.nav_lights = model.nav_lights.map(m => m.name === oldName ? { ...m, name: newName } : m);
      updatedModel.joints = model.joints.map(j => j.name === oldName ? { ...j, name: newName } : j);
    } else if (type === "dockpath") {
      updatedModel.dockpaths = model.dockpaths.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "collision") {
      updatedModel.collision_meshes = model.collision_meshes.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "engine_burn") {
      updatedModel.engine_burns = model.engine_burns.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "engine_glow") {
      updatedModel.engine_glows = model.engine_glows.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "engine_shape") {
      updatedModel.engine_shapes = model.engine_shapes.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "mesh") {
      updatedModel.meshes = model.meshes.map(m => m.name === oldName ? { ...m, name: newName } : m);
    } else if (type === "material") {
      updatedModel.materials = model.materials.map(m => m.name === oldName ? { ...m, name: newName } : m);
    }
    
    onModelChange?.(updatedModel);
    if (selectedNode && selectedNode.name === oldName && selectedNode.type === type) {
      setSelectedNode({ type, name: newName });
    }
    invoke("log_event", { level: "INFO", message: `Renamed ${type} from ${oldName} to ${newName}` }).catch(console.error);
  };

const handleDeleteNode = (name: string, type: string) => {
    if (!model || !isNodeDeletable(name, type)) return;
    
    let updatedModel = { ...model };

    if (type === "joint" || type === "engine_nozzle") {
      const jointToDelete = model.joints.find(j => j.name === name);
      const parentJointName = jointToDelete ? jointToDelete.parent_name : "Root";

      updatedModel.joints = model.joints
        .filter(j => j.name !== name)
        .map(j => {
          if (j.parent_name === name) {
            return { ...j, parent_name: parentJointName };
          }
          return j;
        });

      updatedModel.meshes = model.meshes.map(m => {
        if (m.parent_name === name) return { ...m, parent_name: parentJointName || "Root" };
        return m;
      });
      updatedModel.markers = model.markers.map(mrk => {
        if (mrk.parent_joint === name) return { ...mrk, parent_joint: parentJointName || "Root" };
        return mrk;
      });
      updatedModel.engine_burns = model.engine_burns.filter(b => b.parent_name !== name);
      updatedModel.engine_glows = model.engine_glows.filter(g => g.parent_name !== name);
      updatedModel.engine_shapes = model.engine_shapes.filter(s => s.parent_name !== name);
      updatedModel.dockpaths = model.dockpaths.filter(dp => dp.parent_name !== name);
      updatedModel.nav_lights = model.nav_lights.filter(nav => nav.name !== name);
      const deletedKind = type === "engine_nozzle" ? "engine nozzle" : "joint bone";
      invoke("log_event", { level: "INFO", message: `Deleted ${deletedKind}: ${name}. Children re-parented to: ${parentJointName}` }).catch(console.error);
    } else if (type.endsWith("_group")) {
      const familyBases = [name, ...getSlaveBases(model, name)];
      const groupJoints = model.joints.filter(j => isJointInWeaponFamily(j.name, familyBases));
      const jointNames = groupJoints.map(j => j.name);

      updatedModel.joints = model.joints.filter(j => !jointNames.includes(j.name));
      updatedModel.meshes = model.meshes.map(m => {
        if (jointNames.includes(m.parent_name)) return { ...m, parent_name: "Root" };
        return m;
      });
      updatedModel.markers = model.markers.map(mrk => {
        if (jointNames.includes(mrk.parent_joint)) return { ...mrk, parent_joint: "Root" };
        return mrk;
      });
      updatedModel.engine_burns = model.engine_burns.filter(b => !jointNames.includes(b.parent_name));
      updatedModel.engine_glows = model.engine_glows.filter(g => !jointNames.includes(g.parent_name));
      updatedModel.engine_shapes = model.engine_shapes.filter(s => !jointNames.includes(s.parent_name));
      updatedModel.dockpaths = model.dockpaths.filter(dp => !jointNames.includes(dp.parent_name));
      updatedModel.nav_lights = model.nav_lights.filter(nav => !jointNames.includes(nav.name));
      invoke("log_event", { level: "INFO", message: `Deleted weapon group: ${name} (entire joint family removed).` }).catch(console.error);
    } else if (type === "marker") {
      updatedModel.markers = model.markers.filter(mrk => mrk.name !== name);
      invoke("log_event", { level: "INFO", message: `Deleted marker: ${name}` }).catch(console.error);
    } else if (type === "mesh") {
      const getMeshBaseName = (meshName: string) => meshName.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
      const deletedMeshes = model.meshes.filter((m) => {
        const lodKey = `${m.name}_lod_${m.lod}`;
        return getMeshBaseName(m.name) === name || m.name === name || lodKey === name;
      });
      updatedModel.meshes = model.meshes.filter((m) => !deletedMeshes.includes(m));
      setVisibleMeshes((prev) => {
        const updated = { ...prev };
        delete updated[name];
        deletedMeshes.forEach((m) => {
          delete updated[`${m.name}_lod_${m.lod}`];
        });
        return updated;
      });
      invoke("log_event", { level: "INFO", message: `Deleted mesh node: ${name} (${deletedMeshes.length} LOD${deletedMeshes.length === 1 ? "" : "s"} removed).` }).catch(console.error);
    } else if (type === "navlight") {
      updatedModel.nav_lights = model.nav_lights.filter(nav => nav.name !== name);
      updatedModel.joints = model.joints.filter(j => j.name !== name);
      invoke("log_event", { level: "INFO", message: `Deleted navlight and associated position joint: ${name}` }).catch(console.error);
    } else if (type === "dockpath") {
      updatedModel.dockpaths = model.dockpaths.filter(dp => dp.name !== name);
      invoke("log_event", { level: "INFO", message: `Deleted dockpath: ${name}` }).catch(console.error);
    } else if (type === "collision") {
      updatedModel.collision_meshes = model.collision_meshes.filter(col => col.name !== name);
      invoke("log_event", { level: "INFO", message: `Deleted collision hull: ${name}` }).catch(console.error);
    } else if (type === "engine_burn") {
      updatedModel.engine_burns = model.engine_burns.filter(b => b.name !== name);
      invoke("log_event", { level: "INFO", message: `Deleted engine burn plume: ${name}` }).catch(console.error);
    } else if (type === "material") {
      const matIndex = model.materials.findIndex(m => m.name === name);
      updatedModel.materials = model.materials.filter(m => m.name !== name);
      if (matIndex !== -1) {
        updatedModel.meshes = model.meshes.map((mesh) => {
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
      invoke("log_event", { level: "INFO", message: `Deleted material: ${name}` }).catch(console.error);
    }

    onModelChange?.(updatedModel);
    setSelectedNode(null);
  };

  const handleDragOverContainer = (e: React.DragEvent<HTMLDivElement>) => {
    if (!listContainerRef.current) return;
    const container = listContainerRef.current;
    const rect = container.getBoundingClientRect();
    const mouseY = e.clientY;
    
    const edgeSize = 65; // Larger scroll trigger zone (65px)
    const topTrigger = rect.top + edgeSize;
    const bottomTrigger = rect.bottom - edgeSize;
    
    if (mouseY < topTrigger) {
      const intensity = (topTrigger - mouseY) / edgeSize;
      container.scrollTop -= Math.max(2, intensity * 28); // Blazing fast scroll up (up to 28px per step!)
    } else if (mouseY > bottomTrigger) {
      const intensity = (mouseY - bottomTrigger) / edgeSize;
      container.scrollTop += Math.max(2, intensity * 28); // Blazing fast scroll down (up to 28px per step!)
    }
  };

  if (!model) {
    return (
      <div className="panel-content" style={{ display: "flex", justifyContent: "center", alignItems: "center", color: "var(--text-muted)" }}>
        No model loaded
      </div>
    );
  }

  const toggleCollapse = (name: string, e: React.MouseEvent) => {
    e.stopPropagation();
    setCollapsedJoints((prev) => ({
      ...prev,
      [name]: !prev[name],
    }));
  };

  const getDescendantKeys = (jointName: string): string[] => {
    const keys: string[] = [];
    
    // Find direct child joints of this joint
    const childJoints = model.joints.filter((j) => j.parent_name === jointName);
    
    // Find direct meshes, markers, nav_lights, engine_burns, glows, shapes, collisions, dockpaths
    const meshes = model.meshes?.filter((m) => m.parent_name === jointName) || [];
    const markers = model.markers?.filter((m) => m.parent_joint === jointName) || [];
    const navLights = model.nav_lights?.filter((n) => {
      const joint = model.joints.find(j => j.name === n.name);
      if (joint) return joint.parent_name === jointName;
      return n.name === jointName;
    }) || [];
    const engineBurns = model.engine_burns?.filter((b) => b.parent_name === jointName) || [];
    const engineGlows = model.engine_glows?.filter((g) => g.parent_name === jointName) || [];
    const engineShapes = model.engine_shapes?.filter((s) => s.parent_name === jointName) || [];
    const collisions = model.collision_meshes?.filter((c) => c.mesh?.parent_name === jointName || c.name === jointName) || [];
    const dockpaths = model.dockpaths?.filter((p) => p.parent_name === jointName) || [];

    // Add keys for direct items
    meshes.forEach((mesh) => keys.push(`${mesh.name}_lod_${mesh.lod}`));
    markers.forEach((marker) => keys.push(`marker:${marker.name}`));
    navLights.forEach((nav) => keys.push(`navlight:${nav.name}`));
    engineBurns.forEach((burn) => keys.push(`engine_burn:${burn.name}`));
    engineGlows.forEach((glow) => keys.push(`engine_glow:${glow.name}`));
    engineShapes.forEach((shape) => keys.push(`engine_shape:${shape.name}`));
    collisions.forEach((col) => keys.push(`collision:${col.name}`));
    dockpaths.forEach((path) => keys.push(`dockpath:${path.name}`));

    // Recurse for child joints
    childJoints.forEach((child) => {
      keys.push(`joint:${child.name}`);
      keys.push(...getDescendantKeys(child.name));
    });

    return keys;
  };

  const toggleNodeVisibility = (nodeKey: string) => {
    setVisibleMeshes((prev) => {
      let isCurrentlyVisible = prev[nodeKey] !== false;
      if (!nodeKey.includes(":") && model?.meshes) {
        const hasMeshBase = model.meshes.some((m) => m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "") === nodeKey);
        if (hasMeshBase) {
          isCurrentlyVisible = model.meshes.some((m) => {
            const mBase = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
            return mBase === nodeKey && prev[`${m.name}_lod_${m.lod}`] !== false;
          });
        }
      }
      if (nodeKey.startsWith("engine_glow:") && model?.engine_glows) {
        const glowBaseName = nodeKey.substring("engine_glow:".length);
        const hasGlowBase = model.engine_glows.some((g) => g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "") === glowBaseName);
        if (hasGlowBase) {
          isCurrentlyVisible = model.engine_glows.some((g) => {
            const gBase = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
            return gBase === glowBaseName && prev[`engine_glow:${g.name}`] !== false;
          });
        }
      }
      const nextVisibility = !isCurrentlyVisible;
      
      const updated = { ...prev, [nodeKey]: nextVisibility };
      
      // If toggling a mesh base name, toggle only LOD0 and force other LODs to false
      if (!nodeKey.includes(":") && model?.meshes) {
        const baseName = nodeKey;
        model.meshes.forEach((m) => {
          const mBase = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          if (mBase === baseName) {
            updated[`${m.name}_lod_${m.lod}`] = m.lod === 0 ? nextVisibility : false;
          }
        });
      }

      // If toggling an engine glow base name, toggle only LOD0 and force other LODs to false
      if (nodeKey.startsWith("engine_glow:") && model?.engine_glows) {
        const glowBaseName = nodeKey.substring("engine_glow:".length);
        model.engine_glows.forEach((g) => {
          const gBase = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          if (gBase === glowBaseName) {
            updated[`engine_glow:${g.name}`] = g.lod === 0 ? nextVisibility : false;
          }
        });
      }
      
      // If we toggle a Joint node, propagate that same visibility recursively to all descendants!
      if (nodeKey.startsWith("joint:")) {
        const jointName = nodeKey.substring("joint:".length);
        if (jointName.toLowerCase() !== "root") {
          const descendants = getDescendantKeys(jointName);
          descendants.forEach((key) => {
            updated[key] = nextVisibility;
          });
        }
      }

      // Propagate visibility for weapon groups recursively to all component joints and their attachments!
      if (nodeKey.startsWith("weapon_group:")) {
        const baseName = nodeKey.substring("weapon_group:".length);
        const familyBases = [baseName, ...getSlaveBases(model, baseName)];
        const groupJoints = model.joints.filter(j => isJointInWeaponFamily(j.name, familyBases));
        groupJoints.forEach(wj => {
          updated[`joint:${wj.name}`] = nextVisibility;
          const descendants = getDescendantKeys(wj.name);
          descendants.forEach((key) => {
            updated[key] = nextVisibility;
          });
        });
      }
      
      const normalizeMeshLods = () => {
        if (!model) return;
        const groupedMeshes = new Map<string, HODModel["meshes"]>();
        model.meshes.forEach((m) => {
          const baseName = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          if (!groupedMeshes.has(baseName)) groupedMeshes.set(baseName, []);
          groupedMeshes.get(baseName)!.push(m);
        });
        groupedMeshes.forEach((meshes) => {
          const visibleLods = [...meshes]
            .sort((a, b) => a.lod - b.lod)
            .filter((m) => updated[`${m.name}_lod_${m.lod}`] !== false);
          if (visibleLods.length > 1) {
            const keepKey = `${visibleLods[0].name}_lod_${visibleLods[0].lod}`;
            meshes.forEach((m) => {
              const lodKey = `${m.name}_lod_${m.lod}`;
              updated[lodKey] = lodKey === keepKey;
            });
          }
        });
      };

      const normalizeGlowLods = () => {
        if (!model) return;
        const groupedGlows = new Map<string, HODModel["engine_glows"]>();
        model.engine_glows.forEach((g) => {
          const baseName = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          if (!groupedGlows.has(baseName)) groupedGlows.set(baseName, []);
          groupedGlows.get(baseName)!.push(g);
        });
        groupedGlows.forEach((glows) => {
          const visibleLods = [...glows]
            .sort((a, b) => a.lod - b.lod)
            .filter((g) => updated[`engine_glow:${g.name}`] !== false);
          if (visibleLods.length > 1) {
            const keepKey = `engine_glow:${visibleLods[0].name}`;
            glows.forEach((g) => {
              const lodKey = `engine_glow:${g.name}`;
              updated[lodKey] = lodKey === keepKey;
            });
          }
        });
      };

      normalizeMeshLods();
      normalizeGlowLods();

      return updated;
    });
  };

  const renderEyeToggle = (nodeKey: string) => {
    // For mesh base names, check if any LOD is visible
    let isVisible = visibleMeshes[nodeKey] !== false;
    if (!nodeKey.includes(":") && model?.meshes) {
      const baseName = nodeKey;
      const hasVisibleLod = model.meshes.some((m) => {
        const mBase = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
        return mBase === baseName && visibleMeshes[`${m.name}_lod_${m.lod}`] !== false;
      });
      isVisible = hasVisibleLod;
    }
    
    // For engine glow base names, check if any LOD is visible
    if (nodeKey.startsWith("engine_glow:") && model?.engine_glows) {
      const glowBaseName = nodeKey.substring("engine_glow:".length);
      const hasVisibleLod = model.engine_glows.some((g) => {
        const gBase = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
        return gBase === glowBaseName && visibleMeshes[`engine_glow:${g.name}`] !== false;
      });
      if (model.engine_glows.some(g => g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "") === glowBaseName)) {
         isVisible = hasVisibleLod;
      }
    }
    return (
      <span
        onClick={(e) => {
          e.stopPropagation();
          toggleNodeVisibility(nodeKey);
        }}
        style={{
          padding: "2px 4px",
          cursor: "pointer",
          color: isVisible ? "var(--accent-cyan)" : "var(--text-muted)",
          display: "inline-flex",
          alignItems: "center"
        }}
        title={isVisible ? "Hide element" : "Show element"}
      >
        {isVisible ? <Eye size={13} /> : <EyeOff size={13} />}
      </span>
    );
  };

  const handleDragStart = (e: React.DragEvent, name: string, type: string) => {
    e.dataTransfer.setData("application/hod-node-name", name);
    e.dataTransfer.setData("application/hod-node-type", type);
    e.dataTransfer.effectAllowed = "move";
  };

  const handleDrop = (e: React.DragEvent, targetJointName: string) => {
    e.preventDefault();
    const draggedName = e.dataTransfer.getData("application/hod-node-name");
    const draggedType = e.dataTransfer.getData("application/hod-node-type");
    
    if (draggedName && draggedType && targetJointName !== draggedName) {
      onReParentNode?.(draggedName, draggedType, targetJointName);
    }
  };

  const handleExportMaterials = async () => {
    if (!model) return;
    try {
      setIsLoading?.(true);
      setStatusMsg?.("Exporting Materials...");
      const matsToExport = model.materials || [];
      const jsonStr = JSON.stringify(matsToExport, null, 2);
      const defaultName = `${model.name}_materials.json`;
      const savedPath = await invoke<string | null>("save_text_file", {
        defaultName,
        filters: ["json"],
        contents: jsonStr
      });
      if (savedPath) {
        invoke("log_event", { level: "INFO", message: `Material Library exported successfully to: ${savedPath}` }).catch(console.error);
        
        // Extract the parent directory path supporting both Unix / and Windows \ separators
        const lastSlash = Math.max(savedPath.lastIndexOf("/"), savedPath.lastIndexOf("\\"));
        const folderPath = lastSlash !== -1 ? savedPath.substring(0, lastSlash) : ".";
        
        // Export separate TGA files in the same directory!
        if (model.textures && model.textures.length > 0) {
          setStatusMsg?.("Exporting TGA textures...");
          await invoke("export_textures_tga", {
            folderPath,
            textures: model.textures
          });
          invoke("log_event", { level: "INFO", message: `Exported ${model.textures.length} textures as separate TGA files to ${folderPath}` }).catch(console.error);
        }
      }
    } catch (e: any) {
      console.error(e);
      invoke("log_event", { level: "ERROR", message: `Failed to export materials and textures: ${e.toString()}` }).catch(console.error);
    } finally {
      setIsLoading?.(false);
    }
  };

  const handleImportMaterials = async () => {
    if (!model) return;
    try {
      const jsonContent = await invoke<string | null>("load_text_file", {
        filters: ["json"]
      });
      if (jsonContent) {
        setIsLoading?.(true);
        setStatusMsg?.("Importing Materials...");
        
        const parsedMaterials = JSON.parse(jsonContent);
        if (Array.isArray(parsedMaterials)) {
          onModelChange?.({
            ...model,
            materials: parsedMaterials
          });
          invoke("log_event", { level: "INFO", message: `Successfully imported ${parsedMaterials.length} materials from JSON file.` }).catch(console.error);
        } else {
          invoke("log_event", { level: "ERROR", message: "Imported JSON is not a valid materials array." }).catch(console.error);
          alert("The selected file does not contain a valid materials array.");
        }
      }
    } catch (e: any) {
      console.error(e);
      invoke("log_event", { level: "ERROR", message: `Failed to import materials: ${e.toString()}` }).catch(console.error);
    } finally {
      setIsLoading?.(false);
    }
  };

  const matchesSearchRecursive = (name: string): boolean => {
    if (name.toLowerCase().includes(searchTerm.toLowerCase())) return true;
    const descendants = getDescendantKeys(name);
    return descendants.some(key => {
      const cleanKey = key.includes(":") ? key.split(":")[1] : key;
      return cleanKey.toLowerCase().includes(searchTerm.toLowerCase());
    });
  };

  const getWarnings = () => {
    const warnings: { type: "warning" | "info"; message: string }[] = [];
    if (!model) return warnings;

    const assemblyRequirements: Record<string, {
      label: string;
      required: { key: string; suffix: string; allowPrefix?: boolean }[];
    }> = {
      weapon_group: {
        label: "Weapon group",
        required: [
          { key: "Position", suffix: "_Position" },
          { key: "Direction", suffix: "_Direction" },
          { key: "Muzzle", suffix: "_Muzzle" },
          { key: "Rest", suffix: "_Rest" },
        ],
      },
      hardpoint_group: {
        label: "Hardpoint group",
        required: [
          { key: "Position", suffix: "_Position" },
          { key: "Direction", suffix: "_Direction" },
          { key: "Rest", suffix: "_Rest" },
        ],
      },
      capture_point_group: {
        label: "Capture point group",
        required: [
          { key: "Base", suffix: "" },
          { key: "Heading", suffix: "Heading" },
          { key: "Left", suffix: "Left" },
          { key: "Up", suffix: "Up" },
        ],
      },
      repair_point_group: {
        label: "Repair point group",
        required: [
          { key: "Base", suffix: "" },
          { key: "Heading", suffix: "Heading" },
          { key: "Left", suffix: "Left" },
          { key: "Up", suffix: "Up" },
        ],
      },
      salvage_point_group: {
        label: "Salvage point group",
        required: [
          { key: "Base", suffix: "" },
          { key: "Heading", suffix: "Heading" },
          { key: "Left", suffix: "Left" },
          { key: "Up", suffix: "Up" },
        ],
      },
    };

    const uniqueGroups = getUniqueAssemblyGroups();
    uniqueGroups.forEach(baseName => {
      const groupInfo =
        getWeaponGroupInfo(baseName) ||
        getWeaponGroupInfo(`${baseName}_Position`) ||
        getWeaponGroupInfo(`${baseName}_Direction`) ||
        getWeaponGroupInfo(`${baseName}_Heading`);
      if (!groupInfo) return;

      const requirement = assemblyRequirements[groupInfo.type as keyof typeof assemblyRequirements];
      if (!requirement) return;

      const missing: string[] = [];
      requirement.required.forEach(({ key, suffix, allowPrefix }) => {
        const hasJoint = model.joints.some(j => {
          const nameLower = j.name.toLowerCase();
          const targetLower = `${baseName}${suffix}`.toLowerCase();
          if (key === "Muzzle" && groupInfo.type === "weapon_group") {
            return muzzleIndex(j.name, baseName) >= 0;
          }
          if (allowPrefix) {
            return nameLower.startsWith(targetLower);
          }
          return nameLower === targetLower;
        });
        if (!hasJoint) {
          missing.push(key);
        }
      });
      if (missing.length > 0) {
        warnings.push({
          type: "warning",
          message: `${requirement.label} "${baseName}" is missing required joints: ${missing.join(", ")}.`
        });
      }

      if (groupInfo.type === "weapon_group") {
        const hasLatitude = model.joints.some(j => j.name.toLowerCase() === `${baseName}_Latitude`.toLowerCase());
        const hasMuzzle = model.joints.some(j => j.name.toLowerCase().startsWith(`${baseName}_Muzzle`.toLowerCase()));
        
        if (hasLatitude && !hasMuzzle) {
          warnings.push({
            type: "warning",
            message: `Weapon group "${baseName}" has a Latitude node, so it must also have a Muzzle node.`
          });
        }

        const muzzleJoints = model.joints.filter(j =>
          j.name.toLowerCase().startsWith(`${baseName}_Muzzle`.toLowerCase())
        );
        const invalidMuzzles = muzzleJoints.filter(j =>
          !/^_Muzzle\d*$/i.test(j.name.substring(baseName.length))
        );
        if (invalidMuzzles.length > 0) {
          warnings.push({
            type: "warning",
            message: `Weapon group "${baseName}" has muzzle joints with non-standard names (${invalidMuzzles.map(j => j.name).join(", ")}). Use ${baseName}_Muzzle or ${baseName}_MuzzleN; authored numbering may be non-contiguous.`
          });
        }

        const expectedParent = hasLatitude ? `${baseName}_Latitude` : `${baseName}_Position`;
        const misParented = muzzleJoints.filter(j => {
          const parent = (j.parent_name || "").toLowerCase();
          // Root is a legitimate direct-to-hull attachment; vanilla
          // vgr_commandcorvette uses it for a fixed weapon muzzle.
          return parent !== expectedParent.toLowerCase() && parent !== "root";
        });
        if (misParented.length > 0) {
          warnings.push({
            type: "warning",
            message: `Weapon group "${baseName}" has muzzle joints not parented to ${expectedParent} (or Root): ${misParented.map(j => j.name).join(", ")}.`
          });
        }

        const barrelJoints = model.joints.filter(j => j.name.toLowerCase() === `${baseName}_Barrel`.toLowerCase());
        if (barrelJoints.length > 0) {
          warnings.push({
            type: "info",
            message: `Weapon group "${baseName}" contains a Barrel joint. Barrel behavior is unverified in HWRM; it is optional and not required by the standard turret chain.`
          });
        }

        const slaveBases = getSlaveBases(model, baseName);
        for (const slaveBase of slaveBases) {
          const slaveJoints = model.joints.filter(j => isJointInWeaponFamily(j.name, [slaveBase]));
          const missing = ["_Position", "_Muzzle"].filter(suffix => {
            const target = `${slaveBase}${suffix}`.toLowerCase();
            return !slaveJoints.some(j => j.name.toLowerCase() === target);
          });
          if (missing.length > 0) {
            warnings.push({
              type: "warning",
              message: `Slave barrel assembly "${slaveBase}" is missing required joints: ${missing.join(", ")}.`
            });
          }
        }
      }
    });

    if (model.engine_burns && model.engine_burns.length > 9) {
      warnings.push({
        type: "warning",
        message: `Engine burn limit exceeded. Having more than 9 may cause instability.`
      });
    }

    if (!model.collision_meshes || model.collision_meshes.length === 0) {
      warnings.push({
        type: "info",
        message: "No collision mesh defined. The model will not have physical boundaries."
      });
    }

    if (!model.nav_lights || model.nav_lights.length === 0) {
      warnings.push({
        type: "info",
        message: "No navigation lights defined. The model will not have pulsing light sources."
      });
    }

    if (model.meshes) {
      model.meshes.forEach((mesh) => {
        let totalIndices = 0;
        if (mesh.parts) {
          totalIndices = mesh.parts.reduce((sum, part) => sum + (part.indices ? part.indices.length : 0), 0);
        }
        const totalTriangles = Math.floor(totalIndices / 3);
        if (totalTriangles > 21845) {
          warnings.push({
            type: "warning",
            message: `Mesh "${mesh.name}" exceeds the limit of 21845 triangles (${totalTriangles} triangles, ${totalIndices} indices). You must split this mesh to reduce triangles, else the game will render it glitching or crash.`
          });
        }
      });
    }

    return warnings;
  };

  // Build recursive joint hierarchy tree representation
  const renderJointNode = (jointName: string, depth: number, visited = new Set<string>()): React.ReactNode => {
    if (visited.has(jointName)) {
      return null;
    }
    const treeIndex = getTreeIndex(model);
    const joint = treeIndex.jointsByName.get(jointName);
    if (!joint) return null;

    const nextVisited = new Set(visited);
    nextVisited.add(jointName);

    const isSelected = selectedNode?.type === "joint" && selectedNode.name === jointName;
    const isCollapsed = !!collapsedJoints[jointName];
    
    const children = treeIndex.childrenByParent.get(jointName) || [];
    const standardChildJoints = children.filter(c => {
      const isNavLight = treeIndex.navLightNamesLower.has(c.name.toLowerCase());
      if (isNavLight) {
        if (!treeIndex.engineParents.has(c.name)) return false;
      }

      const weaponInfo = treeIndex.groupInfoByName.get(c.name) ?? null;
      if (weaponInfo) {
        const parentJoint = c.parent_name ? treeIndex.jointsByName.get(c.parent_name) : undefined;
        const parentWeaponInfo = parentJoint ? treeIndex.groupInfoByName.get(parentJoint.name) ?? null : null;
        if (!parentWeaponInfo || parentWeaponInfo.baseName.toLowerCase() !== weaponInfo.baseName.toLowerCase()) {
          return false; // Filter out root weapon joint
        }
      }
      return true;
    });
    
    const childWeaponGroups = treeIndex.weaponGroupsByParent.get(jointName) || [];

    const markers = treeIndex.markersByParent.get(jointName) || [];
    const meshes = treeIndex.meshesByParent.get(jointName) || [];

    // Filter and list new elements matching parent/association
    const navLights = treeIndex.navLightsByParent.get(jointName) || [];
    const engineBurns = model.engine_burns?.filter((b) => b.parent_name === jointName) || [];
    const engineGlows = model.engine_glows?.filter((g) => g.parent_name === jointName) || [];
    const engineShapes = model.engine_shapes?.filter((s) => s.parent_name === jointName) || [];
    const collisionMeshes = model.collision_meshes?.filter((c) => c.mesh?.parent_name === jointName || c.name === jointName) || [];
    const dockpaths = model.dockpaths?.filter((p) => p.parent_name === jointName) || [];

    const hasChildren =
      standardChildJoints.length > 0 ||
      childWeaponGroups.length > 0 ||
      markers.length > 0 ||
      meshes.length > 0 ||
      navLights.length > 0 ||
      engineBurns.length > 0 ||
      engineGlows.length > 0 ||
      engineShapes.length > 0 ||
      collisionMeshes.length > 0 ||
      dockpaths.length > 0;

    if (searchTerm && !matchesSearchRecursive(jointName)) return null;

    const isEngineNozzle = jointName.toLowerCase().startsWith("enginenozzle") || engineBurns.length > 0 || engineGlows.length > 0 || engineShapes.length > 0;
    const nodeType = isEngineNozzle ? "engine_nozzle" : "joint";

    return (
      <div key={jointName} style={{ marginLeft: depth > 0 ? "12px" : "0px" }}>
        <div
          className={`list-item ${isSelected ? "active" : ""}`}
          onClick={() => setSelectedNode({ type: nodeType, name: jointName })}
          onContextMenu={(e) => handleContextMenu(e, jointName, nodeType)}
          draggable={jointName !== "Root" && !(getWeaponGroupInfo(jointName) && jointName !== getWeaponGroupInfo(jointName)?.baseName) ? "true" : "false"}
          onDragStart={(e) => {
            if (jointName === "Root") {
              e.preventDefault();
              return;
            }
            handleDragStart(e, jointName, "joint");
          }}
          onDragOver={(e) => {
            e.preventDefault();
            if (dragOverJoint !== jointName) {
              setDragOverJoint(jointName);
            }
          }}
          onDragLeave={() => {
            if (dragOverJoint === jointName) {
              setDragOverJoint(null);
            }
          }}
          onDrop={(e) => {
            setDragOverJoint(null);
            handleDrop(e, jointName);
          }}
          style={{ 
            paddingLeft: "4px",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
            border: dragOverJoint === jointName ? "1px dashed var(--accent-cyan)" : "none",
            background: dragOverJoint === jointName ? "rgba(22, 160, 255, 0.1)" : undefined
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
            {hasChildren ? (
              <span onClick={(e) => toggleCollapse(jointName, e)} style={{ display: "inline-flex", cursor: "pointer" }}>
                {isCollapsed ? <ChevronRight size={14} /> : <ChevronDown size={14} />}
              </span>
            ) : (
              <span style={{ width: "14px" }} />
            )}
            {isCollapsed ? (
              <Folder size={15} style={{ color: "var(--accent-cyan)", flexShrink: 0 }} />
            ) : (
              <FolderOpen size={15} style={{ color: "var(--accent-blue)", flexShrink: 0 }} />
            )}
            <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
              {jointName}
            </span>
          </div>
          <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
            {renderEyeToggle(`joint:${jointName}`)}
            {renderDeleteButton(jointName, "joint")}
          </div>
        </div>

        {!isCollapsed && (
          <div>
            {/* 1. Render markers */}
            {markers.map((marker) => {
              const isMarkerSelected = selectedNode?.type === "marker" && selectedNode.name === marker.name;
              if (searchTerm && !marker.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div key={marker.name}>
                  <div
                    className={`list-item ${isMarkerSelected ? "active" : ""}`}
                    onClick={() => setSelectedNode({ type: "marker", name: marker.name })}
                    onContextMenu={(e) => handleContextMenu(e, marker.name, "marker")}
                    draggable="true"
                    onDragStart={(e) => {
                      e.stopPropagation();
                      handleDragStart(e, marker.name, "marker");
                    }}
                    style={{ 
                      paddingLeft: "16px",
                      display: "flex",
                      justifyContent: "space-between",
                      alignItems: "center"
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                      <span style={{ width: "14px", flexShrink: 0 }} />
                      <Tag size={13} style={{ color: "var(--accent-cyan)", flexShrink: 0 }} />
                      <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                        {marker.name}
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                      {renderEyeToggle(`marker:${marker.name}`)}
                      {renderDeleteButton(marker.name, "marker")}
                    </div>
                  </div>
                  {/* Render Dockpaths attached to this marker */}
                  {model.dockpaths?.filter(p => p.parent_name === marker.name).map(path => {
                    const isPathSelected = selectedNode?.type === "dockpath" && selectedNode.name === path.name;
                    if (searchTerm && !path.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
                    return (
                      <div key={path.name}>
                        <div
                          className={`list-item ${isPathSelected ? "active" : ""}`}
                          onClick={() => setSelectedNode({ type: "dockpath", name: path.name })}
                          onContextMenu={(e) => handleContextMenu(e, path.name, "dockpath")}
                          style={{ 
                            paddingLeft: "32px",
                            display: "flex",
                            justifyContent: "space-between",
                            alignItems: "center"
                          }}
                        >
                          <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                            <span style={{ width: "14px", flexShrink: 0 }} />
                            <Activity size={13} style={{ color: "#00e676", flexShrink: 0 }} />
                            <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                              {path.name}
                            </span>
                          </div>
                          <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                            {renderEyeToggle(`dockpath:${path.name}`)}
                            {renderDeleteButton(path.name, "dockpath")}
                          </div>
                        </div>
                        {path.points?.map((_pt, ptIdx) => {
                          const ptName = `${path.name}:${ptIdx}`;
                          const isPtSelected = selectedNode?.type === "dockpoint" && selectedNode.name === ptName;
                          return (
                            <div
                              key={ptName}
                              className={`list-item ${isPtSelected ? "active" : ""}`}
                              onClick={() => setSelectedNode({ type: "dockpoint", name: ptName })}
                              style={{ paddingLeft: "44px" }}
                            >
                              <span style={{ width: "14px", flexShrink: 0 }} />
                              <Activity size={11} style={{ color: "#ffd54f" }} />
                              <span style={{ fontSize: "11px", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                                Point {ptIdx}
                              </span>
                            </div>
                          );
                        })}
                      </div>
                    );
                  })}
                </div>
              );
            })}

            {/* Render Meshes grouped by base name */}
            {(() => {
              const groupedMeshes = new Map<string, typeof meshes>();
              meshes.forEach((mesh) => {
                const baseName = mesh.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
                if (!groupedMeshes.has(baseName)) groupedMeshes.set(baseName, []);
                groupedMeshes.get(baseName)!.push(mesh);
              });
              return Array.from(groupedMeshes.entries()).map(([baseName, lodMeshes]) => {
                const meshKey = baseName;
                const isMeshSelected = selectedNode?.type === "mesh" && selectedNode.name === meshKey;
                if (searchTerm && !baseName.toLowerCase().includes(searchTerm.toLowerCase())) return null;
                const sortedLods = [...lodMeshes].sort((a, b) => a.lod - b.lod);
                return (
                  <div
                    key={meshKey}
                    className={`list-item ${isMeshSelected ? "active" : ""}`}
                    onClick={() => setSelectedNode({ type: "mesh", name: meshKey })}
                    onContextMenu={(e) => handleContextMenu(e, meshKey, "mesh")}
                    draggable="true"
                    onDragStart={(e) => {
                      e.stopPropagation();
                      handleDragStart(e, meshKey, "mesh");
                    }}
                    style={{ 
                      paddingLeft: "16px",
                      display: "flex",
                      justifyContent: "space-between",
                      alignItems: "center"
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: "8px", overflow: "hidden", flex: 1 }}>
                      <span style={{ width: "14px", flexShrink: 0 }} />
                      <Box size={13} style={{ color: "var(--accent-blue)", flexShrink: 0 }} />
                      <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                        {baseName} ({sortedLods.length} LOD{sortedLods.length !== 1 ? "s" : ""})
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                      {renderEyeToggle(meshKey)}
                      {renderDeleteButton(meshKey, "mesh")}
                    </div>
                  </div>
                );
              });
            })()}

            {/* 2. Render NavLights */}
            {navLights.map((nav) => {
              const isNavSelected = selectedNode?.type === "navlight" && selectedNode.name === nav.name;
              if (searchTerm && !nav.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div
                  key={nav.name}
                  className={`list-item ${isNavSelected ? "active" : ""}`}
                  onClick={() => setSelectedNode({ type: "navlight", name: nav.name })}
                  onContextMenu={(e) => handleContextMenu(e, nav.name, "navlight")}
                  draggable="true"
                  onDragStart={(e) => {
                    e.stopPropagation();
                    handleDragStart(e, nav.name, "navlight");
                  }}
                  style={{ 
                    paddingLeft: "16px",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center"
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                    <span style={{ width: "14px", flexShrink: 0 }} />
                    <Radio size={13} style={{ color: "#f50057", flexShrink: 0 }} />
                    <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                      {nav.name}
                    </span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                    {renderEyeToggle(`navlight:${nav.name}`)}
                    {renderDeleteButton(nav.name, "navlight")}
                  </div>
                </div>
              );
            })}

            {/* 3. Render Engine Burns */}
            {engineBurns.map((burn) => {
              const isBurnSelected = selectedNode?.type === "engine_burn" && selectedNode.name === burn.name;
              if (searchTerm && !burn.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div
                  key={burn.name}
                  className={`list-item ${isBurnSelected ? "active" : ""}`}
                  onClick={() => setSelectedNode({ type: "engine_burn", name: burn.name })}
                  onContextMenu={(e) => handleContextMenu(e, burn.name, "engine_burn")}
                  draggable="true"
                  onDragStart={(e) => {
                    e.stopPropagation();
                    handleDragStart(e, burn.name, "engine_burn");
                  }}
                  style={{ 
                    paddingLeft: "16px",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center"
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                    <span style={{ width: "14px", flexShrink: 0 }} />
                    <Flame size={13} style={{ color: "#ff3d00", flexShrink: 0 }} />
                    <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                      {burn.name}
                    </span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                    {renderEyeToggle(`engine_burn:${burn.name}`)}
                    {renderDeleteButton(burn.name, "engine_burn")}
                  </div>
                </div>
              );
            })}

            {/* 4. Render Engine Glows */}
            {(() => {
              const groupedGlows = new Map<string, typeof engineGlows>();
              engineGlows.forEach((glow) => {
                const baseName = glow.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
                if (!groupedGlows.has(baseName)) groupedGlows.set(baseName, []);
                groupedGlows.get(baseName)!.push(glow);
              });
              return Array.from(groupedGlows.entries()).map(([baseName, lodGlows]) => {
                const glowKey = baseName;
                const isGlowSelected = selectedNode?.type === "engine_glow" && selectedNode.name === glowKey;
                if (searchTerm && !baseName.toLowerCase().includes(searchTerm.toLowerCase())) return null;
                const sortedLods = [...lodGlows].sort((a, b) => a.lod - b.lod);
                return (
                  <div
                    key={glowKey}
                    className={`list-item ${isGlowSelected ? "active" : ""}`}
                    onClick={() => setSelectedNode({ type: "engine_glow", name: glowKey })}
                    onContextMenu={(e) => handleContextMenu(e, glowKey, "engine_glow")}
                    style={{ 
                      paddingLeft: "16px",
                      display: "flex",
                      justifyContent: "space-between",
                      alignItems: "center"
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                      <span style={{ width: "14px", flexShrink: 0 }} />
                      <Flame size={13} style={{ color: "#ffd600", flexShrink: 0 }} />
                      <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                        {baseName} ({sortedLods.length} LOD{sortedLods.length !== 1 ? "s" : ""})
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                      {renderEyeToggle(`engine_glow:${glowKey}`)}
                      {renderDeleteButton(glowKey, "engine_glow")}
                    </div>
                  </div>
                );
              });
            })()}

            {/* 5. Render Engine Shapes */}
            {engineShapes.map((shape) => {
              const isShapeSelected = selectedNode?.type === "engine_shape" && selectedNode.name === shape.name;
              if (searchTerm && !shape.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div
                  key={shape.name}
                  className={`list-item ${isShapeSelected ? "active" : ""}`}
                  onClick={() => setSelectedNode({ type: "engine_shape", name: shape.name })}
                  onContextMenu={(e) => handleContextMenu(e, shape.name, "engine_shape")}
                  style={{ 
                    paddingLeft: "16px",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center"
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                    <span style={{ width: "14px", flexShrink: 0 }} />
                    <Flame size={13} style={{ color: "#7209b7", flexShrink: 0 }} />
                    <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                      {shape.name}
                    </span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                    {renderEyeToggle(`engine_shape:${shape.name}`)}
                    {renderDeleteButton(shape.name, "engine_shape")}
                  </div>
                </div>
              );
            })}

            {/* 6. Render Collision Meshes */}
            {collisionMeshes.map((col) => {
              const isColSelected = selectedNode?.type === "collision" && selectedNode.name === col.name;
              if (searchTerm && !col.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div
                  key={col.name}
                  className={`list-item ${isColSelected ? "active" : ""}`}
                  onClick={() => setSelectedNode({ type: "collision", name: col.name })}
                  onContextMenu={(e) => handleContextMenu(e, col.name, "collision")}
                  style={{ 
                    paddingLeft: "16px",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center"
                  }}
                >
                  <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                    <span style={{ width: "14px", flexShrink: 0 }} />
                    <Shield size={13} style={{ color: "#ff1744", flexShrink: 0 }} />
                    <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                      {col.name} (COL)
                    </span>
                  </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                    {renderEyeToggle(`collision:${col.name}`)}
                    {renderDeleteButton(col.name, "collision")}
                  </div>
                </div>
              );
            })}

            {/* 7. Render Dockpaths & Dockpoints */}
            {dockpaths.map((path) => {
              const isPathSelected = selectedNode?.type === "dockpath" && selectedNode.name === path.name;
              if (searchTerm && !path.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
              return (
                <div key={path.name}>
                  <div
                    className={`list-item ${isPathSelected ? "active" : ""}`}
                    onClick={() => setSelectedNode({ type: "dockpath", name: path.name })}
                  onContextMenu={(e) => handleContextMenu(e, path.name, "dockpath")}
                  style={{ 
                    paddingLeft: "16px",
                    display: "flex",
                    justifyContent: "space-between",
                    alignItems: "center"
                  }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                      <span style={{ width: "14px", flexShrink: 0 }} />
                      <Activity size={13} style={{ color: "#00e676", flexShrink: 0 }} />
                      <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                        {path.name}
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                      {renderEyeToggle(`dockpath:${path.name}`)}
                      {renderDeleteButton(path.name, "dockpath")}
                    </div>
                  </div>
                  {path.points?.map((_pt, ptIdx) => {
                    const ptName = `${path.name}:${ptIdx}`;
                    const isPtSelected = selectedNode?.type === "dockpoint" && selectedNode.name === ptName;
                    return (
                      <div
                        key={ptName}
                        className={`list-item ${isPtSelected ? "active" : ""}`}
                        onClick={() => setSelectedNode({ type: "dockpoint", name: ptName })}
                        style={{ paddingLeft: "28px" }}
                      >
                        <span style={{ width: "14px", flexShrink: 0 }} />
                        <Activity size={11} style={{ color: "#ffd54f" }} />
                        <span style={{ fontSize: "11px", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                          Point {ptIdx}
                        </span>
                      </div>
                    );
                  })}
                </div>
              );
            })}

            {/* Render child weapon groups */}
            {childWeaponGroups.map((baseName) => renderAssemblyNode(baseName, depth + 1))}

            {/* Recursively render child joints */}
            {standardChildJoints.map((child) => renderJointNode(child.name, depth + 1, nextVisited))}
          </div>
        )}
      </div>
    );
  };

  const renderAssemblyNode = (baseName: string, depth: number): React.ReactNode => {
    const isSelected = selectedNode?.type === "weapon_group" && selectedNode.name === baseName;
    const isCollapsed = !!collapsedJoints[`weapon_group:${baseName}`];
    const isSlaveAssembly = /Slave\d*$/i.test(baseName);

    // Find all joints belonging to this weapon group
    const groupJoints = getTreeIndex(model).groupJointsByBase.get(baseName)
      || model.joints.filter(j => isJointInWeaponBase(j.name, baseName));
    
    // Find the root joints of this weapon group (the ones parented to something outside the group)
    const groupMemberNames = new Set(groupJoints.map(j => j.name));
    const rootGroupJoints = groupJoints.filter(j => !j.parent_name || !groupMemberNames.has(j.parent_name));

    const hasChildren = rootGroupJoints.length > 0;
    const matchesSearch = baseName.toLowerCase().includes(searchTerm.toLowerCase()) || groupJoints.some(j => matchesSearchRecursive(j.name));

    if (searchTerm && !matchesSearch) return null;

    return (
      <div key={`weapon_group:${baseName}`} style={{ marginLeft: depth > 0 ? "12px" : "0px" }}>
        <div
          className={`list-item ${isSelected ? "active" : ""}`}
          onClick={() => {
            const info = getWeaponGroupInfo(baseName + "_Position") || getWeaponGroupInfo(baseName + "_Heading") || getWeaponGroupInfo(baseName);
            setSelectedNode({ type: info?.type || "weapon_group", name: baseName });
          }}
          onContextMenu={(e) => {
            const info = getWeaponGroupInfo(baseName + "_Position") || getWeaponGroupInfo(baseName + "_Heading") || getWeaponGroupInfo(baseName);
            handleContextMenu(e, baseName, info?.type || "weapon_group");
          }}
          draggable="true"
          onDragStart={(e) => {
            e.stopPropagation();
            const info = getWeaponGroupInfo(baseName + "_Position") || getWeaponGroupInfo(baseName + "_Heading") || getWeaponGroupInfo(baseName);
            handleDragStart(e, baseName, info?.type || "weapon_group");
          }}
          style={{
            paddingLeft: "4px",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
          }}
        >
          <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
            {hasChildren ? (
              <span onClick={(e) => toggleCollapse(`weapon_group:${baseName}`, e)} style={{ display: "inline-flex", cursor: "pointer" }}>
                {isCollapsed ? <ChevronRight size={14} /> : <ChevronDown size={14} />}
              </span>
            ) : (
              <span style={{ width: "14px" }} />
            )}
            <Crosshair size={14} style={{ color: "var(--accent-cyan)", flexShrink: 0 }} />
            <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", fontWeight: "600" }}>
              {(() => {
                const info = getWeaponGroupInfo(baseName + "_Position") || getWeaponGroupInfo(baseName + "_Heading") || getWeaponGroupInfo(baseName);
                const type = info?.type || "weapon_group";
                if (type === "weapon_group") return "Weapon: " + baseName + (isSlaveAssembly ? " (Slave)" : "");
                if (type === "hardpoint_group") return "Hardpoint: " + baseName;
                return "Point: " + baseName;
              })()}
            </span>
          </div>
          <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
            {renderEyeToggle(`weapon_group:${baseName}`)}
            {renderDeleteButton(baseName, "weapon_group")}
          </div>
        </div>

        {!isCollapsed && (
          <div>
            {/* Recursively render the root joints of the weapon family as sub-collapsible skeletal trees! */}
            {rootGroupJoints.map(j => renderJointNode(j.name, depth + 1))}
          </div>
        )}
      </div>
    );
  };

  // Find root joints (joints with no parent joint, or whose parent joint doesn't exist in the joints list)
  const rootJoints = model.joints.filter((j) => {
    if (getWeaponGroupInfo(j.name) !== null) return false; // Filter out weapon joints from general joints
    if (model.nav_lights?.some(n => n.name.toLowerCase() === j.name.toLowerCase())) {
      const isEngineParent =
        model.engine_burns?.some(b => b.parent_name === j.name) ||
        model.engine_glows?.some(g => g.parent_name === j.name) ||
        model.engine_shapes?.some(s => s.parent_name === j.name);
      if (!isEngineParent) return false;
    }
    if (!j.parent_name || j.parent_name === j.name) return true;
    return !model.joints.some((other) => other.name === j.parent_name && other.name !== j.name);
  });

  const rootNavLights = model.nav_lights?.filter(nav => {
    const joint = model.joints.find(j => j.name === nav.name);
    if (!joint) return true; // root by default if missing joint
    if (!joint.parent_name || joint.parent_name === joint.name) return true;
    return !model.joints.some((other) => other.name === joint.parent_name && other.name !== joint.name);
  }) || [];

  // Find root weapon groups (groups whose top-most parent is null, or parentless)
  const rootWeaponGroups = getUniqueAssemblyGroups().filter(baseName => {
    const groupJoints = model.joints.filter(j => isJointInWeaponBase(j.name, baseName));
    return groupJoints.some(j => {
      const hasParentInGroup = groupJoints.some(other => other.name === j.parent_name);
      if (hasParentInGroup) return false;
      if (!j.parent_name || j.parent_name === j.name) return true;
      return !model.joints.some((other) => other.name === j.parent_name && other.name !== j.name);
    });
  });

  // Find root dockpaths (dockpaths with no parent, or parent isn't a joint/marker)
  const rootDockpaths = model.dockpaths?.filter(p => {
    if (!p.parent_name || p.parent_name === "Root") return true;
    const hasJointParent = model.joints.some(j => j.name === p.parent_name);
    const hasMarkerParent = model.markers?.some(m => m.name === p.parent_name);
    return !hasJointParent && !hasMarkerParent;
  }) || [];

  return (
    <div className="panel" style={{ height: "100%", display: "flex", flexDirection: "column" }}>
      {/* Tab Selector */}
      <div style={{ display: "flex", borderBottom: "1px solid var(--border-color)", background: "rgba(10, 16, 27, 0.5)", flexShrink: 0 }}>
        <button
          onClick={() => { setActiveTab("hierarchy"); setSearchTerm(""); onTabChange?.("hierarchy"); }}
          style={{
            flex: 1,
            background: activeTab === "hierarchy" ? "rgba(22, 160, 255, 0.12)" : "transparent",
            border: "none",
            borderBottom: activeTab === "hierarchy" ? "2px solid var(--accent-cyan)" : "none",
            color: activeTab === "hierarchy" ? "var(--accent-cyan)" : "var(--text-secondary)",
            borderRadius: 0,
            padding: "10px 0",
            fontSize: "12px",
            fontWeight: "600",
            display: "flex",
            justifyContent: "center",
            alignItems: "center",
            gap: "6px"
          }}
        >
          <FolderOpen size={14} />
          Hierarchy
        </button>
        <button
          onClick={() => { setActiveTab("materials"); setSearchTerm(""); onTabChange?.("materials"); }}
          style={{
            flex: 1,
            background: activeTab === "materials" ? "rgba(22, 160, 255, 0.12)" : "transparent",
            border: "none",
            borderBottom: activeTab === "materials" ? "2px solid var(--accent-cyan)" : "none",
            color: activeTab === "materials" ? "var(--accent-cyan)" : "var(--text-secondary)",
            borderRadius: 0,
            padding: "10px 0",
            fontSize: "12px",
            fontWeight: "600",
            display: "flex",
            justifyContent: "center",
            alignItems: "center",
            gap: "6px"
          }}
        >
          <Palette size={14} />
          Materials
        </button>
        <button
          onClick={() => { setActiveTab("animations"); setSearchTerm(""); onTabChange?.("animations"); }}
          style={{
            flex: 1,
            background: activeTab === "animations" ? "rgba(22, 160, 255, 0.12)" : "transparent",
            border: "none",
            borderBottom: activeTab === "animations" ? "2px solid var(--accent-cyan)" : "none",
            color: activeTab === "animations" ? "var(--accent-cyan)" : "var(--text-secondary)",
            borderRadius: 0,
            padding: "10px 0",
            fontSize: "11px",
            fontWeight: "600",
            display: "flex",
            justifyContent: "center",
            alignItems: "center",
            gap: "4px"
          }}
        >
          <Activity size={12} />
          Animations
        </button>
        <button
          onClick={() => { setActiveTab("targetboxes"); setSearchTerm(""); onTabChange?.("targetboxes"); }}
          style={{
            flex: 1,
            background: activeTab === "targetboxes" ? "rgba(22, 160, 255, 0.12)" : "transparent",
            border: "none",
            borderBottom: activeTab === "targetboxes" ? "2px solid var(--accent-cyan)" : "none",
            color: activeTab === "targetboxes" ? "var(--accent-cyan)" : "var(--text-secondary)",
            borderRadius: 0,
            padding: "10px 0",
            fontSize: "11px",
            fontWeight: "600",
            display: "flex",
            justifyContent: "center",
            alignItems: "center",
            gap: "4px"
          }}
        >
          <Crosshair size={12} />
          Target Boxes
        </button>
      </div>

      {/* Main Panel Area */}
      <div style={{ flex: 1, display: "flex", flexDirection: "column", minHeight: 0 }}>
        <div className="panel-header" style={{ display: "flex", flexDirection: "column", gap: "6px", padding: "10px 14px", alignItems: "stretch", borderBottom: "1px solid var(--border-color)", background: "rgba(10, 16, 27, 0.3)" }}>
          {/* Row 1: Title and Actions */}
          <div style={{ display: "flex", flexDirection: "column", alignItems: "flex-start", gap: "8px" }}>
            <span style={{ fontWeight: "700", textTransform: "uppercase", fontSize: "11px", letterSpacing: "0.08em", color: "var(--accent-cyan)" }}>
              {activeTab === "hierarchy" ? "Skeleton Tree" : activeTab === "materials" ? "Material Library" : activeTab === "animations" ? "Animation Library" : "Target Boxing Editor"}
            </span>
            
            <div style={{ display: "flex", flexWrap: "wrap", gap: "6px", alignItems: "center", width: "100%" }}>
              {activeTab === "hierarchy" ? (
                <>
                  <button
                    onClick={handleExportNodes}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(22, 160, 255, 0.12)",
                      color: "var(--accent-cyan)",
                      border: "1px solid rgba(22, 160, 255, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "600",
                      display: "inline-flex",
                      alignItems: "center",
                      gap: "4px",
                      whiteSpace: "nowrap"
                    }}
                    title="Export Node Tree (No Meshes)"
                  >
                    <Upload size={11} />
                    Export JSON
                  </button>
                  <button
                    onClick={handleImportNodes}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(22, 160, 255, 0.12)",
                      color: "var(--accent-cyan)",
                      border: "1px solid rgba(22, 160, 255, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "600",
                      display: "inline-flex",
                      alignItems: "center",
                      gap: "4px",
                      whiteSpace: "nowrap"
                    }}
                    title="Import Node Tree"
                  >
                    <Download size={11} />
                    Import JSON
                  </button>
                  <button
                    onClick={() => setIsAddNodeOpen(true)}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(22, 160, 255, 0.12)",
                      color: "var(--accent-cyan)",
                      border: "1px solid rgba(22, 160, 255, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "600",
                      display: "inline-flex",
                      alignItems: "center",
                      gap: "4px",
                      whiteSpace: "nowrap"
                    }}
                    title="Add a new node to the HOD skeleton"
                  >
                    <Plus size={11} />
                    Add Node
                  </button>
                  <button
                    onClick={() => {
                      const resetVisibility: Record<string, boolean> = {};
                      const groupedMeshes = new Map<string, HODModel["meshes"]>();
                      model?.meshes.forEach((m) => {
                        const baseName = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
                        if (!groupedMeshes.has(baseName)) groupedMeshes.set(baseName, []);
                        groupedMeshes.get(baseName)!.push(m);
                      });
                      groupedMeshes.forEach((meshes) => {
                        [...meshes].sort((a, b) => a.lod - b.lod).forEach((m, idx) => {
                          resetVisibility[`${m.name}_lod_${m.lod}`] = idx === 0;
                        });
                      });

                      const groupedGlows = new Map<string, HODModel["engine_glows"]>();
                      model?.engine_glows.forEach((g) => {
                        const baseName = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
                        if (!groupedGlows.has(baseName)) groupedGlows.set(baseName, []);
                        groupedGlows.get(baseName)!.push(g);
                      });
                      groupedGlows.forEach((glows) => {
                        [...glows].sort((a, b) => a.lod - b.lod).forEach((g, idx) => {
                          resetVisibility[`engine_glow:${g.name}`] = idx === 0;
                        });
                      });
                      setVisibleMeshes(resetVisibility);
                    }}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(0, 230, 118, 0.12)",
                      color: "#00e676",
                      border: "1px solid rgba(0, 230, 118, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "600",
                      display: "inline-flex",
                      alignItems: "center",
                      gap: "4px",
                      whiteSpace: "nowrap"
                    }}
                    title="Toggle all nodes back on"
                  >
                    <Eye size={11} />
                    Show All
                  </button>
                </>
              ) : activeTab === "materials" ? (
                <div style={{ display: "flex", gap: "6px", alignItems: "center" }}>
                  <button
                    onClick={() => setIsAddMatOpen(true)}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(22, 160, 255, 0.12)",
                      color: "var(--accent-cyan)",
                      border: "1px solid rgba(22, 160, 255, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "600",
                      display: "inline-flex",
                      alignItems: "center",
                      gap: "4px",
                      whiteSpace: "nowrap"
                    }}
                    title="Create a new material"
                  >
                    <Plus size={11} />
                    Add Mat
                  </button>
                  <button
                    onClick={handleExportMaterials}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(22, 160, 255, 0.12)",
                      color: "var(--accent-cyan)",
                      border: "1px solid rgba(22, 160, 255, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "500",
                      display: "inline-flex",
                      alignItems: "center"
                    }}
                    title="Export Material Library to JSON"
                  >
                    Export
                  </button>
                  <button
                    onClick={handleImportMaterials}
                    style={{
                      fontSize: "10px",
                      padding: "3px 8px",
                      background: "rgba(0, 230, 118, 0.12)",
                      color: "#00e676",
                      border: "1px solid rgba(0, 230, 118, 0.3)",
                      borderRadius: "4px",
                      cursor: "pointer",
                      fontWeight: "500",
                      display: "inline-flex",
                      alignItems: "center"
                    }}
                    title="Import Material Library from JSON"
                  >
                    Import
                  </button>
                </div>
              ) : (
                <div style={{ fontSize: "10px", color: "var(--text-muted)", fontWeight: "500" }}>
                  Interactive Scrub Engine
                </div>
              )}
            </div>
          </div>
 
          {/* Row 2: Subtitle Stats */}
          <div style={{ display: "flex", justifyContent: "flex-start", gap: "10px", alignItems: "center", borderTop: "1px solid rgba(255,255,255,0.03)", paddingTop: "4px" }}>
            {activeTab === "hierarchy" ? (
              <div style={{ fontSize: "10px", color: "var(--text-muted)", display: "flex", gap: "8px", alignItems: "center" }}>
                <span style={{ color: "var(--accent-cyan)", fontWeight: "600" }}>{model.meshes.length}</span>
                <span>Meshes</span>
                <span style={{ color: "rgba(255,255,255,0.1)" }}>|</span>
                <span style={{ color: "var(--accent-cyan)", fontWeight: "600" }}>{model.joints.length}</span>
                <span>Joints</span>
              </div>
            ) : activeTab === "materials" ? (
              <div style={{ fontSize: "10px", color: "var(--text-muted)", display: "flex", gap: "8px", alignItems: "center" }}>
                <span style={{ color: "var(--accent-cyan)", fontWeight: "600" }}>{model.materials?.length || 0}</span>
                <span>Materials Allocated</span>
              </div>
            ) : activeTab === "animations" ? (
              <div style={{ display: "flex", justifyContent: "space-between", width: "100%", alignItems: "center" }}>
                <div style={{ fontSize: "10px", color: "var(--text-muted)", display: "flex", gap: "8px", alignItems: "center" }}>
                  <span style={{ color: "var(--accent-cyan)", fontWeight: "600" }}>{model.animations?.length || 0}</span>
                  <span>Animations Configured</span>
                </div>
                <button
                  onClick={() => setIsCreateAnimOpen(true)}
                  style={{
                    display: "flex", alignItems: "center", gap: "4px",
                    background: "rgba(0,230,118,0.1)", border: "1px solid rgba(0,230,118,0.3)",
                    color: "#00e676", padding: "2px 8px", borderRadius: "4px",
                    fontSize: "10px", fontWeight: "600", cursor: "pointer"
                  }}
                >
                  <Plus size={10} /> New Anim
                </button>
              </div>
            ) : (
              <div style={{ fontSize: "10px", color: "var(--text-muted)", display: "flex", gap: "8px", alignItems: "center" }}>
                <span style={{ color: "var(--accent-cyan)", fontWeight: "600" }}>{targetBoxes?.length || 0}</span>
                <span>Target Boxes</span>
              </div>
            )}
          </div>
        </div>
        
        {/* Search Input */}
        <div style={{ padding: "8px 12px", borderBottom: "1px solid var(--border-color)", display: "flex", gap: "8px", alignItems: "center" }}>
          <Search size={14} style={{ color: "var(--text-muted)" }} />
          <input
            placeholder={activeTab === "hierarchy" ? "Filter nodes..." : activeTab === "materials" ? "Filter materials..." : "Filter animations..."}
            value={searchTerm}
            onChange={(e) => setSearchTerm(e.target.value)}
            style={{ height: "28px", fontSize: "12px", width: "100%" }}
          />
        </div>

        <div
          ref={listContainerRef}
          onDragOver={handleDragOverContainer}
          className="panel-content"
          style={{ padding: "8px 4px", overflowY: "auto", flex: 1 }}
        >
          {activeTab === "hierarchy" ? (
            rootJoints.length > 0 || rootWeaponGroups.length > 0 || rootNavLights.length > 0 || rootDockpaths.length > 0 ? (
              <>
                {rootWeaponGroups.map((baseName) => renderAssemblyNode(baseName, 0))}
                {rootJoints.map((root) => renderJointNode(root.name, 0))}
                {rootNavLights.map((nav) => {
                  const isNavSelected = selectedNode?.type === "navlight" && selectedNode.name === nav.name;
                  return (
                    <div
                      key={nav.name}
                      className={`list-item ${isNavSelected ? "active" : ""}`}
                      onClick={() => setSelectedNode({ type: "navlight", name: nav.name })}
                  onContextMenu={(e) => handleContextMenu(e, nav.name, "navlight")}
                      draggable="true"
                      onDragStart={(e) => {
                        e.stopPropagation();
                        handleDragStart(e, nav.name, "navlight");
                      }}
                      style={{ 
                        paddingLeft: "4px",
                        display: "flex",
                        justifyContent: "space-between",
                        alignItems: "center"
                      }}
                    >
                      <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                        <Radio size={13} style={{ color: "#f50057", flexShrink: 0 }} />
                        <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                          {nav.name}
                        </span>
                      </div>
                  <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                    {renderEyeToggle(`navlight:${nav.name}`)}
                    {renderDeleteButton(nav.name, "navlight")}
                  </div>
                    </div>
                  );
                })}
                {rootDockpaths.map((path) => {
                  const isPathSelected = selectedNode?.type === "dockpath" && selectedNode.name === path.name;
                  if (searchTerm && !path.name.toLowerCase().includes(searchTerm.toLowerCase())) return null;
                  return (
                    <div key={path.name}>
                      <div
                        className={`list-item ${isPathSelected ? "active" : ""}`}
                        onClick={() => setSelectedNode({ type: "dockpath", name: path.name })}
                        onContextMenu={(e) => handleContextMenu(e, path.name, "dockpath")}
                        style={{ 
                          display: "flex",
                          justifyContent: "space-between",
                          alignItems: "center"
                        }}
                      >
                        <div style={{ display: "flex", alignItems: "center", gap: "6px", overflow: "hidden", flex: 1 }}>
                          <span style={{ width: "14px", flexShrink: 0 }} />
                          <Activity size={13} style={{ color: "#00e676", flexShrink: 0 }} />
                          <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                            {path.name}
                          </span>
                        </div>
                        <div style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                          {renderEyeToggle(`dockpath:${path.name}`)}
                          {renderDeleteButton(path.name, "dockpath")}
                        </div>
                      </div>
                      {path.points?.map((_pt, ptIdx) => {
                        const ptName = `${path.name}:${ptIdx}`;
                        const isPtSelected = selectedNode?.type === "dockpoint" && selectedNode.name === ptName;
                        return (
                          <div
                            key={ptName}
                            className={`list-item ${isPtSelected ? "active" : ""}`}
                            onClick={() => setSelectedNode({ type: "dockpoint", name: ptName })}
                            style={{ paddingLeft: "12px" }}
                          >
                            <span style={{ width: "14px", flexShrink: 0 }} />
                            <Activity size={11} style={{ color: "#ffd54f" }} />
                            <span style={{ fontSize: "11px", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                              Point {ptIdx}
                            </span>
                          </div>
                        );
                      })}
                    </div>
                  );
                })}
              </>
            ) : (
              <div style={{ padding: "20px", color: "var(--text-muted)", fontSize: "13px", textAlign: "center" }}>
                Skeletal hierarchy is empty.
              </div>
            )
          ) : activeTab === "materials" ? (
            <div style={{ display: "flex", flexDirection: "column", gap: "16px", padding: "4px" }}>
              <div>
                <div style={{ fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "8px", paddingLeft: "8px" }}>Materials</div>
                {model.materials && model.materials.length > 0 ? (
                  <div style={{ display: "flex", flexDirection: "column" }}>
                    {model.materials
                      .filter(m => !searchTerm || m.name.toLowerCase().includes(searchTerm.toLowerCase()) || m.shader_name.toLowerCase().includes(searchTerm.toLowerCase()))
                      .map((material, idx) => {
                      const isMatSelected = selectedNode?.type === "material" && selectedNode.name === material.name;
                      return (
                        <div
                          key={material.name}
                          className={`list-item ${isMatSelected ? "active" : ""}`}
                          onClick={() => setSelectedNode({ type: "material", name: material.name })}
                          style={{ padding: "10px 12px", display: "flex", flexDirection: "column", gap: "2px", alignItems: "flex-start", marginBottom: "4px" }}
                        >
                          <div style={{ display: "flex", alignItems: "center", gap: "8px", width: "100%" }}>
                            <Palette size={14} style={{ color: "var(--accent-cyan)", flexShrink: 0 }} />
                            <span style={{ fontWeight: "600", color: isMatSelected ? "var(--accent-cyan)" : "var(--text-primary)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                              {material.name}
                            </span>
                            {renderDeleteButton(material.name, "material")}
                            <span style={{ fontSize: "9px", padding: "1px 4px", background: "rgba(255,255,255,0.05)", borderRadius: "3px", color: "var(--text-muted)", marginLeft: "auto", flexShrink: 0 }}>
                              ID {idx}
                            </span>
                          </div>
                          <div style={{ fontSize: "11px", color: "var(--text-secondary)", fontFamily: "var(--font-mono)", paddingLeft: "22px" }}>
                            Shader: {material.shader_name}
                          </div>
                        </div>
                      );
                    })}
                  </div>
                ) : (
                  <div style={{ padding: "10px", color: "var(--text-muted)", fontSize: "12px", textAlign: "center" }}>
                    No materials defined.
                  </div>
                )}
              </div>

              <hr style={{ border: "none", borderTop: "1px solid rgba(255,255,255,0.05)", margin: "0 8px" }} />

              <div>
              <div style={{ display: "flex", flexDirection: "column", gap: "8px", marginBottom: "8px" }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", paddingLeft: "8px", paddingRight: "8px" }}>
                  <div style={{ fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase" }}>Textures</div>
                  <div style={{ display: "flex", alignItems: "center", gap: "8px" }}>
                    <button
                      onClick={handleAutoFixTextures}
                      className="icon-button"
                      style={{ padding: "2px 6px", background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "4px", display: "flex", alignItems: "center", gap: "4px" }}
                      title="Auto-fix texture types (_DIFF, _GLOW, etc.) based on Material usage"
                    >
                      <Wand2 size={10} style={{ color: "var(--accent-cyan)" }} />
                      <span style={{ fontSize: "9px", color: "var(--text-primary)" }}>Auto-Fix</span>
                    </button>
                    <button
                      onClick={handleImportTGA}
                      className="icon-button"
                      style={{ padding: "2px 6px", background: "rgba(255,255,255,0.05)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "4px", display: "flex", alignItems: "center", gap: "4px" }}
                      title="Import multiple external .TGA image files"
                    >
                      <Upload size={10} style={{ color: "var(--accent-cyan)" }} />
                      <span style={{ fontSize: "9px", color: "var(--text-primary)" }}>Import TGA</span>
                    </button>
                    <span style={{ fontSize: "9px", background: "rgba(255,255,255,0.05)", padding: "2px 6px", borderRadius: "4px", color: "var(--text-muted)" }}>
                      {model.textures?.length || 0}
                    </span>
                  </div>
                </div>

                <div style={{ padding: "0 8px" }}>
                  {(() => {
                    const tgaDirs = new Set<string>();
                    model.textures?.forEach(t => {
                      if (t.source_path) {
                        const dir = t.source_path.substring(0, Math.max(t.source_path.lastIndexOf("/"), t.source_path.lastIndexOf("\\\\")));
                        if (dir) tgaDirs.add(dir);
                      }
                    });
                    const dirs = Array.from(tgaDirs);
                    if (dirs.length > 0) {
                      return (
                        <div style={{ background: "rgba(0,0,0,0.15)", padding: "6px", borderRadius: "4px", border: "1px solid rgba(255,255,255,0.03)" }}>
                          <div style={{ fontSize: "9px", color: "var(--text-secondary)", marginBottom: "4px" }}>
                            <strong>📁 Loaded TGA Directories:</strong>
                          </div>
                          <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                            {dirs.map((d, i) => (
                              <span key={i} style={{ fontFamily: "var(--font-mono)", fontSize: "9px", color: "var(--accent-cyan)", wordBreak: "break-all" }}>
                                {d}
                              </span>
                            ))}
                          </div>
                        </div>
                      );
                    }
                    return null;
                  })()}
                </div>
              </div>
                {model.textures && model.textures.length > 0 ? (
                  <div style={{ display: "grid", gridTemplateColumns: "1fr", gap: "4px" }}>
                    <div style={{ display: "contents" }}>
                      {parseTextureGroups(model.textures)
                        .filter(g => !searchTerm || g.baseName.toLowerCase().includes(searchTerm.toLowerCase()))
                        .map((group, idx) => {
                          const isSelected = selectedNode?.type === "texture_group" && selectedNode.name === group.baseName;
                          return (
                            <div
                              key={group.baseName + "_" + idx}
                              onClick={() => setSelectedNode({ type: "texture_group", name: group.baseName })}
                              onContextMenu={(e) => handleContextMenu(e, group.baseName, "texture_group")}
                              className={`list-item ${isSelected ? "active" : ""}`}
                              style={{
                                padding: "6px 8px",
                                display: "flex",
                                alignItems: "center",
                                gap: "8px",
                                cursor: "pointer",
                                background: isSelected ? "var(--selection-bg)" : "transparent",
                              }}
                            >
                              <div style={{ width: "20px", height: "20px", borderRadius: "3px", background: "rgba(255,255,255,0.1)", display: "flex", alignItems: "center", justifyContent: "center", flexShrink: 0 }}>
                                <Folder size={12} style={{ color: "var(--text-muted)" }} />
                              </div>
                              <div style={{ display: "flex", flexDirection: "column", overflow: "hidden" }}>
                                <span style={{ fontSize: "11px", fontWeight: "600", color: "var(--text-primary)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                                  {group.baseName}
                                </span>
                                <span style={{ fontSize: "9px", color: "var(--text-muted)" }}>
                                  {group.textures.length} textures
                                </span>
                              </div>
                            </div>
                          );
                        })}
                    </div>
                  </div>
                ) : (
                  <div style={{ padding: "10px", color: "var(--text-muted)", fontSize: "12px", textAlign: "center" }}>
                    No textures loaded.
                  </div>
                )}
              </div>
            </div>
          ) : activeTab === "animations" ? (
            model.animations && model.animations.length > 0 ? (
              <div style={{ display: "flex", flexDirection: "column" }}>
                {model.animations
                  .filter(a => !searchTerm || a.name.toLowerCase().includes(searchTerm.toLowerCase()))
                  .map((anim) => {
                    const animIdx = model.animations!.indexOf(anim);
                    const isActive = animIdx === selectedAnimIdx;
                    return (
                      <div
                        key={anim.name}
                        className={`list-item ${isActive ? "active" : ""}`}
                        onClick={() => setSelectedAnimIdx(animIdx)}
                      style={{
                        padding: "10px 12px",
                        display: "flex",
                        flexDirection: "column",
                        gap: "6px",
                        alignItems: "stretch",
                        marginBottom: "6px",
                        cursor: "pointer"
                      }}
                    >
                      <div style={{ display: "flex", alignItems: "center", gap: "8px", width: "100%" }}>
                        <Activity size={14} style={{ color: isActive ? "var(--accent-success)" : "var(--text-secondary)", flexShrink: 0 }} />
                        <span style={{ fontWeight: "600", color: isActive ? "var(--accent-success)" : "var(--text-primary)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
                          {anim.name}
                        </span>
                        {editingDurationIdx === animIdx ? (
                          <input
                            autoFocus
                            type="number"
                            step="0.1"
                            min="0.1"
                            value={editDurationText}
                            onChange={(e) => setEditDurationText(e.target.value)}
                            onBlur={() => handleDurationCommit(animIdx)}
                            onKeyDown={(e) => {
                              if (e.key === "Enter") handleDurationCommit(animIdx);
                              else if (e.key === "Escape") setEditingDurationIdx(null);
                            }}
                            onClick={(e) => e.stopPropagation()}
                            title="Animation duration in seconds (Enter to apply, Esc to cancel)"
                            style={{ width: "76px", fontSize: "12px", padding: "3px 6px", background: "rgba(0,0,0,0.4)", border: "1px solid var(--accent-cyan)", borderRadius: "3px", color: "var(--text-primary)", marginLeft: "auto", flexShrink: 0 }}
                          />
                        ) : (
                          <span
                            onClick={(e) => { e.stopPropagation(); setEditingDurationIdx(animIdx); setEditDurationText(anim.duration.toString()); }}
                            onMouseEnter={(e) => { e.currentTarget.style.borderColor = "var(--accent-cyan)"; e.currentTarget.style.color = "var(--accent-cyan)"; }}
                            onMouseLeave={(e) => { e.currentTarget.style.borderColor = "rgba(255,255,255,0.15)"; e.currentTarget.style.color = "var(--text-muted)"; }}
                            title="Click to edit duration"
                            style={{ fontSize: "11px", padding: "3px 8px", background: "rgba(255,255,255,0.05)", border: "1px dashed rgba(255,255,255,0.15)", borderRadius: "3px", color: "var(--text-muted)", marginLeft: "auto", flexShrink: 0, cursor: "pointer", display: "inline-flex", alignItems: "center", gap: "4px" }}
                          >
                            <Pencil size={11} />
                            {anim.duration.toFixed(2)}s
                          </span>
                        )}
                        <button
                          onClick={(e) => handleDeleteAnimation(animIdx, anim.name, e)}
                          title="Delete Animation"
                          style={{
                            background: "rgba(255, 64, 64, 0.1)",
                            border: "1px solid rgba(255, 64, 64, 0.3)",
                            color: "#ff4040",
                            borderRadius: "4px",
                            padding: "4px 9px",
                            cursor: "pointer",
                            display: "flex",
                            alignItems: "center",
                            justifyContent: "center",
                            flexShrink: 0
                          }}
                        >
                          <Trash2 size={14} />
                        </button>
                      </div>
                      
                      <div style={{ fontSize: "11px", color: "var(--text-secondary)", display: "flex", justifyContent: "space-between" }}>
                        <span>Tracks: {anim.tracks.length}</span>
                        <span>Keyframes: {anim.tracks.reduce((acc, t) => acc + t.keyframes.length, 0)}</span>
                      </div>

                      {isActive && anim.tracks.length > 0 && (
                        <div style={{
                          marginTop: "6px",
                          borderTop: "1px solid rgba(255,255,255,0.05)",
                          paddingTop: "6px",
                          display: "flex",
                          flexDirection: "column",
                          gap: "6px"
                        }}>
                          <div style={{ fontSize: "9px", color: "var(--accent-cyan)", fontWeight: "700", textTransform: "uppercase", letterSpacing: "0.05em" }}>Joint Channels</div>
                          {anim.tracks.map((track) => (
                            <div key={track.joint_name} style={{ display: "flex", flexDirection: "column", gap: "2px", background: "rgba(0,0,0,0.15)", padding: "6px 8px", borderRadius: "4px" }}>
                              <div style={{ fontSize: "10px", fontWeight: "600", color: "var(--text-primary)", overflow: "hidden", textOverflow: "ellipsis" }}>{track.joint_name}</div>
                              <div style={{ display: "flex", flexWrap: "wrap", gap: "4px", marginTop: "2px" }}>
                                {track.keyframes.map((kf, kfIdx) => (
                                  <span
                                    key={kfIdx}
                                    style={{
                                      fontSize: "8px",
                                      padding: "1px 4px",
                                      background: "rgba(22, 160, 255, 0.15)",
                                      border: "1px solid rgba(22, 160, 255, 0.3)",
                                      borderRadius: "3px",
                                      color: "var(--accent-cyan)"
                                    }}
                                    title={`Pos: ${kf.position ? "Yes" : "No"}, Rot: ${kf.rotation ? "Yes" : "No"}`}
                                  >
                                    {kf.time.toFixed(2)}s
                                  </span>
                                ))}
                              </div>
                            </div>
                          ))}
                        </div>
                      )}
                      </div>
                    );
                  })}
              </div>
            ) : (
              <div style={{ padding: "30px 20px", color: "var(--text-muted)", fontSize: "13px", textAlign: "center", display: "flex", flexDirection: "column", gap: "10px", alignItems: "center" }}>
                <Activity size={24} style={{ color: "var(--border-color)", opacity: 0.5 }} />
                <span>No animations loaded. Import a glTF file (bottom animation toolbar) or click on "New Anim" to create animation sequences and add tracks!</span>
              </div>
            )
          ) : (
            <div style={{ display: "flex", flexDirection: "column", gap: "12px", height: "100%", padding: "10px" }}>
              <div style={{ display: "flex", gap: "8px", alignItems: "center" }}>
                <button
                  className="primary"
                  onClick={() => {
                    const newBox = {
                      index: targetBoxes.length,
                      min: { x: -0.5, y: -0.5, z: -0.5 },
                      max: { x: 0.5, y: 0.5, z: 0.5 },
                      visible: true,
                    };
                    setTargetBoxes?.([...targetBoxes, newBox]);
                    setSelectedBoxIdx(targetBoxes.length);
                  }}
                  style={{ flex: 1, height: "30px", fontSize: "11px", display: "flex", justifyContent: "center", gap: "6px" }}
                >
                  <Plus size={12} />
                  <span>Add Target Box</span>
                </button>
                {targetBoxes.length > 0 && (
                  <button
                    className="secondary"
                    onClick={() => setShowLuaCode(true)}
                    style={{ height: "30px", fontSize: "11px", display: "flex", justifyContent: "center", gap: "6px" }}
                  >
                    <Crosshair size={12} />
                    <span>Show LUA Code</span>
                  </button>
                )}
              </div>

              <div style={{ flex: 1, overflowY: "auto", display: "flex", flexDirection: "column", gap: "6px" }}>
                {targetBoxes.length > 0 ? (
                  targetBoxes.map((box, idx) => {
                    const isSelected = selectedBoxIdx === idx;
                    const bounds = getShipBounds();
                    const halfSize = { x: bounds.size.x / 2, y: bounds.size.y / 2, z: bounds.size.z / 2 };
                    return (
                      <div
                        key={idx}
                        className={`list-item ${isSelected ? "active" : ""}`}
                        onClick={() => setSelectedBoxIdx(idx)}
                        style={{
                          padding: "10px 12px",
                          display: "flex",
                          flexDirection: "column",
                          gap: "6px",
                          alignItems: "stretch",
                          cursor: "pointer",
                          border: isSelected ? "1px solid var(--accent-cyan)" : "1px solid rgba(255,255,255,0.03)",
                          background: isSelected ? "rgba(22, 160, 255, 0.1)" : "rgba(0,0,0,0.15)",
                          borderRadius: "6px",
                        }}
                      >
                        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
                          <div style={{ display: "flex", alignItems: "center", gap: "6px" }}>
                            <input
                              type="checkbox"
                              checked={box.visible !== false}
                              onChange={(e) => {
                                e.stopPropagation();
                                const updated = targetBoxes.map((b, i) => i === idx ? { ...b, visible: e.target.checked } : b);
                                setTargetBoxes?.(updated);
                              }}
                              style={{ width: "auto", height: "auto", cursor: "pointer", margin: 0 }}
                            />
                            <span style={{ fontSize: "11px", fontWeight: "600", color: isSelected ? "var(--accent-cyan)" : "var(--text-primary)" }}>
                              targetBox{box.index}
                            </span>
                          </div>
                          <button
                            onClick={(e) => {
                              e.stopPropagation();
                              const updated = targetBoxes.filter((_, i) => i !== idx);
                              setTargetBoxes?.(updated);
                              setSelectedBoxIdx(null);
                            }}
                            style={{ background: "transparent", border: "none", color: "#ff8888", cursor: "pointer", padding: "2px" }}
                          >
                            <Trash2 size={12} />
                          </button>
                        </div>

                        {isSelected && (
                          <div style={{ display: "flex", flexDirection: "column", gap: "8px", marginTop: "4px", borderTop: "1px dashed rgba(255,255,255,0.08)", paddingTop: "8px" }}>
                            <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                              <span style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>Box Index</span>
                              <NumericInput
                                value={box.index}
                                onChange={(v) => {
                                  const val = parseInt(v) || 0;
                                  const updated = targetBoxes.map((b, i) => i === idx ? { ...b, index: val } : b);
                                  setTargetBoxes?.(updated);
                                }}
                                style={{ height: "24px", fontSize: "11px" }}
                              />
                            </div>

                            <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                              <span style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>Min Extents Factors (-1.0 to 1.0)</span>
                              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "6px" }}>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-danger)" }}>X</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.min.x}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, x: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, x: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-success)" }}>Y</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.min.y}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, y: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, y: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-blue)" }}>Z</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.min.z}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, z: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, min: { ...b.min, z: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                              </div>
                            </div>

                            <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                              <span style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>Max Extents Factors (-1.0 to 1.0)</span>
                              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "6px" }}>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-danger)" }}>X</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.max.x}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, x: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, x: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-success)" }}>Y</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.max.y}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, y: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, y: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                                <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                                  <span style={{ fontSize: "8px", color: "var(--accent-blue)" }}>Z</span>
                                  <NumericInput
                                    step="0.05"
                                    min="-1"
                                    max="1"
                                    value={box.max.z}
                                    onChange={(v) => {
                                      const val = parseFloat(v) || 0;
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, z: val } } : b);
                                      setTargetBoxes?.(updated);
                                    }}
                                    onWheel={(e) => handleNumericWheel(e, (v: string) => {
                                      const updated = targetBoxes.map((b, i) => i === idx ? { ...b, max: { ...b.max, z: parseFloat(v) } } : b);
                                      setTargetBoxes?.(updated);
                                    }, 0.05)}
                                    style={{ height: "24px", fontSize: "11px" }}
                                  />
                                </div>
                              </div>
                            </div>

                            <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "6px", background: "rgba(255,255,255,0.02)", padding: "6px", borderRadius: "4px", border: "1px solid rgba(255,255,255,0.04)" }}>
                              <div style={{ display: "flex", flexDirection: "column", alignItems: "center" }}>
                                <span style={{ fontSize: "7px", color: "var(--text-muted)", textTransform: "uppercase" }}>Width</span>
                                <span style={{ fontSize: "10px", fontWeight: "600", color: "var(--accent-cyan)", fontFamily: "var(--font-mono)" }}>
                                  {Math.abs((box.max.x - box.min.x) * halfSize.x * 2).toFixed(2)}m
                                </span>
                              </div>
                              <div style={{ display: "flex", flexDirection: "column", alignItems: "center" }}>
                                <span style={{ fontSize: "7px", color: "var(--text-muted)", textTransform: "uppercase" }}>Height</span>
                                <span style={{ fontSize: "10px", fontWeight: "600", color: "var(--accent-success)", fontFamily: "var(--font-mono)" }}>
                                  {Math.abs((box.max.y - box.min.y) * halfSize.y * 2).toFixed(2)}m
                                </span>
                              </div>
                              <div style={{ display: "flex", flexDirection: "column", alignItems: "center" }}>
                                <span style={{ fontSize: "7px", color: "var(--text-muted)", textTransform: "uppercase" }}>Length</span>
                                <span style={{ fontSize: "10px", fontWeight: "600", color: "var(--accent-blue)", fontFamily: "var(--font-mono)" }}>
                                  {Math.abs((box.max.z - box.min.z) * halfSize.z * 2).toFixed(2)}m
                                </span>
                              </div>
                            </div>
                          </div>
                        )}
                      </div>
                    );
                  })
                ) : (
                  <div style={{ padding: "40px 20px", color: "var(--text-muted)", fontSize: "13px", textAlign: "center", display: "flex", flexDirection: "column", gap: "10px", alignItems: "center" }}>
                    <Crosshair size={28} style={{ color: "var(--border-color)", opacity: 0.5 }} />
                    <span>No target boxes defined. Click "Add Target Box" to begin visual boxing editing for .ship LUA export.</span>
                  </div>
                )}
              </div>
            </div>
          )}
        </div>

        {showLuaCode && (
          <div style={{
            position: "fixed", top: 0, left: 0, right: 0, bottom: 0,
            background: "rgba(3, 8, 16, 0.8)", backdropFilter: "blur(6px)",
            display: "flex", justifyContent: "center", alignItems: "center", zIndex: 3000
          }}>
            <div style={{
              background: "rgba(10, 20, 35, 0.98)",
              border: "1px solid var(--accent-cyan)",
              borderRadius: "12px", width: "500px", padding: "20px",
              boxShadow: "0 8px 32px rgba(0,0,0,0.7)",
              display: "flex", flexDirection: "column", gap: "16px"
            }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", borderBottom: "1px solid var(--border-color)", paddingBottom: "10px" }}>
                <span style={{ fontWeight: "700", fontSize: "14px", color: "var(--accent-cyan)", textTransform: "uppercase" }}>
                  Target Box LUA Code Export
                </span>
                <button
                  onClick={() => setShowLuaCode(false)}
                  style={{ background: "transparent", border: "none", color: "var(--text-muted)", fontSize: "16px", cursor: "pointer" }}
                >✕</button>
              </div>
              
              <div style={{ display: "flex", flexDirection: "column", gap: "6px" }}>
                <span style={{ fontSize: "11px", color: "var(--text-secondary)" }}>
                  Add the following LUA code blocks to your ship's <code>.ship</code> script file:
                </span>
                <textarea
                  readOnly
                  value={targetBoxes.map(box => `setTargetBox(NewShipType, ${box.index}, ${box.min.x.toFixed(4)}, ${box.min.y.toFixed(4)}, ${box.min.z.toFixed(4)}, ${box.max.x.toFixed(4)}, ${box.max.y.toFixed(4)}, ${box.max.z.toFixed(4)})`).join("\n")}
                  style={{
                    width: "100%", height: "200px", background: "#050a12", color: "var(--accent-cyan)",
                    border: "1px solid var(--border-color)", borderRadius: "6px",
                    padding: "10px", fontFamily: "var(--font-mono)", fontSize: "11px",
                    resize: "none"
                  }}
                />
              </div>

              <div style={{ display: "flex", justifyContent: "flex-end", gap: "10px" }}>
                <button
                  className="secondary"
                  onClick={() => {
                    const code = targetBoxes.map(box => `setTargetBox(NewShipType, ${box.index}, ${box.min.x.toFixed(4)}, ${box.min.y.toFixed(4)}, ${box.min.z.toFixed(4)}, ${box.max.x.toFixed(4)}, ${box.max.y.toFixed(4)}, ${box.max.z.toFixed(4)})`).join("\n");
                    navigator.clipboard.writeText(code);
                    alert("LUA Code copied to clipboard successfully!");
                  }}
                  style={{ fontSize: "12px", padding: "6px 16px" }}
                >
                  Copy to Clipboard
                </button>
                <button
                  className="primary"
                  onClick={() => setShowLuaCode(false)}
                  style={{ fontSize: "12px", padding: "6px 16px" }}
                >
                  Close
                </button>
              </div>
            </div>
          </div>
        )}

        {activeTab === "hierarchy" && (
          <div style={{
            borderTop: "1px solid var(--border-color)",
            background: "rgba(10, 16, 27, 0.6)",
            padding: "12px 14px",
            flexShrink: 0,
            display: "flex",
            flexDirection: "column",
            gap: "8px",
            position: "relative"
          }}>
            {/* Draggable drag handle resize divider bar */}
            <div
              onMouseDown={handleDiagnosticsDragStart}
              style={{
                position: "absolute",
                top: "-4px",
                left: 0,
                right: 0,
                height: "8px",
                cursor: "ns-resize",
                zIndex: 100,
                background: "rgba(22, 160, 255, 0.04)",
                transition: "background 0.2s"
              }}
              className="diagnostics-handle-hover"
              title="Drag to resize Diagnostics panel"
            />

            <div style={{
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center"
            }}>
              <span style={{
                fontWeight: "700",
                textTransform: "uppercase",
                fontSize: "10px",
                letterSpacing: "0.08em",
                color: "var(--text-secondary)",
                display: "flex",
                alignItems: "center",
                gap: "6px"
              }}>
                <AlertTriangle size={12} style={{ color: "#ff9100" }} />
                Model Diagnostics
              </span>
              <span style={{
                fontSize: "10px",
                background: getWarnings().length > 0 ? "rgba(255, 145, 0, 0.15)" : "rgba(0, 230, 118, 0.15)",
                color: getWarnings().length > 0 ? "#ff9100" : "#00e676",
                padding: "2px 6px",
                borderRadius: "10px",
                fontWeight: "600"
              }}>
                {getWarnings().length} {getWarnings().length === 1 ? "Issue" : "Issues"}
              </span>
            </div>

            <div style={{
              height: `${diagnosticsHeight}px`,
              maxHeight: "450px",
              overflowY: "auto",
              display: "flex",
              flexDirection: "column",
              gap: "6px"
            }}>
              {getWarnings().length > 0 ? (
                getWarnings().map((warn, idx) => (
                  <div
                    key={idx}
                    style={{
                      display: "flex",
                      gap: "8px",
                      alignItems: "flex-start",
                      padding: "8px 10px",
                      background: warn.type === "warning" ? "rgba(255, 145, 0, 0.06)" : "rgba(22, 160, 255, 0.04)",
                      border: warn.type === "warning" ? "1px solid rgba(255, 145, 0, 0.15)" : "1px solid rgba(22, 160, 255, 0.12)",
                      borderRadius: "6px"
                    }}
                  >
                    {warn.type === "warning" ? (
                      <AlertTriangle size={14} style={{ color: "#ff9100", marginTop: "1px", flexShrink: 0 }} />
                    ) : (
                      <Info size={14} style={{ color: "var(--accent-cyan)", marginTop: "1px", flexShrink: 0 }} />
                    )}
                    <span style={{
                      fontSize: "11px",
                      color: warn.type === "warning" ? "#ffe0b2" : "var(--text-secondary)",
                      lineHeight: "1.4"
                    }}>
                      {warn.message}
                    </span>
                  </div>
                ))
              ) : (
                <div style={{
                  display: "flex",
                  gap: "8px",
                  alignItems: "center",
                  padding: "6px 10px",
                  color: "var(--text-muted)",
                  fontSize: "11px"
                }}>
                  <span style={{ color: "#00e676" }}>✓</span> All skeletal structures nominal.
                </div>
              )}
            </div>
          </div>
        )}
      </div>

      {isAddNodeOpen && (
        <div style={{
          position: "fixed",
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: "rgba(3, 8, 16, 0.75)",
          backdropFilter: "blur(6px)",
          display: "flex",
          justifyContent: "center",
          alignItems: "center",
          zIndex: 1000,
          padding: "20px"
        }}>
          <div style={{
            background: "rgba(10, 20, 35, 0.95)",
            border: "1px solid rgba(22, 160, 255, 0.35)",
            borderRadius: "12px",
            width: "100%",
            maxWidth: "460px",
            boxShadow: "0 8px 32px rgba(0,0,0,0.6)",
            display: "flex",
            flexDirection: "column",
            overflow: "hidden"
          }}>
            {/* Modal Header */}
            <div style={{
              background: "linear-gradient(135deg, rgba(22, 160, 255, 0.15), transparent)",
              padding: "16px 20px",
              borderBottom: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center"
            }}>
              <span style={{ fontWeight: "700", fontSize: "15px", color: "var(--accent-cyan)", letterSpacing: "0.03em" }}>
                Add New HOD Node
              </span>
              <button 
                onClick={() => setIsAddNodeOpen(false)}
                style={{ background: "transparent", border: "none", color: "var(--text-muted)", fontSize: "16px", cursor: "pointer" }}
              >
                ✕
              </button>
            </div>

            {/* Modal Body */}
            <div style={{ padding: "20px", display: "flex", flexDirection: "column", gap: "16px", overflowY: "auto", maxHeight: "450px" }}>
              {/* Node Type Selector */}
              <div>
                <label style={{ display: "block", fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "6px" }}>Node Type</label>
                <select 
                  value={addNodeType} 
                  onChange={(e: any) => setAddNodeType(e.target.value)}
                  style={{ height: "36px", fontSize: "13px" }}
                >
                  <option value="joint">Skeletal Joint Bone</option>
                  <option value="marker">Marker / Attachment Point</option>
                  <option value="mesh">Visual Mesh Hull / LOD Part</option>
                  <option value="navlight">NavLight (Pulsing Light Source)</option>
                  <option value="dockpath">Docking Path</option>
                  <option value="collision">Collision Hull Mesh</option>
                  <option value="weapon_template">Weapon Assembly (Toggleable)</option>
                  <option value="engine_nozzle">Engine Nozzle (Joint + Burn plume)</option>
                  <option value="repair_point_template">Repair Point Template</option>
                  <option value="capture_point_template">Capture Point Template</option>
                  <option value="hardpoint_template">Hardpoint Template</option>
                  <option value="salvage_point_template">Salvage Point Template</option>
                </select>
              </div>

              {/* Node Name */}
              {addNodeType === "collision" && (
                <div style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic", marginBottom: "8px" }}>
                  COL nodes are engine-defined: only one may exist, and it is always named Root.
                </div>
              )}
              {![
                "engine_nozzle",
                "repair_point_template",
                "capture_point_template",
                "salvage_point_template",
                "collision"
              ].includes(addNodeType) && (
                <div>
                  <label style={{ display: "block", fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "6px" }}>
                    {addNodeType === "weapon_template" || addNodeType === "turret_template" ? "Base Weapon Name" : "Node Name"}
                  </label>
                  <input
                    placeholder={addNodeType === "weapon_template" || addNodeType === "turret_template" ? "e.g. Laser_Turret" : "e.g. MyNewNode"}
                    value={newNodeName}
                    onChange={(e) => setNewNodeName(e.target.value)}
                    style={{ height: "36px", fontSize: "13px" }}
                  />
                  {(addNodeType === "weapon_template" || addNodeType === "turret_template") && (
                    <div style={{ fontSize: "10px", color: "var(--accent-cyan)", marginTop: "4px" }}>
                      ℹ️ This will auto-generate the complete compliant {addNodeType === "turret_template" ? "5-joint Turret" : "4-joint Weapon"} family!
                    </div>
                  )}
                </div>
              )}
              {["engine_nozzle", "repair_point_template", "capture_point_template", "salvage_point_template"].includes(addNodeType) && (
                <div style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic", marginBottom: "8px" }}>
                  Name will be auto-generated sequentially (e.g. {addNodeType === "engine_nozzle" ? "EngineNozzle0" : "Point0"}).
                </div>
              )}

              {/* Node Parent */}
              <div>
                <label style={{ display: "block", fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "6px" }}>Parent Node Attachment</label>
                <select
                  value={newNodeParent}
                  onChange={(e) => setNewNodeParent(e.target.value)}
                  style={{ height: "36px", fontSize: "13px" }}
                >
                  <option value="(None)">(None - Root Attachment)</option>
                  {model.joints.map((j) => (
                    <option key={j.name} value={j.name}>{j.name}</option>
                  ))}
                </select>
              </div>

              {/* Conditional NavLight Fields */}
              {addNodeType === "navlight" && (
                <div style={{ background: "rgba(225,225,225,0.02)", border: "1px solid var(--border-color)", padding: "14px", borderRadius: "8px", display: "flex", flexDirection: "column", gap: "12px" }}>
                  <div style={{ fontSize: "11px", fontWeight: "600", color: "var(--accent-cyan)", borderBottom: "1px solid var(--border-color)", paddingBottom: "6px" }}>
                    NavLight Configuration Presets
                  </div>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "10px" }}>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Section</label>
                      <input type="number" value={navLightSection} onChange={(e) => setNavLightSection(parseInt(e.target.value) || 0)} style={{ height: "28px", fontSize: "12px", width: "100%", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px" }} />
                    </div>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Size</label>
                      <input type="number" step="0.1" value={navLightSize} onChange={(e) => setNavLightSize(parseFloat(e.target.value) || 1.0)} style={{ height: "28px", fontSize: "12px", width: "100%", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px" }} />
                    </div>
                  </div>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "10px" }}>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Phase</label>
                      <input type="number" step="0.1" value={navLightPhase} onChange={(e) => setNavLightPhase(parseFloat(e.target.value) || 0.0)} style={{ height: "28px", fontSize: "12px", width: "100%", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px" }} />
                    </div>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Frequency</label>
                      <input type="number" step="0.1" value={navLightFreq} onChange={(e) => setNavLightFreq(parseFloat(e.target.value) || 1.0)} style={{ height: "28px", fontSize: "12px", width: "100%", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 8px" }} />
                    </div>
                  </div>
                  <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "10px", alignItems: "center" }}>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)" }}>Style Pattern</label>
                      <select value={navLightStyle} onChange={(e) => setNavLightStyle(e.target.value)} style={{ height: "28px", fontSize: "11px", width: "100%" }}>
                        <option value="Default">Default Solid</option>
                        <option value="Flicker">Flicker / Pulse</option>
                        <option value="Strobe">Strobe flash</option>
                      </select>
                    </div>
                    <div>
                      <label style={{ fontSize: "11px", color: "var(--text-secondary)", display: "block" }}>Color Picker</label>
                      <input type="color" value={navLightColor} onChange={(e) => setNavLightColor(e.target.value)} style={{ height: "28px", padding: 0, width: "100%", border: "none", background: "none", cursor: "pointer" }} />
                    </div>
                  </div>
                </div>
              )}
            </div>

            {/* Modal Footer */}
            <div style={{
              padding: "14px 20px",
              background: "rgba(3, 8, 16, 0.4)",
              borderTop: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "flex-end",
              gap: "10px"
            }}>
              <button
                className="secondary"
                onClick={() => setIsAddNodeOpen(false)}
                style={{ padding: "8px 16px", fontSize: "12px", borderRadius: "4px" }}
              >
                Cancel
              </button>
              <button
                className="primary"
                onClick={handleAddNode}
                disabled={!canSubmitAddNode}
                style={{
                  padding: "8px 16px",
                  fontSize: "12px",
                  borderRadius: "4px",
                  background: "var(--accent-cyan)",
                  color: "#000",
                  fontWeight: "700",
                  border: "none",
                  cursor: canSubmitAddNode ? "pointer" : "not-allowed",
                  opacity: canSubmitAddNode ? 1 : 0.5
                }}
              >
                Add Node
              </button>
            </div>
          </div>
        </div>
      )}

      {isAddMatOpen && (
        <div style={{
          position: "fixed",
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: "rgba(3, 8, 16, 0.75)",
          backdropFilter: "blur(6px)",
          display: "flex",
          justifyContent: "center",
          alignItems: "center",
          zIndex: 1000,
          padding: "20px"
        }}>
          <div style={{
            background: "rgba(10, 20, 35, 0.95)",
            border: "1px solid rgba(22, 160, 255, 0.35)",
            borderRadius: "12px",
            width: "100%",
            maxWidth: "400px",
            boxShadow: "0 8px 32px rgba(0,0,0,0.6)",
            display: "flex",
            flexDirection: "column",
            overflow: "hidden"
          }}>
            {/* Modal Header */}
            <div style={{
              background: "linear-gradient(135deg, rgba(22, 160, 255, 0.15), transparent)",
              padding: "16px 20px",
              borderBottom: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center"
            }}>
              <span style={{ fontWeight: "700", fontSize: "15px", color: "var(--accent-cyan)", letterSpacing: "0.03em" }}>
                Add New Material
              </span>
              <button 
                onClick={() => setIsAddMatOpen(false)}
                style={{ background: "transparent", border: "none", color: "var(--text-muted)", fontSize: "16px", cursor: "pointer" }}
              >
                ✕
              </button>
            </div>

            {/* Modal Body */}
            <div style={{ padding: "20px", display: "flex", flexDirection: "column", gap: "16px" }}>
              {/* Material Name */}
              <div>
                <label style={{ display: "block", fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "6px" }}>Material Name</label>
                <input
                  placeholder="e.g. sob_ship_hull"
                  value={newMatName}
                  onChange={(e) => setNewMatName(e.target.value)}
                  style={{ height: "36px", fontSize: "13px" }}
                />
              </div>

              {/* Shader / Pipeline */}
              <div>
                <label style={{ display: "block", fontSize: "11px", fontWeight: "600", color: "var(--text-muted)", textTransform: "uppercase", marginBottom: "6px" }}>Shader Pipeline</label>
                <select
                  value={newMatShader}
                  onChange={(e) => setNewMatShader(e.target.value)}
                  style={{ height: "36px", fontSize: "13px" }}
                >
                  {pipelines.map((p) => (
                    <option key={p} value={p}>{p}</option>
                  ))}
                </select>
                <div style={{ fontSize: "10px", color: "var(--text-muted)", marginTop: "4px" }}>
                  ℹ️ Texture mappings will default to (None).
                </div>
              </div>
            </div>

            {/* Modal Footer */}
            <div style={{
              padding: "14px 20px",
              background: "rgba(3, 8, 16, 0.4)",
              borderTop: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "flex-end",
              gap: "10px"
            }}>
              <button
                className="secondary"
                onClick={() => setIsAddMatOpen(false)}
                style={{ padding: "8px 16px", fontSize: "12px", borderRadius: "4px" }}
              >
                Cancel
              </button>
              <button
                className="primary"
                onClick={handleAddMaterial}
                disabled={!newMatName.trim()}
                style={{
                  padding: "8px 16px",
                  fontSize: "12px",
                  borderRadius: "4px",
                  background: "var(--accent-cyan)",
                  color: "#000",
                  fontWeight: "700",
                  border: "none",
                  cursor: newMatName.trim() ? "pointer" : "not-allowed",
                  opacity: newMatName.trim() ? 1 : 0.5
                }}
              >
                Add Material
              </button>
            </div>
          </div>
        </div>
      )}

      {contextMenu && createPortal(
        <>
          <div 
            style={{ position: 'fixed', top: 0, left: 0, width: '100vw', height: '100vh', zIndex: 9999 }} 
            onClick={() => setContextMenu(null)}
            onContextMenu={(e) => { e.preventDefault(); setContextMenu(null); }}
          />
          <div 
            style={{
              position: 'fixed',
              top: contextMenu.y,
              left: contextMenu.x,
              zIndex: 10000,
              background: 'var(--bg-panel)',
              border: '1px solid rgba(255,255,255,0.1)',
              borderRadius: '4px',
              boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
              padding: '4px 0',
              minWidth: '150px',
              display: 'flex',
              flexDirection: 'column'
            }}
          >
            {contextMenu.type === "texture_group" ? (
              <>
                <div 
                  className="list-item"
                  style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: 'var(--text-primary)' }}
                  onClick={() => {
                     handleRenameNode(contextMenu.name, contextMenu.type);
                     setContextMenu(null);
                  }}
                >
                  ✏️ Rename Group
                </div>
                <div 
                  className="list-item"
                  style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: '#ff1744', display: 'flex', alignItems: 'center' }}
                  onClick={(e) => {
                    e.stopPropagation();
                    if (!model) return;
                    
                    const groups = parseTextureGroups(model.textures || []);
                    const group = groups.find(g => g.baseName === contextMenu.name);
                    if (!group) return;

                    if (!window.confirm(`Are you sure you want to delete the texture group '${group.baseName}' and all its ${group.textures.length} sub-textures?`)) return;

                    const groupTextureNames = new Set(group.textures.map(t => t.originalName));
                    const updatedTextures = model.textures.filter(t => !groupTextureNames.has(t.name));
                    
                    const updatedMaterials = model.materials?.map(m => {
                       let maps = [...(m.texture_maps || [])];
                       let changed = false;
                       maps.forEach((name, idx) => {
                          if (groupTextureNames.has(name)) {
                             maps[idx] = "";
                             changed = true;
                          }
                       });
                       return changed ? { ...m, texture_maps: maps } : m;
                    }) || [];

                    onModelChange?.({ ...model, textures: updatedTextures, materials: updatedMaterials, textures_modified: true });
                    if (selectedNode?.name === contextMenu.name) {
                      setSelectedNode(null);
                    }
                    setContextMenu(null);
                  }}
                >
                  <Trash2 size={12} style={{ marginRight: 6 }} />
                  Delete Texture Group
                </div>
              </>
            ) : contextMenu.type === "texture" ? (
              <>
                <div 
                  className="list-item"
                  style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: 'var(--text-primary)', display: 'flex', alignItems: 'center' }}
                  onClick={(e) => {
                    e.stopPropagation();
                    if (!model) return;
                    const updatedTextures = model.textures.map(t => {
                      if (t.name === contextMenu.name) {
                        return { ...t, legacy_storage_y_flipped: !t.legacy_storage_y_flipped };
                      }
                      return t;
                    });
                    onModelChange?.({ ...model, textures: updatedTextures, textures_modified: true });
                    setContextMenu(null);
                  }}
                >
                  <FlipVertical size={12} style={{ marginRight: 6 }} />
                  Toggle Y-Flip
                </div>
                <div 
                  className="list-item"
                  style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: '#ff1744', display: 'flex', alignItems: 'center' }}
                  onClick={(e) => {
                    e.stopPropagation();
                    if (!model) return;
                    const updatedTextures = model.textures.filter(t => t.name !== contextMenu.name);
                    onModelChange?.({ ...model, textures: updatedTextures, textures_modified: true });
                    setContextMenu(null);
                  }}
                >
                  <Trash2 size={12} style={{ marginRight: 6 }} />
                  Remove Texture
                </div>
              </>
            ) : (
              <>
                {contextMenu.type === "weapon_group" && (
                  <div
                    className="list-item"
                    style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: 'var(--text-primary)' }}
                    onClick={() => {
                       handleAddMuzzleToGroup(contextMenu.name);
                       setContextMenu(null);
                    }}
                  >
                    + Add Muzzle
                  </div>
                )}
                {contextMenu.type === "weapon_group" && !/Slave\d*$/i.test(contextMenu.name) && (
                  <div
                    className="list-item"
                    style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: 'var(--text-primary)' }}
                    onClick={() => {
                       handleAddSlaveToGroup(contextMenu.name);
                       setContextMenu(null);
                    }}
                  >
                    + Add Slave Barrel
                  </div>
                )}
                {contextMenu.type !== "collision" && (
                  <div 
                    className="list-item"
                    style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: 'var(--text-primary)' }}
                    onClick={() => {
                       handleRenameNode(contextMenu.name, contextMenu.type);
                       setContextMenu(null);
                    }}
                  >
                    ✏️ Rename
                  </div>
                )}
                {isNodeDeletable(contextMenu.name, contextMenu.type) && (
                  <div 
                    className="list-item"
                    style={{ padding: '8px 12px', cursor: 'pointer', fontSize: '12px', color: '#ff1744' }}
                    onClick={() => {
                       const confirmMsg = contextMenu.type === "weapon_group" 
                         ? `Are you sure you want to delete the entire weapon/turret family "${contextMenu.name}"? This will remove all of its component joints safely.` 
                         : `Are you sure you want to delete "${contextMenu.name}"?`;
                       if (window.confirm(confirmMsg)) {
                         handleDeleteNode(contextMenu.name, contextMenu.type);
                       }
                       setContextMenu(null);
                    }}
                  >
                    ✕ Delete
                  </div>
                )}
              </>
            )}
          </div>
        </>,
        document.body
      )}

      {/* ── Create Animation Modal ── */}
      {isCreateAnimOpen && (
        <div
          style={{
            position: "fixed", top: 0, left: 0, right: 0, bottom: 0,
            background: "rgba(3,8,16,0.75)", backdropFilter: "blur(6px)",
            display: "flex", justifyContent: "center", alignItems: "center", zIndex: 2000,
          }}
        >
          <div
            style={{
              background: "rgba(10,20,35,0.97)",
              border: "1px solid rgba(22,160,255,0.35)",
              borderRadius: "12px", width: "380px",
              boxShadow: "0 8px 32px rgba(0,0,0,0.7)",
              display: "flex", flexDirection: "column", overflow: "hidden",
            }}
          >
            <div
              style={{
                background: "linear-gradient(135deg, rgba(22,160,255,0.15), transparent)",
                padding: "14px 18px",
                borderBottom: "1px solid var(--border-color)",
                display: "flex", justifyContent: "space-between", alignItems: "center",
              }}
            >
              <span style={{ fontWeight: "700", fontSize: "14px", color: "var(--accent-cyan)" }}>
                Create New Animation
              </span>
              <button
                onClick={() => setIsCreateAnimOpen(false)}
                style={{ background: "transparent", border: "none", color: "var(--text-muted)", fontSize: "16px", cursor: "pointer" }}
              >✕</button>
            </div>
            <div style={{ padding: "18px", display: "flex", flexDirection: "column", gap: "14px" }}>
              <div>
                <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", fontWeight: "600", marginBottom: "6px", textTransform: "uppercase" }}>Animation Name</label>
                <input
                  autoFocus
                  type="text"
                  value={newAnimName}
                  onChange={(e) => setNewAnimName(e.target.value)}
                  onKeyDown={(e) => e.key === "Enter" && handleCreateAnimation()}
                  placeholder="e.g., Open_Bay"
                  style={{ width: "100%", background: "rgba(0,0,0,0.4)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "4px", padding: "8px 10px", color: "var(--text-primary)", fontSize: "12px", outline: "none" }}
                />
              </div>
              <div>
                <label style={{ display: "block", fontSize: "11px", color: "var(--text-muted)", fontWeight: "600", marginBottom: "6px", textTransform: "uppercase" }}>Duration (seconds)</label>
                <input
                  type="number"
                  step="0.1" min="0.1"
                  value={newAnimDuration}
                  onChange={(e) => setNewAnimDuration(parseFloat(e.target.value) || 0)}
                  onKeyDown={(e) => e.key === "Enter" && handleCreateAnimation()}
                  style={{ width: "100%", background: "rgba(0,0,0,0.4)", border: "1px solid rgba(255,255,255,0.1)", borderRadius: "4px", padding: "8px 10px", color: "var(--text-primary)", fontSize: "12px", outline: "none" }}
                />
              </div>
            </div>
            <div style={{ display: "flex", justifyContent: "flex-end", gap: "10px", padding: "14px 18px", background: "rgba(0,0,0,0.2)", borderTop: "1px solid rgba(255,255,255,0.05)" }}>
              <button
                onClick={() => setIsCreateAnimOpen(false)}
                style={{ background: "transparent", border: "1px solid rgba(255,255,255,0.2)", color: "var(--text-secondary)", borderRadius: "6px", padding: "6px 14px", fontSize: "12px", fontWeight: "600", cursor: "pointer" }}
              >
                Cancel
              </button>
              <button
                onClick={handleCreateAnimation}
                style={{ background: "#16a0ff", border: "none", color: "#fff", borderRadius: "6px", padding: "6px 16px", fontSize: "12px", fontWeight: "700", cursor: "pointer", opacity: newAnimName.trim() ? 1 : 0.5 }}
                disabled={!newAnimName.trim()}
              >
                Create
              </button>
            </div>
          </div>
        </div>
      )}

    </div>
  );
};
