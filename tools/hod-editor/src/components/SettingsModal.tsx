import React from "react";
import { FolderOpen, X, AlertTriangle } from "lucide-react";
import { invoke } from "../bridge";

interface SettingsModalProps {
  isOpen: boolean;
  onClose: () => void;
  keeperTxtPaths: string[];
  setKeeperTxtPaths: (paths: string[]) => void;
}

export const SettingsModal: React.FC<SettingsModalProps> = ({
  isOpen,
  onClose,
  keeperTxtPaths,
  setKeeperTxtPaths,
}) => {
  if (!isOpen) return null;

  const selectAndSaveKeeperPath = async () => {
    try {
      const selectedPath = await invoke<string | null>("select_shader_directory");
      if (selectedPath) {
        let dirPath = selectedPath;
        if (dirPath.endsWith("/") || dirPath.endsWith("\\")) {
          dirPath = dirPath.substring(0, dirPath.length - 1);
        }
        if (!keeperTxtPaths.includes(dirPath)) {
          const updated = [...keeperTxtPaths, dirPath];
          setKeeperTxtPaths(updated);
          await invoke("save_shader_config", { config: { shader_directories: updated } });
        }
      }
    } catch (e: any) {
      console.error(e);
      invoke("log_event", { level: "ERROR", message: "Failed to select shader directory: " + e.toString() }).catch(console.error);
    }
  };

  return (
    <div
      style={{
        position: "fixed",
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: "rgba(0, 0, 0, 0.7)",
        display: "flex",
        alignItems: "center",
        justifyContent: "center",
        zIndex: 9999,
        backdropFilter: "blur(4px)",
      }}
    >
      <div
        style={{
          background: "var(--bg-secondary)",
          border: "1px solid var(--border-color)",
          borderRadius: "8px",
          width: "500px",
          maxWidth: "90vw",
          display: "flex",
          flexDirection: "column",
          boxShadow: "0 8px 32px rgba(0,0,0,0.5)",
        }}
      >
        <div
          style={{
            padding: "16px",
            borderBottom: "1px solid var(--border-color)",
            display: "flex",
            justifyContent: "space-between",
            alignItems: "center",
          }}
        >
          <h2 style={{ margin: 0, fontSize: "16px", color: "var(--accent-cyan)", display: "flex", alignItems: "center", gap: "8px" }}>
            <FolderOpen size={18} /> Settings
          </h2>
          <button
            onClick={onClose}
            style={{ background: "none", border: "none", color: "var(--text-muted)", cursor: "pointer" }}
          >
            <X size={18} />
          </button>
        </div>
        <div style={{ padding: "16px", display: "flex", flexDirection: "column", gap: "16px" }}>
          <div>
            <div style={{ display: "flex", alignItems: "center", gap: "8px", marginBottom: "8px" }}>
              <AlertTriangle size={16} color="#ffb74d" />
              <h3 style={{ margin: 0, fontSize: "14px", color: "var(--text-primary)" }}>Shader Directories</h3>
            </div>
            <p style={{ margin: "0 0 12px 0", fontSize: "12px", color: "var(--text-muted)" }}>
              Configure your uncompressed game data directories to automatically render .TGA textures and high-fidelity shader materials. You can add multiple paths.
            </p>
            <div style={{ display: "flex", flexDirection: "column", gap: "8px" }}>
              {keeperTxtPaths.map((p, i) => (
                <div key={i} style={{ display: "flex", gap: "8px" }}>
                  <input
                    value={p}
                    onChange={(e) => {
                      const updated = [...keeperTxtPaths];
                      updated[i] = e.target.value;
                      setKeeperTxtPaths(updated);
                    }}
                    style={{ flex: 1, height: "32px", fontSize: "12px", padding: "0 8px", background: "rgba(0,0,0,0.3)", border: "1px solid var(--border-color)", color: "var(--text-primary)", borderRadius: "4px" }}
                  />
                  <button
                    onClick={async () => {
                      const updated = keeperTxtPaths.filter((_, j) => j !== i);
                      setKeeperTxtPaths(updated);
                      await invoke("save_shader_config", { config: { shader_directories: updated } });
                    }}
                    style={{ background: "#c62828", color: "#fff", border: "none", borderRadius: "4px", padding: "0 12px", cursor: "pointer", fontSize: "12px" }}
                  >
                    Remove
                  </button>
                </div>
              ))}
              <button
                onClick={selectAndSaveKeeperPath}
                style={{
                  height: "32px",
                  fontSize: "12px",
                  padding: "0 12px",
                  background: "#ff9800",
                  color: "#000",
                  border: "none",
                  borderRadius: "4px",
                  cursor: "pointer",
                  fontWeight: "600",
                  display: "flex",
                  alignItems: "center",
                  gap: "6px",
                  alignSelf: "flex-start",
                  marginTop: "4px"
                }}
              >
                <FolderOpen size={14} /> Add Directory...
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
