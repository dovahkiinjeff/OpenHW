use base64::Engine;
use hwr_hod_parser::hod::{generate_v2_from_model, HODModel};
use hwr_hod_parser::shader_map::ShadersMap;
use std::fs;
use std::path::Path;

fn main() -> Result<(), String> {
    println!("=== HOD 2.0 Validation Suite ===\n");

    let test_cases = vec![
        ("pebble_0", "../testing/pebble_0"),
        ("pebble_1", "../testing/pebble_1"),
        ("pebble_2", "../testing/pebble_2"),
    ];

    let mut passed = 0;
    let mut failed = 0;
    let mut total = 0;

    for (name, path) in &test_cases {
        total += 1;
        println!("--- Testing {} ---", name);

        match run_test_case(name, Path::new(path)) {
            Ok(_) => {
                println!("✅ PASSED\n");
                passed += 1;
            }
            Err(e) => {
                println!("❌ FAILED: {}\n", e);
                failed += 1;
            }
        }
    }

    println!("=== Validation Results ===");
    println!("Total: {}", total);
    println!("Passed: {}", passed);
    println!("Failed: {}", failed);
    println!(
        "Success Rate: {:.1}%",
        (passed as f64 / total as f64) * 100.0
    );

    if failed > 0 {
        Err(format!("{} test(s) failed", failed))
    } else {
        Ok(())
    }
}

fn run_test_case(name: &str, dir: &Path) -> Result<(), String> {
    // Test 1: Parse vanilla HOD
    println!("  1. Parsing vanilla HOD...");
    let vanilla_path = dir.join(format!("{}_vanilla.hod", name));
    let vanilla_bytes =
        fs::read(&vanilla_path).map_err(|e| format!("Failed to read vanilla: {}", e))?;
    let vanilla_model =
        HODModel::parse(&vanilla_bytes).map_err(|e| format!("Failed to parse vanilla: {}", e))?;
    println!(
        "     Vanilla: {} bytes, {} meshes, {} materials",
        vanilla_bytes.len(),
        vanilla_model.meshes.len(),
        vanilla_model.materials.len()
    );

    // Test 2: Build model from assets
    println!("  2. Building model from assets...");
    let asset_model = build_model_from_assets(dir)?;
    println!(
        "     Assets: {} meshes, {} materials, {} textures",
        asset_model.meshes.len(),
        asset_model.materials.len(),
        asset_model.textures.len()
    );

    // Test 3: Generate HOD 2.0
    println!("  3. Generating HOD 2.0...");
    let generated = generate_v2_from_model(&[], &asset_model)?;
    let out_path = dir.join(format!("{}_validation.hod", name));
    fs::write(&out_path, &generated).map_err(|e| format!("Failed to write: {}", e))?;
    println!("     Generated: {} bytes", generated.len());

    // Test 4: Re-parse generated HOD
    println!("  4. Re-parsing generated HOD...");
    let reparsed = HODModel::parse(&generated).map_err(|e| format!("Failed to re-parse: {}", e))?;
    println!(
        "     Re-parsed: {} meshes, {} materials",
        reparsed.meshes.len(),
        reparsed.materials.len()
    );

    // Test 5: Compare structures
    println!("  5. Comparing structures...");
    compare_structures(&vanilla_model, &reparsed)?;

    // Test 6: Verify round-trip integrity
    println!("  6. Verifying round-trip integrity...");
    verify_roundtrip(&generated)?;

    // Clean up validation file
    let _ = fs::remove_file(&out_path);

    Ok(())
}

fn build_model_from_assets(dir: &Path) -> Result<HODModel, String> {
    // Load materials
    let materials_path = dir.join("Homeworld2 Multi Mesh File_materials.json");
    let materials: Vec<hwr_hod_parser::hod::HODMaterial> =
        serde_json::from_slice(&fs::read(&materials_path).map_err(|e| e.to_string())?)
            .map_err(|e| e.to_string())?;

    // Load textures
    let texture_names: Vec<String> = fs::read_dir(dir)
        .map_err(|e| e.to_string())?
        .filter_map(|entry| entry.ok().map(|e| e.path()))
        .filter(|path| path.extension().and_then(|ext| ext.to_str()) == Some("tga"))
        .filter_map(|path| path.file_stem()?.to_str()?.to_string().into())
        .collect();

    let mut textures = Vec::new();
    for tex_name in &texture_names {
        textures.push(load_tga_texture(
            &dir.join(format!("{}.tga", tex_name)),
            tex_name,
        )?);
    }

    // Load OBJ meshes
    let mut obj_paths: Vec<std::path::PathBuf> = fs::read_dir(dir)
        .map_err(|e| e.to_string())?
        .filter_map(|entry| entry.ok().map(|e| e.path()))
        .filter(|path| path.extension().and_then(|ext| ext.to_str()) == Some("obj"))
        .collect();
    obj_paths.sort();

    let mut meshes = Vec::new();
    for path in obj_paths {
        if let Some((base_name, lod)) = parse_lod_filename(&path) {
            meshes.push(parse_obj_mesh(&path, &base_name, lod)?);
        }
    }

    let root_pos = hwr_hod_parser::hod::Vector3 {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    };
    let root_rot = hwr_hod_parser::hod::Vector3 {
        x: 0.0,
        y: 0.0,
        z: 0.0,
    };
    let root_scale = hwr_hod_parser::hod::Vector3 {
        x: 1.0,
        y: 1.0,
        z: 1.0,
    };

    Ok(HODModel {
        version: 512,
        is_v2: true,
        name: "Homeworld2 Multi Mesh File".to_string(),
        textures,
        textures_modified: false,
        materials,
        meshes,
        joints: vec![hwr_hod_parser::hod::HODJoint {
            name: "Root".to_string(),
            parent_name: None,
            local_transform: hwr_hod_parser::hod::compose_transform_matrix(
                root_pos.clone(),
                root_rot.clone(),
                root_scale.clone(),
            ),
            position: Some(root_pos),
            rotation: Some(root_rot),
            scale: Some(root_scale),
        }],
        markers: Vec::new(),
        nav_lights: Vec::new(),
        engine_burns: Vec::new(),
        engine_glows: Vec::new(),
        engine_shapes: Vec::new(),
        collision_meshes: Vec::new(),
        dockpaths: Vec::new(),
        animations: Vec::new(),
        preserved_chunks: Vec::new(),
    })
}

fn load_tga_texture(path: &Path, name: &str) -> Result<hwr_hod_parser::hod::HODTexture, String> {
    let bytes = fs::read(path).map_err(|e| e.to_string())?;
    let img = image::load_from_memory_with_format(&bytes, image::ImageFormat::Tga)
        .map_err(|e| e.to_string())?;
    let width = img.width();
    let height = img.height();
    let format = if img.to_rgba8().pixels().any(|pixel| pixel[3] < 250) {
        "DXT5"
    } else {
        "DXT1"
    };
    let img = image::imageops::flip_vertical(&img);
    let mut png_bytes = Vec::new();
    img.write_to(
        &mut std::io::Cursor::new(&mut png_bytes),
        image::ImageFormat::Png,
    )
    .map_err(|e| e.to_string())?;
    let b64 = base64::prelude::BASE64_STANDARD.encode(&png_bytes);
    Ok(hwr_hod_parser::hod::HODTexture {
        name: name.to_string(),
        width,
        height,
        format: format.to_string(),
        png_preview: Some(format!("data:image/png;base64,{}", b64)),
        png_data: Some(b64),
        source_path: None,
    })
}

fn parse_lod_filename(path: &Path) -> Option<(String, i32)> {
    let stem = path.file_stem()?.to_str()?;
    let idx = stem.rfind("_lod")?;
    let lod = stem[idx + 4..].parse().ok()?;
    Some((stem[..idx].to_string(), lod))
}

fn parse_obj_mesh(
    path: &Path,
    name: &str,
    lod: i32,
) -> Result<hwr_hod_parser::hod::HODMesh, String> {
    let text = fs::read_to_string(path).map_err(|e| e.to_string())?;
    let mut positions: Vec<hwr_hod_parser::hod::Vector3> = Vec::new();
    let mut uvs: Vec<hwr_hod_parser::hod::Vector2> = Vec::new();
    let mut normals: Vec<hwr_hod_parser::hod::Vector3> = Vec::new();
    let mut vertices = Vec::new();
    let mut indices = Vec::new();

    for line in text.lines() {
        let line = line.trim_matches(char::from(0)).trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        let mut parts = line.split_whitespace();
        match parts.next() {
            Some("v") => {
                let vals: Vec<f32> = parts.filter_map(|s| s.parse().ok()).collect();
                if vals.len() >= 3 {
                    positions.push(hwr_hod_parser::hod::Vector3 {
                        x: vals[0],
                        y: vals[1],
                        z: vals[2],
                    });
                }
            }
            Some("vt") => {
                let vals: Vec<f32> = parts.filter_map(|s| s.parse().ok()).collect();
                if vals.len() >= 2 {
                    uvs.push(hwr_hod_parser::hod::Vector2 {
                        u: vals[0],
                        v: 1.0 - vals[1],
                    });
                }
            }
            Some("vn") => {
                let vals: Vec<f32> = parts.filter_map(|s| s.parse().ok()).collect();
                if vals.len() >= 3 {
                    normals.push(hwr_hod_parser::hod::Vector3 {
                        x: vals[0],
                        y: vals[1],
                        z: vals[2],
                    });
                }
            }
            Some("f") => {
                let face_tokens: Vec<&str> = parts.collect();
                if face_tokens.len() < 3 {
                    continue;
                }
                let first = parse_obj_vertex(face_tokens[0], &positions, &uvs, &normals)?;
                let mut prev = parse_obj_vertex(face_tokens[1], &positions, &uvs, &normals)?;
                for token in face_tokens.iter().skip(2) {
                    let current = parse_obj_vertex(token, &positions, &uvs, &normals)?;
                    for vertex in [first.clone(), prev.clone(), current.clone()] {
                        indices.push(vertices.len() as u16);
                        vertices.push(vertex);
                    }
                    prev = current;
                }
            }
            _ => {}
        }
    }

    Ok(hwr_hod_parser::hod::HODMesh {
        name: name.to_string(),
        parent_name: "Root".to_string(),
        lod,
        has_mult_tags: false,
        parts: vec![hwr_hod_parser::hod::HODMeshPart {
            material_index: 0,
            vertex_mask: 0x600B,
            vertices,
            indices,
        }],
    })
}

fn parse_obj_vertex(
    token: &str,
    positions: &[hwr_hod_parser::hod::Vector3],
    uvs: &[hwr_hod_parser::hod::Vector2],
    normals: &[hwr_hod_parser::hod::Vector3],
) -> Result<hwr_hod_parser::hod::HODVertex, String> {
    let mut fields = token.split('/');
    let vi = parse_obj_index(fields.next().unwrap_or_default(), positions.len())?;
    let ti = fields
        .next()
        .filter(|s| !s.is_empty())
        .map(|s| parse_obj_index(s, uvs.len()))
        .transpose()?;
    let ni = fields
        .next()
        .filter(|s| !s.is_empty())
        .map(|s| parse_obj_index(s, normals.len()))
        .transpose()?;
    Ok(hwr_hod_parser::hod::HODVertex {
        position: positions
            .get(vi)
            .cloned()
            .ok_or_else(|| "position index out of range".to_string())?,
        normal: ni.and_then(|idx| normals.get(idx).cloned()).or(Some(
            hwr_hod_parser::hod::Vector3 {
                x: 0.0,
                y: 1.0,
                z: 0.0,
            },
        )),
        color: None,
        uv: ti
            .and_then(|idx| uvs.get(idx).cloned())
            .or(Some(hwr_hod_parser::hod::Vector2 { u: 0.0, v: 0.0 })),
        uv1: None,
        uv2: None,
        tangent: Some(hwr_hod_parser::hod::Vector3 {
            x: 1.0,
            y: 0.0,
            z: 0.0,
        }),
        binormal: Some(hwr_hod_parser::hod::Vector3 {
            x: 0.0,
            y: 0.0,
            z: 1.0,
        }),
        skinning_data: None,
    })
}

fn parse_obj_index(raw: &str, len: usize) -> Result<usize, String> {
    let idx = raw.parse::<isize>().map_err(|e| e.to_string())?;
    if idx > 0 {
        Ok((idx - 1) as usize)
    } else if idx < 0 {
        Ok((len as isize + idx) as usize)
    } else {
        Err("OBJ index cannot be zero".to_string())
    }
}

fn compare_structures(vanilla: &HODModel, generated: &HODModel) -> Result<(), String> {
    // Compare mesh counts
    if vanilla.meshes.len() != generated.meshes.len() {
        return Err(format!(
            "Mesh count mismatch: {} vs {}",
            vanilla.meshes.len(),
            generated.meshes.len()
        ));
    }

    // Compare material counts
    if vanilla.materials.len() != generated.materials.len() {
        return Err(format!(
            "Material count mismatch: {} vs {}",
            vanilla.materials.len(),
            generated.materials.len()
        ));
    }

    // Compare texture counts
    if vanilla.textures.len() != generated.textures.len() {
        return Err(format!(
            "Texture count mismatch: {} vs {}",
            vanilla.textures.len(),
            generated.textures.len()
        ));
    }

    // Compare joint counts
    if vanilla.joints.len() != generated.joints.len() {
        return Err(format!(
            "Joint count mismatch: {} vs {}",
            vanilla.joints.len(),
            generated.joints.len()
        ));
    }

    println!("     Structure comparison: OK");
    Ok(())
}

fn verify_roundtrip(bytes: &[u8]) -> Result<(), String> {
    // Parse the generated HOD
    let model =
        HODModel::parse(bytes).map_err(|e| format!("Failed to parse for round-trip: {}", e))?;

    // Re-generate from parsed model
    let regenerated =
        generate_v2_from_model(&[], &model).map_err(|e| format!("Failed to re-generate: {}", e))?;

    // Parse again to verify
    let _reparsed = HODModel::parse(&regenerated)
        .map_err(|e| format!("Failed to re-parse regenerated: {}", e))?;

    println!("     Round-trip verification: OK");
    Ok(())
}
