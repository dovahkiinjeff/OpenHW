import React from "react";
import { FolderOpen, Save, Hammer, FilePlus, Settings } from "lucide-react";

interface ToolbarProps {
  modelName: string;
  onGameModels: () => void;
  onReturn: () => void;
  onSubsystems?: () => void;
  onOpenClick: () => void;
  onSaveClick: () => void;
  onSaveToMod: () => void;
  onEditFile:(kind:string)=>void;
  onSaveAsClick?: () => void;
  isSaving: boolean;
  onNewClick?: () => void;
  onImportDAEClick?: () => void;
  onSettingsClick?: () => void;
}

export const Toolbar: React.FC<ToolbarProps> = ({
  modelName, onGameModels, onReturn, onSubsystems,
  onOpenClick,
  onSaveClick, onSaveToMod, onEditFile,
  onSaveAsClick,
  isSaving,
  onNewClick,
  onImportDAEClick,
  onSettingsClick,
}) => {
  return (
    <header className="toolbar" style={{ padding: "8px 16px", minHeight: "48px", display: "flex", flexWrap:"wrap", gap:8, justifyContent: "space-between", alignItems: "center" }}>
      {/* Brand Logo & Model Title */}
      <div style={{ display: "flex", alignItems: "center", gap: "12px" }}>
        <Hammer size={18} style={{ color: "var(--accent-cyan)", filter: "drop-shadow(var(--shadow-glow))" }} />
        <span style={{ fontSize: "14px", fontWeight: "600", textTransform: "uppercase", letterSpacing: "0.1em", color: "var(--text-primary)" }}>
          OpenHW HOD Editor
        </span>
        <span style={{ height: "14px", width: "1px", background: "var(--border-color)" }} />
        <span style={{ fontSize: "12px", color: "var(--accent-cyan)", fontWeight: "500", fontFamily: "var(--font-mono)", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap", maxWidth: "400px" }}>
          {modelName || "NO HOD FILE LOADED"}
        </span>
      </div>

      {/* Editor Controls */}
      <div style={{ display: "flex", alignItems: "center", gap: "16px" }}>

        {/* File Operation Triggers */}
        <div style={{ display: "flex", flexWrap:"wrap", gap: "8px" }}>
          <button onClick={onReturn}>Return to game</button>
          <button onClick={onGameModels}>Game models</button>
          <button disabled={!onSubsystems} onClick={onSubsystems} title="Add a gameplay subsystem or duplicate a mounted component">Quick Add subsystem</button>
          {onNewClick && (
            <button onClick={onNewClick} style={{ height: "30px" }}>
              <FilePlus size={14} style={{ color: "var(--accent-blue)" }} />
              <span>New HOD</span>
            </button>
          )}
          {onSettingsClick && (
            <button className="toolbar-btn outline" onClick={onSettingsClick} title="Settings">
              <Settings size={16} /> Settings
            </button>
          )}
          <button className="toolbar-btn primary" onClick={onOpenClick} title="Open HOD File">
            <FolderOpen size={16} /> Open
          </button>
          {onImportDAEClick && (
            <button className="toolbar-btn outline" onClick={onImportDAEClick} title="Import DAE File">
              <FolderOpen size={16} /> Import DAE
            </button>
          )}
           <button
            onClick={onSaveClick}
            disabled={isSaving}
            style={{ height: "30px", opacity: isSaving ? 0.6 : 1 }}
          >
            <Save size={14} style={{ color: "var(--accent-success)" }} />
            <span>{isSaving ? "Saving..." : "Save HOD"}</span>
          </button>
          <button disabled={isSaving} onClick={onSaveToMod}>Save to mod</button>
          <button onClick={()=>onEditFile("ship")}>Ship file</button>
          <button onClick={()=>onEditFile("events")}>Events file</button>
          <button onClick={()=>onEditFile("weapon")}>Weapon file</button>
          {onSaveAsClick && (
            <button
              onClick={onSaveAsClick}
              disabled={isSaving}
              style={{ height: "30px", opacity: isSaving ? 0.6 : 1 }}
            >
              <Save size={14} style={{ color: "var(--accent-cyan)" }} />
              <span>Save As...</span>
            </button>
          )}
        </div>
      </div>
    </header>
  );
};
