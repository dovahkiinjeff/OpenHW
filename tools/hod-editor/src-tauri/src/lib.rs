use chrono::Local;
use hwr_hod_parser::hod::{HODModel, HODTexture};
use serde::{Deserialize, Serialize};
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use game_window::GameWindow;

#[derive(Serialize, Deserialize, Default)]
struct ShaderConfig {
    shader_directories: Vec<String>,
}

static LOG_PATH: Mutex<Option<PathBuf>> = Mutex::new(None);

pub fn write_log(level: &str, message: &str) {
    let log_path = match LOG_PATH.lock() {
        Ok(g) => g.clone(),
        Err(_) => None,
    };

    let path = match log_path {
        Some(p) => p,
        None => {
            let mut p = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
            p.push("hwr_hod_editor.log");
            p
        }
    };

    let now = Local::now().format("%Y-%m-%d %H:%M:%S");
    let log_line = format!("[{}] [{}] {}\n", now, level, message);

    if level == "ERROR" || level == "PANIC" {
        eprint!("{}", log_line);
    } else {
        print!("{}", log_line);
    }

    if let Ok(mut file) = OpenOptions::new()
        .create(true)
        .write(true)
        .append(true)
        .open(&path)
    {
        let _ = file.write_all(log_line.as_bytes());
    }
}

fn log_event(level: String, message: String) {
    write_log(&level, &message);
}

fn select_hod_file(window: GameWindow) -> Result<Option<String>, String> {
    #[cfg(test)]
    if let Ok(path)=std::env::var("OPENHW_EDITOR_TEST_SAVE"){return Ok(Some(path));}
    write_log("INFO", "Opening native file dialog...");
    let file = rfd::FileDialog::new().set_parent(&window)
        .add_filter("Homeworld HOD Files", &["hod"])
        .pick_file();

    match &file {
        Some(path) => {
            write_log(
                "INFO",
                &format!("User selected HOD file: {}", path.to_string_lossy()),
            );
            Ok(Some(path.to_string_lossy().to_string()))
        }
        None => {
            write_log("INFO", "User canceled file selection dialog");
            Ok(None)
        }
    }
}

fn select_shader_directory(window: GameWindow) -> Result<Option<String>, String> {
    write_log("INFO", "Opening native folder dialog for shader directory...");
    let folder = rfd::FileDialog::new().set_parent(&window)
        .pick_folder();

    match &folder {
        Some(path) => {
            write_log(
                "INFO",
                &format!("User selected shader directory: {}", path.to_string_lossy()),
            );
            Ok(Some(path.to_string_lossy().to_string()))
        }
        None => {
            write_log("INFO", "User canceled shader directory selection dialog");
            Ok(None)
        }
    }
}

fn select_dae_file(window: GameWindow) -> Result<Option<String>, String> {
    write_log("INFO", "Opening native file dialog for DAE file...");
    let file = rfd::FileDialog::new().set_parent(&window)
        .add_filter("Collada DAE Files", &["dae", "DAE"])
        .pick_file();

    match &file {
        Some(path) => {
            write_log(
                "INFO",
                &format!("User selected DAE file: {}", path.to_string_lossy()),
            );
            Ok(Some(path.to_string_lossy().to_string()))
        }
        None => {
            write_log("INFO", "User canceled DAE file selection dialog");
            Ok(None)
        }
    }
}

mod document;
mod runtime_materials;
mod mod_workspace;
mod event_document;
mod game_assets;
#[cfg(windows)]
mod game_window;

fn installed_archive() -> Result<game_assets::GameAssets, String> {
    game_assets::GameAssets::open(&game_window::root()?.join("OpenHW-HW1.obig")).map_err(|e| format!("Cannot open installed game models: {e}"))
}

fn list_game_models() -> Result<Vec<String>, String> {
    Ok(installed_archive()?.names())
}

fn load_hod(file_path: String, keeper_path: Option<String>) -> Result<HODModel, String> {
    let (bytes, external, mad) = if let Some(key) = file_path.strip_prefix("game://") {
        let bytes = mod_workspace::read(key)?;
        let mad = mod_workspace::companion(key)?;
        (bytes, None, mad)
    } else {
        (fs::read(&file_path).map_err(|e| e.to_string())?, Some(file_path.as_str()), None)
    };
    let mut model = HODModel::parse_with_external(&bytes, external, keeper_path.as_deref())?;
    if let Some(mad) = mad {
        model.animations = hwr_hod_parser::animation::parse_mad_bytes(&mad, &model.joints)?;
    }
    if !model.is_v2 {
        hwr_hod_parser::hod::synthesize_engine_nozzles_v1(&mut model);
        hwr_hod_parser::hod::validate_marker_parents(&mut model);
        model.is_v2 = true;
        model.version = 512;
    }
    *document::SOURCE.lock().map_err(|_| "Cannot retain original HOD")? =
        Some(document::SourceDocument {name: file_path, bytes, chunks: std::mem::take(&mut model.preserved_chunks)});
    Ok(model)
}

fn select_save_hod_file(window: GameWindow, default_name: Option<String>) -> Result<Option<String>, String> {
    #[cfg(test)]
    if let Ok(path)=std::env::var("OPENHW_EDITOR_TEST_SAVE"){return Ok(Some(path));}
    write_log("INFO", "Opening native save HOD file dialog...");
    let mut dialog = rfd::FileDialog::new().set_parent(&window).add_filter("Homeworld HOD Files", &["hod"]);

    if let Some(name) = default_name {
        dialog = dialog.set_file_name(&name);
    }

    let projects = game_window::root()?.join("Modding").join("HODs");
    fs::create_dir_all(&projects).map_err(|e| e.to_string())?;
    let file = dialog.set_directory(projects).save_file();

    match &file {
        Some(path) => {
            write_log(
                "INFO",
                &format!("User selected save HOD path: {}", path.to_string_lossy()),
            );
            Ok(Some(path.to_string_lossy().to_string()))
        }
        None => {
            write_log("INFO", "User canceled save HOD file dialog");
            Ok(None)
        }
    }
}

fn get_config_path() -> Result<PathBuf, String> {
    let config_dir = PathBuf::from(std::env::var_os("APPDATA").ok_or("Missing application data folder")?).join("com.gbx.hodeditorjs");
    if !config_dir.exists() {
        fs::create_dir_all(&config_dir)
            .map_err(|e| format!("Failed to create config directory: {}", e))?;
    }
    
    Ok(config_dir.join("hod_editor_config.json"))
}

fn load_shader_config() -> Result<ShaderConfig, String> {
    let config_path = get_config_path()?;
    if config_path.exists() {
        let data = fs::read_to_string(&config_path)
            .map_err(|e| format!("Failed to read config: {}", e))?;
        serde_json::from_str(&data)
            .map_err(|e| format!("Failed to parse config: {}", e))
    } else {
        Ok(ShaderConfig::default())
    }
}

fn save_shader_config(config: ShaderConfig) -> Result<(), String> {
    let config_path = get_config_path()?;
    let data = serde_json::to_string_pretty(&config)
        .map_err(|e| format!("Failed to serialize config: {}", e))?;
    fs::write(&config_path, data)
        .map_err(|e| format!("Failed to write config: {}", e))?;
    write_log("INFO", &format!("Saved shader config to {:?}", config_path));
    Ok(())
}


fn shader_base_path(path_str: &str) -> PathBuf {
    let keeper_dir = Path::new(path_str);
    if keeper_dir.ends_with("shaders") {
        keeper_dir.to_path_buf()
    } else {
        keeper_dir.join("shaders")
    }
}

/// Parse HOD shader aliases from `hod_alias.manifest` / `mod_alias.manifest`.
/// Each entry maps a surface/program stem to the shader name stored in a HOD.
fn parse_hod_alias_manifest(content: &str) -> Vec<(String, String)> {
    content
        .lines()
        .filter_map(|line| {
            let line = line.split("//").next().unwrap_or("").trim();
            let mut parts = line.split_whitespace();
            if parts.next()? != "hod_alias" {
                return None;
            }
            let target = parts.next()?.to_string();
            let alias = parts.next()?.to_string();
            Some((target, alias))
        })
        .collect()
}

fn read_hod_alias_entries(shaders_base: &Path) -> Vec<(String, String)> {
    let mut aliases = Vec::new();
    for file_name in ["hod_alias.manifest", "mod_alias.manifest"] {
        if let Ok(content) = fs::read_to_string(shaders_base.join(file_name)) {
            aliases.extend(parse_hod_alias_manifest(&content));
        }
    }
    aliases
}

#[derive(serde::Serialize)]
struct ShaderInfo {
    name: String,
    slots: Vec<String>,
}

fn get_dynamic_shader_slots(keeper_paths: Vec<String>) -> Result<Vec<ShaderInfo>, String> {
    use std::collections::HashMap;
    use regex::Regex;
    let mut shaders: HashMap<String, Vec<String>> = HashMap::new();
    let mut program_params: HashMap<String, Vec<String>> = HashMap::new();
    let mut aliases: Vec<(String, String)> = Vec::new();

    // We match inTexParam
    let re = Regex::new(r"inTex([a-zA-Z0-9_]+)").map_err(|e| e.to_string())?;

    for path_str in &keeper_paths {
        let shaders_base = shader_base_path(path_str);

        // Sometimes they are in shaders/ or shaders/gl_prog/
        let dirs_to_check = [
            shaders_base.clone(),
            shaders_base.join("gl_prog"),
        ];

        for dir in &dirs_to_check {
            if dir.is_dir() {
                if let Ok(entries) = std::fs::read_dir(dir) {
                    for entry in entries.flatten() {
                        let path = entry.path();
                        if path.is_file() {
                            let ext = path.extension().and_then(|s| s.to_str()).unwrap_or("");
                            if ext == "prog" || ext == "fx" || ext == "shader" {
                                if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                                    let content = std::fs::read_to_string(&path).unwrap_or_default();
                                    let mut params = Vec::new();
                                    for cap in re.captures_iter(&content) {
                                        let param = cap[1].to_string();
                                        if !params.contains(&param) {
                                            params.push(param);
                                        }
                                    }

                                    // Keep the program parameters so HOD aliases can reuse them.
                                    let program = program_params.entry(stem.to_string()).or_default();
                                    for param in &params {
                                        if !program.contains(param) {
                                            program.push(param.clone());
                                        }
                                    }

                                    let mut name = stem.to_string();
                                    if let Some(stripped) = name.strip_prefix("sob_") {
                                        name = stripped.to_string();
                                    }

                                    let entry = shaders.entry(name).or_default();
                                    for param in params {
                                        if !entry.contains(&param) {
                                            entry.push(param);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        aliases.extend(read_hod_alias_entries(&shaders_base));
    }

    // HODs store alias names such as `matteScissor`, while the shader
    // directories contain the backing program (`sob_matte_s.prog`).
    for (target, alias) in aliases {
        let Some(params) = program_params.get(&target) else {
            continue;
        };
        if params.is_empty() {
            continue;
        }
        let entry = shaders.entry(alias).or_default();
        for param in params {
            if !entry.contains(param) {
                entry.push(param.clone());
            }
        }
    }

    let mut result = Vec::new();
    for (name, slots) in shaders {
        result.push(ShaderInfo { name, slots });
    }
    result.sort_by(|a, b| a.name.cmp(&b.name));
    Ok(result)
}

fn get_shader_pipelines(keeper_paths: Vec<String>) -> Result<Vec<String>, String> {
    write_log(
        "INFO",
        &format!(
            "Scanning shader pipelines from {} keeper path(s)",
            keeper_paths.len()
        ),
    );
    let mut pipelines: Vec<String> = Vec::new();

    for path_str in &keeper_paths {
        let shaders_base = shader_base_path(path_str);
        let shaders_dir = shaders_base.join("gl_prog");
        write_log("INFO", &format!("Looking for shaders in: {:?}", shaders_dir));
        if shaders_dir.is_dir() {
            if let Ok(entries) = fs::read_dir(&shaders_dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.is_file()
                        && path.extension().and_then(|s| s.to_str()) == Some("prog")
                    {
                        if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                            if !pipelines.contains(&stem.to_string()) {
                                pipelines.push(stem.to_string());
                            }
                            // If it starts with "sob_", also add the stripped version (e.g. "sob_ship" -> "ship")
                            if let Some(stripped) = stem.strip_prefix("sob_") {
                                let simplified = stripped.to_string();
                                if !pipelines.contains(&simplified) {
                                    pipelines.push(simplified);
                                }
                            }
                        }
                    }
                }
            }
        }

        // HOD shader names are aliases defined in hod_alias.manifest, not
        // necessarily the .prog filenames (e.g. `mattescissor` -> `sob_matte_s`).
        for (_target, alias) in read_hod_alias_entries(&shaders_base) {
            if !pipelines.contains(&alias) {
                pipelines.push(alias);
            }
        }
    }

    pipelines.sort();
    pipelines.dedup();
    Ok(pipelines)
}

fn export_textures_tga(folder_path: String, textures: Vec<HODTexture>) -> Result<(), String> {
    write_log(
        "INFO",
        &format!(
            "Exporting {} textures as TGA files to: {}",
            textures.len(),
            folder_path
        ),
    );

    let path = PathBuf::from(folder_path);
    if !path.exists() {
        fs::create_dir_all(&path).map_err(|e| e.to_string())?;
    }

    for tex in textures {
        if let Some(b64_data) = tex.png_data.or(tex.png_preview) {
            let clean_b64 = if b64_data.contains("base64,") {
                b64_data.split("base64,").collect::<Vec<&str>>()[1]
            } else {
                &b64_data
            };

            use base64::prelude::*;
            let png_bytes = BASE64_STANDARD
                .decode(clean_b64)
                .map_err(|e| format!("Base64 decode error: {}", e))?;

            let img = image::load_from_memory_with_format(&png_bytes, image::ImageFormat::Png)
                .map_err(|e| format!("Failed to parse PNG bytes: {}", e))?;

            let tga_name = if tex.name.to_lowercase().ends_with(".tga") {
                tex.name.clone()
            } else {
                format!("{}.tga", tex.name)
            };

            let tga_path = path.join(&tga_name);

            img.save_with_format(&tga_path, image::ImageFormat::Tga)
                .map_err(|e| format!("Failed to save TGA file: {}", e))?;

            write_log(
                "INFO",
                &format!("Successfully exported TGA: {:?}", tga_path),
            );
        }
    }

    Ok(())
}

fn save_text_file(window: GameWindow, 
    default_name: String,
    filters: Vec<String>,
    contents: String,
) -> Result<Option<String>, String> {
    write_log(
        "INFO",
        &format!(
            "Opening native save dialog for default name: {}",
            default_name
        ),
    );
    let mut dialog = rfd::FileDialog::new().set_parent(&window).set_file_name(&default_name);
    if !filters.is_empty() {
        let filter_slices: Vec<&str> = filters.iter().map(|s| s.as_str()).collect();
        dialog = dialog.add_filter("Export File", &filter_slices);
    }

    if let Some(path) = dialog.save_file() {
        fs::write(&path, contents).map_err(|e| e.to_string())?;
        write_log(
            "INFO",
            &format!("Successfully saved file: {}", path.to_string_lossy()),
        );
        Ok(Some(path.to_string_lossy().to_string()))
    } else {
        write_log("INFO", "User canceled save file selection dialog");
        Ok(None)
    }
}

fn load_text_file(window: GameWindow, filters: Vec<String>) -> Result<Option<String>, String> {
    write_log("INFO", "Opening native pick file dialog...");
    let mut dialog = rfd::FileDialog::new().set_parent(&window);
    if !filters.is_empty() {
        let filter_slices: Vec<&str> = filters.iter().map(|s| s.as_str()).collect();
        dialog = dialog.add_filter("Import File", &filter_slices);
    }

    if let Some(path) = dialog.pick_file() {
        let content = fs::read_to_string(&path).map_err(|e| e.to_string())?;
        write_log(
            "INFO",
            &format!("Successfully loaded file: {}", path.to_string_lossy()),
        );
        Ok(Some(content))
    } else {
        write_log("INFO", "User canceled pick file selection dialog");
        Ok(None)
    }
}

fn import_tga_textures(window: GameWindow) -> Result<Vec<HODTexture>, String> {
    write_log(
        "INFO",
        "Opening native pick file dialog for importing TGA textures...",
    );
    let files = rfd::FileDialog::new().set_parent(&window)
        .add_filter("TGA Image", &["tga"])
        .pick_files();

    match files {
        Some(src_paths) => {
            let mut textures = Vec::new();
            for src_path in src_paths {
                write_log(
                    "INFO",
                    &format!(
                        "User selected TGA to import: {}",
                        src_path.to_string_lossy()
                    ),
                );

                let file_name = src_path
                    .file_name()
                    .ok_or_else(|| "Invalid file name".to_string())?;

                let img_bytes = fs::read(&src_path)
                    .map_err(|e| format!("Failed to read source TGA: {}", e))?;

                let img = image::load_from_memory_with_format(&img_bytes, image::ImageFormat::Tga)
                    .map_err(|e| format!("Failed to decode TGA file: {}", e))?;

                let width = img.width();
                let height = img.height();
                let format = if img.to_rgba8().pixels().any(|pixel| pixel[3] < 250) {
                    "DXT5"
                } else {
                    "DXT1"
                };

                let mut png_bytes: Vec<u8> = Vec::new();
                let mut cursor = std::io::Cursor::new(&mut png_bytes);
                img.write_to(&mut cursor, image::ImageFormat::Png)
                    .map_err(|e| format!("Failed to encode preview as PNG: {}", e))?;

                use base64::prelude::*;
                let b64_preview = BASE64_STANDARD.encode(&png_bytes);

                let tex_name = file_name.to_string_lossy().to_string();
                let final_tex_name = if tex_name.to_lowercase().ends_with(".tga") {
                    tex_name[..tex_name.len() - 4].to_string()
                } else {
                    tex_name
                };

                textures.push(HODTexture {
                    name: final_tex_name,
                    width: width,
                    height: height,
                    format: format.to_string(),
                    png_preview: Some(format!("data:image/png;base64,{}", b64_preview)),
                    png_data: Some(b64_preview),
                    source_path: Some(src_path.to_string_lossy().to_string()),
                });
            }
            Ok(textures)
        }
        None => {
            write_log("INFO", "User canceled TGA import file dialog");
            Ok(Vec::new())
        }
    }
}

fn import_dae_file(path: String) -> Result<hwr_hod_parser::hod::HODModel, String> {
    write_log("INFO", &format!("Importing DAE file from: {}", path));

    let xml_str = std::fs::read_to_string(&path).map_err(|e| {
        let err_msg = format!("Failed to read DAE file: {}", e);
        write_log("ERROR", &err_msg);
        err_msg
    })?;

    let mut model = hwr_hod_parser::dae::parse_dae(&xml_str).map_err(|e| {
        let err_msg = format!("Failed to parse DAE XML: {}", e);
        write_log("ERROR", &err_msg);
        err_msg
    })?;
    // Clean up hierarchy and resolve name collisions
    model.auto_repair_assembly_names();
    model.clean_hierarchy();
    model.deduplicate_names();
    hwr_hod_parser::hod::normalize_collision_meshes(&mut model);
    model.auto_assign_and_resize_textures();

    write_log(
        "INFO",
        &format!(
            "Successfully imported DAE as HOD 2.0 ({} meshes, {} joints)",
            model.meshes.len(),
            model.joints.len()
        ),
    );
    Ok(model)
}

fn convert_weapon_to_turret(
    mut model: hwr_hod_parser::hod::HODModel,
    base_name: String,
) -> Result<hwr_hod_parser::hod::HODModel, String> {
    write_log(
        "INFO",
        &format!("Converting Weapon to Turret for assembly: {}", base_name),
    );
    model.convert_weapon_to_turret(&base_name)?;
    Ok(model)
}

fn auto_generate_collision_from_mesh(
    mut model: HODModel,
    collision_mesh_name: String,
    source_mesh_name: Option<String>,
    source_mesh_names: Option<Vec<String>>,
) -> Result<HODModel, String> {
    let mesh_names: Vec<String> = match source_mesh_names {
        Some(names) if !names.is_empty() => names,
        _ => match source_mesh_name {
            Some(name) if !name.trim().is_empty() => vec![name],
            _ => return Err("No source meshes selected for collision generation".to_string()),
        },
    };

    write_log(
        "INFO",
        &format!(
            "Auto-generating collision mesh '{}' from visible meshes {:?}...",
            collision_mesh_name, mesh_names
        ),
    );

    // Attempt to generate the combined collision mesh using convex hull
    let new_col_mesh = hwr_hod_parser::collision::generate_collision_from_model_meshes(
        &model.meshes,
        &model.joints,
        &mesh_names,
    )
    .ok_or_else(|| {
        "Failed to generate collision mesh: no valid vertices found in selected source meshes"
            .to_string()
    })?;

    // Update the model with the newly generated mesh parts and computed bounds
    if let Some(col) = model
        .collision_meshes
        .iter_mut()
        .find(|c| c.name == collision_mesh_name)
    {
        col.mesh.parts = new_col_mesh.mesh.parts;
        col.min_extents = new_col_mesh.min_extents;
        col.max_extents = new_col_mesh.max_extents;
        col.center = new_col_mesh.center;
        col.radius = new_col_mesh.radius;
    } else {
        return Err(format!(
            "Collision mesh '{}' not found in model",
            collision_mesh_name
        ));
    }

    // Clear any preserved raw COLD/KDOP chunks so save serializes the newly generated collision mesh into POOL/DTRM
    model
        .preserved_chunks
        .retain(|chunk| chunk.id != "KDOP" && chunk.id != "COLD");
    for chunk in &mut model.preserved_chunks {
        if chunk.id == "DTRM" {
            chunk
                .children
                .retain(|child| child.id != "KDOP" && child.id != "COLD");
        }
    }

    // Normalize and ensure collision mesh validity
    hwr_hod_parser::hod::generate_collision_mesh(&mut model);

    write_log("INFO", "Collision mesh generation successful.");
    Ok(model)
}

mod subsystems;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn shader_base_path_accepts_shaders_or_parent_directory() {
        assert_eq!(
            shader_base_path("/game/HWRM/shaders"),
            PathBuf::from("/game/HWRM/shaders")
        );
        assert_eq!(
            shader_base_path("/game/HWRM"),
            PathBuf::from("/game/HWRM/shaders")
        );
    }

    #[test]
    fn parses_hod_alias_manifest_with_comments_and_extra_columns() {
        let content = r#"
// Ships
hod_alias	sob_ship		ship			SOB_
hod_alias	sob_matte_s		mattescissor	SOB_
//hod_alias	sob_matte		default		SOB_
"#;
        assert_eq!(
            parse_hod_alias_manifest(content),
            vec![
                ("sob_ship".to_string(), "ship".to_string()),
                ("sob_matte_s".to_string(), "mattescissor".to_string()),
            ]
        );
    }

    #[test]
    fn shader_aliases_reuse_backing_program_slots() {
        let root = std::env::temp_dir().join(format!(
            "hodeditorjs-shader-alias-test-{}",
            std::process::id()
        ));
        let shaders_dir = root.join("shaders");
        let gl_prog = shaders_dir.join("gl_prog");
        fs::create_dir_all(&gl_prog).unwrap();
        fs::write(
            shaders_dir.join("hod_alias.manifest"),
            "hod_alias\tsob_matte_s\tmattescissor\tSOB_\n",
        )
        .unwrap();
        fs::write(
            gl_prog.join("sob_matte_s.prog"),
            "local tex inTexDiff 0\nlocal tex inTexGlow 1\nlocal tex inTexTeam 2\nlocal tex inTexNorm 3\n",
        )
        .unwrap();

        let shaders =
            get_dynamic_shader_slots(vec![root.to_string_lossy().into_owned()]).unwrap();
        let alias = shaders
            .iter()
            .find(|shader| shader.name == "mattescissor")
            .expect("mattescissor alias missing");
        assert_eq!(alias.slots, vec!["Diff", "Glow", "Team", "Norm"]);

        let _ = fs::remove_dir_all(&root);
    }
}

#[cfg(test)]
mod document_tests;
