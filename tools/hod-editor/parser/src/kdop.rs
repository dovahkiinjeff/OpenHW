use byteorder::{LittleEndian, WriteBytesExt};
use std::io::Write;

/// The 13 half-directions for a 26-DOP.
/// Each direction has an opposite, giving 26 planes total.
/// Order matches HODOR's convention (verified from face normal analysis).
const DOP_DIRECTIONS: [[f32; 3]; 13] = [
    // Axis-aligned (3)
    [1.0, 0.0, 0.0],
    [0.0, 1.0, 0.0],
    [0.0, 0.0, 1.0],
    // Edge diagonals (6) — (1,1,0)/sqrt(2) variants
    [0.70710677, 0.70710677, 0.0],
    [0.70710677, -0.70710677, 0.0],
    [0.0, 0.70710677, 0.70710677],
    [0.0, -0.70710677, 0.70710677],
    [0.70710677, 0.0, 0.70710677],
    [0.70710677, 0.0, -0.70710677],
    // Cube diagonals (4) — (1,1,1)/sqrt(3) variants
    [0.57735026, 0.57735026, 0.57735026],
    [0.57735026, 0.57735026, -0.57735026],
    [0.57735026, -0.57735026, 0.57735026],
    [0.57735026, -0.57735026, -0.57735026],
];

/// Generate a 26-DOP KDOP collision chunk from mesh vertices.
///
/// The KDOP is computed by:
/// 1. Projecting all mesh vertices onto 13 DOP directions to get min/max bounds
/// 2. Computing vertices as intersections of triple planes from the 26 half-spaces
/// 3. Filtering valid vertices and triangulating the faces
pub fn generate_kdop(mesh_vertices: &[[f32; 3]], _mesh_indices: &[u16]) -> Vec<u8> {
    if mesh_vertices.is_empty() {
        return generate_empty_kdop();
    }

    // Step 1: Compute AABB
    let mut aabb_min = [f32::MAX; 3];
    let mut aabb_max = [f32::MIN; 3];
    for v in mesh_vertices {
        for i in 0..3 {
            aabb_min[i] = aabb_min[i].min(v[i]);
            aabb_max[i] = aabb_max[i].max(v[i]);
        }
    }

    // Step 2: Compute min/max projections for each of the 13 DOP directions
    let mut proj_min = [f32::MAX; 13];
    let mut proj_max = [f32::MIN; 13];
    for v in mesh_vertices {
        for (d, dir) in DOP_DIRECTIONS.iter().enumerate() {
            let proj = v[0] * dir[0] + v[1] * dir[1] + v[2] * dir[2];
            proj_min[d] = proj_min[d].min(proj);
            proj_max[d] = proj_max[d].max(proj);
        }
    }

    // Expand projections slightly to avoid degenerate slabs
    for d in 0..13 {
        if (proj_max[d] - proj_min[d]).abs() < 1e-4 {
            proj_min[d] -= 0.001;
            proj_max[d] += 0.001;
        }
    }

    // Step 3: Build the 26 planes: for each direction d, two planes:
    //   plane 2*d:   dot(v, dir) = proj_min[d]  (normal = -dir)
    //   plane 2*d+1: dot(v, dir) = proj_max[d]  (normal = +dir)
    // A vertex is valid if it satisfies ALL 26 inequalities.

    // Step 4: Generate candidate vertices by intersecting all C(26,3) triples
    let planes = build_planes(&proj_min, &proj_max);
    let candidates = generate_candidate_vertices(&planes, &aabb_min, &aabb_max);

    // Step 5: Filter valid vertices (satisfy all 26 plane inequalities)
    let valid_vertices = filter_valid_vertices(&candidates, &planes);

    // Step 6: Deduplicate vertices
    let unique_vertices = deduplicate_vertices(&valid_vertices);

    // Step 7: Generate faces by grouping vertices by their supporting planes
    let faces = generate_faces(&unique_vertices, &planes);

    // Step 8: Write the KDOP binary data
    write_kdop_binary(
        &aabb_min,
        &aabb_max,
        &proj_min,
        &proj_max,
        &unique_vertices,
        &faces,
    )
}

fn generate_empty_kdop() -> Vec<u8> {
    let min = [0.0f32; 3];
    let max = [0.0f32; 3];
    let proj_min = [0.0f32; 13];
    let proj_max = [0.0f32; 13];
    write_kdop_binary(&min, &max, &proj_min, &proj_max, &[], &[])
}

/// Build 26 planes from the 13 DOP directions and their projection bounds.
/// Each plane is (normal_x, normal_y, normal_z, d) where dot(v, n) <= d.
fn build_planes(proj_min: &[f32; 13], proj_max: &[f32; 13]) -> Vec<[f32; 4]> {
    let mut planes = Vec::with_capacity(26);
    for d in 0..13 {
        let dir = DOP_DIRECTIONS[d];
        // Min plane: dot(v, dir) >= proj_min, i.e., dot(v, -dir) <= -proj_min
        planes.push([-dir[0], -dir[1], -dir[2], -proj_min[d]]);
        // Max plane: dot(v, dir) <= proj_max
        planes.push([dir[0], dir[1], dir[2], proj_max[d]]);
    }
    planes
}

/// Generate candidate vertices by intersecting all C(26,3) triples of planes.
fn generate_candidate_vertices(
    planes: &[[f32; 4]],
    aabb_min: &[f32; 3],
    aabb_max: &[f32; 3],
) -> Vec<[f32; 3]> {
    let n = planes.len();
    let mut candidates = Vec::new();

    for i in 0..n {
        for j in (i + 1)..n {
            for k in (j + 1)..n {
                if let Some(v) = intersect_three_planes(&planes[i], &planes[j], &planes[k]) {
                    // Quick AABB check to reject obviously invalid points
                    let margin = 0.01;
                    if v[0] >= aabb_min[0] - margin
                        && v[0] <= aabb_max[0] + margin
                        && v[1] >= aabb_min[1] - margin
                        && v[1] <= aabb_max[1] + margin
                        && v[2] >= aabb_min[2] - margin
                        && v[2] <= aabb_max[2] + margin
                    {
                        candidates.push(v);
                    }
                }
            }
        }
    }
    candidates
}

/// Solve the 3x3 linear system: plane_i · v = d_i for i=0,1,2
/// Returns None if the system is singular (planes are parallel or coplanar).
fn intersect_three_planes(p0: &[f32; 4], p1: &[f32; 4], p2: &[f32; 4]) -> Option<[f32; 3]> {
    // Cramer's rule
    let a = p0[0];
    let b = p0[1];
    let c = p0[2];
    let d0 = p0[3];
    let e = p1[0];
    let f = p1[1];
    let g = p1[2];
    let d1 = p1[3];
    let h = p2[0];
    let i = p2[1];
    let j = p2[2];
    let d2 = p2[3];

    let det = a * (f * j - g * i) - b * (e * j - g * h) + c * (e * i - f * h);
    if det.abs() < 1e-10 {
        return None;
    }

    let inv_det = 1.0 / det;
    let x = (d0 * (f * j - g * i) - b * (d1 * j - g * d2) + c * (d1 * i - f * d2)) * inv_det;
    let y = (a * (d1 * j - g * d2) - d0 * (e * j - g * h) + c * (e * d2 - d1 * h)) * inv_det;
    let z = (a * (f * d2 - d1 * i) - b * (e * d2 - d1 * h) + d0 * (e * i - f * h)) * inv_det;

    Some([x, y, z])
}

/// Filter vertices that satisfy all 26 plane inequalities.
fn filter_valid_vertices(candidates: &[[f32; 3]], planes: &[[f32; 4]]) -> Vec<[f32; 3]> {
    let eps = 1e-4;
    candidates
        .iter()
        .filter(|v| {
            planes
                .iter()
                .all(|p| v[0] * p[0] + v[1] * p[1] + v[2] * p[2] <= p[3] + eps)
        })
        .copied()
        .collect()
}

/// Deduplicate vertices within a small epsilon.
fn deduplicate_vertices(vertices: &[[f32; 3]]) -> Vec<[f32; 3]> {
    let eps = 1e-4;
    let mut unique = Vec::new();
    for v in vertices {
        let is_dup = unique.iter().any(|u: &[f32; 3]| {
            (v[0] - u[0]).abs() < eps && (v[1] - u[1]).abs() < eps && (v[2] - u[2]).abs() < eps
        });
        if !is_dup {
            unique.push(*v);
        }
    }
    unique
}

/// Generate triangulated faces for the 26-DOP.
/// For each of the 26 planes, find all vertices that lie on that plane,
/// compute a 2D projection, compute a convex hull in 2D, and triangulate.
fn generate_faces(vertices: &[[f32; 3]], planes: &[[f32; 4]]) -> Vec<[u16; 3]> {
    let eps = 1e-3;
    let mut faces = Vec::new();

    for plane in planes {
        // Find all vertex indices that lie on this plane
        let on_plane: Vec<usize> = vertices
            .iter()
            .enumerate()
            .filter(|(_, v)| {
                let dist = v[0] * plane[0] + v[1] * plane[1] + v[2] * plane[2] - plane[3];
                dist.abs() < eps
            })
            .map(|(i, _)| i)
            .collect();

        if on_plane.len() < 3 {
            continue;
        }

        // Project vertices onto the plane's 2D coordinate system
        let normal = [plane[0], plane[1], plane[2]];
        let (u_axis, v_axis) = compute_plane_axes(&normal);

        let projected: Vec<[f32; 2]> = on_plane
            .iter()
            .map(|&idx| {
                let p = &vertices[idx];
                let u = p[0] * u_axis[0] + p[1] * u_axis[1] + p[2] * u_axis[2];
                let v = p[0] * v_axis[0] + p[1] * v_axis[1] + p[2] * v_axis[2];
                [u, v]
            })
            .collect();

        // Compute 2D convex hull and triangulate via fan from centroid
        let hull_indices = convex_hull_2d(&projected);
        if hull_indices.len() >= 3 {
            // Triangulate using fan from first vertex
            for i in 1..hull_indices.len() - 1 {
                faces.push([
                    on_plane[hull_indices[0]] as u16,
                    on_plane[hull_indices[i]] as u16,
                    on_plane[hull_indices[i + 1]] as u16,
                ]);
            }
        }
    }

    faces
}

/// Compute two orthogonal axes on a plane defined by its normal.
fn compute_plane_axes(normal: &[f32; 3]) -> ([f32; 3], [f32; 3]) {
    // Pick the axis least aligned with the normal for the first tangent
    let abs_n = [normal[0].abs(), normal[1].abs(), normal[2].abs()];
    let up = if abs_n[0] <= abs_n[1] && abs_n[0] <= abs_n[2] {
        [1.0, 0.0, 0.0]
    } else if abs_n[1] <= abs_n[2] {
        [0.0, 1.0, 0.0]
    } else {
        [0.0, 0.0, 1.0]
    };

    // u = normalize(up - (up · n) * n)
    let dot = up[0] * normal[0] + up[1] * normal[1] + up[2] * normal[2];
    let u = [
        up[0] - dot * normal[0],
        up[1] - dot * normal[1],
        up[2] - dot * normal[2],
    ];
    let len = (u[0] * u[0] + u[1] * u[1] + u[2] * u[2]).sqrt();
    let u_norm = if len > 1e-10 {
        [u[0] / len, u[1] / len, u[2] / len]
    } else {
        [1.0, 0.0, 0.0]
    };

    // v = n × u
    let v = [
        normal[1] * u_norm[2] - normal[2] * u_norm[1],
        normal[2] * u_norm[0] - normal[0] * u_norm[2],
        normal[0] * u_norm[1] - normal[1] * u_norm[0],
    ];

    (u_norm, v)
}

/// Compute the 2D convex hull of a set of 2D points using Graham scan.
/// Returns indices into the input array, in counter-clockwise order.
fn convex_hull_2d(points: &[[f32; 2]]) -> Vec<usize> {
    if points.len() < 3 {
        return (0..points.len()).collect();
    }

    // Find the point with the lowest y (and leftmost if tied)
    let mut min_idx = 0;
    for i in 1..points.len() {
        if points[i][1] < points[min_idx][1]
            || (points[i][1] == points[min_idx][1] && points[i][0] < points[min_idx][0])
        {
            min_idx = i;
        }
    }

    // Sort by polar angle relative to the bottom-most point
    let mut indices: Vec<usize> = (0..points.len()).collect();
    let pivot = points[min_idx];
    indices.sort_by(|&a, &b| {
        if a == min_idx {
            return std::cmp::Ordering::Less;
        }
        if b == min_idx {
            return std::cmp::Ordering::Greater;
        }
        let angle_a = (points[a][1] - pivot[1]).atan2(points[a][0] - pivot[0]);
        let angle_b = (points[b][1] - pivot[1]).atan2(points[b][0] - pivot[0]);
        angle_a
            .partial_cmp(&angle_b)
            .unwrap_or(std::cmp::Ordering::Equal)
    });

    // Graham scan
    let mut hull: Vec<usize> = Vec::new();
    for &idx in &indices {
        while hull.len() >= 2 {
            let a = hull[hull.len() - 2];
            let b = hull[hull.len() - 1];
            let cross = cross_2d(&points[a], &points[b], &points[idx]);
            if cross <= 0.0 {
                hull.pop();
            } else {
                break;
            }
        }
        hull.push(idx);
    }

    hull
}

/// 2D cross product of vectors (b-a) × (c-a).
fn cross_2d(a: &[f32; 2], b: &[f32; 2], c: &[f32; 2]) -> f32 {
    (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
}

/// Write the KDOP binary data.
fn write_kdop_binary(
    aabb_min: &[f32; 3],
    aabb_max: &[f32; 3],
    proj_min: &[f32; 13],
    proj_max: &[f32; 13],
    vertices: &[[f32; 3]],
    faces: &[[u16; 3]],
) -> Vec<u8> {
    let mut data = Vec::new();

    // AABB header: 7 floats (28 bytes)
    let extent = [
        (aabb_max[0] - aabb_min[0]) / 2.0,
        (aabb_max[1] - aabb_min[1]) / 2.0,
        (aabb_max[2] - aabb_min[2]) / 2.0,
    ];
    let radius = (extent[0] * extent[0] + extent[1] * extent[1] + extent[2] * extent[2]).sqrt();

    let _ = data.write_f32::<LittleEndian>(radius);
    let _ = data.write_f32::<LittleEndian>(aabb_min[0]);
    let _ = data.write_f32::<LittleEndian>(aabb_min[1]);
    let _ = data.write_f32::<LittleEndian>(aabb_min[2]);
    let _ = data.write_f32::<LittleEndian>(aabb_max[0]);
    let _ = data.write_f32::<LittleEndian>(aabb_max[1]);
    let _ = data.write_f32::<LittleEndian>(aabb_max[2]);

    // Direction records: 13 × 32 bytes (8 floats each)
    // Each record stores the projection bounds for one DOP direction.
    // Format: [min_proj, max_proj, dir_x, dir_y, dir_z, 0, 0, 0]
    for d in 0..13 {
        let dir = DOP_DIRECTIONS[d];
        let _ = data.write_f32::<LittleEndian>(proj_min[d]);
        let _ = data.write_f32::<LittleEndian>(proj_max[d]);
        let _ = data.write_f32::<LittleEndian>(dir[0]);
        let _ = data.write_f32::<LittleEndian>(dir[1]);
        let _ = data.write_f32::<LittleEndian>(dir[2]);
        let _ = data.write_f32::<LittleEndian>(0.0);
        let _ = data.write_f32::<LittleEndian>(0.0);
        let _ = data.write_f32::<LittleEndian>(0.0);
    }
    // Total header: 28 + 13*32 = 28 + 416 = 444 bytes

    // Vertex count + vertices
    let _ = data.write_u32::<LittleEndian>(vertices.len() as u32);
    for v in vertices {
        let _ = data.write_f32::<LittleEndian>(v[0]);
        let _ = data.write_f32::<LittleEndian>(v[1]);
        let _ = data.write_f32::<LittleEndian>(v[2]);
    }

    // Face count + faces
    let _ = data.write_u32::<LittleEndian>(faces.len() as u32);
    for f in faces {
        let _ = data.write_u16::<LittleEndian>(f[0]);
        let _ = data.write_u16::<LittleEndian>(f[1]);
        let _ = data.write_u16::<LittleEndian>(f[2]);
    }

    // Trailing padding (8 bytes)
    let _ = data.write_u64::<LittleEndian>(0);

    data
}
