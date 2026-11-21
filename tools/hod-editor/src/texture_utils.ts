import { HODTexture } from "./components/Viewport";

export interface TextureGroupItem {
  type: string; // "_DIFF", "_GLOW", "_TEAM", "_NORM", "_SPEC", etc. or ""
  originalName: string;
  texture: HODTexture;
  compression: string;
}

export interface TextureGroup {
  baseName: string;
  textures: TextureGroupItem[];
}

// File-suffix roles observed in vanilla HOD LMIP names (e.g. Bridge_DIFF,
// Engine_DIFX, Resource_SPEC, Kharak_CLD1, Galaxy_Disc_MASK). Order is
// irrelevant for matching: no suffix is a suffix of another.
export let KNOWN_TYPES = ["_DIFF", "_DIFX", "_GLOW", "_GLOX", "_SPEC", "_SPEX", "_REFX", "_TEAM", "_STRP", "_PAIN", "_NORM", "_PROG", "_CLD1", "_CLD2", "_CLD3", "_WARP", "_MASK", "_MATTER", "_TERR", "_NIGHT", "_ENVIRO", "_BURN", "_SCORCH", "_DISC", "_NOIZ", "_EMIS"];

export function parseTextureGroups(textures: HODTexture[]): TextureGroup[] {
  const groups = new Map<string, TextureGroup>();

  for (const tex of textures) {
    let name = tex.name;
    // Strip compression if present at the end
    if (name.toUpperCase().endsWith(tex.format.toUpperCase())) {
      name = name.substring(0, name.length - tex.format.length);
    }

    let type = "";
    let baseName = name;

    // Find the known type
    for (const t of KNOWN_TYPES) {
      if (name.toUpperCase().endsWith(t)) {
        type = t;
        baseName = name.substring(0, name.length - t.length);
        break;
      }
    }

    // If no known type, maybe the whole name is the base name and type is empty
    if (!groups.has(baseName)) {
      groups.set(baseName, { baseName, textures: [] });
    }

    groups.get(baseName)!.textures.push({
      type,
      originalName: tex.name,
      texture: tex,
      compression: tex.format,
    });
  }

  return Array.from(groups.values());
}

// Canonical HOD texture slots per shader, in HOD STAT param order.
// Ground truth: GBXTools/HODOR/SHADERS.MAP plus STAT dumps of vanilla HODs
// (870 files swept: ship = 4 params, shipglow = 5, thruster = 4-6, bg_* = 1-8).
//
// Critical: REFL is NEVER a separate texture. It is the R channel packed
// inside the $glow texture by HODOR (G=GLOW, B=SPEC, R=REFL), so no shader
// gets a REFL slot. Likewise SPEC on `ship`/`badge`/`bay` is a channel of
// $glow, and TEAM/STRP/PAIN are channels of the single $team texture.
const SHIP_SLOTS = ["Diffuse Map (DIFF)", "Glow Map (GLOW)", "Team Paint Map (TEAM)", "Normal Map (NORM)"];
const SHIPGLOW_SLOTS = ["Diffuse Map (DIFF)", "Specular Map (SPEC)", "Glow Map (GLOW)", "Team Paint Map (TEAM)", "Normal Map (NORM)"];

export let SHADER_SLOTS: Record<string, string[]> = {
  // 4-texture ship family
  ship: SHIP_SLOTS,
  matte: SHIP_SLOTS,
  matte2s: SHIP_SLOTS,
  monolith: SHIP_SLOTS,
  megalith: SHIP_SLOTS,
  fxmatte: SHIP_SLOTS,
  mattealpha: SHIP_SLOTS,
  mattealpha2s: SHIP_SLOTS,
  mattescissor: SHIP_SLOTS,
  mattescissor2s: SHIP_SLOTS,
  fxmatte_a: SHIP_SLOTS,
  fxmatte_s: SHIP_SLOTS,
  bay: SHIP_SLOTS,
  badge: SHIP_SLOTS,
  // 5-texture discrete-spec family ($spec sits between $diffuse and $glow)
  shipglow: SHIPGLOW_SLOTS,
  shipglow_ns: SHIPGLOW_SLOTS,
  badgeglow: SHIPGLOW_SLOTS,
  // Thruster on/off pairs ($diffuseOn/$diffuseOff/$glowOn/$glowOff + team + normal)
  thruster: ["Diffuse On (DIFF)", "Diffuse Off (DIFX)", "Glow On (GLOW)", "Glow Off (GLOX)", "Team Paint Map (TEAM)", "Normal Map (NORM)"],
  // Salvage/progress shaders ($spec file uses the _PAIN suffix, see SLOT_ALTERNATES)
  fx_burn: ["Diffuse Map (DIFF)", "Glow Map (GLOW)", "Paint Wear Map (PAIN)", "Progress Map (PROG)", "Normal Map (NORM)"],
  salvage_burn: ["Diffuse Map (DIFF)", "Glow Map (GLOW)", "Paint Wear Map (PAIN)", "Progress Map (PROG)", "Normal Map (NORM)"],
  ore: ["Diffuse Map (DIFF)", "Glow Map (GLOW)", "Specular Map (SPEC)", "Progress Map (PROG)", "Normal Map (NORM)"],
  // Single-texture shaders
  background: ["Diffuse Map (DIFF)"],
  fx: ["Diffuse Map (DIFF)"],
  fxsolid: ["Diffuse Map (DIFF)"],
  fxsoliddalphatest: ["Diffuse Map (DIFF)"],
  fx_a: ["Diffuse Map (DIFF)"],
  fx_s: ["Diffuse Map (DIFF)"],
  fx_harvest: ["Diffuse Map (DIFF)"],
  res_tendril: ["Diffuse Map (DIFF)"],
  dustcloud: ["Diffuse Map (DIFF)"],
  dustcloudflash: ["Diffuse Map (DIFF)"],
  dustcloudnebula: ["Diffuse Map (DIFF)"],
  nis_galaxy_vectors: ["Diffuse Map (DIFF)"],
  res_dustvein: ["Diffuse Map (DIFF)", "Noise Map (NOIZ)"],
  nis_galaxy_disc: ["Disc Map (DIFF)", "Masks Map (MASK)"],
  // Background planets: file suffixes mirror channel roles (terrain=_DIFF,
  // night=_GLOW, enviro=_SPEC, clouds=_CLD1 packed, warp=_WARP)
  bg_planet: ["Terrain Map (DIFF)", "Night Map (GLOW)", "Environment Map (SPEC)", "Cloud Map (CLD1)", "Warp Map (WARP)", "Normal Map (NORM)"],
  bg_planetmelt: ["Terrain Map (DIFF)", "Scorched Map (DIFX)", "Night Map (GLOW)", "Burn Map (GLOX)", "Environment Map (SPEC)", "Cloud Map (CLD1)", "Warp Map (WARP)", "Normal Map (NORM)"],
  bg_moon: ["Terrain Map (DIFF)", "Night Map (GLOW)", "Environment Map (SPEC)", "Normal Map (NORM)"],
  bg_planetoid: ["Terrain Map (DIFF)", "Night Map (GLOW)", "Environment Map (SPEC)", "Normal Map (NORM)"],
  bg_cosmic: ["Matter Map (DIFF)", "Mask Map (MASK)", "Warp Map (WARP)"],
  bg_cosmic_a: ["Matter Map (DIFF)", "Mask Map (MASK)", "Warp Map (WARP)"],
  bg_cosmic_an: ["Matter Map (DIFF)", "Mask Map (MASK)", "Warp Map (WARP)", "Normal Map (NORM)"],
  // No texture params in any observed HOD (STAT param_count = 0)
  innatess: [],
  default: [],
};

export function getExpectedTextureType(shaderName: string, mapIndex: number): string | null {
  const slots = SHADER_SLOTS[shaderName.toLowerCase()];
  if (!slots || mapIndex >= slots.length) return null;
  const slot = slots[mapIndex];
  const match = slot.match(/\((.*?)\)/);
  if (match) {
    return "_" + match[1];
  }
  return null;
}

// Expected slot labels for a material. Known shaders use the canonical
// table; unknown (mod) shaders fall back to a length-preserving list derived
// from the material's own bindings so a group change can never truncate or
// pad bindings we do not understand.
export function getExpectedSlots(shaderName: string, textureMaps: string[]): string[] {
  const slots = SHADER_SLOTS[(shaderName || "").toLowerCase()];
  if (slots) return slots;
  return (textureMaps || []).map((mapName, idx) => {
    const upper = (mapName || "").toUpperCase();
    const hit = KNOWN_TYPES.find((t) => upper.endsWith(t));
    const role = hit ? hit.substring(1) : `SLOT${idx + 1}`;
    return `Texture ${idx + 1} (${role})`;
  });
}

// Some slots accept more than one file suffix: HODOR packs the fx_burn $spec
// texture from _PAIN sources (Salvage_TP_PAIN), while shipglow-family $spec
// textures use _SPEC (Resource_SPEC).
const SLOT_ALTERNATES: Record<string, string[]> = {
  SPEC: ["SPEC", "PAIN"],
  PAIN: ["PAIN", "SPEC"],
};

export function findTextureForSlot(group: TextureGroup, expectedType: string) {
  const role = expectedType.startsWith("_") ? expectedType.substring(1) : expectedType;
  const candidates = SLOT_ALTERNATES[role] ?? [role];
  for (const candidate of candidates) {
    const hit = group.textures.find((t) => t.type === `_${candidate}`);
    if (hit) return hit;
  }
  return undefined;
}


const PARAM_TO_SUFFIX: Record<string, string> = {
  diff: "DIFF",
  diffuse: "DIFF",
  diffon: "DIFF",
  diffuseon: "DIFF",
  difx: "DIFX",
  diffoff: "DIFX",
  diffuseoff: "DIFX",
  glow: "GLOW",
  glowon: "GLOW",
  glox: "GLOX",
  glowoff: "GLOX",
  normal: "NORM",
  norm: "NORM",
  specular: "SPEC",
  spec: "SPEC",
  spex: "SPEX",
  refl: "REFL",
  refx: "REFX",
  team: "TEAM",
  stripe: "STRP",
  strp: "STRP",
  paint: "PAIN",
  prog: "PROG",
  progress: "PROG",
  warp: "WARP",
  mask: "MASK",
  masks: "MASK",
  matter: "MATTER",
  terr: "TERR",
  terrain: "TERR",
  night: "NIGHT",
  enviro: "ENVIRO",
  burn: "BURN",
  scorch: "SCORCH",
  scorched: "SCORCH",
  cld1: "CLD1",
  cloud: "CLD1",
  clouds: "CLD1",
  cld2: "CLD2",
  cld3: "CLD3",
  disc: "DISC",
  noiz: "NOIZ",
  noise: "NOIZ"
};

// GPU-only samplers found in .prog files that are never HOD LMIP textures
// (environment cubemaps, shadow maps, global badge atlas). They must not
// become texture slots or the editor would append phantom params on save.
const NON_TEXTURE_SAMPLERS = new Set(["ENV0", "ENV1", "SHADOW", "BADGE"]);

export function updateDynamicShaderSlots(dynamicShaders: {name: string, slots: string[]}[]) {
  const newTypes = new Set(KNOWN_TYPES);
  for (const shader of dynamicShaders) {
    const key = shader.name.toLowerCase();
    // Static SHADER_SLOTS is HOD ground truth (SHADERS.MAP + STAT dumps);
    // dynamic .prog samplers only add shaders we do not already know.
    if (SHADER_SLOTS[key]) continue;
    const filtered = shader.slots.filter((slot) => !NON_TEXTURE_SAMPLERS.has(slot.toUpperCase()));
    if (filtered.length === 0) continue;
    const formattedSlots = filtered.map(slot => {
      const mappedSuffix = PARAM_TO_SUFFIX[slot.toLowerCase()] || slot.toUpperCase();
      const suffix = "_" + mappedSuffix;
      newTypes.add(suffix);
      return `${slot.charAt(0).toUpperCase() + slot.slice(1)} Map (${mappedSuffix})`;
    });
    // Merge or replace
    SHADER_SLOTS[key] = formattedSlots;
  }
  KNOWN_TYPES = Array.from(newTypes);
}
