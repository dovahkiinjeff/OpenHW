import { HODTexture } from "./components/Viewport";
import { parseTextureGroups, KNOWN_TYPES } from "./texture_utils";

export interface MtlMaterialDefinition {
  name: string;
  diffuseMap?: string;
}

export const parseMtl = (text: string): MtlMaterialDefinition[] => {
  const materials: MtlMaterialDefinition[] = [];
  let current: MtlMaterialDefinition | null = null;

  for (const rawLine of text.split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line || line.startsWith("#")) continue;

    const match = line.match(/^(newmtl|map_Kd)\s+(.+)$/i);
    if (!match) continue;

    const key = match[1].toLowerCase();
    const value = match[2].trim();

    if (key === "newmtl") {
      current = { name: value };
      materials.push(current);
    } else if (key === "map_kd" && current) {
      current.diffuseMap = value;
    }
  }

  return materials;
};

const normalizeName = (value: string): string => value.replace(/[^a-z0-9]/gi, "").toLowerCase();

const fileStem = (path: string): string => {
  const fileName = path.split(/[\\/]/).pop() || path;
  return fileName.replace(/\.[^.]+$/, "");
};

// Matches an MTL map path (e.g. "C:/Smt_Acolyte_Main.dds") to a loaded HOD texture group.
export const resolveDiffuseTexture = (mapPath: string, textures: HODTexture[]): string | null => {
  const stem = fileStem(mapPath);
  let baseName = stem;

  for (const type of KNOWN_TYPES) {
    if (baseName.toUpperCase().endsWith(type)) {
      baseName = baseName.slice(0, -type.length);
      break;
    }
  }

  const target = normalizeName(baseName);
  if (!target) return null;

  const groups = parseTextureGroups(textures);
  let group = groups.find((g) => normalizeName(g.baseName) === target);

  if (!group) {
    group = groups.find((g) => {
      const groupName = normalizeName(g.baseName);
      return groupName.length > 0 && (groupName.startsWith(target) || target.startsWith(groupName));
    });
  }

  if (!group) return null;

  const diffuse = group.textures.find((t) => t.type.toUpperCase() === "_DIFF");
  return (diffuse || group.textures[0])?.originalName ?? null;
};
