import {useEffect,useState} from "react";
import {invoke} from "../bridge";
type RelatedFile={key:string;kind:string};
export function GameTextEditor({path,kind,close}:{path:string;kind:string;close:()=>void}){
 const [files,setFiles]=useState<RelatedFile[]>([]);
 const [key,setKey]=useState("");const [text,setText]=useState("");
 const [dirty,setDirty]=useState(false);const [busy,setBusy]=useState(false);
 const [error,setError]=useState("");const [status,setStatus]=useState("");
 useEffect(()=>{invoke<RelatedFile[]>("related_files",{path}).then(rows=>setFiles(rows.filter(f=>f.kind===kind))).catch(e=>setError(String(e)));},[path,kind]);
 async function open(next:string){
  if(dirty&&!confirm("Discard unsaved text changes?"))return;
  setBusy(true);setError("");
  try{setText(await invoke<string>("read_related_file",{path,key:next}));setKey(next);setDirty(false);setStatus("");}
  catch(e){setError(String(e));}finally{setBusy(false);}
 }
 async function save(){
  setBusy(true);setError("");
  try{await invoke("write_related_file",{path,key,text});setDirty(false);setStatus("Saved to your mod. Restart OpenHW to apply.");}
  catch(e){setError(String(e));}finally{setBusy(false);}
 }
 return <div className="model-library-overlay"><section className="model-library" style={{width:"min(1200px,95vw)",height:"90vh",display:"flex",flexDirection:"column"}} role="dialog" aria-label={kind+" file editor"}>
  <header><h2>{kind==="ship"?"Ship settings":kind==="events"?"Presentation events":"Weapons and effects"}</h2>
   <button disabled={busy} onClick={()=>{if(!dirty||confirm("Discard unsaved text changes?"))close();}}>Close</button></header>
  {kind==="events"&&<p>Edit the event timelines, markers, effects and sounds as JSON. The game file is rebuilt when you save.</p>}
  {files.length>0?<select aria-label="Game file" disabled={busy} value={key} onChange={e=>void open(e.target.value)}><option value="" disabled>Select a file</option>{files.map(f=><option key={f.key} value={f.key}>{f.key}</option>)}</select>:<p>This model has no referenced files in this category.</p>}
  {error&&<p role="alert">{error}</p>}
  <textarea aria-label="File contents" disabled={!key||busy} value={text} spellCheck={false} onChange={e=>{setText(e.target.value);setDirty(true);}} style={{flex:1,minHeight:200,fontFamily:"monospace",whiteSpace:"pre",tabSize:4}}/>
  <footer><span>{status}</span><button disabled={!dirty||busy} onClick={()=>void save()}>Save file</button></footer>
 </section></div>;
}
