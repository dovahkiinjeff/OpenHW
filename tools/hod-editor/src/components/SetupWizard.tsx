import { useState } from "react";
import { invoke } from "../bridge";

interface SetupWizardProps {
  onComplete: (paths: string[]) => void;
}

export function SetupWizard({ onComplete }: SetupWizardProps) {
  const [loading, setLoading] = useState(false);

  const handleSelectDirectory = async () => {
    try {
      setLoading(true);
      const dirPath = await invoke<string | null>("select_shader_directory");
      if (dirPath) {
        // Save to config
        await invoke("save_shader_config", {
          config: { shader_directories: [dirPath] }
        });
        onComplete([dirPath]);
      }
    } catch (e) {
      console.error(e);
      alert("Failed to save directory: " + e);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{
      position: "fixed",
      top: 0, left: 0, right: 0, bottom: 0,
      backgroundColor: "rgba(0,0,0,0.9)",
      zIndex: 9999,
      display: "flex",
      alignItems: "center",
      justifyContent: "center",
      color: "white"
    }}>
      <div style={{
        backgroundColor: "#1e1e1e",
        padding: "40px",
        borderRadius: "8px",
        maxWidth: "600px",
        boxShadow: "0 10px 30px rgba(0,0,0,0.5)",
        textAlign: "center"
      }}>
        <h2 style={{ marginBottom: "20px" }}>Initial Setup</h2>
        <p style={{ marginBottom: "30px", lineHeight: "1.5", color: "#ccc" }}>
          To dynamically determine available shader parameters and texture mapping types, HODEditorJS needs to scan the game's shader files.
          <br /><br />
          Please select the directory containing your extracted Homeworld 2 `.big` files. We will use this to scan the "shaders" directory for available texture mapping types.
        </p>
        <button
          onClick={handleSelectDirectory}
          disabled={loading}
          style={{
            padding: "12px 24px",
            backgroundColor: "#4caf50",
            color: "white",
            border: "none",
            borderRadius: "4px",
            fontSize: "16px",
            cursor: loading ? "wait" : "pointer"
          }}
        >
          {loading ? "Selecting..." : "Select Game Data Directory"}
        </button>
      </div>
    </div>
  );
}
