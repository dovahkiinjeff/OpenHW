use super::*;
use std::collections::BTreeSet;
use std::io::{self, Write};
use tempfile::NamedTempFile;

fn valid_name(name: &str) -> bool {
    !name.is_empty()
        && name.len() <= 64
        && name
            .bytes()
            .all(|c| c.is_ascii_alphanumeric() || b"_-".contains(&c))
        && ![
            "con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6", "com7",
            "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
        ]
        .contains(&name.to_lowercase().as_str())
}
fn relative(key: &str) -> Result<PathBuf, String> {
    let key = key.replace('\\', "/");
    if key
        .split('/')
        .any(|s| s.is_empty() || s == "." || s == ".." || s.contains(':'))
        || key.starts_with('/')
    {
        return Err("Invalid game file path".into());
    }
    if key.to_lowercase().starts_with("parade-") {
        return Err("Parade formations are personal settings".into());
    }
    Ok(PathBuf::from(key))
}
fn safe(path: &Path) -> Result<(), String> {
    let mut at = PathBuf::new();
    for part in path.components() {
        at.push(part);
        if let Ok(meta) = fs::symlink_metadata(&at) {
            use std::os::windows::fs::MetadataExt;
            if meta.file_attributes() & 0x400 != 0 {
                return Err("Choose a mod folder without linked directories".into());
            }
        }
    }
    Ok(())
}
fn mods_root() -> Result<PathBuf, String> {
    Ok(game_window::root()?.join("mods"))
}
pub fn list() -> Result<Vec<String>, String> {
    let root = mods_root()?;
    if !root.exists() {
        return Ok(vec![]);
    }
    let mut names = vec![];
    for entry in fs::read_dir(root).map_err(|e| e.to_string())? {
        let entry = entry.map_err(|e| e.to_string())?;
        let name = entry.file_name().to_string_lossy().into_owned();
        if valid_name(&name) && entry.path().is_dir() {
            safe(&entry.path())?;
            names.push(name);
        }
    }
    names.sort();
    Ok(names)
}
fn enable(name: &str) -> Result<(), String> {
    let path = mods_root()?.join("load-order.txt");
    safe(&path)?;
    let previous = match fs::read_to_string(&path) {
        Ok(s) => s,
        Err(e) if e.kind() == io::ErrorKind::NotFound => String::new(),
        Err(e) => return Err(e.to_string()),
    };
    let mut lines: Vec<String> = previous
        .lines()
        .filter(|line| {
            !line
                .trim_start_matches("#disabled=")
                .eq_ignore_ascii_case(name)
        })
        .map(str::to_owned)
        .collect();
    lines.push(name.into());
    let mut pending = NamedTempFile::new_in(path.parent().unwrap()).map_err(|e| e.to_string())?;
    pending
        .write_all((lines.join("\n") + "\n").as_bytes())
        .map_err(|e| e.to_string())?;
    pending.persist(path).map_err(|e| e.to_string())?;
    Ok(())
}
pub fn create(name: String) -> Result<Vec<String>, String> {
    if !valid_name(&name) {
        return Err("Use up to 64 letters, numbers, underscores or hyphens".into());
    }
    let root = mods_root()?;
    safe(&root)?;
    fs::create_dir_all(&root).map_err(|e| e.to_string())?;
    let folder = root.join(&name);
    safe(&folder)?;
    fs::create_dir(&folder).map_err(|e| e.to_string())?;
    if let Err(error) = enable(&name) {
        let _ = fs::remove_dir(&folder);
        return Err(error);
    }
    list()
}
pub fn read(key: &str) -> Result<Vec<u8>, String> {
    read_with_archive(key, &mut installed_archive()?)
}
pub fn read_with_archive(
    key: &str,
    archive: &mut game_assets::GameAssets,
) -> Result<Vec<u8>, String> {
    let rel = relative(key)?;
    let base = game_window::root()?;
    let order = fs::read_to_string(base.join("mods/load-order.txt")).unwrap_or_default();
    for name in order.lines().rev().map(str::trim).filter(|s| valid_name(s)) {
        let path = base.join("mods").join(name).join(&rel);
        safe(&path)?;
        if path.is_file() {
            return fs::read(path).map_err(|e| e.to_string());
        }
    }
    for path in [base.join(&rel), base.join("Data").join(&rel)] {
        if path.is_file() {
            return fs::read(path).map_err(|e| e.to_string());
        }
    }
    archive.read(&key.to_lowercase()).map_err(|e| e.to_string())
}
fn asset_key(source: &str, template: &str) -> Result<String, String> {
    if let Some(key) = source.strip_prefix("game://") {
        relative(key)?;
        return Ok(key.into());
    }
    let path = Path::new(source);
    if let Ok(rel) = path.strip_prefix(mods_root()?) {
        let key = rel
            .components()
            .skip(1)
            .collect::<PathBuf>()
            .to_string_lossy()
            .replace('\\', "/");
        relative(&key)?;
        return Ok(key);
    }
    if template.is_empty() {
        return Err("Choose a ship template before saving to a mod".into());
    }
    relative(template)?;
    Ok(template.into())
}
fn dependency(key: &str, definition: &str) -> Result<String, String> {
    let parent = Path::new(definition)
        .parent()
        .ok_or("Missing game folder")?;
    let key = if let Some(key) = key.strip_prefix("@/") {
        key.to_owned()
    } else {
        parent.join(key).to_string_lossy().replace('\\', "/")
    };
    relative(&key)?;
    Ok(key)
}
pub fn save(
    name: String,
    source: String,
    model: HODModel,
    mounts: Vec<subsystems::Mount>,
    template: String,
) -> Result<String, String> {
    if !valid_name(&name) || !list()?.contains(&name) {
        return Err("Choose an existing mod".into());
    }
    let key = asset_key(&source, &template)?;
    let root = mods_root()?.join(&name);
    safe(&root)?;
    let staging = tempfile::tempdir_in(&root).map_err(|e| e.to_string())?;
    let target = staging.path().join(relative(&key)?);
    fs::create_dir_all(target.parent().unwrap()).map_err(|e| e.to_string())?;
    let has_animations = !model.animations.is_empty();
    subsystems::save(
        target.to_string_lossy().into(),
        source,
        model,
        mounts,
        template,
    )?;
    // Read the canonical saved model so material exports follow the writer's
    // resolved texture assignments, including changes made in the inspector.
    let bytes = fs::read(&target).map_err(|e| e.to_string())?;
    let saved = HODModel::parse_with_external(&bytes, None, None)?;
    let material_count = saved.materials.len();
    runtime_materials::stage(&saved, target.parent().unwrap())?;
    let definition = if target
        .file_stem()
        .is_some_and(|s| s.eq_ignore_ascii_case("hull"))
    {
        target.with_file_name("ship.owshipdef")
    } else {
        target.with_extension("owshipdef")
    };
    let definition = if definition.exists() {
        definition
    } else {
        let document_key = Path::new(&key)
            .parent()
            .unwrap()
            .join("subsystem.owsubsystem")
            .to_string_lossy()
            .replace('\\', "/");
        let text = String::from_utf8(local_read(&root, &document_key)?).map_err(|e| e.to_string())?;
        let definition = target.with_file_name("subsystem.owsubsystem");
        fs::write(&definition, text).map_err(|e| e.to_string())?;
        definition
    };
    let mut text = fs::read_to_string(&definition).map_err(|e| e.to_string())?;
    // The runtime model and edited material pages use the same material indices.
    let count = material_count;
    let split = text
        .find("[Ship]")
        .or_else(|| text.find("[Weapons]"))
        .unwrap_or(text.len());
    let head = text[..split]
        .lines()
        .filter(|s| !s.starts_with("surface 0 "))
        .map(|s| {
            let fields: Vec<_> = s.split_whitespace().collect();
            match fields.first().copied() {
                Some("hull") => format!("hull {}", target.file_name().unwrap().to_string_lossy()),
                Some("mad") => format!(
                    "mad {}",
                    if !has_animations {
                        "-".into()
                    } else {
                        format!("{}.mad", target.file_stem().unwrap().to_string_lossy())
                    }
                ),
                _ => s.to_owned(),
            }
        })
        .collect::<Vec<_>>()
        .join("\n");
    text = format!(
        "{head}\n{}\n{}",
        (0..count)
            .map(|i| format!("surface 0 {i} ow_m{i}"))
            .collect::<Vec<_>>()
            .join("\n"),
        &text[split..]
    );
    fs::write(&definition, &text).map_err(|e| e.to_string())?;
    let def_key = definition
        .strip_prefix(staging.path())
        .unwrap()
        .to_string_lossy()
        .replace('\\', "/");
    let mut pending = vec![(def_key.clone(), text)];
    let mut seen = BTreeSet::new();
    while let Some((owner, body)) = pending.pop() {
        for line in body.lines() {
            let words: Vec<_> = line.split_whitespace().collect();
            let reference = match words.as_slice() {
                ["presentation", _, path] => Some(*path),
                ["WeaponFire", path] => Some(*path),
                _ => None,
            };
            if let Some(reference) = reference {
                let key = dependency(reference, &owner)?;
                if seen.insert(key.clone()) {
                    let dest = staging.path().join(&key);
                    fs::create_dir_all(dest.parent().unwrap()).map_err(|e| e.to_string())?;
                    let current = root.join(&key);
                    let bytes = if current.is_file() {
                        fs::read(current).map_err(|e| e.to_string())?
                    } else {
                        read(&key)?
                    };
                    fs::write(dest, bytes).map_err(|e| e.to_string())?;
                }
            }
            if line.starts_with("StartShipHardPointConfig(") {
                let args: Vec<_> = line.split('"').skip(1).step_by(2).collect();
                if let Some(asset) = args.get(5) {
                    let key = format!("subsystems/{asset}/subsystem.owsubsystem");
                    if seen.insert(key.clone()) {
                        let bytes = local_read(&root, &key)?;
                        let body = String::from_utf8(bytes.clone()).map_err(|e| e.to_string())?;
                        let dest = staging.path().join(&key);
                        fs::create_dir_all(dest.parent().unwrap()).map_err(|e| e.to_string())?;
                        fs::write(dest, bytes).map_err(|e| e.to_string())?;
                        pending.push((key, body));
                    }
                }
            }
        }
    }
    commit(staging.path(), &root)?;
    enable(&name)?;
    Ok(root.join(key).to_string_lossy().into())
}
fn commit(staging: &Path, root: &Path) -> Result<(), String> {
    fn collect(dir: &Path, base: &Path, files: &mut Vec<PathBuf>) -> Result<(), String> {
        for entry in fs::read_dir(dir).map_err(|e| e.to_string())? {
            let path = entry.map_err(|e| e.to_string())?.path();
            if path.is_dir() {
                collect(&path, base, files)?
            } else {
                files.push(path.strip_prefix(base).unwrap().to_owned())
            }
        }
        Ok(())
    }
    let mut files = vec![];
    collect(staging, staging, &mut files)?;
    let backup = tempfile::tempdir_in(root).map_err(|e| e.to_string())?;
    let mut written = vec![];
    let result = (|| {
        for rel in &files {
            let dest = root.join(rel);
            safe(&dest)?;
            fs::create_dir_all(dest.parent().unwrap()).map_err(|e| e.to_string())?;
            let old = if dest.exists() {
                let path = backup.path().join(rel);
                fs::create_dir_all(path.parent().unwrap()).map_err(|e| e.to_string())?;
                fs::copy(&dest, &path).map_err(|e| e.to_string())?;
                Some(path)
            } else {
                None
            };
            let mut file =
                NamedTempFile::new_in(dest.parent().unwrap()).map_err(|e| e.to_string())?;
            io::copy(
                &mut fs::File::open(staging.join(rel)).map_err(|e| e.to_string())?,
                &mut file,
            )
            .map_err(|e| e.to_string())?;
            file.persist(&dest).map_err(|e| e.to_string())?;
            written.push((dest, old));
        }
        Ok(())
    })();
    if result.is_err() {
        for (dest, old) in written.into_iter().rev() {
            if let Some(old) = old {
                fs::copy(old, dest).map_err(|e| e.to_string())?;
            } else {
                fs::remove_file(dest).map_err(|e| e.to_string())?;
            }
        }
    }
    result
}

#[derive(Serialize)]
pub struct RelatedFile {
    pub key: String,
    pub kind: String,
}
fn saved_ship(path: &str) -> Result<(PathBuf, String), String> {
    let base = mods_root()?;
    let path = PathBuf::from(path);
    let rel = path
        .strip_prefix(&base)
        .map_err(|_| "Save the ship to a mod first")?;
    let name = rel
        .components()
        .next()
        .ok_or("Missing mod name")?
        .as_os_str()
        .to_string_lossy();
    if !valid_name(&name) {
        return Err("Invalid mod name".into());
    }
    let root = base.join(name.as_ref());
    safe(&path)?;
    let key = rel
        .components()
        .skip(1)
        .collect::<PathBuf>()
        .to_string_lossy()
        .replace('\\', "/");
    relative(&key)?;
    Ok((root, key))
}
pub fn companion(key: &str) -> Result<Option<Vec<u8>>, String> {
    let document = if key.starts_with("subsystems/") {
        Path::new(key)
            .with_file_name("subsystem.owsubsystem")
            .to_string_lossy()
            .replace('\\', "/")
    } else {
        ship_definition(key)
    };
    if let Ok(bytes) = read(&document) {
        if let Ok(text) = String::from_utf8(bytes) {
            for line in text.lines() {
                let fields: Vec<_> = line.split_whitespace().collect();
                if let ["mad", name] = fields.as_slice() {
                    return if *name == "-" {
                        Ok(None)
                    } else {
                        read(&dependency(name, &document)?).map(Some)
                    };
                }
            }
        }
    }
    let mad = Path::new(key)
        .with_extension("mad")
        .to_string_lossy()
        .replace('\\', "/");
    match read(&mad) {
        Ok(bytes) => Ok(Some(bytes)),
        Err(_) => Ok(None),
    }
}
fn ship_definition(key: &str) -> String {
    let path = Path::new(key);
    if key.starts_with("subsystems/") {
        return path
            .with_file_name("subsystem.owsubsystem")
            .to_string_lossy()
            .replace('\\', "/");
    }

    let path = if path
        .file_stem()
        .is_some_and(|s| s.eq_ignore_ascii_case("hull"))
    {
        path.with_file_name("ship.owshipdef")
    } else {
        path.with_extension("owshipdef")
    };
    path.to_string_lossy().replace('\\', "/")
}
fn local_read(root: &Path, key: &str) -> Result<Vec<u8>, String> {
    let path = root.join(relative(key)?);
    safe(&path)?;
    if path.is_file() {
        fs::read(path).map_err(|e| e.to_string())
    } else {
        read(key)
    }
}
pub fn files(path: String) -> Result<Vec<RelatedFile>, String> {
    let (root, key) = saved_ship(&path)?;
    let mut todo = vec![ship_definition(&key)];
    let mut seen = BTreeSet::new();
    let mut result = vec![];
    while let Some(key) = todo.pop() {
        if !seen.insert(key.clone()) {
            continue;
        }
        if seen.len() > 512 {
            return Err("Too many referenced files".into());
        }
        let kind = if key.ends_with(".owevents") {
            "events"
        } else if key.ends_with(".owshipdef") {
            "ship"
        } else {
            "weapon"
        };
        result.push(RelatedFile {
            key: key.clone(),
            kind: kind.into(),
        });
        if kind == "events" {
            continue;
        }
        let text = String::from_utf8(local_read(&root, &key)?).map_err(|e| e.to_string())?;
        for line in text.lines() {
            let fields: Vec<_> = line.split_whitespace().collect();
            match fields.as_slice() {
                ["presentation", _, path] | ["WeaponFire", path] => {
                    todo.push(dependency(path, &key)?)
                }
                _ => {}
            }
            if line.starts_with("StartShipHardPointConfig(") {
                if let Some(asset) = line.split('"').skip(1).step_by(2).nth(5) {
                    todo.push(format!("subsystems/{asset}/subsystem.owsubsystem"));
                }
            }
        }
    }
    result.sort_by(|a, b| a.key.cmp(&b.key));
    Ok(result)
}
pub fn read_text(path: String, key: String) -> Result<String, String> {
    if !files(path.clone())?.iter().any(|f| f.key == key) {
        return Err("Select a file referenced by this ship".into());
    }
    let (root, _) = saved_ship(&path)?;
    let bytes = local_read(&root, &key)?;
    if key.ends_with(".owevents") {
        serde_json::to_string_pretty(&event_document::decode(&bytes)?).map_err(|e| e.to_string())
    } else {
        String::from_utf8(bytes).map_err(|e| e.to_string())
    }
}
pub fn write_text(path: String, key: String, text: String) -> Result<(), String> {
    if text.len() > 4 * 1024 * 1024 || text.contains('\0') {
        return Err("Invalid or oversized document".into());
    }
    if !files(path.clone())?.iter().any(|f| f.key == key) {
        return Err("Select a file referenced by this ship".into());
    }
    let (root, _) = saved_ship(&path)?;
    let bytes = if key.ends_with(".owevents") {
        let doc = serde_json::from_str(&text).map_err(|e| e.to_string())?;
        event_document::encode(&doc)?
    } else {
        text.into_bytes()
    };
    let dest = root.join(relative(&key)?);
    safe(&dest)?;
    fs::create_dir_all(dest.parent().unwrap()).map_err(|e| e.to_string())?;
    let mut pending = NamedTempFile::new_in(dest.parent().unwrap()).map_err(|e| e.to_string())?;
    pending.write_all(&bytes).map_err(|e| e.to_string())?;
    pending.persist(dest).map_err(|e| e.to_string())?;
    Ok(())
}
