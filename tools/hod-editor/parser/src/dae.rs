use crate::hod::*;
use roxmltree::{Document, Node};
use std::collections::HashMap;

/// Hashable key for vertex deduplication: (position_index, normal_index, uv_index)
type VertexKey = (usize, usize, usize);

fn parse_float_array(node: Node) -> Result<Vec<f32>, String> {
    if let Some(text) = node.text() {
        let floats: Result<Vec<f32>, _> =
            text.split_whitespace().map(|s| s.parse::<f32>()).collect();
        floats.map_err(|e| format!("Failed to parse floats: {}", e))
    } else {
        Ok(Vec::new())
    }
}

fn parse_int_array(node: Node) -> Result<Vec<usize>, String> {
    if let Some(text) = node.text() {
        let ints: Result<Vec<usize>, _> = text
            .split_whitespace()
            .map(|s| s.parse::<usize>())
            .collect();
        ints.map_err(|e| format!("Failed to parse ints: {}", e))
    } else {
        Ok(Vec::new())
    }
}

/// Extract value between brackets after a prefix, e.g. "_Sz[12]" → "12"
fn extract_bracket_value<'a>(text: &'a str, prefix: &str) -> Option<&'a str> {
    let start = text.find(prefix)? + prefix.len();
    let end = text[start..].find(']')? + start;
    Some(&text[start..end])
}

pub fn parse_dae(xml_str: &str) -> Result<HODModel, String> {
    let doc = Document::parse(xml_str).map_err(|e| e.to_string())?;

    let mut model = HODModel::new();
    model.is_v2 = true;
    model.version = 0x200; // HOD 2.0 file version (VERS), as written by HODOR

    // Parse materials first (just names for now)
    let mut material_names = Vec::new();
    for node in doc.descendants() {
        if node.has_tag_name("material") {
            if let Some(name) = node.attribute("name") {
                // DAEnerys format: MAT[texture]_SHD[shader]
                // Extract core name and shader separately
                let (mat_name, shader_name) = if name.starts_with("MAT[") {
                    let core = name
                        .find("]_SHD[")
                        .map(|end| name[4..end].to_string())
                        .unwrap_or_else(|| name.to_string());
                    let shader = name
                        .find("]_SHD[")
                        .and_then(|start| {
                            let rest = &name[start + 6..];
                            rest.find(']').map(|end| rest[..end].to_string())
                        })
                        .unwrap_or_else(|| "default".to_string());
                    (core, shader)
                } else {
                    (name.to_string(), "default".to_string())
                };

                material_names.push(name.to_string());
                model.materials.push(HODMaterial {
                    name: mat_name,
                    shader_name,
                    texture_maps: Vec::new(),
                    parameters: Vec::new(),
                    params: Vec::new(),
                });
            }
        }
    }

    if doc.descendants().any(|node| {
        (node.has_tag_name("triangles") || node.has_tag_name("polylist"))
            && node.attribute("material").is_none()
            && !node.ancestors().any(|a| {
                a.has_tag_name("geometry")
                    && a.attribute("id").map_or(false, |id| {
                        id.starts_with("COL[") || id.starts_with("ROOT_COL")
                    })
            })
    }) {
        material_names.push("unknown".to_string());
        model.materials.push(HODMaterial {
            name: "unknown".to_string(),
            shader_name: "default".to_string(),
            texture_maps: Vec::new(),
            parameters: Vec::new(),
            params: Vec::new(),
        });
    }

    // Build meshes from library_geometries
    let mut parsed_meshes_with_lod: Vec<(String, i32, String, HODMeshPart)> = Vec::new();

    for geometry in doc.descendants().filter(|n| n.has_tag_name("geometry")) {
        let geom_id = geometry.attribute("id").unwrap_or("unknown");
        let geom_name = geometry.attribute("name").unwrap_or(geom_id);

        if let Some(mesh) = geometry.children().find(|n| n.has_tag_name("mesh")) {
            let mut source_map: HashMap<String, Vec<f32>> = HashMap::new();

            for source in mesh.children().filter(|n| n.has_tag_name("source")) {
                let id = source.attribute("id").unwrap_or("");
                if let Some(float_array) = source.children().find(|n| n.has_tag_name("float_array"))
                {
                    if let Ok(floats) = parse_float_array(float_array) {
                        source_map.insert(format!("#{}", id), floats);
                    }
                }
            }

            // Extract the position source ID
            let mut pos_source_id = String::new();
            if let Some(vertices) = mesh.children().find(|n| n.has_tag_name("vertices")) {
                if let Some(input) = vertices.children().find(|n| {
                    n.has_tag_name("input") && n.attribute("semantic") == Some("POSITION")
                }) {
                    pos_source_id = input.attribute("source").unwrap_or("").to_string();
                }
            }

            // Parse ALL triangles/polylist groups (one per material)
            for triangles in mesh
                .children()
                .filter(|n| n.has_tag_name("triangles") || n.has_tag_name("polylist"))
            {
                let has_material = triangles.attribute("material").is_some();

                // HODOR behavior: parts without a material attribute get
                // mask=0xB (pos+normal+UV, no tangent/binormal) and are
                // deduplicated by source indices. Parts with a material
                // get mask=0x600B (includes tangent/binormal) and are
                // kept as flat per-corner vertices.
                let vertex_mask = if has_material {
                    0x01 | 0x02 | 0x08 | 0x2000 | 0x4000
                } else {
                    0x01 | 0x02 | 0x08
                };

                let mut mesh_part = HODMeshPart {
                    material_index: 0,
                    vertex_mask,
                    vertices: Vec::new(),
                    indices: Vec::new(),
                };

                let mat = triangles.attribute("material").unwrap_or("unknown");
                if let Some(idx) = material_names.iter().position(|m| m == mat) {
                    mesh_part.material_index = idx;
                }

                let mut pos_offset = 0;
                let mut norm_offset = -1;
                let mut uv_offset = -1;
                let mut max_offset = 0;

                let mut norm_source_id = String::new();
                let mut uv_source_id = String::new();

                for input in triangles.children().filter(|n| n.has_tag_name("input")) {
                    let semantic = input.attribute("semantic").unwrap_or("");
                    let offset: i32 = input
                        .attribute("offset")
                        .unwrap_or("0")
                        .parse()
                        .unwrap_or(0);
                    let source = input.attribute("source").unwrap_or("");

                    if offset > max_offset {
                        max_offset = offset;
                    }

                    if semantic == "VERTEX" {
                        pos_offset = offset;
                    } else if semantic == "NORMAL" {
                        norm_offset = offset;
                        norm_source_id = source.to_string();
                    } else if semantic == "TEXCOORD" {
                        uv_offset = offset;
                        uv_source_id = source.to_string();
                    }
                }

                let stride = (max_offset + 1) as usize;

                if let Some(p) = triangles.children().find(|n| n.has_tag_name("p")) {
                    if let Ok(indices) = parse_int_array(p) {
                        let pos_data = source_map.get(&pos_source_id);
                        let norm_data = source_map.get(&norm_source_id);
                        let uv_data = source_map.get(&uv_source_id);

                        // For parts without material: deduplicate by source
                        // indices (pos, norm, uv) to match HODOR's indexed
                        // vertex output.
                        let mut vertex_dedup: HashMap<(usize, usize, usize), u16> = HashMap::new();

                        let mut v_idx = 0;
                        while v_idx + stride <= indices.len() {
                            let p_i = indices[v_idx + pos_offset as usize];
                            let n_i = if norm_offset >= 0 {
                                indices[v_idx + norm_offset as usize]
                            } else {
                                0
                            };
                            let u_i = if uv_offset >= 0 {
                                indices[v_idx + uv_offset as usize]
                            } else {
                                0
                            };

                            if !has_material {
                                let key = (p_i, n_i, u_i);
                                if let Some(&existing_idx) = vertex_dedup.get(&key) {
                                    mesh_part.indices.push(existing_idx);
                                    v_idx += stride;
                                    continue;
                                }
                                let new_idx = mesh_part.vertices.len() as u16;
                                vertex_dedup.insert(key, new_idx);
                            }

                            let mut vertex = HODVertex {
                                position: Vector3 {
                                    x: 0.0,
                                    y: 0.0,
                                    z: 0.0,
                                },
                                normal: Some(Vector3 {
                                    x: 0.0,
                                    y: 1.0,
                                    z: 0.0,
                                }),
                                tangent: Some(Vector3 {
                                    x: 1.0,
                                    y: 0.0,
                                    z: 0.0,
                                }),
                                binormal: Some(Vector3 {
                                    x: 0.0,
                                    y: 0.0,
                                    z: 1.0,
                                }),
                                uv: Some(Vector2 { u: 0.0, v: 0.0 }),
                                uv1: None,
                                uv2: None,
                                color: Some(0xFFFFFFFF),
                                skinning_data: None,
                            };

                            if let Some(pd) = pos_data {
                                if p_i * 3 + 2 < pd.len() {
                                    vertex.position.x = pd[p_i * 3];
                                    vertex.position.y = pd[p_i * 3 + 1];
                                    vertex.position.z = pd[p_i * 3 + 2];
                                }
                            }

                            if norm_offset >= 0 {
                                if let Some(nd) = norm_data {
                                    if n_i * 3 + 2 < nd.len() {
                                        if let Some(ref mut n) = vertex.normal {
                                            n.x = nd[n_i * 3];
                                            n.y = nd[n_i * 3 + 1];
                                            n.z = nd[n_i * 3 + 2];
                                        }
                                    }
                                }
                            }

                            if uv_offset >= 0 {
                                if let Some(ud) = uv_data {
                                    if u_i * 2 + 1 < ud.len() {
                                        if let Some(ref mut uv_coords) = vertex.uv {
                                            uv_coords.u = ud[u_i * 2];
                                            uv_coords.v = ud[u_i * 2 + 1];
                                        }
                                    }
                                }
                            }

                            let idx = mesh_part.vertices.len() as u16;
                            mesh_part.vertices.push(vertex);
                            mesh_part.indices.push(idx);
                            v_idx += stride;
                        }
                    }
                }

                // Extract the MULT[name] tag and LOD
                let mut mesh_target_name = geom_name.to_string();

                // Handle collision meshes: COL[...] geometries become
                // HODCollisionMesh entries instead of regular visible meshes.
                if mesh_target_name.starts_with("COL[") {
                    if mesh_target_name.find("]").is_some() && model.collision_meshes.is_empty() {
                        // Collect all parts from this geometry
                        let mut col_parts = Vec::new();
                        for _triangles in mesh
                            .children()
                            .filter(|n| n.has_tag_name("triangles") || n.has_tag_name("polylist"))
                        {
                            col_parts.push(HODMeshPart {
                                material_index: 0,
                                vertex_mask: 0x01,
                                vertices: Vec::new(),
                                indices: Vec::new(),
                            });
                        }
                        // Build a minimal collision mesh with bounding info
                        // The actual vertex data will be generated from extents
                        // when saving, matching HODOR's behavior.
                        model.collision_meshes.push(HODCollisionMesh {
                            name: "Root".to_string(),
                            min_extents: Vector3 {
                                x: -10.0,
                                y: -10.0,
                                z: -10.0,
                            },
                            max_extents: Vector3 {
                                x: 10.0,
                                y: 10.0,
                                z: 10.0,
                            },
                            center: Vector3 {
                                x: 0.0,
                                y: 0.0,
                                z: 0.0,
                            },
                            radius: 17.32,
                            mesh: HODMesh {
                                name: "CollisionMesh".to_string(),
                                parent_name: "Root".to_string(),
                                lod: 0,
                                has_mult_tags: false,
                                parts: Vec::new(),
                            },
                        });
                    }
                    continue;
                }

                let mut lod = 0i32;
                if mesh_target_name.starts_with("MULT[") {
                    if let Some(end) = mesh_target_name.find("]") {
                        mesh_target_name = mesh_target_name[5..end].to_string();
                    }
                    // Extract LOD from "MULT[name]_LOD[N]"
                    if let Some(lod_start) = geom_name.find("_LOD[") {
                        let lod_part = &geom_name[lod_start + 5..];
                        if let Some(lod_end) = lod_part.find("]") {
                            lod = lod_part[..lod_end].parse().unwrap_or(0);
                        }
                    }
                }

                let key = format!("{}_{}", mesh_target_name, lod);
                parsed_meshes_with_lod.push((mesh_target_name, lod, key, mesh_part));
            }
        }
    }

    // Group mesh parts into full HODMeshes by (name, lod)
    let mut mesh_map: HashMap<String, HODMesh> = HashMap::new();
    for (name, lod, key, part) in parsed_meshes_with_lod {
        if let Some(mesh) = mesh_map.get_mut(&key) {
            mesh.parts.push(part);
        } else {
            mesh_map.insert(
                key.clone(),
                HODMesh {
                    name: name.clone(),
                    parent_name: "Root".to_string(),
                    lod,
                    has_mult_tags: false,
                    parts: vec![part],
                },
            );
        }
    }

    for (_, mesh) in mesh_map {
        model.meshes.push(mesh);
    }

    // Parse visual scenes to construct hierarchy
    if let Some(visual_scene) = doc.descendants().find(|n| n.has_tag_name("visual_scene")) {
        for child in visual_scene.children().filter(|n| n.has_tag_name("node")) {
            parse_scene_node(child, None, &mut model);
        }
    }

    // Ensure a "Root" joint exists so the frontend can display the full
    // hierarchy tree. Meshes are assigned parent_name="Root" during
    // geometry parsing, but the scene parser skips ROOT_ wrapper nodes.
    // Without this, the HierarchyTree has no root to attach meshes under.
    if !model.joints.iter().any(|j| j.name == "Root") {
        model.joints.insert(
            0,
            HODJoint {
                name: "Root".to_string(),
                parent_name: None,
                local_transform: Matrix4 {
                    m: [
                        [1.0, 0.0, 0.0, 0.0],
                        [0.0, 1.0, 0.0, 0.0],
                        [0.0, 0.0, 1.0, 0.0],
                        [0.0, 0.0, 0.0, 1.0],
                    ],
                },
                position: Some(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                }),
                rotation: Some(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                }),
                scale: Some(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                }),
            },
        );
    }

    // Parse animations from library_animations
    parse_animations(&doc, &mut model);
    normalize_collision_meshes(&mut model);

    Ok(model)
}

fn parse_matrix(node: Node) -> Matrix4 {
    let mut mat = Matrix4 { m: [[0.0; 4]; 4] };
    mat.m[0][0] = 1.0;
    mat.m[1][1] = 1.0;
    mat.m[2][2] = 1.0;
    mat.m[3][3] = 1.0;

    if let Some(text) = node.text() {
        let floats: Vec<f32> = text
            .split_whitespace()
            .filter_map(|s| s.parse::<f32>().ok())
            .collect();
        if floats.len() == 16 {
            // Collada matrices are row-major
            mat.m[0] = [floats[0], floats[4], floats[8], floats[12]];
            mat.m[1] = [floats[1], floats[5], floats[9], floats[13]];
            mat.m[2] = [floats[2], floats[6], floats[10], floats[14]];
            mat.m[3] = [floats[3], floats[7], floats[11], floats[15]];
        }
    }
    mat
}

/// Compose a scene node's local transform from its <matrix> or
/// <translate>/<rotate> children, in document order.
fn node_transform(node: Node) -> Matrix4 {
    let mut transform = Matrix4 { m: [[0.0; 4]; 4] };
    transform.m[0][0] = 1.0;
    transform.m[1][1] = 1.0;
    transform.m[2][2] = 1.0;
    transform.m[3][3] = 1.0;

    if let Some(matrix_node) = node.children().find(|n| n.has_tag_name("matrix")) {
        transform = parse_matrix(matrix_node);
    } else {
        // DAE uses <translate> + <rotate> elements instead of <matrix>.
        // Build transform by composing them in order.
        for child in node.children() {
            if child.has_tag_name("translate") {
                if let Some(text) = child.text() {
                    let t: Vec<f32> = text
                        .split_whitespace()
                        .filter_map(|s| s.parse().ok())
                        .collect();
                    if t.len() >= 3 {
                        transform.m[3][0] = t[0];
                        transform.m[3][1] = t[1];
                        transform.m[3][2] = t[2];
                    }
                }
            } else if child.has_tag_name("rotate") {
                if let Some(text) = child.text() {
                    let r: Vec<f32> = text
                        .split_whitespace()
                        .filter_map(|s| s.parse().ok())
                        .collect();
                    if r.len() >= 4 {
                        let (ax, ay, az, angle) = (r[0], r[1], r[2], r[3]);
                        let rad = angle * std::f32::consts::PI / 180.0;
                        let (s, c) = (rad.sin(), rad.cos());
                        let t = 1.0 - c;
                        let len = (ax * ax + ay * ay + az * az).sqrt();
                        let (ax, ay, az) = if len > 0.0 {
                            (ax / len, ay / len, az / len)
                        } else {
                            (0.0, 0.0, 0.0)
                        };
                        let rot = Matrix4 {
                            m: [
                                [
                                    t * ax * ax + c,
                                    t * ax * ay - s * az,
                                    t * ax * az + s * ay,
                                    0.0,
                                ],
                                [
                                    t * ax * ay + s * az,
                                    t * ay * ay + c,
                                    t * ay * az - s * ax,
                                    0.0,
                                ],
                                [
                                    t * ax * az - s * ay,
                                    t * ay * az + s * ax,
                                    t * az * az + c,
                                    0.0,
                                ],
                                [0.0, 0.0, 0.0, 1.0],
                            ],
                        };
                        // Multiply: transform = rot * transform (rotate around origin)
                        let mut result = Matrix4 { m: [[0.0; 4]; 4] };
                        for r in 0..4 {
                            for c in 0..4 {
                                result.m[r][c] = rot.m[r][0] * transform.m[0][c]
                                    + rot.m[r][1] * transform.m[1][c]
                                    + rot.m[r][2] * transform.m[2][c]
                                    + rot.m[r][3] * transform.m[3][c];
                            }
                        }
                        transform = result;
                    }
                }
            }
        }
    }
    transform
}

fn parse_scene_node(node: Node, parent_name: Option<&str>, model: &mut HODModel) {
    let mut name = node.attribute("name").unwrap_or("unknown").to_string();
    let mut is_joint = false;
    let mut is_navl = false;
    let mut is_burn = false;
    let mut is_mark = false;
    let mut is_dock = false;

    // Extract real name and type from prefixes
    if name.starts_with("JNT[") {
        if let Some(end) = name.find("]") {
            name = name[4..end].to_string();
            is_joint = true;
        }
    } else if name.starts_with("MARK[") {
        if let Some(end) = name.find("]") {
            name = name[5..end].to_string();
            is_mark = true;
        }
    } else if name.starts_with("NAVL[") {
        if let Some(end) = name.find("]") {
            name = name[5..end].to_string();
            is_navl = true;
        }
    } else if name.starts_with("BURN[") {
        if let Some(end) = name.find("]") {
            name = name[5..end].to_string();
            is_burn = true;
        }
    } else if name.starts_with("DOCK[") {
        if let Some(end) = name.find("]") {
            name = name[5..end].to_string();
            is_dock = true;
        }
    }

    let mut transform = node_transform(node);

    let p_name = parent_name.map(|s| s.to_string());

    // If this is a top-level node (p_name is None), discard the transform.
    // Blender's Collada exporter adds an X-rotation matrix to convert Z_UP to Y_UP.
    // Homeworld modders model in Y_UP natively, so this matrix incorrectly rotates the ship.
    if p_name.is_none() {
        transform = Matrix4 {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        };
    }

    if is_joint {
        let (pos, rot, _) = decompose_matrix(transform.clone());
        model.joints.push(HODJoint {
            name: name.clone(),
            parent_name: p_name.clone(),
            local_transform: transform.clone(),
            position: Some(pos),
            rotation: Some(rot),
            scale: Some(Vector3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            }),
        });
    } else if is_mark {
        model.markers.push(HODMarker {
            name: name.clone(),
            parent_joint: p_name.clone().unwrap_or_default(),
            position: Vector3 {
                x: transform.m[3][0],
                y: transform.m[3][1],
                z: transform.m[3][2],
            },
            rotation: transform.clone(),
            rotation_euler: None,
        });
    } else if is_navl {
        // Parse navlight attributes from name string:
        // NAVL[name]_Type[style]_Sz[size]_Ph[phase]_Fr[freq]_Col[r,g,b]_Dist[dist]_Flags[flags]
        // NOTE: the style string comes from _Type[...] (e.g. "default"), NOT
        // from _Flags[...] — RODOH writes the Type verbatim and ignores Flags
        // (ter_zephyrus: Type[default] + Flags[Sprite] -> style "default").
        // Writing the Flags value ("Sprite") selects an unknown engine effect
        // and access-violates at match start when navlights render.
        let full_name = node.attribute("name").unwrap_or("");
        let size = extract_bracket_value(full_name, "_Sz[")
            .and_then(|v| v.parse::<f32>().ok())
            .unwrap_or(10.0);
        let phase = extract_bracket_value(full_name, "_Ph[")
            .and_then(|v| v.parse::<f32>().ok())
            .unwrap_or(0.0);
        let frequency = extract_bracket_value(full_name, "_Fr[")
            .and_then(|v| v.parse::<f32>().ok())
            .unwrap_or(0.0);
        let (cr, cg, cb) = extract_bracket_value(full_name, "_Col[")
            .and_then(|v| {
                let parts: Vec<f32> = v.split(',').filter_map(|s| s.parse().ok()).collect();
                if parts.len() >= 3 {
                    Some((parts[0], parts[1], parts[2]))
                } else {
                    None
                }
            })
            .unwrap_or((1.0, 1.0, 1.0));
        let distance = extract_bracket_value(full_name, "_Dist[")
            .and_then(|v| v.parse::<f32>().ok())
            .unwrap_or(1000.0);
        let style = extract_bracket_value(full_name, "_Type[")
            .unwrap_or("default")
            .to_string();

        model.nav_lights.push(HODNavLight {
            name: name.clone(),
            section: 0,
            size,
            phase,
            frequency,
            style,
            color: Vector3 {
                x: cr,
                y: cg,
                z: cb,
            },
            distance,
            sprite_visible: true,
            high_end_only: false,
        });
        // Also create a joint so the frontend can parent the navlight
        // in the hierarchy tree. Without this, rootNavLights filter
        // places it at the root level since no matching joint exists.
        let (pos, rot, _) = decompose_matrix(transform.clone());
        model.joints.push(HODJoint {
            name: name.clone(),
            parent_name: p_name.clone(),
            local_transform: transform.clone(),
            position: Some(pos),
            rotation: Some(rot),
            scale: Some(Vector3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            }),
        });
    } else if is_burn {
        // Parse Flame children to extract burn vertices
        let mut burn_vertices = Vec::new();
        for child in node.children().filter(|n| n.has_tag_name("node")) {
            let child_name = child.attribute("name").unwrap_or("");
            if child_name.starts_with("Flame[") {
                // Extract position from translate element
                if let Some(t) = child.children().find(|n| n.has_tag_name("translate")) {
                    if let Some(text) = t.text() {
                        let coords: Vec<f32> = text
                            .split_whitespace()
                            .filter_map(|s| s.parse().ok())
                            .collect();
                        if coords.len() >= 3 {
                            burn_vertices.push(Vector3 {
                                x: coords[0],
                                y: coords[1],
                                z: coords[2],
                            });
                        }
                    }
                }
            }
        }
        let num_divisions = burn_vertices.len().max(2) as i32;
        model.engine_burns.push(HODEngineBurn {
            name: name.clone(),
            parent_name: p_name.clone().unwrap_or_default(),
            num_divisions,
            num_flames: 1,
            vertices: burn_vertices,
        });
    } else if is_dock {
        // Dock path: DOCK[path]_Fam[family]_Flags[flags] with
        // SEG[i]_Tol[t]_Spd[s]_Flags[...] point children.
        // Reverse-engineered from RODOH output (ter_zephyrus_2.0_original.hod):
        // parent is always 'world', compatible_ships comes from Fam[...],
        // val1/val2/val4 are constant (6/10/1), val3 is 1 when the Flags
        // contain "Exit" (Exit Latch paths) and 0 otherwise (Latch paths).
        let full_name = node.attribute("name").unwrap_or("");
        let compatible_ships = extract_bracket_value(full_name, "_Fam[")
            .unwrap_or("")
            .to_string();
        let flags = extract_bracket_value(full_name, "_Flags[").unwrap_or("");
        let mut indexed_points: Vec<(usize, HODDockpoint)> = Vec::new();
        for child in node.children().filter(|n| n.has_tag_name("node")) {
            let child_name = child.attribute("name").unwrap_or("");
            if !child_name.starts_with("SEG[") {
                continue;
            }
            let seg_idx = extract_bracket_value(child_name, "SEG[")
                .and_then(|v| v.parse::<usize>().ok())
                .unwrap_or(usize::MAX);
            let tolerance = extract_bracket_value(child_name, "_Tol[")
                .and_then(|v| v.parse::<f32>().ok())
                .unwrap_or(0.0);
            let max_speed = extract_bracket_value(child_name, "_Spd[")
                .and_then(|v| v.parse::<f32>().ok())
                .unwrap_or(0.0);
            let t = node_transform(child);
            let mut rotation = Matrix4 {
                m: [
                    [1.0, 0.0, 0.0, 0.0],
                    [0.0, 1.0, 0.0, 0.0],
                    [0.0, 0.0, 1.0, 0.0],
                    [0.0, 0.0, 0.0, 1.0],
                ],
            };
            for r in 0..3 {
                for c in 0..3 {
                    rotation.m[r][c] = t.m[r][c];
                }
            }
            indexed_points.push((
                seg_idx,
                HODDockpoint {
                    position: Vector3 {
                        x: t.m[3][0],
                        y: t.m[3][1],
                        z: t.m[3][2],
                    },
                    rotation,
                    tolerance,
                    max_speed,
                    extra1: 0,
                    extra2: 0,
                },
            ));
        }
        indexed_points.sort_by_key(|(idx, _)| *idx);
        let points = indexed_points.into_iter().map(|(_, p)| p).collect();
        model.dockpaths.push(HODDockpath {
            name: name.clone(),
            parent_name: "world".to_string(),
            points,
            val1: 6,
            val2: 10,
            val3: if flags.contains("Exit") { 1 } else { 0 },
            val4: 1,
            val5: 0,
            compatible_ships,
            padding1: 0,
            padding2: String::new(),
        });
        // SEG children are consumed inline above; do not recurse into them
        // (or they would leak into the joint hierarchy as bogus joints).
        return;
    } else {
        // Generic node — add as joint so the hierarchy tree can display it.
        // Filtered prefixes: Flame[], Class[], ROOT_, UVSets[], COL[], HOLD_
        // MULT[...] nodes: update mesh parent_name from scene graph, don't add as joint.
        if name.starts_with("MULT[") {
            // MULT[radar]_LOD[0] → mesh name "radar"
            if let Some(end) = name.find("]") {
                let mesh_name = name[5..end].to_string();
                let new_parent = p_name.clone().unwrap_or_else(|| "Root".to_string());

                // Only update the mesh parent if the new parent is NOT "Root".
                // This prevents ROOT_LOD[x] wrapper nodes from overwriting the actual joint parent (like JNT[RadarDish])
                // which was correctly assigned when LOD[0] was parsed inside the joint hierarchy.
                if new_parent != "Root" {
                    for mesh in &mut model.meshes {
                        if mesh.name == mesh_name {
                            mesh.parent_name = new_parent.clone();
                        }
                    }
                }
            }
        } else if !name.starts_with("Flame[")
            && !name.starts_with("Class[")
            && !name.starts_with("ROOT_")
            && !name.starts_with("UVSets[")
            && !name.starts_with("COL[")
            && !name.starts_with("HOLD_")
            && !name.starts_with("ANIM[")
            && !name.starts_with("DOCK[")
            && !name.starts_with("SEG[")
        {
            let (pos, rot, _) = decompose_matrix(transform.clone());
            model.joints.push(HODJoint {
                name: name.clone(),
                parent_name: p_name.clone(),
                local_transform: transform.clone(),
                position: Some(pos),
                rotation: Some(rot),
                scale: Some(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                }),
            });
        }
    }

    let next_parent = if name.starts_with("ROOT_") || name.starts_with("HOLD_") {
        Some("Root") // Or None, since HOD defaults to Root when not specified
    } else {
        Some(name.as_str())
    };

    // Parse children
    for child in node.children().filter(|n| n.has_tag_name("node")) {
        parse_scene_node(child, next_parent, model);
    }
}

fn parse_animations(doc: &Document, model: &mut HODModel) {
    if let Some(lib_anims) = doc
        .descendants()
        .find(|n| n.has_tag_name("library_animations"))
    {
        let mut tracks_map: HashMap<
            String,
            (
                Vec<(f64, f64)>,
                Vec<(f64, f64)>,
                Vec<(f64, f64)>,
                Vec<(f64, f64)>,
                Vec<(f64, f64)>,
                Vec<(f64, f64)>,
            ),
        > = HashMap::new();

        for anim in lib_anims.children().filter(|n| n.has_tag_name("animation")) {
            if let Some(channel) = anim.children().find(|n| n.has_tag_name("channel")) {
                let target = channel.attribute("target").unwrap_or("");
                let parts: Vec<&str> = target.split('/').collect();
                if parts.len() == 2 {
                    let mut joint_name = parts[0].to_string();
                    if joint_name.starts_with("JNT[") {
                        if let Some(end) = joint_name.find("]") {
                            joint_name = joint_name[4..end].to_string();
                        }
                    }
                    let prop = parts[1];

                    let source_id = channel
                        .attribute("source")
                        .unwrap_or("")
                        .trim_start_matches('#');
                    if let Some(sampler) = anim
                        .children()
                        .find(|n| n.has_tag_name("sampler") && n.attribute("id") == Some(source_id))
                    {
                        let mut input_id = "";
                        let mut output_id = "";
                        for input in sampler.children().filter(|n| n.has_tag_name("input")) {
                            let semantic = input.attribute("semantic").unwrap_or("");
                            if semantic == "INPUT" {
                                input_id = input
                                    .attribute("source")
                                    .unwrap_or("")
                                    .trim_start_matches('#');
                            } else if semantic == "OUTPUT" {
                                output_id = input
                                    .attribute("source")
                                    .unwrap_or("")
                                    .trim_start_matches('#');
                            }
                        }

                        let mut times = Vec::new();
                        let mut values = Vec::new();

                        for source in anim.children().filter(|n| n.has_tag_name("source")) {
                            let id = source.attribute("id").unwrap_or("");
                            if id == input_id {
                                if let Some(float_array) =
                                    source.children().find(|n| n.has_tag_name("float_array"))
                                {
                                    if let Ok(floats) = parse_float_array(float_array) {
                                        times = floats.into_iter().map(|f| f as f64).collect();
                                    }
                                }
                            } else if id == output_id {
                                if let Some(float_array) =
                                    source.children().find(|n| n.has_tag_name("float_array"))
                                {
                                    if let Ok(floats) = parse_float_array(float_array) {
                                        values = floats.into_iter().map(|f| f as f64).collect();
                                    }
                                }
                            }
                        }

                        if times.len() == values.len() && !times.is_empty() {
                            let curve: Vec<(f64, f64)> =
                                times.into_iter().zip(values.into_iter()).collect();
                            let entry = tracks_map.entry(joint_name).or_insert_with(|| {
                                (
                                    Vec::new(),
                                    Vec::new(),
                                    Vec::new(),
                                    Vec::new(),
                                    Vec::new(),
                                    Vec::new(),
                                )
                            });
                            if prop.starts_with("translate.X") {
                                entry.0 = curve;
                            } else if prop.starts_with("translate.Y") {
                                entry.1 = curve;
                            } else if prop.starts_with("translate.Z") {
                                entry.2 = curve;
                            } else if prop.starts_with("rotateX") {
                                entry.3 = curve;
                            } else if prop.starts_with("rotateY") {
                                entry.4 = curve;
                            } else if prop.starts_with("rotateZ") {
                                entry.5 = curve;
                            }
                        }
                    }
                }
            }
        }

        if !tracks_map.is_empty() {
            let mut hod_anim = HODAnimation {
                name: "DefaultAnimation".to_string(),
                duration: 0.0,
                tracks: Vec::new(),
            };

            for (joint_name, (tx, ty, tz, rx, ry, rz)) in tracks_map {
                let mut unique_times: Vec<f64> = Vec::new();
                for curve in [&tx, &ty, &tz, &rx, &ry, &rz] {
                    for &(t, _) in curve {
                        if !unique_times.iter().any(|&ut| (ut - t).abs() < 1e-5) {
                            unique_times.push(t);
                        }
                    }
                }
                unique_times.sort_by(|a, b| a.partial_cmp(b).unwrap());
                if let Some(&max_t) = unique_times.last() {
                    if max_t > hod_anim.duration {
                        hod_anim.duration = max_t;
                    }
                }

                let eval = |curve: &Vec<(f64, f64)>, t: f64, default: f64| -> f64 {
                    if curve.is_empty() {
                        return default;
                    }
                    if t <= curve[0].0 {
                        return curve[0].1;
                    }
                    if t >= curve.last().unwrap().0 {
                        return curve.last().unwrap().1;
                    }
                    for i in 0..curve.len() - 1 {
                        if t >= curve[i].0 && t <= curve[i + 1].0 {
                            let range = curve[i + 1].0 - curve[i].0;
                            if range < 1e-5 {
                                return curve[i].1;
                            }
                            let f = (t - curve[i].0) / range;
                            return curve[i].1 + f * (curve[i + 1].1 - curve[i].1);
                        }
                    }
                    default
                };

                let mut def_pos = Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                };
                let def_rot = Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                };
                if let Some(joint) = model.joints.iter().find(|j| j.name == joint_name) {
                    def_pos = Vector3 {
                        x: joint.local_transform.m[3][0],
                        y: joint.local_transform.m[3][1],
                        z: joint.local_transform.m[3][2],
                    };
                }

                let mut keyframes = Vec::new();
                for &time in &unique_times {
                    let px = eval(&tx, time, def_pos.x as f64);
                    let py = eval(&ty, time, def_pos.y as f64);
                    let pz = eval(&tz, time, def_pos.z as f64);

                    let rx_deg = eval(&rx, time, def_rot.x as f64);
                    let ry_deg = eval(&ry, time, def_rot.y as f64);
                    let rz_deg = eval(&rz, time, def_rot.z as f64);

                    let rot_euler = Vector3 {
                        x: (rx_deg * std::f64::consts::PI / 180.0) as f32,
                        y: (ry_deg * std::f64::consts::PI / 180.0) as f32,
                        z: (rz_deg * std::f64::consts::PI / 180.0) as f32,
                    };

                    let rot_quat = euler_to_quaternion(&rot_euler);

                    keyframes.push(HODKeyframe {
                        time,
                        position: Some(Vector3 {
                            x: px as f32,
                            y: py as f32,
                            z: pz as f32,
                        }),
                        rotation: Some(rot_quat),
                        rotation_euler: Some(rot_euler),
                        scale: Some(Vector3 {
                            x: 1.0,
                            y: 1.0,
                            z: 1.0,
                        }),
                    });
                }

                hod_anim.tracks.push(HODAnimationTrack {
                    joint_name,
                    keyframes,
                });
            }

            model.animations.push(hod_anim);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const DOCK_DAE: &str = r##"<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <library_visual_scenes>
    <visual_scene id="test" name="test">
      <node name="ROOT_LOD[0]" id="ROOT_LOD[0]">
        <node name="HOLD_DOCK" id="HOLD_DOCK">
          <translate sid="translate">0 0 0</translate>
          <node name="DOCK[path1]_Fam[Utility]_Flags[Latch]" id="DOCK[path1]_Fam[Utility]_Flags[Latch]">
            <translate sid="translate">0 0 0</translate>
            <rotate sid="rotateZ">0 0 1 0</rotate>
            <rotate sid="rotateY">0 1 0 0</rotate>
            <rotate sid="rotateX">1 0 0 0</rotate>
            <node name="SEG[0]_Tol[50]_Spd[0]_Flags[Player Queue]" id="s0">
              <translate sid="translate">-170.4814 0 -68.30959</translate>
              <rotate sid="rotateZ">0 0 1 0</rotate>
              <rotate sid="rotateY">0 1 0 0</rotate>
              <rotate sid="rotateX">1 0 0 0</rotate>
            </node>
            <node name="SEG[1]_Tol[30]_Spd[5]_Flags[UseRot]" id="s1">
              <translate sid="translate">-79.468 -7.882353 -19.99933</translate>
              <rotate sid="rotateZ">0 0 1 0</rotate>
              <rotate sid="rotateY">0 1 0 -90</rotate>
              <rotate sid="rotateX">1 0 0 0</rotate>
            </node>
          </node>
          <node name="DOCK[path2]_Fam[Utility]_Flags[Exit Latch]" id="DOCK[path2]_Fam[Utility]_Flags[Exit Latch]">
            <translate sid="translate">0 0 0</translate>
            <node name="SEG[0]_Tol[5]_Spd[0]" id="s2">
              <translate sid="translate">1 2 3</translate>
            </node>
          </node>
        </node>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#test"/>
  </scene>
</COLLADA>"##;

    #[test]
    fn dae_top_level_export_transform_is_discarded_in_y_up_space() {
        // Blender/Collada often wraps the scene in a Z_UP -> Y_UP rotation.
        // HODEditorJS models in HWRM's native Y-up/+Z-forward space, so the
        // wrapper transform must be dropped without rotating the ship.
        let xml = r##"<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <asset><up_axis>Y_UP</up_axis></asset>
  <library_visual_scenes>
    <visual_scene id="test" name="test">
      <node name="ROOT_LOD[0]" id="ROOT_LOD[0]">
        <matrix sid="transform">1 0 0 0 0 0 1 0 0 -1 0 0 0 0 0 1</matrix>
        <node name="JNT[TestJoint]" id="j1">
          <translate sid="translate">0 0 10</translate>
        </node>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene><instance_visual_scene url="#test"/></scene>
</COLLADA>"##;

        let model = parse_dae(xml).expect("DAE with top-level export transform must parse");
        let joint = model.joints.iter().find(|j| j.name == "TestJoint").expect("TestJoint must exist");

        // The +Z offset survives, and the wrapper's X-rotation is not applied.
        assert!((joint.local_transform.m[3][0] - 0.0).abs() < 1e-5);
        assert!((joint.local_transform.m[3][1] - 0.0).abs() < 1e-5);
        assert!((joint.local_transform.m[3][2] - 10.0).abs() < 1e-5);
        assert!((joint.local_transform.m[0][0] - 1.0).abs() < 1e-5);
        assert!((joint.local_transform.m[1][1] - 1.0).abs() < 1e-5);
        assert!((joint.local_transform.m[2][2] - 1.0).abs() < 1e-5);
        assert!(joint.local_transform.m[1][2].abs() < 1e-5);
        assert!(joint.local_transform.m[2][1].abs() < 1e-5);
    }

    #[test]
    fn dae_navlight_style_comes_from_type_not_flags() {
        let xml = r##"<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
  <library_visual_scenes>
    <visual_scene id="test" name="test">
      <node name="ROOT_LOD[0]" id="ROOT_LOD[0]">
        <node name="NAVL[e1]_Type[default]_Sz[50]_Ph[0]_Fr[0]_Col[0,0,1]_Dist[130]_Flags[Sprite]" id="n1">
          <translate sid="translate">0 0 0</translate>
        </node>
      </node>
    </visual_scene>
  </library_visual_scenes>
  <scene>
    <instance_visual_scene url="#test"/>
  </scene>
</COLLADA>"##;
        let model = parse_dae(xml).expect("DAE with NAVL node must parse");
        assert_eq!(model.nav_lights.len(), 1);
        // RODOH writes the _Type value verbatim ("default"), never _Flags ("Sprite").
        assert_eq!(model.nav_lights[0].style, "default");
    }

    #[test]
    fn dae_dockpaths_parse_name_family_points() {
        let model = parse_dae(DOCK_DAE).expect("DAE with DOCK nodes must parse");
        assert_eq!(model.dockpaths.len(), 2);

        let p1 = &model.dockpaths[0];
        assert_eq!(p1.name, "path1");
        assert_eq!(p1.parent_name, "world");
        assert_eq!(p1.compatible_ships, "Utility");
        assert_eq!(p1.val3, 0);
        assert_eq!(p1.points.len(), 2);
        assert!((p1.points[0].position.x - -170.4814).abs() < 1e-3);
        assert!((p1.points[0].position.z - -68.30959).abs() < 1e-3);
        assert_eq!(p1.points[0].tolerance, 50.0);
        assert_eq!(p1.points[1].tolerance, 30.0);
        assert_eq!(p1.points[1].max_speed, 5.0);
        // -90deg about Y: local +X maps to -Z in the rotation rows.
        assert!((p1.points[1].rotation.m[0][2] - -1.0).abs() < 1e-5);

        let p2 = &model.dockpaths[1];
        assert_eq!(p2.name, "path2");
        assert_eq!(p2.val3, 1);
        assert_eq!(p2.points.len(), 1);

        // DOCK/SEG scaffolding must not leak into the joint hierarchy.
        assert!(!model.joints.iter().any(|j| j.name.starts_with("DOCK[")
            || j.name.starts_with("SEG[")
            || j.name.starts_with("HOLD_")));
    }
}
