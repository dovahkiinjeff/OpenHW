import React, { useRef, useCallback } from "react";
import * as THREE from "three";
import { GLTFExporter } from 'three/examples/jsm/exporters/GLTFExporter.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import { invoke } from "../bridge";
import {
  HODModel,
  HODAnimation,
  HODAnimationTrack,
  HODKeyframe,
} from "./Viewport";

interface AnimationDockProps {
  model: HODModel | null;
  selectedAnimIdx: number;
  setSelectedAnimIdx: (idx: number) => void;
  isPlaying: boolean;
  setIsPlaying: (v: boolean) => void;
  currentTime: number;
  setCurrentTime: (v: number) => void;
  loopPlayback: boolean;
  setLoopPlayback: (v: boolean) => void;
  playbackSpeed: number;
  setPlaybackSpeed: (v: number) => void;
  onModelChange?: (m: HODModel) => void;
  selectedNode: { type: string; name: string } | null;
  onSelectedNodeChange?: (node: { type: string; name: string } | null) => void;
}

export const AnimationDock: React.FC<AnimationDockProps> = ({
  model,
  selectedAnimIdx,
  setSelectedAnimIdx,
  isPlaying,
  setIsPlaying,
  currentTime,
  setCurrentTime,
  loopPlayback,
  setLoopPlayback,
  playbackSpeed,
  setPlaybackSpeed,
  onModelChange,
  selectedNode,
  onSelectedNodeChange,
}) => {
  const rulerRef = useRef<HTMLDivElement>(null);

  // ─── Derived ─────────────────────────────────────────────────────────────
  const activeAnim: HODAnimation | undefined = model?.animations?.[selectedAnimIdx];
  const duration = activeAnim?.duration ?? 4.0;
  const hasAnims = (model?.animations?.length ?? 0) > 0;

  // ─── Ruler click / drag to scrub ─────────────────────────────────────────
  const scrubFromEvent = useCallback(
    (e: React.MouseEvent | MouseEvent) => {
      if (!rulerRef.current) return;
      const rect = rulerRef.current.getBoundingClientRect();
      const ratio = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
      setCurrentTime(ratio * duration);
    },
    [duration, setCurrentTime]
  );

  const handleRulerMouseDown = (e: React.MouseEvent) => {
    setIsPlaying(false);
    scrubFromEvent(e);
    const onMove = (mv: MouseEvent) => scrubFromEvent(mv);
    const onUp = () => {
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    };
    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  };



  const recordKeyframeForJoint = useCallback((jointName: string, time: number) => {
    if (!model || !activeAnim) return;
    const joint = model.joints.find((j) => j.name.toLowerCase() === jointName.toLowerCase());
    if (!joint) return;

    const m = joint.local_transform.m;
    const matrix = new THREE.Matrix4().set(
      m[0][0], m[1][0], m[2][0], m[3][0],
      m[0][1], m[1][1], m[2][1], m[3][1],
      m[0][2], m[1][2], m[2][2], m[3][2],
      m[0][3], m[1][3], m[2][3], m[3][3]
    );
    const pos = new THREE.Vector3();
    const quat = new THREE.Quaternion();
    const scale = new THREE.Vector3();
    matrix.decompose(pos, quat, scale);

    const euler = new THREE.Euler().setFromQuaternion(quat, "YXZ");
    const euler_vec = { x: euler.x, y: euler.y, z: euler.z };

    const newKf: HODKeyframe = {
      time: time,
      position: { x: pos.x, y: pos.y, z: pos.z },
      rotation: { x: quat.x, y: quat.y, z: quat.z, w: quat.w },
      rotation_euler: euler_vec,
      scale: { x: scale.x, y: scale.y, z: scale.z }
    };

    let track = activeAnim.tracks.find((t) => t.joint_name.toLowerCase() === jointName.toLowerCase());
    let updatedTracks = [...activeAnim.tracks];
    if (!track) {
      track = {
        joint_name: jointName,
        keyframes: [newKf]
      };
      updatedTracks.push(track);
    } else {
      const existingIdx = track.keyframes.findIndex((kf) => Math.abs(kf.time - time) < 0.01);
      const newKeyframes = [...track.keyframes];
      if (existingIdx !== -1) {
        newKeyframes[existingIdx] = newKf;
      } else {
        newKeyframes.push(newKf);
        newKeyframes.sort((a, b) => a.time - b.time);
      }
      track = { ...track, keyframes: newKeyframes };
      updatedTracks = updatedTracks.map((t) => t.joint_name.toLowerCase() === jointName.toLowerCase() ? track! : t);
    }

    const updatedAnim = { ...activeAnim, tracks: updatedTracks };
    const updatedAnims = (model.animations ?? []).map((a, i) => i === selectedAnimIdx ? updatedAnim : a);
    onModelChange?.({ ...model, animations: updatedAnims });
  }, [model, activeAnim, selectedAnimIdx, onModelChange]);

  const handleAddKeyframe = () => {
    if (!selectedNode || selectedNode.type !== "joint") return;
    recordKeyframeForJoint(selectedNode.name, currentTime);
  };

  const handleAddTrack = (jointName: string) => {
    if (!model || !jointName || !activeAnim) return;
    if (activeAnim.tracks.find((t) => t.joint_name.toLowerCase() === jointName.toLowerCase())) return;
    const newTrack: HODAnimationTrack = { joint_name: jointName, keyframes: [] };
    const updatedAnim: HODAnimation = { ...activeAnim, tracks: [...activeAnim.tracks, newTrack] };
    const updatedAnims = (model.animations ?? []).map((a, i) => (i === selectedAnimIdx ? updatedAnim : a));
    onModelChange?.({ ...model, animations: updatedAnims });
  };

  const handleDeleteTrack = (jointName: string) => {
    if (!model || !activeAnim) return;
    const updatedAnim: HODAnimation = {
      ...activeAnim,
      tracks: activeAnim.tracks.filter((t) => t.joint_name !== jointName),
    };
    const updatedAnims = (model.animations ?? []).map((a, i) => (i === selectedAnimIdx ? updatedAnim : a));
    onModelChange?.({ ...model, animations: updatedAnims });
  };

  const handleDeleteKeyframe = (jointName: string, kfIdx: number) => {
    if (!model || !activeAnim) return;
    const updatedTracks: HODAnimationTrack[] = activeAnim.tracks.map((t) => {
      if (t.joint_name !== jointName) return t;
      const kfs = t.keyframes.filter((_, i) => i !== kfIdx);
      return { ...t, keyframes: kfs };
    });
    const updatedAnim: HODAnimation = { ...activeAnim, tracks: updatedTracks };
    const updatedAnims = (model.animations ?? []).map((a, i) => (i === selectedAnimIdx ? updatedAnim : a));
    onModelChange?.({ ...model, animations: updatedAnims });
  };

  const [contextMenu, setContextMenu] = React.useState<{
    x: number;
    y: number;
    jointName: string;
    kfIdx: number;
  } | null>(null);

  const handleDragKeyframeEnd = (jointName: string, kfIdx: number, newTime: number) => {
    if (!model || !activeAnim) return;
    const updatedTracks = activeAnim.tracks.map((t) => {
      if (t.joint_name.toLowerCase() !== jointName.toLowerCase()) return t;
      const newKeyframes = t.keyframes.map((kf, idx) => {
        if (idx === kfIdx) {
          return { ...kf, time: newTime };
        }
        return kf;
      });
      newKeyframes.sort((a, b) => a.time - b.time);
      return { ...t, keyframes: newKeyframes };
    });
    const updatedAnim = { ...activeAnim, tracks: updatedTracks };
    const updatedAnims = (model.animations ?? []).map((a, i) => i === selectedAnimIdx ? updatedAnim : a);
    onModelChange?.({ ...model, animations: updatedAnims });
  };

  const handleCompileToMAD = async () => {
    if (!model || !activeAnim) return;
    try {
      const savedPath = await invoke<string | null>("save_mad_file", {
        model,
        animIdx: selectedAnimIdx,
      });
      if (savedPath) {
        alert(`.MAD file compiled and saved to:\n${savedPath}`);
      }
    } catch (e: any) {
      // Fallback: text stub if Rust command not yet available
      const lines: string[] = [
        `// HODEditorJS compiled .MAD`,
        `version 1.0`,
        `duration ${duration.toFixed(3)}`,
        `tracks ${activeAnim.tracks.length}`,
        "",
      ];
      activeAnim.tracks.forEach((track) => {
        lines.push(`track "${track.joint_name}" {`);
        track.keyframes.forEach((kf) => {
          const p = kf.position ?? { x: 0, y: 0, z: 0 };
          const q = kf.rotation ?? { x: 0, y: 0, z: 0, w: 1 };
          lines.push(
            `  key ${kf.time.toFixed(3)} pos [${p.x.toFixed(4)},${p.y.toFixed(4)},${p.z.toFixed(4)}] rot [${q.x.toFixed(4)},${q.y.toFixed(4)},${q.z.toFixed(4)},${q.w.toFixed(4)}]`
          );
        });
        lines.push(`}`);
        lines.push("");
      });
      const savedPath2 = await invoke<string | null>("save_text_file", {
        defaultName: `${model.name}_${activeAnim.name}.mad`,
        filters: ["mad"],
        contents: lines.join("\n"),
      });
      if (savedPath2) alert(`.MAD text stub saved to:\n${savedPath2}`);
    }
  };

  const handleExportGLTF = () => {
    if (!model || !activeAnim) return;
    const scene = new THREE.Scene();
    
    // Create bones for all joints
    const bones: Record<string, THREE.Bone> = {};
    model.joints.forEach(j => {
        const bone = new THREE.Bone();
        bone.name = j.name;
        
        const m = j.local_transform.m;
        const matrix = new THREE.Matrix4().set(
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2],
            m[0][3], m[1][3], m[2][3], m[3][3]
        );
        matrix.decompose(bone.position, bone.quaternion, bone.scale);
        bones[j.name] = bone;
    });

    let rootBone: THREE.Bone | null = null;
    model.joints.forEach(j => {
        if (j.parent_name && bones[j.parent_name]) {
            bones[j.parent_name].add(bones[j.name]);
        } else {
            if (!rootBone) rootBone = bones[j.name];
        }
    });

    if (rootBone) scene.add(rootBone);

    const tracks: THREE.KeyframeTrack[] = [];
    activeAnim.tracks.forEach(track => {
        if (track.keyframes.length === 0) return;
        const times = track.keyframes.map(k => k.time);
        
        const posValues: number[] = [];
        const rotValues: number[] = [];
        const scaleValues: number[] = [];

        track.keyframes.forEach(k => {
            const p = k.position || { x:0, y:0, z:0 };
            const q = k.rotation || { x:0, y:0, z:0, w:1 };
            const s = k.scale || { x:1, y:1, z:1 };
            posValues.push(p.x, p.y, p.z);
            rotValues.push(q.x, q.y, q.z, q.w);
            scaleValues.push(s.x, s.y, s.z);
        });

        tracks.push(new THREE.VectorKeyframeTrack(`${track.joint_name}.position`, times, posValues));
        tracks.push(new THREE.QuaternionKeyframeTrack(`${track.joint_name}.quaternion`, times, rotValues));
        tracks.push(new THREE.VectorKeyframeTrack(`${track.joint_name}.scale`, times, scaleValues));
    });

    const clip = new THREE.AnimationClip(activeAnim.name, activeAnim.duration, tracks);

    const exporter = new GLTFExporter();
    exporter.parse(scene, async (gltf) => {
        try {
            const savedPath = await invoke<string | null>("save_text_file", {
                defaultName: `${model.name}_${activeAnim.name}.gltf`,
                filters: ["gltf"],
                contents: JSON.stringify(gltf, null, 2),
            });
            if (savedPath) alert(`glTF animation saved to:\n${savedPath}`);
        } catch (err) {
            console.error("Failed to save GLTF file:", err);
            alert("Failed to save GLTF file");
        }
    }, (err) => {
        console.error("Failed to export GLTF:", err);
    }, { animations: [clip] });
  };

  const handleImportGLTF = async () => {
    try {
      const gltfContent = await invoke<string | null>("load_text_file", {
        filters: ["gltf"],
      });
      if (!gltfContent) return;

      const loader = new GLTFLoader();
      loader.parse(gltfContent, '', (gltf: any) => {
          if (!gltf.animations || gltf.animations.length === 0) {
              alert("No animations found in GLTF file.");
              return;
          }
          const clip = gltf.animations[0];
          
          const newAnim: HODAnimation = {
              name: clip.name || "Imported_Anim",
              duration: clip.duration,
              tracks: []
          };
          
          const trackMap = new Map<string, HODAnimationTrack>();
          clip.tracks.forEach((t: any) => {
              const [jointName, property] = t.name.split(".");
              if (!trackMap.has(jointName)) {
                  trackMap.set(jointName, { joint_name: jointName, keyframes: [] });
              }
              const hodTrack = trackMap.get(jointName)!;
              
              const times = Array.from(t.times as number[]);
              times.forEach((time, i) => {
                  let kf = hodTrack.keyframes.find(k => Math.abs(k.time - time) < 0.001);
                  if (!kf) {
                      kf = { time, position: {x:0, y:0, z:0}, rotation: {x:0, y:0, z:0, w:1}, scale: {x:1, y:1, z:1} };
                      hodTrack.keyframes.push(kf);
                  }
                  
                  const valIndex = i * t.getValueSize();
                  if (property === "position") {
                      kf.position = { x: t.values[valIndex], y: t.values[valIndex+1], z: t.values[valIndex+2] };
                  } else if (property === "quaternion") {
                      kf.rotation = { x: t.values[valIndex], y: t.values[valIndex+1], z: t.values[valIndex+2], w: t.values[valIndex+3] };
                  } else if (property === "scale") {
                      kf.scale = { x: t.values[valIndex], y: t.values[valIndex+1], z: t.values[valIndex+2] };
                  }
              });
          });
          
          trackMap.forEach(t => {
              t.keyframes.sort((a,b) => a.time - b.time);
              newAnim.tracks.push(t);
          });
          
          const updatedAnims = [...(model?.animations || []), newAnim];
          onModelChange?.({ ...model!, animations: updatedAnims });
          setSelectedAnimIdx(updatedAnims.length - 1);
      }, (err: any) => {
          console.error("Failed to parse GLTF:", err);
          alert("Failed to parse GLTF file");
      });
    } catch (err) {
      console.error("Failed to load GLTF file:", err);
      alert("Failed to load GLTF file");
    }
  };

  // ─── Ruler helpers ────────────────────────────────────────────────────────
  const tickCount = Math.max(4, Math.ceil(duration / 0.5));
  const ticks: number[] = [];
  for (let i = 0; i <= tickCount; i++) {
    ticks.push((i / tickCount) * duration);
  }

  const playheadPct = duration > 0 ? (currentTime / duration) * 100 : 0;

  // ─── Track colours ────────────────────────────────────────────────────────
  const TRACK_COLORS = [
    "#16a0ff", "#00e676", "#ffd600", "#ff4081",
    "#aa00ff", "#ff6d00", "#00bcd4", "#69f0ae",
  ];

  if (!model) return null;

  // ─── Render ───────────────────────────────────────────────────────────────
  return (
    <>
      {/* ── Main Dock Bar ── */}
      <div
        style={{
          background: "rgba(8, 16, 28, 0.97)",
          borderTop: "1px solid rgba(22, 160, 255, 0.22)",
          display: "flex",
          flexDirection: "column",
          flexShrink: 0,
          userSelect: "none",
          position: "relative",
          zIndex: 20,
        }}
      >
        {/* ── Top Controls Row ── */}
        <div
          style={{
            display: "flex",
            alignItems: "center",
            gap: "8px",
            padding: "6px 12px",
            borderBottom: "1px solid rgba(255,255,255,0.05)",
            flexWrap: "wrap",
          }}
        >
          {/* ─ Playback cluster ─ */}
          <div style={{ display: "flex", gap: "4px", alignItems: "center" }}>
            {/* Rewind */}
            <button
              title="Rewind to start"
              onClick={() => { setIsPlaying(false); setCurrentTime(0); }}
              style={btnStyle("#ffffff", "rgba(255,255,255,0.06)", "rgba(255,255,255,0.18)")}
            >
              ⏮
            </button>
            {/* Play/Pause */}
            <button
              title={isPlaying ? "Pause" : "Play"}
              onClick={() => setIsPlaying(!isPlaying)}
              style={btnStyle(
                isPlaying ? "#ff1744" : "#00e676",
                isPlaying ? "rgba(255,23,68,0.12)" : "rgba(0,230,118,0.12)",
                isPlaying ? "rgba(255,23,68,0.35)" : "rgba(0,230,118,0.35)",
              )}
            >
              {isPlaying ? "⏸" : "▶"}
            </button>
            {/* Stop */}
            <button
              title="Stop and reset"
              onClick={() => { setIsPlaying(false); setCurrentTime(0); }}
              style={btnStyle("#ccc", "rgba(255,255,255,0.06)", "rgba(255,255,255,0.18)")}
            >
              ⏹
            </button>
            {/* Loop */}
            <button
              title={loopPlayback ? "Looping ON" : "Looping OFF"}
              onClick={() => setLoopPlayback(!loopPlayback)}
              style={btnStyle(
                loopPlayback ? "#ffd600" : "#555",
                loopPlayback ? "rgba(255,214,0,0.12)" : "rgba(255,255,255,0.04)",
                loopPlayback ? "rgba(255,214,0,0.35)" : "rgba(255,255,255,0.1)",
              )}
            >
              🔁
            </button>
            {/* Speed */}
            <select
              value={playbackSpeed}
              onChange={(e) => setPlaybackSpeed(parseFloat(e.target.value))}
              title="Playback speed"
              style={{
                height: "22px", fontSize: "10px", background: "#080f1a",
                border: "1px solid rgba(255,255,255,0.15)", color: "#ccc",
                borderRadius: "4px", padding: "0 4px", cursor: "pointer",
              }}
            >
              <option value={0.25}>0.25×</option>
              <option value={0.5}>0.5×</option>
              <option value={1.0}>1×</option>
              <option value={2.0}>2×</option>
            </select>
          </div>

          {/* ─ Divider ─ */}
          <div style={{ width: "1px", height: "20px", background: "rgba(255,255,255,0.1)" }} />

          {/* ─ Animation selector ─ */}
          <div style={{ display: "flex", gap: "4px", alignItems: "center" }}>
            {hasAnims && (
              <select
                value={selectedAnimIdx}
                onChange={(e) => {
                  setIsPlaying(false);
                  setCurrentTime(0);
                  setSelectedAnimIdx(parseInt(e.target.value, 10));
                }}
                style={{
                  height: "22px", fontSize: "11px", background: "#080f1a",
                  border: "1px solid rgba(22,160,255,0.35)", color: "#16a0ff",
                  borderRadius: "4px", padding: "0 6px", fontWeight: "600", cursor: "pointer",
                  maxWidth: "180px",
                }}
              >
                {model.animations!.map((anim, idx) => (
                  <option key={anim.name} value={idx}>{anim.name}</option>
                ))}
              </select>
            )}
            {!hasAnims && (
              <span style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic" }}>
                No animations
              </span>
            )}

            {/* Time readout */}
            <span
              style={{
                fontSize: "10px", fontFamily: "var(--font-mono)", color: "var(--accent-cyan)",
                background: "rgba(22,160,255,0.08)", border: "1px solid rgba(22,160,255,0.2)",
                borderRadius: "4px", padding: "2px 6px", whiteSpace: "nowrap",
              }}
            >
              {currentTime.toFixed(2)}s / {duration.toFixed(2)}s
            </span>
          </div>

          {/* ─ Divider ─ */}
          <div style={{ width: "1px", height: "20px", background: "rgba(255,255,255,0.1)" }} />

          {/* ─ Edit cluster ─ */}
          <div style={{ display: "flex", gap: "4px", alignItems: "center" }}>


            {/* Add Track */}
            {hasAnims && (
              <select
                value=""
                onChange={(e) => { handleAddTrack(e.target.value); e.target.value = ""; }}
                title="Add joint track to current animation"
                style={{
                  height: "22px", fontSize: "10px", background: "#080f1a",
                  border: "1px solid rgba(255,255,255,0.15)", color: "#ccc",
                  borderRadius: "4px", padding: "0 4px", cursor: "pointer",
                }}
              >
                <option value="">＋ Add Track…</option>
                {model.joints
                  .filter((j) => {
                    const existing = new Set(activeAnim?.tracks.map((t) => t.joint_name.toLowerCase()) ?? []);
                    return !existing.has(j.name.toLowerCase());
                  })
                  .map((j) => (
                    <option key={j.name} value={j.name}>{j.name}</option>
                  ))}
              </select>
            )}

            {hasAnims && selectedNode?.type === "joint" && (
              <button
                title={`Record current transform of joint "${selectedNode.name}" as a keyframe at ${currentTime.toFixed(2)}s`}
                onClick={handleAddKeyframe}
                style={btnStyle("#ffab00", "rgba(255,171,0,0.12)", "rgba(255,171,0,0.35)")}
              >
                ✚ Record Keyframe
              </button>
            )}

            {/* Compile .MAD */}
            {hasAnims && (
              <button
                title="Compile animation to binary .mad file"
                onClick={handleCompileToMAD}
                style={btnStyle("#16a0ff", "rgba(22,160,255,0.12)", "rgba(22,160,255,0.35)")}
              >
                💾 Compile .MAD
              </button>
            )}

            {/* Export GLTF */}
            {hasAnims && (
              <button
                title="Export animation as glTF"
                onClick={handleExportGLTF}
                style={btnStyle("#00e676", "rgba(0,230,118,0.12)", "rgba(0,230,118,0.35)")}
              >
                ⬇ Export glTF
              </button>
            )}

            {/* Import GLTF */}
            <button
              title="Import animation from glTF"
              onClick={handleImportGLTF}
              style={btnStyle("#00e676", "rgba(0,230,118,0.12)", "rgba(0,230,118,0.35)")}
            >
              ⬆ Import glTF
            </button>
          </div>
        </div>

        {/* ── Timeline Ruler + Track Rows ── */}
        {hasAnims && activeAnim && (
          <div style={{ display: "flex", flexDirection: "column" }}>
            {/* Ruler */}
            <div
              ref={rulerRef}
              onMouseDown={handleRulerMouseDown}
              style={{
                position: "relative",
                height: "24px",
                background: "rgba(0,0,0,0.4)",
                cursor: "crosshair",
                borderBottom: "1px solid rgba(255,255,255,0.07)",
                overflow: "hidden",
              }}
            >
              {/* Tick marks */}
              {ticks.map((t, i) => (
                <div
                  key={i}
                  style={{
                    position: "absolute",
                    left: `${(t / duration) * 100}%`,
                    top: 0,
                    bottom: 0,
                    display: "flex",
                    flexDirection: "column",
                    alignItems: "center",
                    pointerEvents: "none",
                  }}
                >
                  <div
                    style={{
                      width: "1px",
                      height: i % 2 === 0 ? "10px" : "6px",
                      background: "rgba(255,255,255,0.2)",
                      marginTop: "auto",
                    }}
                  />
                  {i % 2 === 0 && (
                    <span
                      style={{
                        fontSize: "8px",
                        color: "rgba(255,255,255,0.35)",
                        fontFamily: "var(--font-mono)",
                        position: "absolute",
                        bottom: "2px",
                        transform: "translateX(-50%)",
                        whiteSpace: "nowrap",
                      }}
                    >
                      {t.toFixed(1)}s
                    </span>
                  )}
                </div>
              ))}

              {/* Playhead */}
              <div
                style={{
                  position: "absolute",
                  left: `${playheadPct}%`,
                  top: 0,
                  bottom: 0,
                  width: "2px",
                  background: "#ff1744",
                  boxShadow: "0 0 6px rgba(255,23,68,0.8)",
                  pointerEvents: "none",
                  zIndex: 5,
                }}
              />
            </div>

            {/* Track lanes */}
            {activeAnim.tracks.map((track, trackIdx) => {
              const color = TRACK_COLORS[trackIdx % TRACK_COLORS.length];
              return (
                <div
                  key={track.joint_name}
                  style={{
                    position: "relative",
                    height: "22px",
                    display: "flex",
                    alignItems: "center",
                    borderBottom: "1px solid rgba(255,255,255,0.04)",
                    background: trackIdx % 2 === 0 ? "rgba(0,0,0,0.15)" : "transparent",
                  }}
                >
                  {/* Joint label */}
                  <div
                    style={{
                      width: "120px",
                      flexShrink: 0,
                      fontSize: "9px",
                      fontWeight: "600",
                      color,
                      padding: "0 6px",
                      overflow: "hidden",
                      textOverflow: "ellipsis",
                      whiteSpace: "nowrap",
                      borderRight: `1px solid ${color}30`,
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "space-between",
                    }}
                  >
                    <span style={{ overflow: "hidden", textOverflow: "ellipsis" }}>{track.joint_name}</span>
                    <button
                      title={`Remove track "${track.joint_name}"`}
                      onClick={() => handleDeleteTrack(track.joint_name)}
                      style={{
                        background: "transparent",
                        border: "none",
                        color: "rgba(255,100,100,0.5)",
                        cursor: "pointer",
                        fontSize: "9px",
                        padding: "0 2px",
                        flexShrink: 0,
                        lineHeight: 1,
                      }}
                    >
                      ×
                    </button>
                  </div>

                  {/* Keyframe markers lane */}
                  <div
                    onClick={(e) => {
                      const rect = e.currentTarget.getBoundingClientRect();
                      const ratio = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
                      const clickedTime = ratio * duration;
                      recordKeyframeForJoint(track.joint_name, clickedTime);
                    }}
                    style={{
                      flex: 1,
                      position: "relative",
                      height: "100%",
                      overflow: "hidden",
                      cursor: "pointer",
                    }}
                  >
                    {track.keyframes.map((kf, kfIdx) => {
                      const isSelected = selectedNode?.type === "keyframe" && selectedNode.name === `${track.joint_name}:${kfIdx}`;
                      return (
                        <KeyframeDiamond
                          key={kfIdx}
                          kf={kf}
                          kfIdx={kfIdx}
                          color={color}
                          currentTime={currentTime}
                          duration={duration}
                          onSeek={(t) => { setIsPlaying(false); setCurrentTime(t); }}
                          onDelete={() => handleDeleteKeyframe(track.joint_name, kfIdx)}
                          onDragEnd={(newTime) => handleDragKeyframeEnd(track.joint_name, kfIdx, newTime)}
                          onSelect={() => onSelectedNodeChange?.({ type: "keyframe", name: `${track.joint_name}:${kfIdx}` })}
                          isSelected={isSelected}
                          onContextMenu={(e) => {
                            setContextMenu({
                              x: e.clientX,
                              y: e.clientY,
                              jointName: track.joint_name,
                              kfIdx,
                            });
                          }}
                        />
                      );
                    })}

                    {/* Playhead overlay on lane */}
                    <div
                      style={{
                        position: "absolute",
                        left: `${playheadPct}%`,
                        top: 0,
                        bottom: 0,
                        width: "1px",
                        background: "rgba(255,23,68,0.5)",
                        pointerEvents: "none",
                        zIndex: 4,
                      }}
                    />
                  </div>
                </div>
              );
            })}

            {/* Empty state if no tracks */}
            {activeAnim.tracks.length === 0 && (
              <div
                style={{
                  height: "30px",
                  display: "flex",
                  alignItems: "center",
                  paddingLeft: "130px",
                  fontSize: "10px",
                  color: "rgba(255,255,255,0.2)",
                  fontStyle: "italic",
                }}
              >
                No tracks — use "＋ Add Track…" to add a joint channel
              </div>
            )}
          </div>
        )}
      </div>


      {contextMenu && (
        <div
          style={{
            position: "fixed",
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            zIndex: 1000,
          }}
          onClick={() => setContextMenu(null)}
          onContextMenu={(e) => {
            e.preventDefault();
            setContextMenu(null);
          }}
        >
          <div
            style={{
              position: "fixed",
              left: `${contextMenu.x}px`,
              top: `${contextMenu.y}px`,
              background: "rgba(10, 20, 35, 0.95)",
              border: "1px solid rgba(22, 160, 255, 0.4)",
              borderRadius: "4px",
              boxShadow: "0 4px 12px rgba(0,0,0,0.5)",
              padding: "4px 0",
              minWidth: "120px",
              zIndex: 1001,
            }}
            onClick={(e) => e.stopPropagation()}
          >
            <button
              onClick={() => {
                handleDeleteKeyframe(contextMenu.jointName, contextMenu.kfIdx);
                setContextMenu(null);
              }}
              style={{
                width: "100%",
                background: "transparent",
                border: "none",
                color: "#ff4040",
                padding: "6px 12px",
                textAlign: "left",
                fontSize: "11px",
                cursor: "pointer",
                fontWeight: "600",
                transition: "background 0.15s",
              }}
              onMouseEnter={(e) => (e.currentTarget.style.background = "rgba(255, 64, 64, 0.15)")}
              onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            >
              🗑 Delete Keyframe
            </button>
          </div>
        </div>
      )}
    </>
  );
};

// ─── Keyframe Diamond sub-component ──────────────────────────────────────────

interface KeyframeDiamondProps {
  kf: HODKeyframe;
  kfIdx: number;
  color: string;
  currentTime: number;
  duration: number;
  onSeek: (t: number) => void;
  onDelete: () => void;
  onDragEnd: (newTime: number) => void;
  onContextMenu: (e: React.MouseEvent, kfIdx: number) => void;
  onSelect: () => void;
  isSelected: boolean;
}

const KeyframeDiamond: React.FC<KeyframeDiamondProps> = ({
  kf, kfIdx, color, currentTime, duration, onSeek, onDelete, onDragEnd, onContextMenu, onSelect, isSelected,
}) => {
  const [hovered, setHovered] = React.useState(false);
  const [isDragging, setIsDragging] = React.useState(false);
  const [dragTime, setDragTime] = React.useState<number | null>(null);

  const isNearPlayhead = Math.abs(kf.time - currentTime) < 0.05;
  const p = kf.position;
  const q = kf.rotation;

  const visualTime = dragTime !== null ? dragTime : kf.time;
  const visualPct = duration > 0 ? (visualTime / duration) * 100 : 0;

  const handleMouseDown = (e: React.MouseEvent) => {
    if (e.button !== 0) return;
    e.stopPropagation();
    setIsDragging(true);
    setDragTime(kf.time);

    const diamondEl = e.currentTarget as HTMLElement;
    const laneEl = diamondEl.parentElement;
    if (!laneEl) return;

    const onMouseMove = (mv: MouseEvent) => {
      const rect = laneEl.getBoundingClientRect();
      const ratio = Math.max(0, Math.min(1, (mv.clientX - rect.left) / rect.width));
      const newTime = ratio * duration;
      setDragTime(newTime);
    };

    const onMouseUp = (mv: MouseEvent) => {
      window.removeEventListener("mousemove", onMouseMove);
      window.removeEventListener("mouseup", onMouseUp);
      setIsDragging(false);
      setDragTime(null);

      const rect = laneEl.getBoundingClientRect();
      const ratio = Math.max(0, Math.min(1, (mv.clientX - rect.left) / rect.width));
      const finalTime = ratio * duration;
      onDragEnd(finalTime);
    };

    window.addEventListener("mousemove", onMouseMove);
    window.addEventListener("mouseup", onMouseUp);
  };

  return (
    <div
      style={{
        position: "absolute",
        left: `${visualPct}%`,
        top: "50%",
        transform: "translate(-50%, -50%)",
        zIndex: 3,
        cursor: "pointer",
      }}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      onMouseDown={handleMouseDown}
      onContextMenu={(e) => {
        e.preventDefault();
        e.stopPropagation();
        onContextMenu(e, kfIdx);
      }}
      onClick={(e) => {
        e.stopPropagation();
        if (e.shiftKey || e.altKey) {
          onDelete();
        } else {
          onSeek(kf.time);
          onSelect();
        }
      }}
    >
      <div
        style={{
          width: "8px", height: "8px",
          background: isSelected ? "#ffd600" : isNearPlayhead ? "#ffffff" : color,
          boxShadow: isSelected
            ? `0 0 10px #ffd600`
            : isNearPlayhead
            ? `0 0 8px ${color}`
            : hovered ? `0 0 6px ${color}` : `0 0 3px ${color}80`,
          transform: "rotate(45deg)",
          transition: "all 0.1s",
          border: isSelected ? "1px solid #ffffff" : hovered ? `1px solid #fff` : `1px solid ${color}60`,
        }}
      />
      {hovered && !isDragging && (
        <div
          style={{
            position: "absolute",
            bottom: "16px",
            left: "50%",
            transform: "translateX(-50%)",
            background: "rgba(6, 12, 24, 0.98)",
            border: `1px solid ${color}`,
            borderRadius: "6px",
            padding: "8px 12px",
            boxShadow: `0 4px 20px rgba(0,0,0,0.8), 0 0 10px ${color}30`,
            zIndex: 100,
            pointerEvents: "none",
            minWidth: "180px",
            display: "flex",
            flexDirection: "column",
            gap: "4px",
            textAlign: "left",
          }}
        >
          <div style={{ display: "flex", justifyContent: "space-between", borderBottom: "1px solid rgba(255,255,255,0.1)", paddingBottom: "4px", marginBottom: "4px" }}>
            <span style={{ fontSize: "10px", fontWeight: "700", color: "#fff" }}>KEYFRAME DATA</span>
            <span style={{ fontSize: "10px", fontFamily: "var(--font-mono)", color: "var(--accent-cyan)", fontWeight: "700" }}>{visualTime.toFixed(3)}s</span>
          </div>
          {p && (
            <div style={{ display: "flex", flexDirection: "column", gap: "2px" }}>
              <span style={{ fontSize: "8px", color: "rgba(255,255,255,0.4)", fontWeight: "600" }}>POSITION</span>
              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: "4px", fontFamily: "var(--font-mono)", fontSize: "9px", color: "#fff" }}>
                <div><span style={{ color: "rgba(255,100,100,0.8)" }}>X:</span> {p.x.toFixed(3)}</div>
                <div><span style={{ color: "rgba(100,255,100,0.8)" }}>Y:</span> {p.y.toFixed(3)}</div>
                <div><span style={{ color: "rgba(100,100,255,0.8)" }}>Z:</span> {p.z.toFixed(3)}</div>
              </div>
            </div>
          )}
          {q && (
            <div style={{ display: "flex", flexDirection: "column", gap: "2px", marginTop: "4px" }}>
              <span style={{ fontSize: "8px", color: "rgba(255,255,255,0.4)", fontWeight: "600" }}>ROTATION (QUATERNION)</span>
              <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "4px", fontFamily: "var(--font-mono)", fontSize: "9px", color: "#fff" }}>
                <div><span style={{ color: "rgba(255,100,100,0.8)" }}>X:</span> {q.x.toFixed(3)}</div>
                <div><span style={{ color: "rgba(100,255,100,0.8)" }}>Y:</span> {q.y.toFixed(3)}</div>
                <div><span style={{ color: "rgba(100,100,255,0.8)" }}>Z:</span> {q.z.toFixed(3)}</div>
                <div><span style={{ color: "rgba(255,255,100,0.8)" }}>W:</span> {q.w.toFixed(3)}</div>
              </div>
            </div>
          )}
          <div style={{ fontSize: "8px", color: "rgba(255,255,255,0.3)", fontStyle: "italic", marginTop: "4px", borderTop: "1px solid rgba(255,255,255,0.05)", paddingTop: "4px", textAlign: "center" }}>
            Drag to move · Right-click to delete
          </div>
        </div>
      )}
    </div>
  );
};

// ─── Style helpers ────────────────────────────────────────────────────────────

const btnStyle = (color: string, bg: string, border: string): React.CSSProperties => ({
  background: bg,
  color,
  border: `1px solid ${border}`,
  borderRadius: "4px",
  padding: "3px 8px",
  fontSize: "11px",
  fontWeight: "600",
  cursor: "pointer",
  whiteSpace: "nowrap",
  lineHeight: "16px",
  transition: "all 0.15s",
});
