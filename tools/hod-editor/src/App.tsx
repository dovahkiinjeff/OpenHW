import { useState, useEffect, useMemo, useRef } from "react";
import { invoke } from "./bridge";
import { Viewport, HODModel, Vector3D } from "./components/Viewport";
import { Toolbar } from "./components/Toolbar";
import { HierarchyTree } from "./components/HierarchyTree";
import { Inspector } from "./components/Inspector";
import { AnimationDock } from "./components/AnimationDock";
import { GameTextEditor } from "./components/GameTextEditor";
import { ModPicker } from "./components/ModPicker";
import { GameModels } from "./components/GameModels";

import { SettingsModal } from "./components/SettingsModal";
import { updateDynamicShaderSlots } from "./texture_utils";
import { CoordinateSpace, matrixRotationToHodRadians } from "./transform_utils";
import { Info, AlertTriangle, FolderOpen, FilePlus } from "lucide-react";
import { ClipboardMount, Mount, ShipBindings, jointRoot, owner, place, preview, rigFor, snapshot } from "./subsystems";
import "./App.css";

/** Recursively sanitize all numeric values in the model:
 *  - NaN/Infinity → 0 (JSON.stringify converts NaN to null, crashing Rust serde)
 *  - null/undefined properties → DELETED (omitted from JSON, Rust reads as Option::None)
 *  Do NOT convert null→0 for object-typed fields — that breaks Option<Vector3> etc. */
function sanitizeModel<T>(obj: T): T {
  if (typeof obj === "number") {
    return (Number.isFinite(obj) ? obj : 0) as T;
  }
  if (obj === null || obj === undefined) {
    return undefined as T; // Omit from parent object → JSON missing → Rust Option::None
  }
  if (typeof obj !== "object") {
    return obj;
  }
  if (Array.isArray(obj)) {
    return obj.map(sanitizeModel).filter(v => v !== undefined) as T;
  }
  const cleaned: Record<string, any> = {};
  for (const [k, v] of Object.entries(obj)) {
    const sv = sanitizeModel(v);
    if (sv !== undefined) {
      cleaned[k] = sv;
    }
  }
  return cleaned as T;
}

function App() {
  const [model, setModel] = useState<HODModel | null>(null);
  const [filePath, setFilePath] = useState("");
  const [modPicker,setModPicker]=useState(false);
  const [textEditor,setTextEditor]=useState<string|null>(null);
  const [selectedNode, setSelectedNode] = useState<{ type: string; name: string } | null>(null);
  const [transformMode, setTransformMode] = useState<"translate" | "rotate" | "scale">("translate");
  const [coordinateSpace, setCoordinateSpace] = useState<CoordinateSpace>("local");
  const [isSaving] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [isSettingsModalOpen, setIsSettingsModalOpen] = useState(false);
  const [isDirty, setIsDirty] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [selectedAnimIdx, setSelectedAnimIdx] = useState(0);
  const [targetBoxes, setTargetBoxes] = useState<{ index: number; min: { x: number; y: number; z: number }; max: { x: number; y: number; z: number }; visible?: boolean }[]>([]);

  // Lifted animation playback state (shared between AnimationDock and Viewport)
  const [isPlaying, setIsPlaying] = useState(false);
  const [currentTime, setCurrentTime] = useState(0);
  const [loopPlayback, setLoopPlayback] = useState(false);
  const [playbackSpeed, setPlaybackSpeed] = useState(1.0);
  const [activeHierarchyTab, setActiveHierarchyTab] = useState("hierarchy");

  // Workspace resizing widths
  const [sidebarWidth, setSidebarWidth] = useState(364);
  const [inspectorWidth, setInspectorWidth] = useState(320);

  const handleSidebarDragStart = (e: React.MouseEvent) => {
    e.preventDefault();
    const handleMouseMove = (moveEvent: MouseEvent) => {
      const newWidth = Math.max(180, Math.min(500, moveEvent.clientX));
      setSidebarWidth(newWidth);
    };
    const handleMouseUp = () => {
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
    };
    window.addEventListener("mousemove", handleMouseMove);
    window.addEventListener("mouseup", handleMouseUp);
  };

  const handleInspectorDragStart = (e: React.MouseEvent) => {
    e.preventDefault();
    const handleMouseMove = (moveEvent: MouseEvent) => {
      const newWidth = Math.max(200, Math.min(600, window.innerWidth - moveEvent.clientX));
      setInspectorWidth(newWidth);
    };
    const handleMouseUp = () => {
      window.removeEventListener("mousemove", handleMouseMove);
      window.removeEventListener("mouseup", handleMouseUp);
    };
    window.addEventListener("mousemove", handleMouseMove);
    window.addEventListener("mouseup", handleMouseUp);
  };

  const updateModel = (nextModel: HODModel) => {
    setModel(nextModel);
    setIsDirty(true);
  };
  const [statusMsg, setStatusMsg] = useState("System Ready");
  const [visibleMeshes, setVisibleMeshes] = useState<Record<string, boolean>>({});
  const [isMigrationModalOpen, setIsMigrationModalOpen] = useState(false);
  const [migrationMappings, setMigrationMappings] = useState<Record<string, "joint" | "marker" | "weapon" | "collision" | "navlight" | "engine_burn" | "engine_glow">>({});
  const [migrationCoordinateClones, setMigrationCoordinateClones] = useState<Record<string, string>>({});

  // Big Data Settings and Toggling states
  const [showBoneLines, setShowBoneLines] = useState(true);
  const [renderMode, setRenderMode] = useState<"untextured" | "textured" | "shaded" | "wireframe" | "shaded_team" | "textured_team">("shaded");
  const [teamColor, setTeamColor] = useState("#4278a3");
  const [stripeColor, setStripeColor] = useState("#e5d94c");
  const [keeperTxtPaths, setKeeperTxtPaths] = useState<string[]>([]);

  // Load shader directories from config file on startup
  const [library, setLibrary] = useState<"models" | "subsystems" | null>(null);
  const [subsystemShip, setSubsystemShip] = useState("");
  const [mounts,setMounts]=useState<Mount[]>([]);
  const [picker,setPicker]=useState(false);
  const [catalog,setCatalog]=useState<string[]>([]);
  const [filter,setFilter]=useState("");
  const [quickBusy,setQuickBusy]=useState(false);
  const pendingPaste=useRef(false);
  const clipboard=useRef<ClipboardMount | null>(null);
  const viewportModel=useMemo(()=>preview(model,mounts),[model,mounts]);
  const showPicker=async()=>{
    if(!model) return;
    try {setCatalog(await invoke<string[]>("list_subsystems"));setFilter("");setPicker(true);}
    catch(e){setErrorMsg(String(e));}
  };
  const addMount=(asset:string,copy?:ClipboardMount)=>{
    if(!model)return;
    const selected=selectedNode?.name;
    const previous=copy?.root.parent_name;
    const parent=copy && previous && model.joints.some(j=>j.name===previous) ? previous :
      selected && !owner(mounts,selected) && model.joints.some(j=>j.name===selected) ? selected : jointRoot(model);
    if(!parent)throw Error("Create a hull joint first");
    const next=place(model,mounts,asset,parent,copy);
    setMounts([...mounts,next.mount]); updateModel(next.model);
    setSelectedNode({type:"joint",name:next.mount.slot});setErrorMsg(null);
    setStatusMsg("Added "+asset+" at "+next.mount.slot);
  };
  const chooseAsset=async(asset:string)=>{
    setQuickBusy(true);
    try {await rigFor(asset);addMount(asset);setPicker(false);}
    catch(e){setErrorMsg(String(e));}finally{setQuickBusy(false);}
  };
  const editMount=(command:string,chosen?:Mount)=>{
    if(!model)return false;
    const item=chosen ?? owner(mounts,selectedNode?.name);
    try{
      if(command==="paste"){if(!clipboard.current)return false;if(!subsystemShip){pendingPaste.current=true;setLibrary("subsystems");return true;}addMount(clipboard.current.asset,clipboard.current);}
      else if(item){const copy=snapshot(model,item);if(command==="copy"){clipboard.current=copy;setStatusMsg("Copied "+item.asset);}
        else addMount(item.asset,copy);}
      else return false;
    }catch(e){setErrorMsg(String(e));}
    return true;
  };
  useEffect(()=>{
    const key=(e:KeyboardEvent)=>{
      if(!(e.ctrlKey||e.metaKey)||e.altKey||isLoading||quickBusy||
        (e.target instanceof HTMLElement && (e.target.isContentEditable || e.target.closest("input,textarea,select"))))return;
      const op=({c:"copy",v:"paste",d:"duplicate"} as Record<string,string>)[e.key.toLowerCase()];
      if(op && editMount(op)){e.preventDefault();e.stopPropagation();}
    };
    window.addEventListener("keydown",key,true);return()=>window.removeEventListener("keydown",key,true);
  });
  const [sourcePath, setSourcePath] = useState("");
  const returnToGame = () => {
    if (isLoading || isSaving) return;
    if (!isDirty || window.confirm("Discard unsaved HOD changes and return to the Fleet Workshop?")) {
      invoke("close_workshop").catch(e => setErrorMsg(String(e)));
    }
  };
  useEffect(() => {
    invoke<{ shader_directories: string[] }>("load_shader_config")
      .then(async (config) => {
        if (config.shader_directories.length > 0) {
          setKeeperTxtPaths(config.shader_directories);
          try {
            const dynamicShaders = await invoke<{name: string, slots: string[]}[]>("get_dynamic_shader_slots", { keeperPaths: config.shader_directories });
            updateDynamicShaderSlots(dynamicShaders);
          } catch (e) {
            console.error("Failed to load dynamic shader slots", e);
          }
        }
      })
      .catch((e) => console.error("Failed to load shader config:", e))
      ;
  }, []);

  // Log application startup
  useEffect(() => {
    invoke("log_event", { level: "INFO", message: "HOD Remastered Editor React Frontend initialized." }).catch(console.error);
  }, []);

  // Global Escape key listener to clear selection
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        setSelectedNode(null);
      }
    };
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, []);

  // Log selected node changes
  useEffect(() => {
    if (selectedNode) {
      invoke("log_event", {
        level: "INFO",
        message: `User selected ${selectedNode.type} node: "${selectedNode.name}"`,
      }).catch(console.error);
    }
  }, [selectedNode]);

  // Open native OS file selector dialog via Rust host
  const selectAndLoadFile = async () => {
    if (isDirty) {
      const confirmDiscard = window.confirm("You have unsaved changes in the current HOD model. Are you sure you want to load another HOD and discard your changes?");
      if (!confirmDiscard) return;
    }
    try {
      const selectedPath = await invoke<string | null>("select_hod_file");
      if (selectedPath) {
        loadHODFile(selectedPath);
      }
    } catch (e: any) {
      const err = "Failed to open file dialog: " + e.toString();
      invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
      setErrorMsg(err);
      setStatusMsg("File browser error");
    }
  };

  // Open native OS file selector dialog for DAE
  const selectAndImportDAE = async () => {
    if (isDirty) {
      const confirmDiscard = window.confirm("You have unsaved changes. Are you sure you want to load another model and discard your changes?");
      if (!confirmDiscard) return;
    }
    try {
      const selectedPath = await invoke<string | null>("select_dae_file");
      if (selectedPath) {
        setStatusMsg("Parsing DAE file...");
        setIsLoading(true);
        setErrorMsg(null);
        try {
          const parsedModel = await invoke<HODModel>("import_dae_file", { path: selectedPath });
          resetUIState();
          setSourcePath("");
          resetUIState();setModel(parsedModel);
          setFilePath(""); // No original path, it's newly imported
          setIsDirty(true);
          setSelectedNode(null);
          setStatusMsg("Successfully imported DAE as HOD 2.0");
        } catch (e: any) {
          const err = "Failed to import DAE: " + e.toString();
          invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
          setErrorMsg(err);
          setStatusMsg("Import error");
        } finally {
          setIsLoading(false);
        }
      }
    } catch (e: any) {
      const err = "Failed to open DAE file dialog: " + e.toString();
      invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
      setErrorMsg(err);
      setStatusMsg("File browser error");
    }
  };



  const resetUIState = () => {
    setModel(null);
    setSubsystemShip(""); setMounts([]);setPicker(false);
    setSelectedNode(null);
    setSelectedAnimIdx(0);
    setTargetBoxes([]);
    setCurrentTime(0);
    setIsPlaying(false);
    setVisibleMeshes({});
    setMigrationMappings({});
    setErrorMsg(null);
  };

  // Load targeted HOD file from absolute filesystem path
  const loadHODFile = async (path: string) => {
    if (!path.trim()) return;
    setIsLoading(true);
    setStatusMsg("Loading HOD...");
    
    // Give the browser 100ms of breathing room to paint the gorgeous loading overlay!
    setTimeout(async () => {
      try {
        const parsedModel = await invoke<HODModel>("load_hod", { 
          filePath: path, 
          keeperPath: keeperTxtPaths.length > 0 ? keeperTxtPaths[0] : null 
        });
        
        // Ensure collections are arrays to prevent undefined issues
        const processedModel: HODModel = {
          ...parsedModel,
          nav_lights: parsedModel.nav_lights || [],
          engine_burns: parsedModel.engine_burns || [],
          engine_glows: parsedModel.engine_glows || [],
          engine_shapes: parsedModel.engine_shapes || [],
          collision_meshes: parsedModel.collision_meshes || [],
          dockpaths: parsedModel.dockpaths || [],
        };

        const bindings=await invoke<ShipBindings>("ship_mounts",{source:path});
        const rigs=await Promise.allSettled([...new Set(bindings.mounts.map(m=>m.asset))].map(rigFor));
        resetUIState();setMounts(bindings.mounts);setSubsystemShip(bindings.template);
        if(rigs.some(r=>r.status==="rejected"))setErrorMsg("Some mounted subsystem models could not be loaded.");
        setModel(processedModel);
        setSourcePath(path);
        setIsDirty(false);
        setSelectedNode(null);
        setSelectedAnimIdx(0);
        setFilePath(path.startsWith("game://") ? "" : path);
        
        // Initialize only LOD 0 as visible by default, hiding lower-poly LOD 1, 2, 3 overlays
        const initialVisibility: Record<string, boolean> = {};
        processedModel.meshes.forEach((m) => {
          initialVisibility[`${m.name}_lod_${m.lod}`] = m.lod === 0;
        });
        const glowGroups = new Map<string, typeof processedModel.engine_glows>();
        processedModel.engine_glows.forEach((g) => {
          const baseName = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          if (!glowGroups.has(baseName)) glowGroups.set(baseName, []);
          glowGroups.get(baseName)!.push(g);
        });
        glowGroups.forEach((glows) => {
          const sortedGlows = [...glows].sort((a, b) => a.lod - b.lod);
          sortedGlows.forEach((g, idx) => {
            initialVisibility[`engine_glow:${g.name}`] = idx === 0;
          });
        });
        setVisibleMeshes(initialVisibility);

        setStatusMsg(`HOD ${processedModel.name} loaded successfully | Meshes: ${processedModel.meshes.length} | Joints: ${processedModel.joints.length} | Markers: ${processedModel.markers.length}`);
      } catch (e: any) {
        const err = `Frontend failed to load HOD from path ${path}: ${e.toString()}`;
        invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
        setErrorMsg(e.toString());
        setStatusMsg("Error loading file");
      } finally {
        setIsLoading(false);
      }
    }, 100);
  };



  const handleCreateNewHOD = () => {
    if (isDirty) {
      const confirmDiscard = window.confirm("You have unsaved changes in the current HOD model. Are you sure you want to create a new HOD and discard your changes?");
      if (!confirmDiscard) return;
    }
    invoke("log_event", { level: "INFO", message: "Creating fresh, clean HOD 2.0 template..." }).catch(console.error);
    resetUIState();
    
    const newModel: HODModel = {
      version: 512, // HOD 2.0 (0x200)
      name: "New_Model",
      is_v2: true,
      joints: [
        {
          name: "Root",
          parent_name: undefined,
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [0, 0, 0, 1]
            ]
          }
        }
      ],
      markers: [],
      meshes: [],
      nav_lights: [],
      materials: [],
      textures: [],
      collision_meshes: [],
      dockpaths: [],
      engine_burns: [],
      engine_glows: [],
      engine_shapes: [],
      animations: [],
      textures_modified: false
    };
    
    // Slight delay to ensure React unmounts previous Viewport and clears WebGL context
    setTimeout(() => {
      setSourcePath("");
      setModel(newModel);
      setFilePath("");
      setIsDirty(true);
      setStatusMsg("New HOD model successfully initialized!");
    }, 50);
  };

  const handleToggleVisibility = (meshKey: string) => {
    setVisibleMeshes((prev) => {
      const nextVisibility = prev[meshKey] === false;
      const updated = { ...prev, [meshKey]: nextVisibility };

      const mesh = model?.meshes.find((m) => `${m.name}_lod_${m.lod}` === meshKey);
      if (mesh && nextVisibility) {
        const baseName = mesh.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
        model?.meshes.forEach((m) => {
          const mBase = m.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          const lodKey = `${m.name}_lod_${m.lod}`;
          if (mBase === baseName && lodKey !== meshKey) {
            updated[lodKey] = false;
          }
        });
      }

      if (meshKey.startsWith("engine_glow:")) {
        const glowName = meshKey.substring("engine_glow:".length);
        const glow = model?.engine_glows.find((g) => g.name === glowName);
        if (glow && nextVisibility) {
          const baseName = glow.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
          model?.engine_glows.forEach((g) => {
            const gBase = g.name.replace(/_lod_\d+$/i, "").replace(/_LOD\d+$/i, "");
            const lodKey = `engine_glow:${g.name}`;
            if (gBase === baseName && lodKey !== meshKey) {
              updated[lodKey] = false;
            }
          });
        }
      }

      return updated;
    });
  };

  const handleExecuteMigration = () => {
    if (!model) return;
    
    let updatedJoints = [...model.joints];
    let updatedMarkers = [...model.markers];
    let updatedCollisions = [...model.collision_meshes];
    let updatedNavlights = [...model.nav_lights];
    let updatedEngineBurns = [...model.engine_burns];
    let updatedEngineGlows = [...model.engine_glows];
    
    // Process reclassifications
    Object.entries(migrationMappings).forEach(([jointName, targetType]) => {
      // Find the original joint being mapped
      let originalJoint = model.joints.find(j => j.name === jointName);
      
      // If the user requested to clone coordinates from another joint, overwrite the transform temporarily
      const cloneTarget = migrationCoordinateClones[jointName];
      if (cloneTarget && cloneTarget !== "Self" && originalJoint) {
        const sourceJoint = model.joints.find(j => j.name === cloneTarget);
        if (sourceJoint) {
           // We clone the local_transform and position from the source joint
           originalJoint = { 
             ...originalJoint, 
             local_transform: JSON.parse(JSON.stringify(sourceJoint.local_transform)),
             position: sourceJoint.position ? { ...sourceJoint.position } : undefined,
           };
        }
      }

      if (targetType === "marker") {
        // Convert to marker:
        if (originalJoint) {
          // Decompose its transform for position:
          const m = originalJoint.local_transform.m;
          const pos = { x: m[3][0], y: m[3][1], z: m[3][2] };
          
          updatedMarkers.push({
            name: jointName,
            parent_joint: originalJoint.parent_name || "Root",
            position: pos,
            rotation: {
              m: [
                [1, 0, 0, 0],
                [0, 1, 0, 0],
                [0, 0, 1, 0],
                [0, 0, 0, 1]
              ]
            }
          });
          // Remove from joints list:
          updatedJoints = updatedJoints.filter(j => j.name !== jointName);
        }
      } else if (targetType === "weapon") {
        // Convert to standard 4-joint weapon assembly:
        if (originalJoint) {
          const m = originalJoint.local_transform.m;
          const parent = originalJoint.parent_name || "Root";
          const base = jointName.startsWith("Weapon_") || jointName.startsWith("weapon_") ? jointName : `Weapon_${jointName}`;
          
          // Add weapon joints
          updatedJoints.push({
            name: `${base}_Position`,
            parent_name: parent,
            local_transform: { m }
          }, {
            name: `${base}_Direction`,
            parent_name: `${base}_Position`,
            local_transform: {
              m: [
                [1, 0, 0, 0],
                [0, 1, 0, 0],
                [0, 0, 1, 0],
                [0, 0, 0, 1]
              ]
            }
          }, {
            name: `${base}_Muzzle`,
            parent_name: `${base}_Direction`,
            local_transform: {
              m: [
                [1, 0, 0, 0],
                [0, 1, 0, 0],
                [0, 0, 1, 0],
                [0, 0, 0, 1.5]
              ]
            }
          }, {
            name: `${base}_Rest`,
            parent_name: `${base}_Position`,
            local_transform: {
              m: [
                [1, 0, 0, 0],
                [0, 1, 0, 0],
                [0, 0, 1, 0],
                [0, 0, 0, 1.0]
              ]
            }
          });
          // Remove original joint:
          updatedJoints = updatedJoints.filter(j => j.name !== jointName);
        }
      } else if (targetType === "collision") {
        // Convert to collision:
        if (originalJoint && updatedCollisions.length === 0) {
          const m = originalJoint.local_transform.m;
          const pos = { x: m[3][0], y: m[3][1], z: m[3][2] };
          
          updatedCollisions.push({
            name: "Root",
            min_extents: { x: -3, y: -3, z: -3 },
            max_extents: { x: 3, y: 3, z: 3 },
            center: pos,
            radius: 3.0,
            mesh: {
              name: "CollisionMesh",
              parent_name: "Root",
              lod: 0,
              parts: []
            }
          });
        }
      } else if (targetType === "navlight") {
        if (originalJoint) {
          // If cloned coordinates were requested, we must actually modify the joint in updatedJoints
          // because Navlights derive position from the joint directly.
          if (cloneTarget && cloneTarget !== "Self") {
             const index = updatedJoints.findIndex(j => j.name === jointName);
             if (index !== -1) {
                updatedJoints[index].local_transform = JSON.parse(JSON.stringify(originalJoint.local_transform));
                if (originalJoint.position) updatedJoints[index].position = { ...originalJoint.position };
             }
          }
          updatedNavlights.push({
            name: jointName,
            section: 0,
            size: 1.0,
            phase: 0.0,
            frequency: 1.0,
            style: "default",
            color: { x: 1, y: 1, z: 1 },
            distance: 100.0,
            sprite_visible: true,
            high_end_only: false
          });
          // Do NOT remove the original joint! Navlights implicitly bind to a Joint by name to extract their position!
        }
      } else if (targetType === "engine_burn") {
        if (originalJoint) {
          updatedEngineBurns.push({
            name: jointName,
            parent_name: originalJoint.parent_name || "Root",
            num_divisions: 1,
            num_flames: 1,
            vertices: []
          });
          updatedJoints = updatedJoints.filter(j => j.name !== jointName);
        }
      } else if (targetType === "engine_glow") {
        const originalJoint = model.joints.find(j => j.name === jointName);
        if (originalJoint) {
          updatedEngineGlows.push({
            name: jointName,
            parent_name: originalJoint.parent_name || "Root",
            lod: 0,
            mesh: {
              name: jointName,
              parent_name: "Root",
              lod: 0,
              parts: []
            }
          });
          updatedJoints = updatedJoints.filter(j => j.name !== jointName);
        }
      }
    });

    // Save final migrated HOD v2.0 model state:
    const upgradedModel = {
      ...model,
      is_v2: true,
      version: 512, // HOD 2.0 (0x200)
      joints: updatedJoints,
      markers: updatedMarkers,
      collision_meshes: updatedCollisions,
      nav_lights: updatedNavlights,
      engine_burns: updatedEngineBurns,
      engine_glows: updatedEngineGlows,
    };

    setModel(upgradedModel);
    setIsDirty(true);
    setIsMigrationModalOpen(false);
    setStatusMsg("Model successfully auto-transformed to HOD 2.0 with customized schema reclassifications!");
    alert("Schema migration complete! Un-standardized joints have been promoted, and the file layout upgraded to modern HOD 2.0.");
  };

  // Update selected joint/marker/navlight/dockpoint/collision position in model state
  const handleReParentNode = (nodeName: string, nodeType: string, newParentName: string) => {
    if (!model) return;

    invoke("log_event", { 
      level: "INFO", 
      message: `Re-parenting HOD node "${nodeName}" (type: ${nodeType}) under parent joint "${newParentName}"` 
    }).catch(console.error);

    let updatedModel = { ...model };

    if (nodeType === "joint") {
      // Prevent cyclical parenting (cannot parent a joint under itself or its descendants)
      const isDescendant = (parent: string, child: string): boolean => {
        if (parent === child) return true;
        const currentJoint = model.joints.find(j => j.name === parent);
        if (!currentJoint || !currentJoint.parent_name || currentJoint.parent_name === "Root") return false;
        return isDescendant(currentJoint.parent_name, child);
      };

      if (isDescendant(newParentName, nodeName)) {
        setErrorMsg("Error: Cyclical parenting is not allowed (cannot parent a joint bone under its own descendants).");
        return;
      }

      updatedModel.joints = model.joints.map((joint) => {
        if (joint.name === nodeName) {
          return { ...joint, parent_name: newParentName };
        }
        return joint;
      });
    } else if (nodeType === "marker") {
      updatedModel.markers = model.markers.map((marker) => {
        if (marker.name === nodeName) {
          return { ...marker, parent_joint: newParentName };
        }
        return marker;
      });
    } else if (nodeType === "mesh") {
      // In HOD, meshKey contains '_lod_[number]', we extract the base mesh name
      const baseMeshName = nodeName.split("_lod_")[0] || nodeName;
      updatedModel.meshes = model.meshes.map((mesh) => {
        if (mesh.name === baseMeshName) {
          return { ...mesh, parent_name: newParentName };
        }
        return mesh;
      });
    } else if (nodeType === "engine_burn") {
      updatedModel.engine_burns = model.engine_burns.map((burn) => {
        if (burn.name === nodeName) {
          return { ...burn, parent_name: newParentName };
        }
        return burn;
      });
    } else if (nodeType.endsWith("_group")) {
      const groupJoints = model.joints.filter(j => j.name.toLowerCase().startsWith(nodeName.toLowerCase() + "_") || j.name.toLowerCase() === nodeName.toLowerCase());
      const rootGroupJoints = groupJoints.filter(j => {
        const hasParentInGroup = groupJoints.some(other => other.name === j.parent_name);
        return !hasParentInGroup;
      });

      // Prevent cyclical parenting for assemblies
      const isDescendant = (parent: string, child: string): boolean => {
        if (parent === child) return true;
        const currentJoint = model.joints.find(j => j.name === parent);
        if (!currentJoint || !currentJoint.parent_name || currentJoint.parent_name === "Root") return false;
        return isDescendant(currentJoint.parent_name, child);
      };

      for (const root of rootGroupJoints) {
         if (isDescendant(newParentName, root.name)) {
            setErrorMsg("Error: Cyclical parenting is not allowed (cannot parent an assembly under its own descendants).");
            return;
         }
      }

      updatedModel.joints = model.joints.map((joint) => {
        if (rootGroupJoints.some(rj => rj.name === joint.name)) {
          return { ...joint, parent_name: newParentName };
        }
        return joint;
      });
    } else if (nodeType === "navlight") {
      updatedModel.joints = model.joints.map((joint) => {
        if (joint.name === nodeName) {
          return { ...joint, parent_name: newParentName };
        }
        return joint;
      });
    }

    updateModel(updatedModel);
    setStatusMsg(`Successfully re-parented ${nodeName} under ${newParentName}`);
  };

  // Update selected joint/marker/navlight/dockpoint/collision position in model state
  const handleNodeTransform = (name: string, type: string, pos: Vector3D, localMatrix?: { m: number[][] }, localQuat?: { x: number, y: number, z: number, w: number }, localEuler?: Vector3D) => {
    if (!model) return;

    if (type === "joint" || type === "engine_nozzle") {
      const updatedJoints = model.joints.map((joint) => {
        if (joint.name === name) {
          const m = localMatrix ? localMatrix.m : joint.local_transform.m.map(row => [...row]);
          if (!localMatrix) {
            m[3][0] = pos.x;
            m[3][1] = pos.y;
            m[3][2] = pos.z;
          }
          return {
            ...joint,
            local_transform: { m },
            position: pos,
            rotation: matrixRotationToHodRadians({ m }),
          };
        }
        return joint;
      });
      updateModel({ ...model, joints: updatedJoints });
    } else if (type === "marker") {
      const updatedMarkers = model.markers.map((marker) => {
        if (marker.name === name) {
          return { ...marker, position: pos, rotation: localMatrix || marker.rotation };
        }
        return marker;
      });
      updateModel({ ...model, markers: updatedMarkers });
    } else if (type === "navlight") {
      // Translating NavLight translates its matching joint bone
      let foundJoint = false;
      const updatedJoints = model.joints.map((joint) => {
        if (joint.name.toLowerCase() === name.toLowerCase()) {
          foundJoint = true;
          const m = localMatrix ? localMatrix.m : joint.local_transform.m.map(row => [...row]);
          if (!localMatrix) {
            m[3][0] = pos.x;
            m[3][1] = pos.y;
            m[3][2] = pos.z;
          }
          return {
            ...joint,
            local_transform: { m },
            position: pos,
            rotation: matrixRotationToHodRadians({ m }),
          };
        }
        return joint;
      });
      if (!foundJoint) {
        updatedJoints.push({
          name,
          parent_name: "Root",
          position: pos,
          rotation: { x: 0, y: 0, z: 0 },
          scale: { x: 0, y: 0, z: 0 },
          local_transform: {
            m: [
              [1, 0, 0, 0],
              [0, 1, 0, 0],
              [0, 0, 1, 0],
              [pos.x, pos.y, pos.z, 1]
            ]
          }
        });
      }
      updateModel({ ...model, joints: updatedJoints });
    } else if (type.endsWith("_group")) {
      const updatedJoints = model.joints.map((joint) => {
        if (joint.name.toLowerCase() === `${name}_Position`.toLowerCase()) {
          const m = localMatrix ? localMatrix.m : joint.local_transform.m.map(row => [...row]);
          if (!localMatrix) {
            m[3][0] = pos.x;
            m[3][1] = pos.y;
            m[3][2] = pos.z;
          }
          return {
            ...joint,
            local_transform: { m },
            position: pos,
            rotation: matrixRotationToHodRadians({ m }),
          };
        }
        return joint;
      });
      updateModel({ ...model, joints: updatedJoints });
    } else if (type === "dockpoint") {
      const [pathName, ptIdxStr] = name.split(":");
      const ptIdx = parseInt(ptIdxStr, 10);
      const updatedPaths = model.dockpaths.map((dp) => {
        if (dp.name === pathName) {
          const updatedPoints = dp.points.map((pt, idx) => {
            if (idx === ptIdx) {
              return {
                ...pt,
                position: pos,
                rotation: localMatrix || pt.rotation,
              };
            }
            return pt;
          });
          return { ...dp, points: updatedPoints };
        }
        return dp;
      });
      updateModel({ ...model, dockpaths: updatedPaths });
    } else if (type === "collision") {
      const updatedCols = model.collision_meshes.map((col) => {
        if (col.name === name) {
          return { ...col, center: pos };
        }
        return col;
      });
      updateModel({ ...model, collision_meshes: updatedCols });
    } else if (type === "keyframe") {
      const [jointName, kfIdxStr] = name.split(":");
      const kfIdx = parseInt(kfIdxStr, 10);
      const updatedAnims = [...(model.animations || [])];
      if (updatedAnims[selectedAnimIdx]) {
        const anim = { ...updatedAnims[selectedAnimIdx] };
        const updatedTracks = anim.tracks.map((track) => {
          if (track.joint_name.toLowerCase() === jointName.toLowerCase()) {
            const updatedKfs = track.keyframes.map((kf, i) => {
              if (i === kfIdx) {
                return { 
                  ...kf, 
                  position: pos, 
                  rotation: localQuat || kf.rotation, 
                  rotation_euler: localEuler || kf.rotation_euler 
                };
              }
              return kf;
            });
            return { ...track, keyframes: updatedKfs };
          }
          return track;
        });
        anim.tracks = updatedTracks;
        updatedAnims[selectedAnimIdx] = anim;
        updateModel({ ...model, animations: updatedAnims });
      }
    }
    
    setStatusMsg(`Moved ${type} ${name} to [${pos.x.toFixed(2)}, ${pos.y.toFixed(2)}, ${pos.z.toFixed(2)}]`);
  };

  const handleSaveHOD = async () => {
    if (!model) return;
    
    if (!filePath) {
      setModPicker(true);
      return;
    }

    setIsLoading(true);
    setStatusMsg("Saving HOD...");
    setErrorMsg(null);

    setTimeout(async () => {
      try {
        // Phase 4: Trigger native Rust HOD v2 compression and writer
        const mod=filePath.replace(/\\/g,"/").match(/\/mods\/([^/]+)\//)?.[1];
        if(mod) await invoke("save_to_mod",{name:mod,sourcePath,model:sanitizeModel(model),mounts,template:subsystemShip});
        else await invoke("save_hod", { targetPath: filePath, sourcePath, model: sanitizeModel(model), mounts, template:subsystemShip });
        setIsDirty(false);
        invoke("log_event", { level: "INFO", message: `Successfully compiled and saved HOD 2.0 to path: ${filePath}` }).catch(console.error);
        setStatusMsg("HOD 2.0 file compiled successfully");
      } catch (e: any) {
        const err = `Frontend failed to save HOD 2.0 to path ${filePath}: ${e.toString()}`;
        invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
        setErrorMsg(e.toString());
        setStatusMsg("Failed to save HOD 2.0");
      } finally {
        setIsLoading(false);
      }
    }, 50);
  };

  const handleSaveHODAs = async () => {
    if (!model) {
      alert("Please load or create a HOD model first.");
      return;
    }
    
    try {
      const sourceParts = sourcePath.split("/");
      const assetName = sourceParts[sourceParts.length - 1] === "hull.hod" && sourceParts[sourceParts.length - 2] === "rmnative"
        ? sourceParts[sourceParts.length - 3] + ".hod" : sourceParts[sourceParts.length - 1];
      const defaultName = filePath.split(/[/\\]/).pop() || assetName || model.name + ".hod";
      const selectedPath = await invoke<string | null>("select_save_hod_file", { defaultName });
      if (!selectedPath) return;

      setIsLoading(true);
      setStatusMsg("Saving HOD As...");
      setErrorMsg(null);

      setTimeout(async () => {
        try {
          await invoke("save_hod", { sourcePath, targetPath: selectedPath, model: sanitizeModel(model), mounts, template:subsystemShip });
          
          setFilePath(selectedPath);
          setIsDirty(false);
          invoke("log_event", { level: "INFO", message: `Successfully compiled HOD 2.0 as a new file at: ${selectedPath}` }).catch(console.error);
          setStatusMsg("HOD 2.0 file compiled successfully");
          alert(`HOD 2.0 file compiled successfully to new path:\n${selectedPath}\n\nMeshes are natively uncompressed, and textures are compressed (if TGA/PNG mapping existed).`);
        } catch (e: any) {
          const err = `Frontend failed to Compile/Save HOD 2.0 As: ${e.toString()}`;
          invoke("log_event", { level: "ERROR", message: err }).catch(console.error);
          setStatusMsg("Failed to save HOD 2.0");
          setErrorMsg(err);
          alert(`Save As failed: ${e.toString()}`);
        } finally {
          setIsLoading(false);
        }
      }, 50);
    } catch (e: any) {
      setIsLoading(false);
      console.error(e);
    }
  };

  const isLayerVisible = (prefix: string, items: any[] | undefined) => {
    if (!items || items.length === 0) return false;
    const targetItems = (prefix === 'mesh' || prefix === 'engine_glow') ? items.filter(item => item.lod === 0) : items;
    if (targetItems.length === 0) return false;
    return targetItems.every(item => {
      let key = `${prefix}:${item.name}`;
      if (prefix === 'mesh') key = `${item.name}_lod_${item.lod}`;
      return visibleMeshes[key] !== false;
    });
  };

  const toggleLayer = (prefix: string, items: any[] | undefined, currentState: boolean) => {
    if (!items || items.length === 0) return;
    const targetItems = (prefix === 'mesh' || prefix === 'engine_glow') ? items.filter(item => item.lod === 0) : items;
    setVisibleMeshes(prev => {
      const updated = { ...prev };
      // Force all LOD > 0 to be off if we are toggling meshes or engine glows
      if (prefix === 'mesh' || prefix === 'engine_glow') {
        items.forEach(item => {
          if (item.lod > 0) {
            let key = `${prefix}:${item.name}`;
            if (prefix === 'mesh') key = `${item.name}_lod_${item.lod}`;
            updated[key] = false;
          }
        });
      }
      targetItems.forEach(item => {
        let key = `${prefix}:${item.name}`;
        if (prefix === 'mesh') {
          key = `${item.name}_lod_${item.lod}`;
        }
        updated[key] = !currentState;
      });
      return updated;
    });
  };

  return (
    <div className="app-container">
      {textEditor&&<GameTextEditor path={filePath} kind={textEditor} close={()=>setTextEditor(null)}/>}
      {modPicker && model && <ModPicker close={()=>setModPicker(false)} save={async name=>{
        const path=await invoke<string>("save_to_mod",{name,sourcePath,model:sanitizeModel(model),mounts,template:subsystemShip});
        setFilePath(path);setIsDirty(false);setStatusMsg("Saved to "+name+" / enabled; restart OpenHW to apply");
      }}/>}
      {library && <GameModels shipsOnly={library === "subsystems"} close={() => {pendingPaste.current=false;setLibrary(null);}} open={key => {
        if (library === "subsystems") { setLibrary(null);setSubsystemShip(key);
          if(pendingPaste.current && clipboard.current){pendingPaste.current=false;addMount(clipboard.current.asset,clipboard.current);}
          else void showPicker();
          return; }
        if (isDirty && !window.confirm("Discard unsaved changes and open this model?")) return;
        setLibrary(null);
        loadHODFile("game://" + key);
      }} />}
      {picker && <div className="model-library-overlay"><section className="model-library" role="dialog" aria-label="Quick Add subsystem">
        <header><h2>Quick Add subsystem</h2><button disabled={quickBusy} onClick={()=>setPicker(false)}>Close</button></header>
        <input autoFocus placeholder="Find a subsystem" value={filter} onChange={e=>setFilter(e.target.value)}/>
        <div className="model-library-list">{catalog.filter(n=>n.includes(filter.toLowerCase())).map(n=><button key={n} disabled={quickBusy} onClick={()=>void chooseAsset(n)}>{n}</button>)}
        {!catalog.length && "No compatible subsystems installed."}</div>
        {mounts.length>0 && <><h3>Mounted subsystems</h3><div className="model-library-list">{mounts.map(m=><button key={m.slot} disabled={quickBusy}
          onClick={()=>{if(editMount("duplicate",m))setPicker(false);}}>Duplicate {m.slot} · {m.asset}</button>)}</div></>}
      </section></div>}
      {(isLoading || isSaving) && (
        <div style={{
          position: "fixed",
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: "rgba(3, 6, 10, 0.85)",
          backdropFilter: "blur(8px)",
          zIndex: 9999,
          display: "flex",
          flexDirection: "column",
          justifyContent: "center",
          alignItems: "center",
          gap: "20px"
        }}>
          <div style={{
            width: "50px",
            height: "50px",
            border: "3px solid rgba(22, 160, 255, 0.1)",
            borderTop: "3px solid var(--accent-cyan)",
            borderRadius: "50%",
            animation: "spin 1s linear infinite"
          }} />
          <div style={{
            fontSize: "14px",
            fontWeight: "600",
            color: "var(--accent-cyan)",
            letterSpacing: "0.15em",
            textTransform: "uppercase",
            animation: "pulse 1.5s ease-in-out infinite"
          }}>
            Decompressing & Loading HOD Asset
          </div>
          <div style={{
            fontSize: "11px",
            color: "var(--text-muted)",
            fontFamily: "var(--font-mono)",
            maxWidth: "400px",
            textAlign: "center",
            lineHeight: "1.4"
          }}>
            Please wait...
          </div>
        </div>
      )}
      {/* Top Header Toolbar */}
      <Toolbar
        modelName={model ? `${model.name} (${filePath ? filePath.split(/[/\\]/).pop() : sourcePath.replace("game://", "") || "new"})` : ""}
        onGameModels={() => setLibrary("models")}
        onSubsystems={model ? () => subsystemShip ? void showPicker() : setLibrary("subsystems") : undefined}
        onReturn={returnToGame}
        onOpenClick={selectAndLoadFile}
        onSaveClick={handleSaveHOD}
        onSaveToMod={()=>setModPicker(true)}
        onEditFile={kind=>{
          if(!filePath.replace(/\\/g,"/").includes("/mods/")){setErrorMsg("Save this ship to a mod before editing its game files.");return;}
          if(isDirty){setErrorMsg("Save your HOD changes before editing the ship, events or weapon files.");return;}
          setTextEditor(kind);
        }}
        onSaveAsClick={handleSaveHODAs}
        isSaving={isSaving}
        onNewClick={handleCreateNewHOD}
        onImportDAEClick={selectAndImportDAE}
        onSettingsClick={() => setIsSettingsModalOpen(true)}
      />

{/* Main Workspace Panels */}
      <div className="workspace" style={{ gridTemplateColumns: `${sidebarWidth}px 1px 1fr 1px ${inspectorWidth}px` }}>
        {/* Left Side Hierarchy Node Tree */}
        <HierarchyTree
          model={model}
          selectedNode={selectedNode}
          setSelectedNode={setSelectedNode}
          visibleMeshes={visibleMeshes}
          setVisibleMeshes={setVisibleMeshes}
          onReParentNode={handleReParentNode}
          onModelChange={updateModel}
          selectedAnimIdx={selectedAnimIdx}
          setSelectedAnimIdx={setSelectedAnimIdx}
          targetBoxes={targetBoxes}
          setTargetBoxes={setTargetBoxes}
          onTabChange={setActiveHierarchyTab}
          setIsLoading={setIsLoading}
          setStatusMsg={setStatusMsg}
        />

        {/* Sidebar Drag Separator Divider */}
        <div 
          onMouseDown={handleSidebarDragStart}
          style={{
            width: "4px",
            marginLeft: "-2px",
            background: "transparent",
            cursor: "col-resize",
            zIndex: 100,
            height: "100%",
            transition: "background 0.2s"
          }}
          className="divider-hover"
        />

        {/* Center Viewport rendering */}
        <div style={{ position: "relative", height: "100%", overflow: "hidden", display: "flex", flexDirection: "column", flex: 1 }}>
          {model ? (
            <div style={{ position: "relative", width: "100%", height: "100%", flex: 1, minHeight: 0 }}>
                            {/* Floating Viewport Overlays panel */}
              <div
                style={{
                  position: "absolute",
                  top: "12px",
                  right: "12px",
                  background: "rgba(13, 22, 37, 0.85)",
                  border: "1px solid var(--border-color)",
                  borderRadius: "6px",
                  padding: "12px 14px",
                  zIndex: 10,
                  backdropFilter: "blur(8px)",
                  boxShadow: "0 4px 16px rgba(0,0,0,0.6)",
                  display: "flex",
                  flexDirection: "column",
                  gap: "10px",
                  textAlign: "left",
                  width: "180px"
                }}
              >
                <div style={{ fontSize: "11px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)", marginBottom: "4px" }}>
                  Viewport Layers
                </div>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("mesh", model?.meshes)}
                    onChange={() => toggleLayer("mesh", model?.meshes, isLayerVisible("mesh", model?.meshes))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Meshes</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("engine_glow", model?.engine_glows)}
                    onChange={() => toggleLayer("engine_glow", model?.engine_glows, isLayerVisible("engine_glow", model?.engine_glows))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Engine Glows</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("navlight", model?.nav_lights)}
                    onChange={() => toggleLayer("navlight", model?.nav_lights, isLayerVisible("navlight", model?.nav_lights))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>NavLights</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("collision", model?.collision_meshes)}
                    onChange={() => toggleLayer("collision", model?.collision_meshes, isLayerVisible("collision", model?.collision_meshes))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Collision Hulls</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("dockpath", model?.dockpaths)}
                    onChange={() => toggleLayer("dockpath", model?.dockpaths, isLayerVisible("dockpath", model?.dockpaths))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Dockpaths</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("engine_burn", model?.engine_burns)}
                    onChange={() => toggleLayer("engine_burn", model?.engine_burns, isLayerVisible("engine_burn", model?.engine_burns))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Engine Burns</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("marker", model?.markers)}
                    onChange={() => toggleLayer("marker", model?.markers, isLayerVisible("marker", model?.markers))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Markers</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={isLayerVisible("joint", model?.joints)}
                    onChange={() => toggleLayer("joint", model?.joints, isLayerVisible("joint", model?.joints))}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Joint Nodes</span>
                </label>
                <label style={{ display: "flex", alignItems: "center", justifyContent: "flex-start", gap: "10px", fontSize: "12px", color: "var(--text-primary)", cursor: "pointer", userSelect: "none", width: "100%" }}>
                  <input
                    type="checkbox"
                    checked={showBoneLines}
                    onChange={(e) => setShowBoneLines(e.target.checked)}
                    style={{ width: "auto", height: "auto", margin: 0, cursor: "pointer" }}
                  />
                  <span>Node Hierarchy Lines</span>
                </label>

                <div style={{ display: "flex", flexDirection: "column", gap: "4px", marginTop: "4px", borderTop: "1px solid var(--border-color)", paddingTop: "8px" }}>
                  <div style={{ fontSize: "10px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)", marginBottom: "2px" }}>
                    Render Style
                  </div>
                  <select
                    value={renderMode}
                    onChange={(e) => setRenderMode(e.target.value as any)}
                    style={{ height: "26px", padding: "2px 6px", fontSize: "11px", background: "rgba(13, 22, 37, 0.9)", width: "100%", borderRadius: "4px" }}
                  >
                    <option value="shaded">Textured Shaded (Raw)</option>
                    <option value="shaded_team">Textured Shaded (Team Paint)</option>
                    <option value="textured">Textured Flat (Raw)</option>
                    <option value="textured_team">Textured Flat (Team Paint)</option>
                    <option value="untextured">Untextured Solid</option>
                    <option value="wireframe">Wireframe</option>
                  </select>
                </div>

                {(renderMode === "shaded_team" || renderMode === "textured_team") && (
                  <div style={{ display: "flex", flexDirection: "column", gap: "6px", marginTop: "4px", borderTop: "1px solid var(--border-color)", paddingTop: "8px" }}>
                    <div style={{ fontSize: "10px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--accent-cyan)", marginBottom: "2px" }}>
                      Team Colors
                    </div>
                    <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "8px" }}>
                      <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                        <span style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>Primary</span>
                        <div style={{ display: "flex", alignItems: "center", gap: "4px", background: "rgba(0,0,0,0.3)", borderRadius: "4px", padding: "2px 4px", border: "1px solid var(--border-color)" }}>
                          <input
                            type="color"
                            value={teamColor}
                            onChange={(e) => setTeamColor(e.target.value)}
                            style={{ width: "14px", height: "14px", border: "none", padding: 0, cursor: "pointer", background: "none" }}
                          />
                          <span style={{ fontSize: "9px", fontFamily: "var(--font-mono)", color: "var(--text-primary)" }}>{teamColor.toUpperCase()}</span>
                        </div>
                      </div>
                      <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
                        <span style={{ fontSize: "9px", color: "var(--text-muted)", textTransform: "uppercase" }}>Stripe</span>
                        <div style={{ display: "flex", alignItems: "center", gap: "4px", background: "rgba(0,0,0,0.3)", borderRadius: "4px", padding: "2px 4px", border: "1px solid var(--border-color)" }}>
                          <input
                            type="color"
                            value={stripeColor}
                            onChange={(e) => setStripeColor(e.target.value)}
                            style={{ width: "14px", height: "14px", border: "none", padding: 0, cursor: "pointer", background: "none" }}
                          />
                          <span style={{ fontSize: "9px", fontFamily: "var(--font-mono)", color: "var(--text-primary)" }}>{stripeColor.toUpperCase()}</span>
                        </div>
                      </div>
                    </div>
                  </div>
                )}
              </div>
              <Viewport
                model={viewportModel}
                selectedNode={selectedNode}
                setSelectedNode={setSelectedNode}
                transformMode={transformMode}
                setTransformMode={setTransformMode}
                coordinateSpace={coordinateSpace}
                setCoordinateSpace={setCoordinateSpace}
                onNodeTransform={handleNodeTransform}
                visibleMeshes={visibleMeshes}
                showBoneLines={showBoneLines}
                renderMode={renderMode}
                teamColor={teamColor}
                stripeColor={stripeColor}
                onModelChange={changed => { if(model)updateModel({...changed,
                  joints:changed.joints.slice(0,model.joints.length),meshes:changed.meshes.slice(0,model.meshes.length),
                  textures:model.textures,materials:model.materials}); }}
                selectedAnimIdx={selectedAnimIdx}
                setSelectedAnimIdx={setSelectedAnimIdx}
                isPlaying={isPlaying}
                setIsPlaying={setIsPlaying}
                currentTime={currentTime}
                setCurrentTime={setCurrentTime}
                playbackSpeed={playbackSpeed}
                loopPlayback={loopPlayback}
                targetBoxes={targetBoxes}
              />
            </div>
          ) : (
            <div
              style={{
                width: "100%",
                height: "100%",
                display: "flex",
                flexDirection: "column",
                justifyContent: "center",
                alignItems: "center",
                background: "#03060a",
                gap: "16px",
                padding: "20px",
                textAlign: "center",
              }}
            >
              <Info size={40} style={{ color: "var(--border-color)", filter: "drop-shadow(var(--shadow-glow))" }} />
              <div style={{ fontSize: "18px", color: "var(--accent-cyan)", fontWeight: "600", letterSpacing: "0.05em" }}>
                OpenHW HOD Editor
              </div>
              <div style={{ color: "var(--text-muted)", fontSize: "13px", maxWidth: "450px", lineHeight: "1.6" }}>
                Open an installed game model or a HOD file, start a new model, or import a COLLADA mesh.
              </div>
              
                            <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: "16px", marginTop: "12px", width: "100%", maxWidth: "500px" }}>
                <button 
                  className="secondary" 
                  onClick={handleCreateNewHOD} 
                  style={{ 
                    height: "42px", 
                    padding: "0 24px", 
                    display: "flex", 
                    alignItems: "center", 
                    gap: "10px", 
                    fontSize: "14px", 
                    fontWeight: "600", 
                    width: "100%", 
                    justifyContent: "center",
                    border: "1px dashed var(--accent-cyan)",
                    background: "rgba(22, 160, 255, 0.05)",
                    color: "var(--accent-cyan)",
                    cursor: "pointer"
                  }}
                >
                  <FilePlus size={18} />
                  <span>Start a New HOD Model</span>
                </button>

                <button className="primary" onClick={selectAndLoadFile} style={{ height: "42px", padding: "0 24px", display: "flex", alignItems: "center", gap: "10px", fontSize: "14px", fontWeight: "600", width: "100%", justifyContent: "center" }}>
                  <FolderOpen size={18} />
                  <span>Browse HOD File...</span>
                </button>
                
                <div style={{ display: "flex", alignItems: "center", gap: "8px", width: "100%" }}>
                  <div style={{ flex: 1, height: "1px", background: "rgba(255,255,255,0.08)" }} />
                  <span style={{ fontSize: "11px", color: "var(--text-muted)", textTransform: "uppercase", letterSpacing: "0.1em" }}>or enter path manually</span>
                  <div style={{ flex: 1, height: "1px", background: "rgba(255,255,255,0.08)" }} />
                </div>

                <div style={{ width: "100%", display: "flex", gap: "8px" }}>
                  <input
                    placeholder="e.g. /path/to/my_ship.hod"
                    value={filePath}
                    onChange={(e) => setFilePath(e.target.value)}
                    style={{ height: "36px", fontSize: "13px" }}
                  />
                  <button className="secondary" onClick={() => loadHODFile(filePath)} style={{ height: "36px" }}>
                    Load File
                  </button>
                </div>
              </div>

              {/* Error Message banner */}
              {errorMsg && (
                <div
                  style={{
                    marginTop: "20px",
                    background: "rgba(255, 23, 68, 0.1)",
                    border: "1px solid var(--accent-danger)",
                    borderRadius: "6px",
                    padding: "12px 16px",
                    display: "flex",
                    alignItems: "center",
                    gap: "12px",
                    color: "#ffcdd2",
                    fontSize: "13px",
                    maxWidth: "500px",
                    textAlign: "left",
                  }}
                >
                  <AlertTriangle size={20} style={{ color: "var(--accent-danger)", flexShrink: 0 }} />
                  <span>{errorMsg}</span>
                </div>
              )}
            </div>
          )}

          {/* Animation Dock — only visible when animations tab is active */}
          {model && activeHierarchyTab === "animations" && (
            <AnimationDock
              model={model}
              selectedAnimIdx={selectedAnimIdx}
              setSelectedAnimIdx={setSelectedAnimIdx}
              isPlaying={isPlaying}
              setIsPlaying={setIsPlaying}
              currentTime={currentTime}
              setCurrentTime={setCurrentTime}
              loopPlayback={loopPlayback}
              setLoopPlayback={setLoopPlayback}
              playbackSpeed={playbackSpeed}
              setPlaybackSpeed={setPlaybackSpeed}
              onModelChange={updateModel}
              selectedNode={selectedNode}
              onSelectedNodeChange={setSelectedNode}
            />
          )}
        </div>

        {/* Inspector Drag Separator Divider */}
        <div 
          onMouseDown={handleInspectorDragStart}
          style={{
            width: "4px",
            marginRight: "-2px",
            background: "transparent",
            cursor: "col-resize",
            zIndex: 100,
            height: "100%",
            transition: "background 0.2s"
          }}
          className="divider-hover"
        />

        {/* Right Side Detail Inspector */}
        <Inspector
          key={selectedNode ? `${selectedNode.type}:${selectedNode.name}` : "empty"}
          model={model}
          selectedNode={selectedNode}
          onPositionChange={handleNodeTransform}
          coordinateSpace={coordinateSpace}
          setCoordinateSpace={setCoordinateSpace}
          onModelChange={updateModel}
          onSelectedNodeChange={setSelectedNode}
          selectedAnimIdx={selectedAnimIdx}
          visibleMeshes={visibleMeshes}
          onToggleVisibility={handleToggleVisibility}
          setIsLoading={setIsLoading}
          setStatusMsg={setStatusMsg}
        />
      </div>

      {/* Schema Migration Assistant Modal */}
      {isMigrationModalOpen && model && (
        <div style={{
          position: "fixed",
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: "rgba(3, 8, 16, 0.8)",
          backdropFilter: "blur(8px)",
          display: "flex",
          justifyContent: "center",
          alignItems: "center",
          zIndex: 2000,
          padding: "20px"
        }}>
          <div style={{
            background: "rgba(10, 20, 35, 0.98)",
            border: "1px solid rgba(22, 160, 255, 0.45)",
            borderRadius: "14px",
            width: "100%",
            maxWidth: "600px",
            boxShadow: "0 12px 48px rgba(0,0,0,0.7)",
            display: "flex",
            flexDirection: "column",
            overflow: "hidden",
            maxHeight: "90vh"
          }}>
            {/* Header */}
            <div style={{
              background: "linear-gradient(135deg, rgba(22, 160, 255, 0.2), transparent)",
              padding: "18px 24px",
              borderBottom: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "space-between",
              alignItems: "center"
            }}>
              <div style={{ display: "flex", alignItems: "center", gap: "10px" }}>
                <span>🚀</span>
                <span style={{ fontWeight: "700", fontSize: "16px", color: "var(--accent-cyan)", letterSpacing: "0.04em" }}>
                  HOD Schema Migration Assistant
                </span>
              </div>
              <button 
                onClick={() => setIsMigrationModalOpen(false)}
                style={{ background: "transparent", border: "none", color: "var(--text-muted)", fontSize: "18px", cursor: "pointer" }}
              >
                ✕
              </button>
            </div>

            {/* Content Area */}
            <div style={{ padding: "24px", overflowY: "auto", display: "flex", flexDirection: "column", gap: "16px", flex: 1 }}>
              <div style={{ fontSize: "13px", lineHeight: "1.6", color: "rgba(255, 255, 255, 0.85)" }}>
                The following joints in this legacy model do not align with modern Homeworld Remastered rigid joint schemas. To optimize layout performance, reduce rendering overhead, and prevent crashes, you can re-classify them:
              </div>

              {Object.keys(migrationMappings).length > 0 ? (
                <div style={{ display: "flex", flexDirection: "column", gap: "10px", maxHeight: "350px", overflowY: "auto", border: "1px solid var(--border-color)", borderRadius: "8px", background: "rgba(0,0,0,0.2)", padding: "10px" }}>
                  {Object.keys(migrationMappings).map((jointName) => {
                    const mappedType = migrationMappings[jointName];
                    const cloneTarget = migrationCoordinateClones[jointName] || "Self";
                    return (
                      <div key={jointName} style={{ display: "flex", justifyContent: "space-between", alignItems: "center", padding: "10px 14px", background: "rgba(22, 160, 255, 0.03)", border: "1px solid rgba(22, 160, 255, 0.1)", borderRadius: "6px", gap: "10px" }}>
                        <span style={{ fontSize: "13px", fontWeight: "600", color: "#fff", fontFamily: "var(--font-mono)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", flex: 1 }}>
                          {jointName}
                        </span>
                        
                        <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                          <span style={{ fontSize: "10px", color: "var(--text-muted)" }}>Target Type:</span>
                          <select
                            value={mappedType}
                            onChange={(e: any) => {
                              const val = e.target.value;
                              setMigrationMappings(prev => ({ ...prev, [jointName]: val }));
                            }}
                            style={{ width: "160px", height: "30px", fontSize: "12px", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", borderRadius: "4px" }}
                          >
                            <option value="joint">Standard Joint Bone</option>
                            <option value="marker">Marker / Attachment Point</option>
                            <option value="weapon">Weapon Assembly (HWRM template)</option>
                            <option value="collision">Collision Hull Mesh</option>
                            <option value="navlight">Navlight (Standalone)</option>
                            <option value="engine_burn">Engine Burn</option>
                            <option value="engine_glow">Engine Glow</option>
                          </select>
                        </div>

                        <div style={{ display: "flex", flexDirection: "column", gap: "4px" }}>
                          <span style={{ fontSize: "10px", color: "var(--text-muted)" }}>Clone Transform From:</span>
                          <select
                            value={cloneTarget}
                            onChange={(e: any) => {
                              const val = e.target.value;
                              setMigrationCoordinateClones(prev => ({ ...prev, [jointName]: val }));
                            }}
                            style={{ width: "160px", height: "30px", fontSize: "12px", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", borderRadius: "4px" }}
                          >
                            <option value="Self">Self (Preserve)</option>
                            {model?.joints.map(j => (
                               <option key={j.name} value={j.name}>{j.name}</option>
                            ))}
                          </select>
                        </div>
                      </div>
                    );
                  })}
                </div>
              ) : (
                <div style={{ padding: "20px", background: "rgba(0, 230, 118, 0.1)", border: "1px solid #00e676", borderRadius: "8px", fontSize: "13px", color: "#b9f6ca", textAlign: "center" }}>
                  ✓ All joints conform cleanly to Homeworld Remastered standards! No ambiguous nodes detected.
                </div>
              )}

              <div style={{ padding: "12px 16px", background: "rgba(22, 160, 255, 0.05)", borderLeft: "4px solid var(--accent-cyan)", fontSize: "12px", color: "var(--text-secondary)", lineHeight: "1.5" }}>
                💡 <b>Mapping rules:</b> Selecting 'Marker' promotes joint coordinates into a clean HOD attachment marker. Selecting 'Weapon' auto-groups joints and repairs standard turret paths (Position, Direction, Muzzle, Rest) automatically.
              </div>
            </div>

            {/* Footer */}
            <div style={{
              padding: "16px 24px",
              background: "rgba(3, 8, 16, 0.5)",
              borderTop: "1px solid var(--border-color)",
              display: "flex",
              justifyContent: "flex-end",
              gap: "12px"
            }}>
              <button
                className="secondary"
                onClick={() => setIsMigrationModalOpen(false)}
                style={{ padding: "8px 18px", fontSize: "12px", borderRadius: "4px" }}
              >
                Cancel
              </button>
              <button
                className="primary"
                onClick={handleExecuteMigration}
                style={{
                  padding: "8px 18px",
                  fontSize: "12px",
                  borderRadius: "4px",
                  background: "linear-gradient(135deg, var(--accent-cyan), #00e676)",
                  color: "#000",
                  fontWeight: "700",
                  border: "none",
                  cursor: "pointer"
                }}
              >
                Confirm and Convert to HOD 2.0
              </button>
            </div>
          </div>
        </div>
      )}

      <SettingsModal
        isOpen={isSettingsModalOpen}
        onClose={() => setIsSettingsModalOpen(false)}
        keeperTxtPaths={keeperTxtPaths}
        setKeeperTxtPaths={setKeeperTxtPaths}
      />

      {/* Bottom Status bar */}
      <footer className="statusbar">
        <span role="status">{errorMsg || statusMsg}</span>
      </footer>
    </div>
  );
}

export default App;
