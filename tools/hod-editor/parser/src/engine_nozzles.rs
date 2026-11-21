use crate::model_methods::normalize_collision_meshes;
use crate::types::*;

pub fn synthesize_engine_nozzles_v1(model: &mut HODModel) {
    if !model.engine_burns.is_empty() {
        return;
    }

    let mut burns = Vec::new();
    let mut remaining_navs = Vec::new();

    for nav in &model.nav_lights {
        if nav.name.starts_with("e")
            && nav.name.len() >= 2
            && nav.name[1..].chars().all(char::is_numeric)
        {
            let size = if nav.size > 0.0 { nav.size } else { 4.0 };
            burns.push(crate::types::HODEngineBurn {
                name: nav.name.clone(),
                parent_name: nav.name.clone(),
                num_divisions: 8,
                num_flames: 1,
                vertices: vec![
                    crate::types::Vector3 {
                        x: -size,
                        y: -size,
                        z: 0.0,
                    },
                    crate::types::Vector3 {
                        x: size,
                        y: -size,
                        z: 0.0,
                    },
                    crate::types::Vector3 {
                        x: size,
                        y: size,
                        z: 0.0,
                    },
                    crate::types::Vector3 {
                        x: -size,
                        y: size,
                        z: 0.0,
                    },
                ],
            });
        } else {
            remaining_navs.push(nav.clone());
        }
    }
    model.nav_lights = remaining_navs;
    model.engine_burns.extend(burns);
}

pub fn reverse_engine_nozzles_v1(model: &mut HODModel) {
    if model.engine_burns.is_empty() {
        return;
    }

    let mut new_navs = Vec::new();
    let mut i = 0;
    while i < model.engine_burns.len() {
        let burn = &model.engine_burns[i];
        if burn.name.starts_with("e")
            && burn.name.len() >= 2
            && burn.name[1..].chars().all(char::is_numeric)
        {
            new_navs.push(crate::types::HODNavLight {
                name: burn.name.clone(),
                section: 0,
                size: 4.0, // Best guess fallback
                phase: 0.0,
                frequency: 1.0,
                style: "engine".to_string(),
                color: crate::types::Vector3 {
                    x: 1.0,
                    y: 1.0,
                    z: 1.0,
                },
                distance: 0.0,
                sprite_visible: true,
                high_end_only: false,
            });
            model.engine_burns.remove(i);
        } else {
            i += 1;
        }
    }

    model.nav_lights.extend(new_navs);
}

pub fn validate_marker_parents(model: &mut HODModel) {
    for marker in &mut model.markers {
        if marker.parent_joint.is_empty() {
            marker.parent_joint = "Root".to_string();
        }
    }
}

pub fn generate_collision_mesh(model: &mut HODModel) {
    normalize_collision_meshes(model);

    // Always generate box vertices for collision meshes that have extents but no vertices
    for cm in &mut model.collision_meshes {
        if cm.mesh.parts.is_empty() || cm.mesh.parts.iter().all(|p| p.vertices.is_empty()) {
            let min = &cm.min_extents;
            let max = &cm.max_extents;

            println!(
                "[RUST] Generating collision mesh box vertices from extents: min=({},{},{}), max=({},{},{})",
                min.x, min.y, min.z, max.x, max.y, max.z
            );

            // Box vertices: 8 corners
            let vertices = vec![
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: min.x,
                        y: min.y,
                        z: min.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: max.x,
                        y: min.y,
                        z: min.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: max.x,
                        y: max.y,
                        z: min.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: min.x,
                        y: max.y,
                        z: min.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: min.x,
                        y: min.y,
                        z: max.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: max.x,
                        y: min.y,
                        z: max.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: max.x,
                        y: max.y,
                        z: max.z,
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
                crate::types::HODVertex {
                    position: crate::types::Vector3 {
                        x: min.x,
                        y: max.y,
                        z: max.z,
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

            // Box indices: 12 triangles (6 faces × 2 triangles)
            let indices = vec![
                0, 1, 2, 0, 2, 3, // front
                4, 6, 5, 4, 7, 6, // back
                0, 4, 5, 0, 5, 1, // left
                2, 6, 7, 2, 7, 3, // right
                0, 3, 7, 0, 7, 4, // top
                1, 5, 6, 1, 6, 2, // bottom
            ];

            cm.mesh.parts = vec![crate::types::HODMeshPart {
                material_index: 0,
                vertex_mask: 1, // position only
                vertices,
                indices,
            }];

            println!(
                "[RUST] Generated collision mesh: {} vertices, {} indices",
                cm.mesh.parts[0].vertices.len(),
                cm.mesh.parts[0].indices.len()
            );
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn engine_nozzles_validate_marker_parents_defaults_to_root() {
        let mut model = HODModel::new();
        model.markers.push(HODMarker {
            name: "Marker".to_string(),
            parent_joint: String::new(),
            position: Vector3 { x: 0.0, y: 0.0, z: 0.0 },
            rotation: Matrix4 { m: [[0.0; 4]; 4] },
            rotation_euler: None,
        });

        validate_marker_parents(&mut model);

        assert_eq!(model.markers[0].parent_joint, "Root");
    }

    #[test]
    fn engine_nozzles_synthesize_is_reversible() {
        let mut model = HODModel::new();
        model.nav_lights.push(HODNavLight {
            name: "e0".to_string(),
            section: 0,
            size: 2.0,
            phase: 0.0,
            frequency: 1.0,
            style: "engine".to_string(),
            color: Vector3 { x: 1.0, y: 1.0, z: 1.0 },
            distance: 0.0,
            sprite_visible: true,
            high_end_only: false,
        });

        synthesize_engine_nozzles_v1(&mut model);
        assert_eq!(model.engine_burns.len(), 1);
        assert!(model.nav_lights.is_empty());

        reverse_engine_nozzles_v1(&mut model);
        assert!(model.engine_burns.is_empty());
        assert_eq!(model.nav_lights.len(), 1);
        assert_eq!(model.nav_lights[0].name, "e0");
    }
}
