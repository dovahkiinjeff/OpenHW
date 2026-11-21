use crate::hod::{HODMeshPart, HODModel, HODVertex, Vector3};
use crate::iff::IffChunk;
use std::collections::HashMap;

#[derive(Hash, PartialEq, Eq, Clone)]
struct HashableVertex {
    px: u32,
    py: u32,
    pz: u32,
    nx: u32,
    ny: u32,
    nz: u32,
    c: u32,
    u: u32,
    v: u32,
    u1: u32,
    v1: u32,
    u2: u32,
    v2: u32,
    tx: u32,
    ty: u32,
    tz: u32,
    bx: u32,
    by: u32,
    bz: u32,
    skinning: Option<Vec<u8>>,
}

fn f32_to_u32(f: f32) -> u32 {
    if f.is_nan() {
        0
    } else {
        f.to_bits()
    }
}

impl HashableVertex {
    fn from_hod(v: &HODVertex) -> Self {
        let (nx, ny, nz) = v.normal.as_ref().map_or((0, 0, 0), |n| {
            (f32_to_u32(n.x), f32_to_u32(n.y), f32_to_u32(n.z))
        });
        let (u, v_uv) =
            v.uv.as_ref()
                .map_or((0, 0), |uv| (f32_to_u32(uv.u), f32_to_u32(uv.v)));
        let (u1, v1) = v
            .uv1
            .as_ref()
            .map_or((0, 0), |uv| (f32_to_u32(uv.u), f32_to_u32(uv.v)));
        let (u2, v2) = v
            .uv2
            .as_ref()
            .map_or((0, 0), |uv| (f32_to_u32(uv.u), f32_to_u32(uv.v)));
        let (tx, ty, tz) = v.tangent.as_ref().map_or((0, 0, 0), |t| {
            (f32_to_u32(t.x), f32_to_u32(t.y), f32_to_u32(t.z))
        });
        let (bx, by, bz) = v.binormal.as_ref().map_or((0, 0, 0), |b| {
            (f32_to_u32(b.x), f32_to_u32(b.y), f32_to_u32(b.z))
        });

        Self {
            px: f32_to_u32(v.position.x),
            py: f32_to_u32(v.position.y),
            pz: f32_to_u32(v.position.z),
            nx,
            ny,
            nz,
            c: v.color.unwrap_or(0),
            u,
            v: v_uv,
            u1,
            v1,
            u2,
            v2,
            tx,
            ty,
            tz,
            bx,
            by,
            bz,
            skinning: v.skinning_data.clone(),
        }
    }
}

pub struct MeshDeduplicator {
    vertices: Vec<HODVertex>,
    vertex_map: HashMap<HashableVertex, u16>,
}

impl MeshDeduplicator {
    pub fn new() -> Self {
        Self {
            vertices: Vec::new(),
            vertex_map: HashMap::new(),
        }
    }

    pub fn add_vertex(&mut self, v: &HODVertex) -> u16 {
        let hv = HashableVertex::from_hod(v);
        if let Some(&idx) = self.vertex_map.get(&hv) {
            return idx;
        }
        let idx = self.vertices.len() as u16;
        self.vertices.push(v.clone());
        self.vertex_map.insert(hv, idx);
        idx
    }

    pub fn get_vertices(&self) -> &[HODVertex] {
        &self.vertices
    }
}

use byteorder::{BigEndian, LittleEndian, WriteBytesExt};
use std::io::{Cursor, Write};

pub struct CompiledMesh {
    pub name: String,
    pub parent_name: String,
    pub lod: i32,
    pub has_mult_tags: bool,
    pub new_parts: Vec<HODMeshPart>,
}

/// Extra UV sets (0x10 uv1, 0x20 uv2) only exist in BMSH v1401; a v1400
/// reader (engine HodMesh.cpp) copies uv0 into uv1. Keep a bit only when the
/// part really carries that set (parsed from a v1401 file): `badge` decals
/// are placed through uv1, so stripping it moved every badge.
fn normalize_hwrm_vertex_mask(part: &HODMeshPart) -> u32 {
    let mask = part.vertex_mask;
    let mut normalized = mask & !0x30;
    if (mask & 0x10) != 0 {
        normalized |= 0x08;
        if part.vertices.iter().any(|v| v.uv1.is_some()) {
            normalized |= 0x10;
        }
    }
    if (mask & 0x20) != 0 && part.vertices.iter().any(|v| v.uv2.is_some()) {
        normalized |= 0x20;
    }
    normalized
}

/// BMSH version for a compiled part list: v1401 only when a part keeps an
/// extra UV set, so meshes without one stay byte-identical to before.
fn bmsh_version_for_parts(parts: &[HODMeshPart]) -> u32 {
    if parts.iter().any(|part| (part.vertex_mask & 0x30) != 0) {
        1401
    } else {
        1400
    }
}

fn write_len_string<W: Write>(writer: &mut W, s: &str) -> std::io::Result<()> {
    writer.write_u32::<LittleEndian>(s.len() as u32)?;
    writer.write_all(s.as_bytes())?;
    Ok(())
}

fn is_default_tangent_space(vertices: &[HODVertex]) -> bool {
    if vertices.is_empty() {
        return false;
    }
    vertices.iter().all(|v| {
        let tangent_default = v.tangent.as_ref().map_or(true, |t| {
            (t.x - 1.0).abs() < 0.0001 && t.y.abs() < 0.0001 && t.z.abs() < 0.0001
        });
        let binormal_default = v.binormal.as_ref().map_or(true, |b| {
            b.x.abs() < 0.0001 && b.y.abs() < 0.0001 && (b.z - 1.0).abs() < 0.0001
        });
        tangent_default && binormal_default
    })
}

fn add_vec3(a: &mut Vector3, b: &Vector3) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}

fn sub_vec3(a: &Vector3, b: &Vector3) -> Vector3 {
    Vector3 {
        x: a.x - b.x,
        y: a.y - b.y,
        z: a.z - b.z,
    }
}

fn mul_vec3(a: &Vector3, s: f32) -> Vector3 {
    Vector3 {
        x: a.x * s,
        y: a.y * s,
        z: a.z * s,
    }
}

fn dot_vec3(a: &Vector3, b: &Vector3) -> f32 {
    a.x * b.x + a.y * b.y + a.z * b.z
}

fn cross_vec3(a: &Vector3, b: &Vector3) -> Vector3 {
    Vector3 {
        x: a.y * b.z - a.z * b.y,
        y: a.z * b.x - a.x * b.z,
        z: a.x * b.y - a.y * b.x,
    }
}

fn vec3_length(v: &Vector3) -> f32 {
    (v.x * v.x + v.y * v.y + v.z * v.z).sqrt()
}

fn normalize_vec3(v: Vector3, fallback: Vector3) -> Vector3 {
    let len = vec3_length(&v);
    if len > 0.000001 {
        Vector3 {
            x: v.x / len,
            y: v.y / len,
            z: v.z / len,
        }
    } else {
        fallback
    }
}

fn compute_tangent_space(vertices: &mut [HODVertex], indices: &[u16]) {
    let mut tangents = vec![
        Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0
        };
        vertices.len()
    ];
    let mut binormals = vec![
        Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0
        };
        vertices.len()
    ];

    for tri in indices.chunks_exact(3) {
        let i0 = tri[0] as usize;
        let i1 = tri[1] as usize;
        let i2 = tri[2] as usize;
        if i0 >= vertices.len() || i1 >= vertices.len() || i2 >= vertices.len() {
            continue;
        }

        let p0 = &vertices[i0].position;
        let p1 = &vertices[i1].position;
        let p2 = &vertices[i2].position;
        let Some(uv0) = vertices[i0].uv.as_ref() else {
            continue;
        };
        let Some(uv1) = vertices[i1].uv.as_ref() else {
            continue;
        };
        let Some(uv2) = vertices[i2].uv.as_ref() else {
            continue;
        };

        let edge1 = sub_vec3(p1, p0);
        let edge2 = sub_vec3(p2, p0);
        let du1 = uv1.u - uv0.u;
        let dv1 = uv1.v - uv0.v;
        let du2 = uv2.u - uv0.u;
        let dv2 = uv2.v - uv0.v;
        let denom = du1 * dv2 - du2 * dv1;
        if denom.abs() < 0.000001 {
            continue;
        }
        let inv = 1.0 / denom;
        let tangent = Vector3 {
            x: (edge1.x * dv2 - edge2.x * dv1) * inv,
            y: (edge1.y * dv2 - edge2.y * dv1) * inv,
            z: (edge1.z * dv2 - edge2.z * dv1) * inv,
        };
        let binormal = Vector3 {
            x: (edge2.x * du1 - edge1.x * du2) * inv,
            y: (edge2.y * du1 - edge1.y * du2) * inv,
            z: (edge2.z * du1 - edge1.z * du2) * inv,
        };
        for idx in [i0, i1, i2] {
            add_vec3(&mut tangents[idx], &tangent);
            add_vec3(&mut binormals[idx], &binormal);
        }
    }

    // Apply Gram-Schmidt orthogonalization
    for (idx, vertex) in vertices.iter_mut().enumerate() {
        let normal = vertex.normal.clone().unwrap_or(Vector3 {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        });
        let tangent = tangents[idx].clone();
        let binormal = binormals[idx].clone();

        // Gram-Schmidt: tangent = normalize(tangent - normal * dot(tangent, normal))
        let tangent_dot_normal = dot_vec3(&tangent, &normal);
        let tangent_orthogonal = sub_vec3(&tangent, &mul_vec3(&normal, tangent_dot_normal));
        let tangent_normalized = normalize_vec3(
            tangent_orthogonal,
            Vector3 {
                x: 1.0,
                y: 0.0,
                z: 0.0,
            },
        );

        // Calculate binormal as cross product of normal and tangent
        let binormal_cross = cross_vec3(&normal, &tangent_normalized);
        let binormal_normalized = normalize_vec3(
            binormal_cross,
            Vector3 {
                x: 0.0,
                y: 0.0,
                z: 1.0,
            },
        );

        // Calculate handedness
        let handedness = if dot_vec3(&cross_vec3(&normal, &tangent_normalized), &binormal) >= 0.0 {
            1.0
        } else {
            -1.0
        };

        vertex.tangent = Some(tangent_normalized);
        vertex.binormal = Some(mul_vec3(&binormal_normalized, handedness));
    }
}

fn add_tangent_contribution(vertices: &mut [HODVertex], tri: [u16; 3]) {
    let i0 = tri[0] as usize;
    let i1 = tri[1] as usize;
    let i2 = tri[2] as usize;
    if i0 >= vertices.len() || i1 >= vertices.len() || i2 >= vertices.len() {
        return;
    }

    let p0 = &vertices[i0].position;
    let p1 = &vertices[i1].position;
    let p2 = &vertices[i2].position;
    let Some(uv0) = vertices[i0].uv.as_ref() else {
        return;
    };
    let Some(uv1) = vertices[i1].uv.as_ref() else {
        return;
    };
    let Some(uv2) = vertices[i2].uv.as_ref() else {
        return;
    };

    let edge1 = sub_vec3(p1, p0);
    let edge2 = sub_vec3(p2, p0);
    let du1 = uv1.u - uv0.u;
    let dv1 = uv1.v - uv0.v;
    let du2 = uv2.u - uv0.u;
    let dv2 = uv2.v - uv0.v;
    let denom = du1 * dv2 - du2 * dv1;
    // HODOR FUN_0040ea90 line 11067: if (NAN(fVar21) != (fVar21 == 0.0)) { fVar28 = 0.0; }
    // This means: if denom is NaN OR exactly zero, set inv=0 (skip contribution).
    if denom == 0.0 || denom.is_nan() {
        return;
    }

    let inv = 1.0 / denom;
    let tangent = Vector3 {
        x: (edge1.x * dv2 - edge2.x * dv1) * inv,
        y: (edge1.y * dv2 - edge2.y * dv1) * inv,
        z: (edge1.z * dv2 - edge2.z * dv1) * inv,
    };
    let binormal = Vector3 {
        x: (edge2.x * du1 - edge1.x * du2) * inv,
        y: (edge2.y * du1 - edge1.y * du2) * inv,
        z: (edge2.z * du1 - edge1.z * du2) * inv,
    };

    for idx in tri {
        let vertex = &mut vertices[idx as usize];
        let current_tangent = vertex.tangent.get_or_insert(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });
        add_vec3(current_tangent, &tangent);

        let current_binormal = vertex.binormal.get_or_insert(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });
        add_vec3(current_binormal, &binormal);
    }
}

fn finalize_tangent_space(vertices: &mut [HODVertex]) {
    for vertex in vertices {
        let tangent = vertex.tangent.clone().unwrap_or(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });
        let binormal = vertex.binormal.clone().unwrap_or(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });

        // Skip vertices that were never touched by any triangle
        let tangent_len_sq = tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z;
        let binormal_len_sq =
            binormal.x * binormal.x + binormal.y * binormal.y + binormal.z * binormal.z;
        if tangent_len_sq == 0.0 && binormal_len_sq == 0.0 {
            continue;
        }

        let normal = vertex.normal.clone().unwrap_or(Vector3 {
            x: 0.0,
            y: 1.0,
            z: 0.0,
        });

        // HODOR Gram-Schmidt: tangent = tangent - normal * dot(tangent, normal)
        let tangent_dot_normal = dot_vec3(&tangent, &normal);
        let tangent_orthogonal = sub_vec3(&tangent, &mul_vec3(&normal, tangent_dot_normal));
        let tangent_normalized = normalize_vec3(
            tangent_orthogonal,
            Vector3 {
                x: 1.0,
                y: 0.0,
                z: 0.0,
            },
        );

        // HODOR binormal = normalize(accumulated_binormal)
        // NOT cross(normal, tangent). Ghidra FUN_0040ea90 degenerate fallback
        // replaces the cross product with the accumulated binormal.
        let binormal_normalized = normalize_vec3(
            binormal,
            Vector3 {
                x: 0.0,
                y: 0.0,
                z: 1.0,
            },
        );

        vertex.tangent = Some(tangent_normalized);
        vertex.binormal = Some(binormal_normalized);
    }
}

fn find_or_add_current_vertex(vertices: &mut Vec<HODVertex>, vertex: HODVertex) -> u16 {
    let key = HashableVertex::from_hod(&vertex);
    if let Some(index) = vertices
        .iter()
        .position(|existing| HashableVertex::from_hod(existing) == key)
    {
        return index as u16;
    }

    let index = vertices.len() as u16;
    vertices.push(vertex);
    index
}

pub fn compile_hodor_style_tangent_part(
    source_vertices: &[HODVertex],
    source_indices: &[u16],
) -> (Vec<HODVertex>, Vec<u16>) {
    let mut raw_vertices = source_vertices.to_vec();

    // 1. Zero out tangents so we can accumulate properly
    for v in &mut raw_vertices {
        v.tangent = Some(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });
        v.binormal = Some(Vector3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        });
    }

    // 2. Accumulate tangents onto the raw unmerged vertices
    for tri in source_indices.chunks_exact(3) {
        let compiled_tri = [tri[0], tri[1], tri[2]];
        add_tangent_contribution(&mut raw_vertices, compiled_tri);
    }

    // 3. Normalize the tangents
    finalize_tangent_space(&mut raw_vertices);

    // 4. Deduplicate AFTER tangent space is computed (this prevents smoothing across sharp edges)
    let mut deduplicator = MeshDeduplicator::new();
    let mut indices = Vec::with_capacity(source_indices.len());

    for &idx in source_indices {
        let v = &raw_vertices[idx as usize];
        let new_idx = deduplicator.add_vertex(v);
        indices.push(new_idx);
    }

    (deduplicator.get_vertices().to_vec(), indices)
}

fn should_generate_tangent_space(part: &HODMeshPart, vertices: &[HODVertex]) -> bool {
    (part.vertex_mask & 0x6000) == 0x6000 && is_default_tangent_space(vertices)
}

fn prepare_vertices_for_hwrm(part: &HODMeshPart) -> Vec<HODVertex> {
    let mut vertices = part.vertices.clone();
    if should_generate_tangent_space(part, &vertices) {
        compute_tangent_space(&mut vertices, &part.indices);
    }
    vertices
}

pub fn compile_model_meshes(model: &mut HODModel) -> Vec<CompiledMesh> {
    let mut compiled_meshes = Vec::new();

    for mesh in &mut model.meshes {
        println!("[DEBUG] Compiling mesh: {} lod={}", mesh.name, mesh.lod);
        let mut new_parts = Vec::new();
        if !mesh.has_mult_tags {
            for part in &mut mesh.parts {
                let is_flat_triangle_list = part.indices.len() == part.vertices.len()
                    && part
                        .indices
                        .iter()
                        .enumerate()
                        .all(|(idx, &vertex_idx)| vertex_idx as usize == idx);

                let should_generate_tangents = should_generate_tangent_space(part, &part.vertices);
                println!(
                    "[DEBUG]   Part verts={}, indices={}, is_flat={}, should_gen_tangents={}",
                    part.vertices.len(),
                    part.indices.len(),
                    is_flat_triangle_list,
                    should_generate_tangents
                );

                let (vertices, indices) = if is_flat_triangle_list {
                    if should_generate_tangents {
                        compile_hodor_style_tangent_part(&part.vertices, &part.indices)
                    } else {
                        (prepare_vertices_for_hwrm(part), part.indices.clone())
                    }
                } else {
                    (prepare_vertices_for_hwrm(part), part.indices.clone())
                };

                new_parts.push(HODMeshPart {
                    material_index: part.material_index,
                    vertex_mask: normalize_hwrm_vertex_mask(part),
                    vertices,
                    indices,
                });
            }
        } else {
            for part in &mut mesh.parts {
                let vertices = prepare_vertices_for_hwrm(part);
                let mut dedup = MeshDeduplicator::new();
                let mut new_indices = Vec::new();

                for &idx in &part.indices {
                    let v = &vertices[idx as usize];
                    let new_idx = dedup.add_vertex(v);
                    new_indices.push(new_idx);
                }

                new_parts.push(HODMeshPart {
                    material_index: part.material_index,
                    vertex_mask: normalize_hwrm_vertex_mask(part),
                    vertices: dedup.get_vertices().to_vec(),
                    indices: new_indices,
                });
            }
        }

        compiled_meshes.push(CompiledMesh {
            name: mesh.name.clone(),
            parent_name: mesh.parent_name.clone(),
            lod: mesh.lod,
            has_mult_tags: mesh.has_mult_tags,
            new_parts,
        });
    }

    println!("[DEBUG] Finished compile_meshes.");
    compiled_meshes
}

pub fn generate_pool_data(
    compiled_meshes: &[CompiledMesh],
    comp_tex: &[u8],
    decomp_tex_len: u32,
    pool_type: u32,
) -> std::io::Result<Vec<u8>> {
    println!("[DEBUG] Starting generate_pool_data...");
    let mut decomp_mesh = Vec::new();
    let mut decomp_face = Vec::new();

    for mesh in compiled_meshes {
        let bmsh_version = bmsh_version_for_parts(&mesh.new_parts);
        for part in &mesh.new_parts {
            let mut vertex_stride = 0;
            if (part.vertex_mask & 0x01) != 0 {
                vertex_stride += 16;
            }
            if (part.vertex_mask & 0x02) != 0 {
                vertex_stride += 16;
            }
            if (part.vertex_mask & 0x04) != 0 {
                vertex_stride += 4;
            }
            if (part.vertex_mask & 0x08) != 0 {
                vertex_stride += 8;
            }
            if (part.vertex_mask & 0x10) != 0 {
                vertex_stride += 8;
            }
            if (part.vertex_mask & 0x20) != 0 {
                vertex_stride += 8;
            }
            if (part.vertex_mask & 0x2000) != 0 {
                vertex_stride += 12;
            }
            if (part.vertex_mask & 0x4000) != 0 {
                vertex_stride += 12;
            }

            if let Some(first_vert) = part.vertices.first() {
                if let Some(ref skin) = first_vert.skinning_data {
                    vertex_stride += skin.len() as u32;
                }
            }
            if vertex_stride == 0 {
                vertex_stride = 1;
            }

            for v in &part.vertices {
                let _ = crate::hod::write_vertex(
                    &mut decomp_mesh,
                    v,
                    part.vertex_mask,
                    bmsh_version,
                    vertex_stride,
                );
            }

            // Align face pool to 2 bytes
            if decomp_face.len() % 2 != 0 {
                decomp_face.push(0);
            }
            let mut face_cursor = Cursor::new(&mut decomp_face);
            face_cursor.set_position(face_cursor.get_ref().len() as u64);
            for &idx in &part.indices {
                face_cursor.write_u16::<LittleEndian>(idx)?;
            }
        }
    }

    // Compress the mesh and face streams using Xpress compression.
    // The mesh stream MUST always be real XPress output: a 330-file vanilla
    // sweep found raw (comp_len == decomp_len) streams only for face pools,
    // never for mesh pools, so the engine cannot be relied on to accept a
    // raw mesh stream (suspected DAE-sourced in-game access violation).
    // Face pools keep the size threshold: vanilla ships raw face streams.
    let comp_mesh = if decomp_mesh.is_empty() {
        Vec::new()
    } else {
        crate::xpress::compress(&decomp_mesh)
    };

    let comp_face = if decomp_face.is_empty() {
        Vec::new()
    } else {
        crate::xpress::compress_or_raw(&decomp_face)
    };

    let mut pool_buf = Vec::new();
    let mut cursor = Cursor::new(&mut pool_buf);

    cursor.write_u32::<LittleEndian>(pool_type)?; // pool_type

    cursor.write_u32::<LittleEndian>(comp_tex.len() as u32)?;
    cursor.write_u32::<LittleEndian>(decomp_tex_len)?;
    cursor.write_all(comp_tex)?;

    cursor.write_u32::<LittleEndian>(comp_mesh.len() as u32)?;
    cursor.write_u32::<LittleEndian>(decomp_mesh.len() as u32)?;
    cursor.write_all(&comp_mesh)?;

    cursor.write_u32::<LittleEndian>(comp_face.len() as u32)?;
    cursor.write_u32::<LittleEndian>(decomp_face.len() as u32)?;
    cursor.write_all(&comp_face)?;

    Ok(pool_buf)
}

fn get_base_name(name: &str) -> String {
    if let Some(idx) = name.find("_lod_") {
        name[0..idx].to_string()
    } else if let Some(idx) = name.rfind("_LOD") {
        let suffix = &name[idx + 4..];
        if !suffix.is_empty() && suffix.chars().all(|c| c.is_ascii_digit()) {
            return name[0..idx].to_string();
        }
        name.to_string()
    } else if let Some(idx) = name.rfind("_lod") {
        let suffix = &name[idx + 4..];
        if !suffix.is_empty() && suffix.chars().all(|c| c.is_ascii_digit()) {
            return name[0..idx].to_string();
        }
        name.to_string()
    } else {
        name.to_string()
    }
}

pub fn generate_mult_chunks(compiled_meshes: &[CompiledMesh]) -> std::io::Result<Vec<IffChunk>> {
    let mut mult_chunks = Vec::new();
    let mut grouped_meshes: Vec<(String, String, Vec<&CompiledMesh>)> = Vec::new();

    for mesh in compiled_meshes {
        let base_name = get_base_name(&mesh.name);
        if let Some((_, _, lod_meshes)) = grouped_meshes
            .iter_mut()
            .find(|(name, parent_name, _)| name == &base_name && parent_name == &mesh.parent_name)
        {
            lod_meshes.push(mesh);
        } else {
            grouped_meshes.push((base_name, mesh.parent_name.clone(), vec![mesh]));
        }
    }

    for (name, parent_name, lod_meshes) in grouped_meshes {
        let mut mult_buf = Vec::new();
        let mut m_cursor = Cursor::new(&mut mult_buf);

        write_len_string(&mut m_cursor, &name)?;
        write_len_string(&mut m_cursor, &parent_name)?;
        m_cursor.write_u32::<LittleEndian>(lod_meshes.len() as u32)?;

        if lod_meshes.iter().any(|mesh| mesh.has_mult_tags) {
            // Original HOD 2.0 MULT chunks count only the real TAGS payload here:
            // real id "TAGS" + u32 string length + "DoScars" = 15 bytes.
            // Do not include a counted padding byte; doing so shifts the following NRML.
            m_cursor.write_all(b"FORM")?;
            m_cursor.write_u32::<BigEndian>(15)?; // size
            m_cursor.write_all(b"TAGS")?;
            m_cursor.write_u32::<LittleEndian>(7)?; // string length
            m_cursor.write_all(b"DoScars")?;
        }

        for mesh in lod_meshes {
            let bmsh_data_size = 8 + mesh.new_parts.len() * 18;
            let nrml_size = 8 + bmsh_data_size;

            // Wrap BMSH in NRML chunk.
            m_cursor.write_all(b"NRML")?;
            m_cursor.write_u32::<BigEndian>(nrml_size as u32)?;
            m_cursor.write_all(b"BMSH")?;
            // Nested NRML BMSH version: 1401 when a part keeps uv1/uv2.
            m_cursor.write_u32::<BigEndian>(bmsh_version_for_parts(&mesh.new_parts))?;

            m_cursor.write_i32::<LittleEndian>(mesh.lod)?;
            m_cursor.write_i32::<LittleEndian>(mesh.new_parts.len() as i32)?;

            for part in &mesh.new_parts {
                m_cursor.write_i32::<LittleEndian>(part.material_index as i32)?;
                m_cursor.write_u32::<LittleEndian>(part.vertex_mask)?;
                m_cursor.write_i32::<LittleEndian>(part.vertices.len() as i32)?;
                m_cursor.write_i16::<LittleEndian>(-1)?; // prim_group_count
                m_cursor.write_i32::<LittleEndian>(part.indices.len() as i32)?;
            }
        }

        mult_chunks.push(IffChunk {
            id: "MULT".to_string(),
            chunk_type: crate::iff::ChunkType::Normal,
            version: 1400,
            data: mult_buf,
            children: Vec::new(),
        });
    }

    Ok(mult_chunks)
}
