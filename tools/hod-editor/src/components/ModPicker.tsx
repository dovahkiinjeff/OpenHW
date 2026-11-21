import { useEffect,useState } from "react";
import { invoke } from "../bridge";

export function ModPicker({close,save}:{close:()=>void;save:(name:string)=>Promise<void>}) {
  const [mods,setMods]=useState<string[]>([]);
  const [name,setName]=useState("");
  const [error,setError]=useState("");
  const [busy,setBusy]=useState(false);
  useEffect(()=>{invoke<string[]>("list_mods").then(setMods).catch(e=>setError(String(e)));},[]);
  async function create(){
    setBusy(true);setError("");
    try {setMods(await invoke<string[]>("create_mod",{name:name.trim()}));setName("");}
    catch(e){setError(String(e));}finally{setBusy(false);}
  }
  async function select(name:string){
    setBusy(true);setError("");
    try {await save(name);close();}catch(e){setError(String(e));}finally{setBusy(false);}
  }
  return <div className="model-library-overlay"><section className="model-library" role="dialog" aria-label="Save ship to mod">
    <header><h2>Save ship to mod</h2><button disabled={busy} onClick={close}>Cancel</button></header>
    <p>Choose the mod that will override this ship. The mod is enabled when you save.</p>
    <div style={{display:"flex",gap:8}}><input aria-label="New mod name" placeholder="New mod name" value={name} maxLength={64} onChange={e=>setName(e.target.value)}/>
      <button disabled={busy||!name.trim()} onClick={()=>void create()}>Create mod</button></div>
    {error&&<p role="alert">{error}</p>}
    <div className="model-library-list">{mods.map(mod=><button disabled={busy} key={mod} onClick={()=>void select(mod)}>{mod}</button>)}</div>
  </section></div>;
}
