use crate::hod::{HODCollisionMesh, HODJoint, HODMesh, HODMeshPart, HODVertex, Vector3};

/// LOD quality levels: (LOD level, target ratio relative to LOD 0)
const LOD_LEVELS: &[(i32, f32)] = &[
    (1, 0.40), // LOD 1: ~40% of LOD 0 (noticeable reduction)
    (2, 0.20), // LOD 2: ~20% of LOD 0 (clearly simplified)
    (3, 0.08), // LOD 3: ~8% of LOD 0 (very low poly)
];

/// Decimate a mesh using meshopt's industry-standard algorithm.
/// Uses simplify_sloppy for aggressive reduction.
pub fn decimate_mesh(
    vertices: &[HODVertex],
    indices: &[u16],
    target_ratio: f32,
) -> (Vec<HODVertex>, Vec<u16>) {
    if vertices.is_empty() || indices.is_empty() {
        return (Vec::new(), Vec::new());
    }

    let target_index_count = ((indices.len() as f32) * target_ratio) as usize;

    // Build position buffer (x, y, z per vertex) for meshopt
    let mut positions: Vec<f32> = Vec::with_capacity(vertices.len() * 3);
    for v in vertices {
        positions.push(v.position.x);
        positions.push(v.position.y);
        positions.push(v.position.z);
    }
    let position_bytes: Vec<u8> = positions.iter().flat_map(|f| f.to_le_bytes()).collect();

    // Convert indices to u32
    let indices_u32: Vec<u32> = indices.iter().map(|&i| i as u32).collect();

    // Create VertexDataAdapter with position data
    let vertex_adapter = meshopt::VertexDataAdapter::new(&position_bytes, 12, 0)
        .expect("Failed to create vertex adapter");

    // Use simplify_sloppy for aggressive reduction
    let target_count = target_index_count.max(3);

    let simplified = meshopt::simplify_sloppy(
        &indices_u32,
        &vertex_adapter,
        target_count,
        0.90, // Error tolerance (90% of mesh size - very aggressive)
        None,
    );

    // Keep ALL original vertices to preserve UV coordinates
    // The simplified indices reference the original vertex buffer
    // This preserves UVs even though some vertices may be unused
    let final_vertices = vertices.to_vec();
    let final_indices: Vec<u16> = simplified.iter().map(|&i| i as u16).collect();

    (final_vertices, final_indices)
}

// ============================================================================
// 3D Convex Hull (Gift Wrapping Algorithm)
// ============================================================================

fn push_unique_point(sampled: &mut Vec<[f32; 3]>, p: [f32; 3], eps: f32) {
    if !sampled.iter().any(|s| {
        (s[0] - p[0]).abs() < eps && (s[1] - p[1]).abs() < eps && (s[2] - p[2]).abs() < eps
    }) {
        sampled.push(p);
    }
}

fn push_aabb_extremes(sampled: &mut Vec<[f32; 3]>, points: &[[f32; 3]], eps: f32) {
    if points.is_empty() {
        return;
    }
    let extremes: [[f32; 3]; 6] = [
        *points
            .iter()
            .min_by(|a, b| a[0].partial_cmp(&b[0]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
        *points
            .iter()
            .max_by(|a, b| a[0].partial_cmp(&b[0]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
        *points
            .iter()
            .min_by(|a, b| a[1].partial_cmp(&b[1]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
        *points
            .iter()
            .max_by(|a, b| a[1].partial_cmp(&b[1]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
        *points
            .iter()
            .min_by(|a, b| a[2].partial_cmp(&b[2]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
        *points
            .iter()
            .max_by(|a, b| a[2].partial_cmp(&b[2]).unwrap_or(std::cmp::Ordering::Equal))
            .unwrap(),
    ];
    for p in extremes {
        push_unique_point(sampled, p, eps);
    }
}

/// Sample points for convex hull computation.
/// Always keeps the 6 AABB extreme points (min/max on each axis) to preserve shape,
/// then uniformly samples the rest to reach the target count.
fn sample_points_for_hull(points: &[[f32; 3]], target: usize) -> Vec<[f32; 3]> {
    if points.len() <= target {
        return points.to_vec();
    }

    let eps = 1e-3;
    let mut sampled: Vec<[f32; 3]> = Vec::new();

    // Always include the 6 AABB extreme points
    push_aabb_extremes(&mut sampled, points, eps);

    // Uniformly sample remaining points
    let remaining = target.saturating_sub(sampled.len());
    if remaining > 0 {
        let step = (points.len() as f32) / (remaining as f32);
        for i in 0..remaining {
            let idx = ((i as f32) * step) as usize;
            let idx = idx.min(points.len() - 1);
            push_unique_point(&mut sampled, points[idx], eps);
        }
    }

    sampled
}

/// Sample points across multiple meshes for convex hull computation.
/// Preserves the 6 AABB extreme points of each included mesh as well as the global extremes,
/// then uniformly samples the remaining budget.
fn sample_mesh_sets_for_hull(
    mesh_point_sets: &[Vec<[f32; 3]>],
    all_positions: &[[f32; 3]],
    target: usize,
) -> Vec<[f32; 3]> {
    if all_positions.len() <= target {
        return all_positions.to_vec();
    }
    if mesh_point_sets.len() <= 1 {
        return sample_points_for_hull(all_positions, target);
    }

    let eps = 1e-3;
    let mut sampled: Vec<[f32; 3]> = Vec::new();

    push_aabb_extremes(&mut sampled, all_positions, eps);
    for set in mesh_point_sets {
        push_aabb_extremes(&mut sampled, set, eps);
    }

    let remaining = target.saturating_sub(sampled.len());
    if remaining > 0 {
        let step = (all_positions.len() as f32) / (remaining as f32);
        for i in 0..remaining {
            let idx = ((i as f32) * step) as usize;
            let idx = idx.min(all_positions.len() - 1);
            push_unique_point(&mut sampled, all_positions[idx], eps);
        }
    }

    sampled
}

fn strip_lod_suffix(name: &str) -> &str {
    let lower = name.to_ascii_lowercase();
    for marker in ["_lod_", "_lod"] {
        if let Some(pos) = lower.rfind(marker) {
            let digits = &name[pos + marker.len()..];
            if !digits.is_empty() && digits.chars().all(|c| c.is_ascii_digit()) {
                return &name[..pos];
            }
        }
    }
    name
}

fn sanitize_local_transform(m: &[[f32; 4]; 4]) -> [[f32; 4]; 4] {
    let mut out = *m;
    let s0 = (m[0][0] * m[0][0] + m[0][1] * m[0][1] + m[0][2] * m[0][2]).sqrt();
    let s1 = (m[1][0] * m[1][0] + m[1][1] * m[1][1] + m[1][2] * m[1][2]).sqrt();
    let s2 = (m[2][0] * m[2][0] + m[2][1] * m[2][1] + m[2][2] * m[2][2]).sqrt();
    if s0 < 0.0001 {
        out[0][0] = 1.0;
        out[0][1] = 0.0;
        out[0][2] = 0.0;
    }
    if s1 < 0.0001 {
        out[1][0] = 0.0;
        out[1][1] = 1.0;
        out[1][2] = 0.0;
    }
    if s2 < 0.0001 {
        out[2][0] = 0.0;
        out[2][1] = 0.0;
        out[2][2] = 1.0;
    }
    out
}

fn transform_point_to_root(mut p: [f32; 3], parent_name: &str, joints: &[HODJoint]) -> [f32; 3] {
    let mut current = parent_name;
    let mut visited: Vec<String> = Vec::new();
    while !current.is_empty() && !current.eq_ignore_ascii_case("root") {
        let key = current.to_ascii_lowercase();
        if visited.contains(&key) {
            break;
        }
        visited.push(key);
        let Some(joint) = joints.iter().find(|j| j.name.eq_ignore_ascii_case(current)) else {
            break;
        };
        let m = sanitize_local_transform(&joint.local_transform.m);
        p = [
            p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0] + m[3][0],
            p[0] * m[0][1] + p[1] * m[1][1] + p[2] * m[2][1] + m[3][1],
            p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2] + m[3][2],
        ];
        match joint.parent_name.as_deref() {
            Some(next) if !next.eq_ignore_ascii_case(current) => current = next,
            _ => break,
        }
    }
    p
}

fn select_target_meshes<'a>(
    visible_meshes: &'a [HODMesh],
    mesh_names: &[String],
) -> Vec<&'a HODMesh> {
    let has_verts = |m: &HODMesh| m.parts.iter().any(|p| !p.vertices.is_empty());

    if mesh_names.is_empty() {
        if let Some(m) = visible_meshes
            .iter()
            .find(|m| m.lod == 0 && has_verts(m))
            .or_else(|| visible_meshes.iter().find(|m| m.lod == 0))
        {
            return vec![m];
        }
        return Vec::new();
    }

    let mut selected: Vec<&'a HODMesh> = Vec::new();
    for raw_name in mesh_names {
        let req = raw_name.trim();
        if req.is_empty() {
            continue;
        }
        let req_base = strip_lod_suffix(req);

        // 1. Check exact "<name>_lod_<lod>" match first
        let exact_lod_match = visible_meshes.iter().find(|m| {
            has_verts(m) && format!("{}_lod_{}", m.name, m.lod).eq_ignore_ascii_case(req)
        });

        // 2. Otherwise pick the lowest-LOD mesh matching the name or base name
        let chosen = exact_lod_match.or_else(|| {
            visible_meshes
                .iter()
                .filter(|m| {
                    has_verts(m)
                        && (m.name.eq_ignore_ascii_case(req)
                            || strip_lod_suffix(&m.name).eq_ignore_ascii_case(req_base))
                })
                .min_by_key(|m| m.lod)
        });

        if let Some(mesh) = chosen {
            if !selected.iter().any(|existing| std::ptr::eq(*existing, mesh)) {
                selected.push(mesh);
            }
        }
    }

    selected
}

fn cross(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn dot(a: &[f32; 3], b: &[f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn vec_len(v: &[f32; 3]) -> f32 {
    (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt()
}

fn vec_sub(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}

fn vec_add(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}

fn vec_scale(v: &[f32; 3], s: f32) -> [f32; 3] {
    [v[0] * s, v[1] * s, v[2] * s]
}

fn vec_normalize(v: &[f32; 3]) -> [f32; 3] {
    let len = vec_len(v);
    if len < 1e-10 {
        return [0.0, 0.0, 0.0];
    }
    [v[0] / len, v[1] / len, v[2] / len]
}

/// Compute a 3D convex hull using brute force.
/// Returns (hull_vertices, hull_triangles) where triangles are index triples.
pub fn convex_hull_3d(points: &[[f32; 3]]) -> (Vec<[f32; 3]>, Vec<[u16; 3]>) {
    if points.len() < 4 {
        return (points.to_vec(), Vec::new());
    }

    // Deduplicate input points and apply a tiny deterministic perturbation to avoid coplanar degeneracies.
    // This breaks exact coplanarity of faces, preventing the brute force algorithm from
    // generating overlapping triangles on flat surfaces (e.g., quads).
    let mut min_bound = [f32::MAX; 3];
    let mut max_bound = [f32::MIN; 3];
    for p in points {
        min_bound[0] = min_bound[0].min(p[0]);
        min_bound[1] = min_bound[1].min(p[1]);
        min_bound[2] = min_bound[2].min(p[2]);
        max_bound[0] = max_bound[0].max(p[0]);
        max_bound[1] = max_bound[1].max(p[1]);
        max_bound[2] = max_bound[2].max(p[2]);
    }

    let diag = vec_len(&vec_sub(&max_bound, &min_bound));
    // Perturbation scale: small enough to not deform the collision mesh, large enough to exceed the 1e-4 threshold
    let perturb_scale = (diag * 1e-3).max(1e-3);

    let eps = 1e-3;
    let mut unique: Vec<[f32; 3]> = Vec::new();
    for p in points {
        if !unique.iter().any(|u| {
            (p[0] - u[0]).abs() < eps && (p[1] - u[1]).abs() < eps && (p[2] - u[2]).abs() < eps
        }) {
            // Pseudo-random deterministic noise based on index
            let idx = unique.len() as f32;
            let nx = (idx * 1.234).sin() * perturb_scale;
            let ny = (idx * 2.345).cos() * perturb_scale;
            let nz = (idx * 3.456).sin() * perturb_scale;
            unique.push([p[0] + nx, p[1] + ny, p[2] + nz]);
        }
    }

    if unique.len() < 4 {
        return (unique, Vec::new());
    }

    let pts = &unique;
    let num_pts = pts.len();

    // Brute force: check every triangle, keep hull faces
    let mut faces: Vec<[usize; 3]> = Vec::new();

    for i in 0..num_pts {
        for j in (i + 1)..num_pts {
            for k in (j + 1)..num_pts {
                let ea = vec_sub(&pts[j], &pts[i]);
                let eb = vec_sub(&pts[k], &pts[i]);
                let n = cross(&ea, &eb);
                let len = vec_len(&n);
                if len < 1e-10 {
                    continue;
                }
                let n_norm = vec_normalize(&n);

                let mut has_pos = false;
                let mut has_neg = false;
                for m in 0..num_pts {
                    if m == i || m == j || m == k {
                        continue;
                    }
                    let d = dot(&n_norm, &vec_sub(&pts[m], &pts[i]));
                    if d > 1e-4 {
                        has_pos = true;
                    }
                    if d < -1e-4 {
                        has_neg = true;
                    }
                    if has_pos && has_neg {
                        break;
                    }
                }

                if has_pos && has_neg {
                    continue;
                }

                let n_out = if has_pos {
                    vec_scale(&n_norm, -1.0)
                } else {
                    n_norm
                };

                let computed_n = cross(&ea, &eb);
                let (fa, fb, fc) = if dot(&computed_n, &n_out) > 0.0 {
                    (i, j, k)
                } else {
                    (i, k, j)
                };

                faces.push([fa, fb, fc]);
            }
        }
    }

    // Deduplicate exact same faces
    faces.dedup_by_key(|f| {
        let mut s = *f;
        s.sort();
        s
    });

    // Remove degenerate faces
    faces.retain(|f| f[0] != f[1] && f[1] != f[2] && f[0] != f[2]);

    if faces.is_empty() {
        return (Vec::new(), Vec::new());
    }

    // Build output
    let mut used_indices: Vec<usize> = Vec::new();
    for face in &faces {
        for &vi in face {
            if !used_indices.contains(&vi) {
                used_indices.push(vi);
            }
        }
    }
    used_indices.sort_unstable();

    let mut index_map: std::collections::HashMap<usize, usize> = std::collections::HashMap::new();
    for (new_idx, &old_idx) in used_indices.iter().enumerate() {
        index_map.insert(old_idx, new_idx);
    }

    let hull_vertices: Vec<[f32; 3]> = used_indices.iter().map(|&i| pts[i]).collect();
    let hull_triangles: Vec<[u16; 3]> = faces
        .iter()
        .map(|f| {
            [
                index_map[&f[0]] as u16,
                index_map[&f[1]] as u16,
                index_map[&f[2]] as u16,
            ]
        })
        .collect();

    (hull_vertices, hull_triangles)
}

/// Generate a collision mesh from a single visible mesh (or default LOD-0 mesh).
/// Computes a convex hull from the mesh vertices, then inflates it outward
/// by an auto-calculated margin (4% of bounding box diagonal).
pub fn generate_collision_from_visible_mesh(
    visible_meshes: &[HODMesh],
    mesh_name: Option<&str>,
) -> Option<HODCollisionMesh> {
    let names: Vec<String> = mesh_name.map(|n| vec![n.to_string()]).unwrap_or_default();
    generate_collision_from_model_meshes(visible_meshes, &[], &names)
}

/// Generate a collision mesh from a list of visible mesh names.
pub fn generate_collision_from_visible_meshes(
    visible_meshes: &[HODMesh],
    mesh_names: &[String],
) -> Option<HODCollisionMesh> {
    generate_collision_from_model_meshes(visible_meshes, &[], mesh_names)
}

/// Generate a single collision mesh enclosing one or more visible meshes.
/// Vertices from each selected mesh are transformed into Root space via `joints`,
/// combined into a unified point cloud (preserving per-mesh AABB extremes),
/// wrapped in a 3D convex hull, and inflated outward by 4% of the combined AABB diagonal.
pub fn generate_collision_from_model_meshes(
    visible_meshes: &[HODMesh],
    joints: &[HODJoint],
    mesh_names: &[String],
) -> Option<HODCollisionMesh> {
    let target_meshes = select_target_meshes(visible_meshes, mesh_names);
    if target_meshes.is_empty() {
        return None;
    }

    // Collect vertex positions from all selected meshes in Root space
    let mut mesh_point_sets: Vec<Vec<[f32; 3]>> = Vec::with_capacity(target_meshes.len());
    let mut all_positions: Vec<[f32; 3]> = Vec::new();
    for target_mesh in &target_meshes {
        let mut mesh_positions: Vec<[f32; 3]> = Vec::new();
        for part in &target_mesh.parts {
            for v in &part.vertices {
                let raw = [v.position.x, v.position.y, v.position.z];
                let transformed = transform_point_to_root(raw, &target_mesh.parent_name, joints);
                mesh_positions.push(transformed);
            }
        }
        if !mesh_positions.is_empty() {
            all_positions.extend_from_slice(&mesh_positions);
            mesh_point_sets.push(mesh_positions);
        }
    }

    if all_positions.len() < 4 {
        return None;
    }

    // Compute AABB for margin calculation and extreme point selection
    let mut aabb_min = [f32::MAX; 3];
    let mut aabb_max = [f32::MIN; 3];
    for p in &all_positions {
        for j in 0..3 {
            aabb_min[j] = aabb_min[j].min(p[j]);
            aabb_max[j] = aabb_max[j].max(p[j]);
        }
    }

    let diagonal = vec_len(&vec_sub(&aabb_max, &aabb_min));
    let margin = diagonal * 0.04; // 4% of bounding box diagonal

    // Pre-sample points for convex hull computation.
    // Gift wrapping is O(n²) but robust. 500 points → ~250000 operations, instant.
    // More points = better silhouette following.
    let hull_input = sample_mesh_sets_for_hull(&mesh_point_sets, &all_positions, 500);

    // Compute 3D convex hull
    let (hull_verts, hull_faces) = convex_hull_3d(&hull_input);

    if hull_verts.is_empty() || hull_faces.is_empty() {
        println!("[COLLISION] Convex hull failed or too complex, falling back to AABB box");
        // Fall back to AABB box with margin
        let inflated_min = [
            aabb_min[0] - margin,
            aabb_min[1] - margin,
            aabb_min[2] - margin,
        ];
        let inflated_max = [
            aabb_max[0] + margin,
            aabb_max[1] + margin,
            aabb_max[2] + margin,
        ];
        let box_verts = vec![
            HODVertex {
                position: Vector3 {
                    x: inflated_min[0],
                    y: inflated_min[1],
                    z: inflated_min[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_max[0],
                    y: inflated_min[1],
                    z: inflated_min[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_max[0],
                    y: inflated_max[1],
                    z: inflated_min[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_min[0],
                    y: inflated_max[1],
                    z: inflated_min[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_min[0],
                    y: inflated_min[1],
                    z: inflated_max[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_max[0],
                    y: inflated_min[1],
                    z: inflated_max[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_max[0],
                    y: inflated_max[1],
                    z: inflated_max[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
            HODVertex {
                position: Vector3 {
                    x: inflated_min[0],
                    y: inflated_max[1],
                    z: inflated_max[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            },
        ];
        let box_indices: Vec<u16> = vec![
            0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1, 2, 6, 7, 2, 7, 3, 0, 3, 7, 0, 7,
            4, 1, 5, 6, 1, 6, 2,
        ];
        return Some(HODCollisionMesh {
            name: "Root".to_string(),
            min_extents: Vector3 {
                x: inflated_min[0],
                y: inflated_min[1],
                z: inflated_min[2],
            },
            max_extents: Vector3 {
                x: inflated_max[0],
                y: inflated_max[1],
                z: inflated_max[2],
            },
            center: Vector3 {
                x: (inflated_min[0] + inflated_max[0]) / 2.0,
                y: (inflated_min[1] + inflated_max[1]) / 2.0,
                z: (inflated_min[2] + inflated_max[2]) / 2.0,
            },
            radius: diagonal / 2.0 + margin,
            mesh: HODMesh {
                name: "CollisionMesh".to_string(),
                parent_name: String::new(),
                lod: 0,
                has_mult_tags: false,
                parts: vec![HODMeshPart {
                    material_index: 0,
                    vertex_mask: 0x01,
                    vertices: box_verts,
                    indices: box_indices,
                }],
            },
        });
    }

    // Compute centroid of hull vertices
    let mut centroid = [0.0f32; 3];
    for v in &hull_verts {
        centroid[0] += v[0];
        centroid[1] += v[1];
        centroid[2] += v[2];
    }
    let n = hull_verts.len() as f32;
    centroid[0] /= n;
    centroid[1] /= n;
    centroid[2] /= n;

    // Inflate each hull vertex outward from centroid by margin
    let inflated_verts: Vec<[f32; 3]> = hull_verts
        .iter()
        .map(|v| {
            let dir = vec_sub(v, &centroid);
            let dist = vec_len(&dir);
            if dist < 1e-10 {
                // Vertex is at centroid, push along arbitrary axis
                return vec_add(v, &[margin, 0.0, 0.0]);
            }
            let dir_norm = vec_normalize(&dir);
            vec_add(&centroid, &vec_scale(&dir_norm, dist + margin))
        })
        .collect();

    // Recompute extents from inflated vertices
    let mut min_extents = Vector3 {
        x: f32::MAX,
        y: f32::MAX,
        z: f32::MAX,
    };
    let mut max_extents = Vector3 {
        x: f32::MIN,
        y: f32::MIN,
        z: f32::MIN,
    };

    for v in &inflated_verts {
        min_extents.x = min_extents.x.min(v[0]);
        min_extents.y = min_extents.y.min(v[1]);
        min_extents.z = min_extents.z.min(v[2]);
        max_extents.x = max_extents.x.max(v[0]);
        max_extents.y = max_extents.y.max(v[1]);
        max_extents.z = max_extents.z.max(v[2]);
    }

    let center = Vector3 {
        x: (min_extents.x + max_extents.x) / 2.0,
        y: (min_extents.y + max_extents.y) / 2.0,
        z: (min_extents.z + max_extents.z) / 2.0,
    };
    let extent = Vector3 {
        x: (max_extents.x - min_extents.x) / 2.0,
        y: (max_extents.y - min_extents.y) / 2.0,
        z: (max_extents.z - min_extents.z) / 2.0,
    };
    let radius = (extent.x * extent.x + extent.y * extent.y + extent.z * extent.z).sqrt();

    // Build HOD vertices from inflated hull
    let hod_vertices: Vec<HODVertex> = inflated_verts
        .iter()
        .map(|v| HODVertex {
            position: Vector3 {
                x: v[0],
                y: v[1],
                z: v[2],
            },
            normal: None,
            color: None,
            uv: None,
            uv1: None,
            uv2: None,
            tangent: None,
            binormal: None,
            skinning_data: None,
        })
        .collect();

    println!(
        "[COLLISION] Convex hull bubble: {} source verts -> {} hull verts, {} faces, margin={:.3}",
        all_positions.len(),
        hod_vertices.len(),
        hull_faces.len(),
        margin
    );

    Some(HODCollisionMesh {
        name: "Root".to_string(),
        min_extents,
        max_extents,
        center,
        radius,
        mesh: HODMesh {
            name: "CollisionMesh".to_string(),
            parent_name: String::new(),
            lod: 0,
            has_mult_tags: false,
            parts: vec![HODMeshPart {
                material_index: 0,
                vertex_mask: 0x01, // Position only
                vertices: hod_vertices,
                indices: hull_faces.into_iter().flatten().collect(),
            }],
        },
    })
}

/// Generate LOD meshes (LOD 1, 2, 3) from LOD 0 mesh.
/// Each part in LOD 0 is decimated independently to preserve material assignments.
/// Returns a vector of new HODMesh entries for LOD 1, 2, 3.
pub fn generate_lod_meshes(lod0_mesh: &HODMesh) -> Vec<HODMesh> {
    if lod0_mesh.parts.is_empty() {
        return Vec::new();
    }

    let mut lod_meshes = Vec::new();

    for &(lod_level, target_ratio) in LOD_LEVELS {
        let mut lod_parts = Vec::new();

        for part in &lod0_mesh.parts {
            if part.vertices.is_empty() || part.indices.is_empty() {
                continue;
            }

            let orig_vert_count = part.vertices.len();
            let orig_idx_count = part.indices.len();

            let (decimated_verts, decimated_indices) =
                decimate_mesh(&part.vertices, &part.indices, target_ratio);

            if !decimated_verts.is_empty() && !decimated_indices.is_empty() {
                println!("[LOD] Level {}: Part reduced from {} verts/{} indices to {} verts/{} indices ({:.1}% verts, {:.1}% tris)",
                    lod_level,
                    orig_vert_count, orig_idx_count,
                    decimated_verts.len(), decimated_indices.len(),
                    (decimated_verts.len() as f32 / orig_vert_count as f32) * 100.0,
                    (decimated_indices.len() as f32 / orig_idx_count as f32) * 100.0
                );

                lod_parts.push(HODMeshPart {
                    material_index: part.material_index,
                    vertex_mask: part.vertex_mask,
                    vertices: decimated_verts,
                    indices: decimated_indices,
                });
            }
        }

        if !lod_parts.is_empty() {
            lod_meshes.push(HODMesh {
                name: lod0_mesh.name.clone(),
                parent_name: lod0_mesh.parent_name.clone(),
                lod: lod_level,
                has_mult_tags: lod0_mesh.has_mult_tags,
                parts: lod_parts,
            });
        }
    }

    lod_meshes
}

/// Generate LOD meshes for all LOD 0 meshes in the model.
/// Removes existing LOD 1/2/3 meshes and replaces them with decimated versions.
pub fn generate_all_lod_meshes(model: &mut crate::hod::HODModel) {
    // Find all LOD 0 meshes
    let lod0_meshes: Vec<HODMesh> = model
        .meshes
        .iter()
        .filter(|m| m.lod == 0)
        .cloned()
        .collect();

    if lod0_meshes.is_empty() {
        return;
    }

    // Remove existing LOD 1/2/3 meshes
    model.meshes.retain(|m| m.lod == 0);

    // Generate new LOD meshes for each LOD 0 mesh
    for lod0 in &lod0_meshes {
        let new_lods = generate_lod_meshes(lod0);
        model.meshes.extend(new_lods);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_decimate_simple_mesh() {
        // A grid of vertices (4x4) forming a mesh with enough triangles
        // for meshopt simplify_sloppy to not collapse entirely.
        let mut vertices = Vec::new();
        let mut indices = Vec::new();
        for y in 0..4 {
            for x in 0..4 {
                vertices.push(HODVertex {
                    position: Vector3 {
                        x: x as f32,
                        y: y as f32,
                        z: 0.0,
                    },
                    normal: None,
                    color: None,
                    uv: None,
                    uv1: None,
                    uv2: None,
                    tangent: None,
                    binormal: None,
                    skinning_data: None,
                });
            }
        }
        for y in 0..3 {
            for x in 0..3 {
                let i = (y * 4 + x) as u16;
                indices.push(i);
                indices.push(i + 1);
                indices.push(i + 4);
                indices.push(i + 1);
                indices.push(i + 5);
                indices.push(i + 4);
            }
        }

        let (dec_verts, dec_idx) = decimate_mesh(&vertices, &indices, 0.5);
        assert!(!dec_verts.is_empty());
        assert!(!dec_idx.is_empty());
    }

    #[test]
    fn test_convex_hull_cube() {
        // 8 corners of a unit cube
        let points: Vec<[f32; 3]> = vec![
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [1.0, 1.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
            [1.0, 0.0, 1.0],
            [1.0, 1.0, 1.0],
            [0.0, 1.0, 1.0],
        ];

        let (verts, faces) = convex_hull_3d(&points);
        assert!(verts.len() >= 4, "Hull should have at least 4 vertices");
        assert_eq!(faces.len(), 12, "Cube should have exactly 12 triangles");

        // Check that all hull vertices are corners of the original cube
        for v in &verts {
            let found = points.iter().any(|p| {
                (v[0] - p[0]).abs() < 0.01
                    && (v[1] - p[1]).abs() < 0.01
                    && (v[2] - p[2]).abs() < 0.01
            });
            assert!(found, "Hull vertex {:?} is not an original cube corner", v);
        }
    }

    #[test]
    fn test_convex_hull_tetrahedron() {
        let points: Vec<[f32; 3]> = vec![
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [0.5, 1.0, 0.0],
            [0.5, 0.5, 1.0],
        ];

        let (verts, faces) = convex_hull_3d(&points);
        println!("Tetrahedron: {} verts, {} faces", verts.len(), faces.len());
        for (i, f) in faces.iter().enumerate() {
            println!("  Face {}: [{}, {}, {}]", i, f[0], f[1], f[2]);
        }
        assert_eq!(verts.len(), 4, "Tetrahedron hull should have 4 vertices");
        assert_eq!(faces.len(), 4, "Tetrahedron hull should have 4 faces");
    }

    fn make_box_mesh(name: &str, parent_name: &str, min: [f32; 3], max: [f32; 3]) -> HODMesh {
        let corners = [
            [min[0], min[1], min[2]],
            [max[0], min[1], min[2]],
            [max[0], max[1], min[2]],
            [min[0], max[1], min[2]],
            [min[0], min[1], max[2]],
            [max[0], min[1], max[2]],
            [max[0], max[1], max[2]],
            [min[0], max[1], max[2]],
        ];
        let vertices = corners
            .iter()
            .map(|c| HODVertex {
                position: Vector3 {
                    x: c[0],
                    y: c[1],
                    z: c[2],
                },
                normal: None,
                color: None,
                uv: None,
                uv1: None,
                uv2: None,
                tangent: None,
                binormal: None,
                skinning_data: None,
            })
            .collect();
        HODMesh {
            name: name.to_string(),
            parent_name: parent_name.to_string(),
            lod: 0,
            has_mult_tags: false,
            parts: vec![HODMeshPart {
                material_index: 0,
                vertex_mask: 0x01,
                vertices,
                indices: vec![0, 1, 2, 0, 2, 3],
            }],
        }
    }

    #[test]
    fn test_generate_collision_from_multiple_meshes() {
        let front = make_box_mesh("HullFront", "Root", [-5.0, -5.0, 10.0], [5.0, 5.0, 30.0]);
        let rear = make_box_mesh("HullRear", "Root", [-8.0, -4.0, -35.0], [8.0, 4.0, -10.0]);
        let turret = make_box_mesh("TurretTop", "Root", [-1.0, 5.0, 0.0], [1.0, 50.0, 2.0]);
        let meshes = vec![front, rear, turret];

        let selected = vec!["HullFront".to_string(), "HullRear".to_string()];
        let col = generate_collision_from_model_meshes(&meshes, &[], &selected)
            .expect("Combined collision mesh should be generated");

        // Combined hull must span both HullFront (z up to 30) and HullRear (z down to -35),
        // and must NOT include unselected TurretTop (y up to 50).
        assert!(col.min_extents.z <= -35.0);
        assert!(col.max_extents.z >= 30.0);
        assert!(col.min_extents.x <= -8.0);
        assert!(col.max_extents.x >= 8.0);
        assert!(col.max_extents.y < 15.0, "Unselected mesh should not inflate Y bounds");
        assert!(!col.mesh.parts[0].vertices.is_empty());
        assert!(!col.mesh.parts[0].indices.is_empty());
    }

    #[test]
    fn test_generate_collision_applies_joint_transform_to_root() {
        use crate::hod::Matrix4;

        let front = make_box_mesh("SectionA", "Root", [-5.0, -5.0, -5.0], [5.0, 5.0, 5.0]);
        // SectionB vertices are local [-5..5], but parented to JointB at z = +40
        let rear = make_box_mesh("SectionB", "JointB", [-5.0, -5.0, -5.0], [5.0, 5.0, 5.0]);
        let joints = vec![HODJoint {
            name: "JointB".to_string(),
            parent_name: Some("Root".to_string()),
            local_transform: Matrix4 {
                m: [
                    [1.0, 0.0, 0.0, 0.0],
                    [0.0, 1.0, 0.0, 0.0],
                    [0.0, 0.0, 1.0, 0.0],
                    [0.0, 0.0, 40.0, 1.0],
                ],
            },
            position: None,
            rotation: None,
            scale: None,
        }];

        let selected = vec!["SectionA".to_string(), "SectionB".to_string()];
        let col = generate_collision_from_model_meshes(&[front, rear], &joints, &selected)
            .expect("Combined collision mesh with joint offset should be generated");

        assert!(col.min_extents.z <= -5.0);
        assert!(col.max_extents.z >= 45.0);
    }
}
