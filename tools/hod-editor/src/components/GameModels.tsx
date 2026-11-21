import { useEffect, useState } from "react";
import { invoke } from "../bridge";

export const isShipHull = (name: string) => /^(r1|r2|p1|p2|p3|traders)\/[^/]+\/rmnative\/hull\.hod$/i.test(name);

export function GameModels({open, close, shipsOnly}: {shipsOnly?: boolean; open: (name: string) => void; close: () => void}) {
  const [names, setNames] = useState<string[]>([]);
  const [query, setQuery] = useState("");
  const [status, setStatus] = useState("Reading game archive…");
  useEffect(() => {
    let active = true;
    invoke<string[]>("list_game_models").then(items => {
      if (active) { setNames(items); setStatus(items.length ? "" : "No HODs in the game archive."); }
    }).catch(error => { if (active) setStatus(String(error)); });
    return () => { active = false; };
  }, []);
  const filtered = names.filter(name => (!shipsOnly || isShipHull(name)) && name.includes(query.trim().toLowerCase()));
  return <div className="model-library-overlay" onKeyDown={e => {
    if (e.key === "Escape") { e.stopPropagation(); close(); }
  }}>
    <section role="dialog" aria-modal="true" aria-labelledby="model-library-title" className="model-library">
      <header><h2 id="model-library-title">{shipsOnly ? "Choose the gameplay ship for these subsystems" : "Game models"}</h2><button onClick={close}>Close</button></header>
      <input autoFocus aria-label="Filter model paths" placeholder="Filter by ship, derelict or path" value={query} onChange={e => setQuery(e.target.value)} />
      <div className="model-library-list">
        {filtered.map(name => <button key={name} onClick={() => open(name)}>{name}</button>)}
        {status || (!filtered.length && "No matching models.")}
      </div>
    </section>
  </div>;
}
