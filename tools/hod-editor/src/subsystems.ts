import type { HODJoint, HODModel } from "./components/Viewport";
import { IDENTITY_MATRIX4D } from "./transform_utils";
import { invoke } from "./bridge";

export interface Mount {slot: string; asset: string; anchor: string; authored: boolean}
export interface ShipBindings {template: string; mounts: Mount[]}
export interface ClipboardMount {asset: string; root: HODJoint; children: HODJoint[]; oldSlot: string}
export const rigCache = new Map<string, HODModel>();

export async function rigFor(asset: string) {
  const cached = rigCache.get(asset);
  if (cached) return cached;
  const rig = await invoke<HODModel>("subsystem_model", {name: asset});
  rigCache.set(asset, rig);
  return rig;
}
export function jointRoot(model: HODModel) {
  return (model.joints.find(j => !model.joints.some(parent => parent.name === j.parent_name)) ?? model.joints[0])?.name;
}
export function owner(mounts: Mount[], name?: string) {
  return mounts.find(m => name === m.slot || name === m.anchor || (m.authored && name?.startsWith(m.slot + "_J")));
}
export function snapshot(model: HODModel, mount: Mount): ClipboardMount {
  const rig = rigCache.get(mount.asset);
  if (!rig) throw Error("Subsystem model is unavailable");
  const source = model.joints.find(j => j.name === (mount.authored ? mount.slot : mount.anchor));
  const root = structuredClone(source ?? {name: mount.slot, parent_name: jointRoot(model), local_transform: IDENTITY_MATRIX4D});
  const children = mount.authored ? model.joints.filter(j => j.name.startsWith(mount.slot + "_J")) : rig.joints.map((j,i) => {
    const parent = rig.joints.findIndex(p => p.name === j.parent_name);
    return {...j, name: mount.slot + "_J" + i, parent_name: parent < 0 ? mount.slot : mount.slot + "_J" + parent};
  });
  return {asset:mount.asset,root,children:structuredClone(children),oldSlot:mount.slot};
}
export function place(model:HODModel, mounts:Mount[], asset:string, parent:string, copy?:ClipboardMount) {
  const rig = rigCache.get(asset);
  if (!rig || !model.joints.some(j=>j.name===parent)) throw Error("Select a hull joint for this mount");
  let number=1, slot:string;
  do {slot="OWMount_"+number++;} while (mounts.some(m=>m.slot===slot) || model.joints.some(j=>j.name===slot || j.name.startsWith(slot+"_J")));
  const mount:Mount={slot,asset,anchor:slot,authored:true};
  const root:HODJoint=copy ? structuredClone(copy.root) : {name:slot,local_transform:structuredClone(IDENTITY_MATRIX4D)};
  root.name=slot; root.parent_name=parent;
  if(copy){
    const shift=Math.max(1,...rig.joints.map(j=>Math.abs(j.local_transform.m[3][0])));
    root.local_transform.m[3][0]+=shift;
    if(root.position)root.position.x+=shift;
  }
  const children=copy ? copy.children.map(j=>({...structuredClone(j),name:slot+j.name.slice(copy.oldSlot.length),parent_name:slot+(j.parent_name ?? copy.oldSlot).slice(copy.oldSlot.length)})) :
    rig.joints.map((j,i)=>{
      const p=rig.joints.findIndex(n=>n.name===j.parent_name);
      return {...structuredClone(j),name:slot+"_J"+i,parent_name:p<0?slot:slot+"_J"+p};
    });
  return {mount,model:{...model,joints:[...model.joints,root,...children]}};
}

// Subsystem geometry is supplied by its shared asset in gameplay. Show it in
// the viewport without baking another copy into the saved hull.
export function preview(model:HODModel|null,mounts:Mount[]): HODModel|null {
  if(!model || !mounts.length)return model;
  const result={...model,joints:[...model.joints],meshes:[...model.meshes],materials:[...model.materials],textures:[...model.textures]};
  for(const mount of mounts){
    const rig=rigCache.get(mount.asset);
    if(!rig)continue;
    const base=mount.slot+"_J";
    if(!mount.authored){
      if (!model.joints.some(j=>j.name===mount.anchor))continue;
      rig.joints.forEach((j,i)=>{
        const p=rig.joints.findIndex(parent=>parent.name===j.parent_name);
        result.joints.push({...j,name:base+i,parent_name:p<0?mount.anchor:base+p});
      });
    }
    const materialOffset=result.materials.length;
    result.materials.push(...rig.materials);
    for(const t of rig.textures) if(!result.textures.some(existing=>existing.name===t.name))result.textures.push(t);
    for(const mesh of rig.meshes){
      const joint=rig.joints.findIndex(j=>j.name===mesh.parent_name);
      result.meshes.push({...mesh,name:mount.slot+"/"+mesh.name,parent_name:joint<0?mount.anchor:base+joint,
        parts:mesh.parts.map(p=>({...p,material_index:p.material_index+materialOffset}))});
    }
  }
  return result;
}
