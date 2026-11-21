use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use serde::{Deserialize, Serialize};
use std::io::{Cursor, Read, Write};
use std::path::PathBuf;

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Vector3 {
    #[serde(default)]
    pub x: f32,
    #[serde(default)]
    pub y: f32,
    #[serde(default)]
    pub z: f32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Vector2 {
    #[serde(default)]
    pub u: f32,
    #[serde(default)]
    pub v: f32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Matrix4 {
    pub m: [[f32; 4]; 4],
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODVertex {
    pub position: Vector3,
    pub normal: Option<Vector3>,
    pub color: Option<u32>,
    pub uv: Option<Vector2>,
    /// Second UV set (vertex mask 0x10, BMSH v1401). The `badge` shader
    /// samples the team badge through this set (SOB_BADGE `inUV1`), so it
    /// must round-trip verbatim; it is not a copy of `uv`.
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub uv1: Option<Vector2>,
    /// Third UV set (vertex mask 0x20, BMSH v1401).
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub uv2: Option<Vector2>,
    pub tangent: Option<Vector3>,
    pub binormal: Option<Vector3>,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub skinning_data: Option<Vec<u8>>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODMeshPart {
    pub material_index: usize,
    pub vertex_mask: u32,
    pub vertices: Vec<HODVertex>,
    pub indices: Vec<u16>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODMesh {
    pub name: String,
    pub parent_name: String,
    pub lod: i32,
    #[serde(default)]
    pub has_mult_tags: bool,
    pub parts: Vec<HODMeshPart>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODJoint {
    pub name: String,
    pub parent_name: Option<String>,
    pub local_transform: Matrix4,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub position: Option<Vector3>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rotation: Option<Vector3>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scale: Option<Vector3>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODMarker {
    pub name: String,
    pub parent_joint: String,
    pub position: Vector3,
    pub rotation: Matrix4,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rotation_euler: Option<Vector3>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODTexture {
    pub name: String,
    pub width: u32,
    pub height: u32,
    pub format: String,              // e.g. "DXT1", "DXT3", "DXT5", "RGBA"
    pub png_preview: Option<String>, // Base64 encoded PNG for React UI thumbnails (max 128px)
    pub png_data: Option<String>, // Base64 encoded PNG for WebGL high-resolution rendering (max 1024px)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub source_path: Option<String>, // Original path if imported from TGA
}

/// One generic STAT shader parameter record.
/// Layout on disk: `u32 param_type`, `u32 value_size`, `value[value_size]`, then
/// (in the common named variant) a length-prefixed name string.
/// `param_type == 5` holds a texture pool index in the first 4 value bytes;
/// `param_type == 4` holds a 4 x f32 (16 byte) colour/vector.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODMaterialParam {
    pub param_type: u32,
    pub value: Vec<u8>,
    #[serde(default)]
    pub name: String,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODMaterial {
    pub name: String,
    pub shader_name: String,
    pub texture_maps: Vec<String>,
    #[serde(default)]
    pub parameters: Vec<u8>,
    /// Full generic shader parameter list, preserved verbatim for lossless saves.
    #[serde(default)]
    pub params: Vec<HODMaterialParam>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODNavLight {
    pub name: String,
    pub section: u32,
    #[serde(default)]
    pub size: f32,
    #[serde(default)]
    pub phase: f32,
    #[serde(default)]
    pub frequency: f32,
    pub style: String,
    pub color: Vector3,
    #[serde(default)]
    pub distance: f32,
    pub sprite_visible: bool,
    pub high_end_only: bool,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODEngineBurn {
    pub name: String,
    pub parent_name: String,
    pub num_divisions: i32,
    pub num_flames: i32,
    pub vertices: Vec<Vector3>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODEngineGlow {
    pub name: String,
    pub parent_name: String,
    pub lod: i32,
    pub mesh: HODMesh,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODEngineShape {
    pub name: String,
    pub parent_name: String,
    pub mesh: HODMesh,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODCollisionMesh {
    pub name: String,
    pub min_extents: Vector3,
    pub max_extents: Vector3,
    pub center: Vector3,
    #[serde(default)]
    pub radius: f32,
    pub mesh: HODMesh,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODDockpoint {
    pub position: Vector3,
    pub rotation: Matrix4,
    #[serde(default)]
    pub tolerance: f32,
    #[serde(default)]
    pub max_speed: f32,
    #[serde(default)]
    pub extra1: u32,
    #[serde(default)]
    pub extra2: u32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODDockpath {
    pub name: String,
    pub parent_name: String,
    pub points: Vec<HODDockpoint>,
    #[serde(default)]
    pub val1: u32,
    #[serde(default)]
    pub val2: u32,
    #[serde(default)]
    pub val3: u32,
    #[serde(default)]
    pub val4: u32,
    #[serde(default)]
    pub val5: u32,
    #[serde(default)]
    pub compatible_ships: String,
    #[serde(default)]
    pub padding1: u32,
    #[serde(default)]
    pub padding2: String,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODQuaternion {
    #[serde(default)]
    pub x: f32,
    #[serde(default)]
    pub y: f32,
    #[serde(default)]
    pub z: f32,
    #[serde(default)]
    pub w: f32,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODKeyframe {
    pub time: f64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub position: Option<Vector3>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rotation: Option<HODQuaternion>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub rotation_euler: Option<Vector3>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub scale: Option<Vector3>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODAnimationTrack {
    pub joint_name: String,
    pub keyframes: Vec<HODKeyframe>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODAnimation {
    pub name: String,
    pub duration: f64,
    pub tracks: Vec<HODAnimationTrack>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct HODModel {
    pub version: u32,
    pub is_v2: bool,
    pub name: String,
    pub textures: Vec<HODTexture>,
    pub materials: Vec<HODMaterial>,
    pub meshes: Vec<HODMesh>,
    pub joints: Vec<HODJoint>,
    pub markers: Vec<HODMarker>,
    pub nav_lights: Vec<HODNavLight>,
    pub engine_burns: Vec<HODEngineBurn>,
    pub engine_glows: Vec<HODEngineGlow>,
    pub engine_shapes: Vec<HODEngineShape>,
    pub collision_meshes: Vec<HODCollisionMesh>,
    pub dockpaths: Vec<HODDockpath>,
    #[serde(default)]
    pub animations: Vec<HODAnimation>,
    #[serde(default)]
    pub textures_modified: bool,
    #[serde(skip)]
    pub preserved_chunks: Vec<crate::iff::IffChunk>,
}

pub struct ParsingContext {
    pub texture_pool: Cursor<Vec<u8>>,
    pub mesh_pool: Cursor<Vec<u8>>,
    pub face_pool: Cursor<Vec<u8>>,
    pub is_v2: bool,
    pub hod_dir: Option<PathBuf>,
    pub uncompressed_dir: Option<PathBuf>,
    pub hod_file_path: Option<PathBuf>,
}

pub(crate) fn write_len_string<W: Write>(writer: &mut W, s: &str) -> Result<(), String> {
    let bytes = s.as_bytes();
    writer
        .write_u32::<LittleEndian>(bytes.len() as u32)
        .map_err(|e| e.to_string())?;
    writer.write_all(bytes).map_err(|e| e.to_string())?;
    Ok(())
}

pub(crate) fn base64_encode(data: &[u8]) -> String {
    const CHARSET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut result = String::new();
    let mut i = 0;
    while i < data.len() {
        let byte1 = data[i];
        let byte2 = if i + 1 < data.len() {
            Some(data[i + 1])
        } else {
            None
        };
        let byte3 = if i + 2 < data.len() {
            Some(data[i + 2])
        } else {
            None
        };

        let val = ((byte1 as u32) << 16)
            | ((byte2.unwrap_or(0) as u32) << 8)
            | (byte3.unwrap_or(0) as u32);

        let char1 = CHARSET[((val >> 18) & 63) as usize] as char;
        let char2 = CHARSET[((val >> 12) & 63) as usize] as char;
        let char3 = if byte2.is_some() {
            CHARSET[((val >> 6) & 63) as usize] as char
        } else {
            '='
        };
        let char4 = if byte3.is_some() {
            CHARSET[(val & 63) as usize] as char
        } else {
            '='
        };

        result.push(char1);
        result.push(char2);
        result.push(char3);
        result.push(char4);

        i += 3;
    }
    result
}

pub fn read_len_string<R: Read>(reader: &mut R) -> Result<String, String> {
    let len = reader
        .read_u32::<LittleEndian>()
        .map_err(|e| e.to_string())? as usize;
    if len == 0 {
        return Ok(String::new());
    }
    let mut buf = vec![0u8; len];
    reader.read_exact(&mut buf).map_err(|e| e.to_string())?;
    Ok(String::from_utf8_lossy(&buf).to_string())
}
