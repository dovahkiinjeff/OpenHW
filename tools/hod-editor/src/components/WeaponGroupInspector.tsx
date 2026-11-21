import { useState } from "react";
import { Move, RefreshCw, Crosshair, Info, Plus, Trash2 } from "lucide-react";
import { HODModel } from "./Viewport";
import { NumericInput } from "./Inspector";
import {
  CoordinateSpace,
  getDisplayTransform,
  applyPositionEdit,
  applyRotationEdit,
  matrixRotationToHodRadians,
} from "../transform_utils";
import { invoke } from "../bridge";
import {
  getSlaveBases,
  nextSlaveBase,
  isJointInWeaponBase,
  cloneWeaponFamilyJoints,
  renameWeaponFamilyName,
  muzzleIndex,
  muzzleName,
  nextMuzzleIndex,
} from "../weapon_utils";

export function WeaponGroupInspector({
  model,
  selectedNode,
  onModelChange,
  onPositionChange,
  onSelectedNodeChange,
  coordinateSpace = "local",
}: {
  model: HODModel;
  selectedNode: { type: string; name: string };
  onModelChange?: (model: HODModel) => void;
  onPositionChange: (name: string, type: string, pos: { x: number; y: number; z: number }, localMatrix?: { m: number[][] }) => void;
  onSelectedNodeChange?: (node: { type: string; name: string } | null) => void;
  coordinateSpace?: CoordinateSpace;
}) {
  const [renameWeaponName, setRenameWeaponName] = useState(selectedNode.name);
  const baseName = selectedNode.name;

  const getJoint = (suffix: string) => {
    const jointName = suffix ? `${baseName}${suffix}` : baseName;
    return model.joints.find(j => j.name.toLowerCase() === jointName.toLowerCase());
  };

  const posJoint = getJoint("_Position");
  const dirJoint = getJoint("_Direction");
  const latJoint = getJoint("_Latitude");
  const restJoint = getJoint("_Rest");
  const barrelJoint = getJoint("_Barrel");

  const muzzleJoints = model.joints
    .filter(j => muzzleIndex(j.name, baseName) >= 0)
    .sort((a, b) => muzzleIndex(a.name, baseName) - muzzleIndex(b.name, baseName));

  const slaveBases = getSlaveBases(model, baseName);
  const familyBases = [baseName, ...slaveBases];
  const slaveMatch = baseName.match(/^(.*?)(Slave\d*)$/i);
  const slaveParentBase = slaveMatch ? slaveMatch[1] : null;

  const hasDirection = !!dirJoint;
  const hasLatitude = !!latJoint;
  const hasMuzzle = muzzleJoints.length > 0;
  const hasRest = !!restJoint;
  const hasBarrel = !!barrelJoint;

  const muzzleParent = () => (hasLatitude ? `${baseName}_Latitude` : `${baseName}_Position`);

  const handleToggleJoint = (suffix: string, _defaultParentSuffix: string, defaultOffset: { x: number; y: number; z: number }) => {
    if (!model) return;
    const isAdding = !getJoint(suffix);
    let jointsToSave = [...model.joints];

    if (isAdding) {
      const jointName = `${baseName}${suffix}`;
      let parentName = `${baseName}${_defaultParentSuffix}`;

      if (suffix === "_Muzzle" || suffix === "_Barrel") {
        parentName = muzzleParent();
      }

      jointsToSave.push({
        name: jointName,
        parent_name: parentName,
        local_transform: {
          m: [
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [defaultOffset.x, defaultOffset.y, defaultOffset.z, 1]
          ]
        },
        rotation: { x: 0, y: 0, z: 0 },
      });

      // If adding Latitude and muzzles exist, reparent muzzles to Latitude
      if (suffix === "_Latitude" && hasMuzzle) {
        const muzzleNames = new Set(muzzleJoints.map(m => m.name));
        jointsToSave = jointsToSave.map(j =>
          muzzleNames.has(j.name) ? { ...j, parent_name: jointName } : j
        );
      }
    } else {
      const jointToRemove = getJoint(suffix);
      if (suffix === "_Muzzle") {
        // Removing the muzzle component removes every muzzle joint, including
        // additional `${base}_MuzzleN` barrels.
        const muzzleNames = new Set(muzzleJoints.map(m => m.name));
        jointsToSave = jointsToSave.filter(j => !muzzleNames.has(j.name));
      } else if (jointToRemove) {
        jointsToSave = jointsToSave.filter(j => j.name !== jointToRemove.name);
      }

      // If removing Latitude and muzzles still exist, reparent them back to Position
      if (suffix === "_Latitude" && hasMuzzle) {
        const muzzleNames = new Set(muzzleJoints.map(m => m.name));
        jointsToSave = jointsToSave.map(j =>
          muzzleNames.has(j.name) ? { ...j, parent_name: `${baseName}_Position` } : j
        );
      }
    }

    onModelChange?.({ ...model, joints: jointsToSave });
  };

  const handleAddMuzzle = () => {
    if (!model) return;
    const index = nextMuzzleIndex(model, baseName);
    const name = muzzleName(baseName, index);
    onModelChange?.({
      ...model,
      joints: [
        ...model.joints,
        {
          name,
          parent_name: muzzleParent(),
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
  };

  const handleRemoveMuzzle = (jointName: string) => {
    if (!model) return;
    onModelChange?.({
      ...model,
      joints: model.joints.filter(j => j.name !== jointName),
    });
  };

  const handleAddSlave = () => {
    if (!model) return;

    const slaveBase = nextSlaveBase(model, baseName);
    const newJoints = cloneWeaponFamilyJoints(model, baseName, slaveBase);
    if (newJoints.length === 0) return;

    onModelChange?.({ ...model, joints: [...model.joints, ...newJoints] });
    onSelectedNodeChange?.({ type: "weapon_group", name: slaveBase });
    invoke("log_event", {
      level: "INFO",
      message: `Added slave barrel assembly ${slaveBase} to ${baseName}`,
    }).catch(console.error);
  };

  const handleRemoveSlave = (slaveBase: string) => {
    if (!model) return;
    const names = new Set(
      model.joints
        .filter((j) => isJointInWeaponBase(j.name, slaveBase))
        .map((j) => j.name)
    );
    onModelChange?.({
      ...model,
      joints: model.joints.filter((j) => !names.has(j.name)),
    });
    invoke("log_event", {
      level: "INFO",
      message: `Removed slave barrel assembly ${slaveBase} from ${baseName}`,
    }).catch(console.error);
  };

  const handleRenameWeaponGroup = () => {
    const nextName = renameWeaponName.trim();
    if (!nextName || nextName === baseName) return;

    const updatedJoints = model.joints.map((joint) => {
      return {
        ...joint,
        name: renameWeaponFamilyName(joint.name, familyBases, baseName, nextName),
        parent_name: joint.parent_name
          ? renameWeaponFamilyName(joint.parent_name, familyBases, baseName, nextName)
          : joint.parent_name,
      };
    });

    const updatedMeshes = model.meshes.map((mesh) => {
      return {
        ...mesh,
        parent_name: renameWeaponFamilyName(mesh.parent_name, familyBases, baseName, nextName),
      };
    });

    onModelChange?.({
      ...model,
      joints: updatedJoints,
      meshes: updatedMeshes
    });

    invoke("log_event", { level: "INFO", message: `Renamed weapon group from ${baseName} to ${nextName}` }).catch(console.error);
    selectedNode.name = nextName;
    setRenameWeaponName(nextName);
  };

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

  const renderJointCard = (label: string, jointObj: any, extraAction?: React.ReactNode) => {
    if (!jointObj) return null;
    const display = getDisplayTransform(model, "joint", jointObj.name, coordinateSpace);
    const pos = display.position;
    const rot = display.rotation;

    return (
      <div key={jointObj.name} style={{ background: "rgba(255,255,255,0.02)", padding: "12px", borderRadius: "6px", border: "1px solid var(--border-color)" }}>
        <div style={{ fontSize: "12px", fontWeight: "600", color: "var(--accent-cyan)", marginBottom: "8px", display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <span>{label}</span>
          <span style={{ display: "flex", alignItems: "center", gap: "8px" }}>
            <span style={{ color: "var(--text-muted)", fontSize: "10px" }}>{jointObj.name}</span>
            {extraAction}
          </span>
        </div>

        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "4px", fontSize: "10px", color: "var(--text-secondary)", marginBottom: "4px" }}>
              <Move size={12} /> Position
            </div>
            <div style={{ display: "flex", gap: "4px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis} style={{ flex: 1 }}>
                  <NumericInput
                    step="1"
                    value={pos[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleWeaponJointCoordChange(jointObj.name, axis as "x" | "y" | "z", val)}
                    style={{ height: "26px", fontSize: "11px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 6px", width: "100%" }}
                  />
                </div>
              ))}
            </div>
          </div>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "4px", fontSize: "10px", color: "var(--text-secondary)", marginBottom: "4px" }}>
              <RefreshCw size={12} /> Rotation
            </div>
            <div style={{ display: "flex", gap: "4px" }}>
              {["x", "y", "z"].map((axis) => (
                <div key={axis} style={{ flex: 1 }}>
                  <NumericInput
                    step="1"
                    value={rot[axis as "x" | "y" | "z"]}
                    onChange={(val) => handleWeaponJointRotationChange(jointObj.name, axis as "x" | "y" | "z", val)}
                    style={{ height: "26px", fontSize: "11px", fontFamily: "var(--font-mono)", background: "#050a12", border: "1px solid var(--border-color)", color: "#fff", padding: "0 6px", width: "100%" }}
                  />
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    );
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: "16px" }}>
      <div>
        <div style={{ fontSize: "10px", textTransform: "uppercase", letterSpacing: "0.15em", color: "var(--text-muted)", marginBottom: "4px", display: "flex", alignItems: "center", gap: "6px" }}>
          <Crosshair size={12} /> Unified Weapon Assembly
        </div>
        <div style={{ display: "flex", gap: "8px", alignItems: "center", marginTop: "8px" }}>
          <input
            value={renameWeaponName}
            onChange={(e) => setRenameWeaponName(e.target.value)}
            style={{ fontSize: "14px", fontWeight: "600", color: "var(--accent-cyan)", background: "rgba(22, 160, 255, 0.05)", border: "1px solid var(--border-color)", borderRadius: "4px", padding: "4px 8px", height: "30px", flex: 1 }}
          />
          <button onClick={handleRenameWeaponGroup} className="secondary" style={{ fontSize: "11px", height: "30px", padding: "0 10px" }}>Rename</button>
        </div>
      </div>

      <hr style={{ border: "none", borderTop: "1px solid var(--border-color)", margin: 0 }} />

      <div style={{ display: "flex", flexDirection: "column", gap: "8px", background: "rgba(255,255,255,0.02)", padding: "12px", borderRadius: "4px", border: "1px solid var(--border-color)" }}>
        <div style={{ fontSize: "11px", fontWeight: "600", color: "var(--text-secondary)", marginBottom: "4px" }}>Toggle Weapon Components</div>

        {!posJoint && <div style={{ color: "var(--accent-red)", fontSize: "11px" }}>Error: Weapon Position joint is missing!</div>}

        <div style={{ display: "flex", gap: "8px", flexWrap: "wrap" }}>
          {hasMuzzle ?
            <button onClick={() => handleToggleJoint("_Muzzle", "_Position", { x: 0, y: 0, z: 5.0 })} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Muzzles</button> :
            <button onClick={handleAddMuzzle} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Muzzle (Spawn Point)</button>}

          {hasDirection ?
            <button onClick={() => handleToggleJoint("_Direction", "_Position", { x: 0, y: 5.0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Direction</button> :
            <button onClick={() => handleToggleJoint("_Direction", "_Position", { x: 0, y: 5.0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Direction (Up Vector)</button>}

          {hasLatitude ?
            <button onClick={() => handleToggleJoint("_Latitude", "_Position", { x: 0, y: 5.0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Latitude</button> :
            <button onClick={() => handleToggleJoint("_Latitude", "_Position", { x: 0, y: 5.0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Latitude (Pitch Axis)</button>}

          {hasRest ?
            <button onClick={() => handleToggleJoint("_Rest", "_Position", { x: 0, y: 0, z: 5.0 })} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Rest</button> :
            <button onClick={() => handleToggleJoint("_Rest", "_Position", { x: 0, y: 0, z: 5.0 })} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Rest (Yaw/Idle Vector)</button>}

          {hasBarrel ?
            <button onClick={() => handleToggleJoint("_Barrel", "_Latitude", { x: 0, y: 0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px", background: "rgba(255, 50, 50, 0.2)", borderColor: "rgba(255, 50, 50, 0.5)" }}>- Remove Barrel</button> :
            <button onClick={() => handleToggleJoint("_Barrel", "_Latitude", { x: 0, y: 0, z: 0 })} style={{ fontSize: "11px", padding: "4px 8px" }}>+ Add Barrel (Unverified)</button>}
        </div>

        <div style={{ display: "flex", alignItems: "flex-start", gap: "6px", fontSize: "10px", color: "var(--text-muted)", lineHeight: "1.5", marginTop: "4px" }}>
          <Info size={12} style={{ flexShrink: 0, marginTop: "1px" }} />
          <span>
            Barrel joints are not present in vanilla HWRM ship HODs and their runtime behavior is unverified.
            Treat them as optional; the standard turret chain is Position → Latitude → Muzzle.
          </span>
        </div>
      </div>

      <div style={{ display: "flex", flexDirection: "column", gap: "8px", background: "rgba(255,255,255,0.02)", padding: "12px", borderRadius: "4px", border: "1px solid var(--border-color)" }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
          <span style={{ fontSize: "11px", fontWeight: "600", textTransform: "uppercase", color: "var(--text-secondary)" }}>
            {slaveParentBase ? "Slave Assembly" : `Slave Barrels (${slaveBases.length})`}
          </span>
          {!slaveParentBase && (
            <button onClick={handleAddSlave} style={{ fontSize: "11px", padding: "4px 8px", display: "flex", alignItems: "center", gap: "4px" }}>
              <Plus size={12} /> Add Slave Barrel
            </button>
          )}
          {slaveParentBase && (
            <button onClick={() => onSelectedNodeChange?.({ type: "weapon_group", name: slaveParentBase })} style={{ fontSize: "10px", padding: "3px 6px" }}>
              Edit parent weapon
            </button>
          )}
        </div>

        <div style={{ display: "flex", alignItems: "flex-start", gap: "6px", fontSize: "10px", color: "var(--text-muted)", lineHeight: "1.5" }}>
          <Info size={12} style={{ flexShrink: 0, marginTop: "1px" }} />
          <span>
            {slaveParentBase ? (
              <>
                This is a slave assembly of <code>{slaveParentBase}</code>. Vanilla HWRM binds only the main weapon in the
                <code>.ship</code>; the engine detects the <code>Slave</code> suffix, and the <code>.wepn</code>
                <code>setMiscValues</code> slave delay fires it.
              </>
            ) : (
              <>
                Vanilla HWRM multi-barrel weapons use a parallel <code>{baseName}Slave</code> / <code>{baseName}Slave1</code> chain.
                Only the main weapon is bound in the <code>.ship</code>; the <code>.wepn</code> <code>setMiscValues</code> slave delay fires it.
                A few vanilla subsystem launchers also use <code>_MuzzleN</code> joints (observed indices 0, 2, 3), so keep the authored numbering.
              </>
            )}
          </span>
        </div>

        {!slaveParentBase && slaveBases.length === 0 && (
          <div style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic" }}>
            No slave barrel assemblies. Add one to mirror the main weapon onto a second barrel.
          </div>
        )}

        {slaveBases.map((slaveBase) => {
          const slaveJoints = model.joints.filter((j) => isJointInWeaponBase(j.name, slaveBase));
          return (
            <div key={slaveBase} style={{ display: "flex", alignItems: "center", justifyContent: "space-between", gap: "8px", padding: "6px 8px", background: "rgba(0,0,0,0.15)", border: "1px solid var(--border-color)", borderRadius: "4px" }}>
              <span style={{ fontSize: "11px", fontFamily: "var(--font-mono)", color: "var(--accent-cyan)" }}>
                {slaveBase} <span style={{ color: "var(--text-muted)" }}>({slaveJoints.length} joints)</span>
              </span>
              <span style={{ display: "flex", alignItems: "center", gap: "4px" }}>
                <button onClick={() => onSelectedNodeChange?.({ type: "weapon_group", name: slaveBase })} style={{ fontSize: "10px", padding: "3px 6px" }}>
                  Edit
                </button>
                <button onClick={() => handleRemoveSlave(slaveBase)} title={`Remove ${slaveBase}`} style={{ padding: "2px 4px", background: "transparent", border: "none", color: "#f44", cursor: "pointer", display: "inline-flex" }}>
                  <Trash2 size={12} />
                </button>
              </span>
            </div>
          );
        })}
      </div>

      <div style={{ display: "flex", flexDirection: "column", gap: "12px" }}>
        {renderJointCard("Position (Pivot / Yaw)", posJoint)}
        {renderJointCard("Direction (Up Vector / Heading)", dirJoint)}
        {renderJointCard("Latitude (Pitch Axis)", latJoint)}
        {renderJointCard("Barrel (Optional / Unverified)", barrelJoint)}

        <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
            <span style={{ fontSize: "11px", fontWeight: "600", textTransform: "uppercase", color: "var(--text-secondary)" }}>
              Muzzles ({muzzleJoints.length})
            </span>
            <button onClick={handleAddMuzzle} style={{ fontSize: "11px", padding: "4px 8px", display: "flex", alignItems: "center", gap: "4px" }}>
              <Plus size={12} /> Add Muzzle
            </button>
          </div>
          {muzzleJoints.length === 0 && (
            <div style={{ fontSize: "11px", color: "var(--text-muted)", fontStyle: "italic" }}>
              No muzzle joints. The weapon will fire from the Position joint.
            </div>
          )}
          {muzzleJoints.map((m, index) => (
            renderJointCard(
              index === 0 ? "Muzzle (Primary Projectile Spawn)" : `Muzzle ${index} (Additional Projectile Spawn)`,
              m,
              <button
                onClick={() => handleRemoveMuzzle(m.name)}
                title={`Remove ${m.name}`}
                style={{ padding: "2px 4px", background: "transparent", border: "none", color: "#f44", cursor: "pointer", display: "inline-flex" }}
              >
                <Trash2 size={12} />
              </button>
            )
          ))}
        </div>

        {renderJointCard("Rest (Idle Vector)", restJoint)}
      </div>
    </div>
  );
}
