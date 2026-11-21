use super::*;
use serde_json::{json, Value};
use std::{
    cell::Cell,
    collections::BTreeSet,
    ffi::{c_char, CStr, CString},
};

pub type Complete = unsafe extern "C" fn(
    *const c_char,
    *const c_char,
    usize,
    *mut c_char,
    usize,
    *mut c_char,
    usize,
) -> i32;
thread_local! {pub static COMPLETE:Cell<Option<Complete>>=const{Cell::new(None)};}
#[derive(Clone, Deserialize, Serialize)]
pub struct Mount {
    pub slot: String,
    pub asset: String,
    pub anchor: String,
    pub authored: bool,
}
fn id(s: &str) -> bool {
    !s.is_empty() && s.len() < 150 && s.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_')
}
fn check_ship(key: &str) -> Result<&str, String> {
    let parts: Vec<_> = key.split('/').collect();
    if parts.len() == 4
        && ["r1", "r2", "p1", "p2", "p3", "traders", "derelicts"].contains(&parts[0])
        && id(parts[1])
        && parts[2] == "rmnative"
        && parts[3] == "hull.hod"
    {
        Ok(key.trim_end_matches("hull.hod"))
    } else {
        Err("Select a gameplay ship template".into())
    }
}
fn read_asset(key: &str, archive: &mut game_assets::GameAssets) -> Result<Vec<u8>, String> {
    if !key.split('/').all(|s| {
        !s.is_empty()
            && s != "."
            && s != ".."
            && s.bytes()
                .all(|b| b.is_ascii_alphanumeric() || b"_.-".contains(&b))
    }) {
        return Err("Invalid asset path".into());
    }
    mod_workspace::read_with_archive(key,archive)
}
fn definition(name: &str, archive: &mut game_assets::GameAssets) -> Result<String, String> {
    if !id(name) {
        return Err("Invalid subsystem name".into());
    }
    String::from_utf8(read_asset(
        &format!("subsystems/{name}/subsystem.owsubsystem"),
        archive,
    )?)
    .map_err(|e| e.to_string())
}
fn usable(name: &str, def: &str) -> bool {
    name.contains("engine")
        || (def.contains("[Weapons]") && def.lines().any(|s| s.trim_start().starts_with("GUN ")))
}
pub fn catalog() -> Result<Vec<String>, String> {
    let mut archive = installed_archive()?;
    let mut names: BTreeSet<String> = archive.subsystem_names().into_iter().collect();
    if let Ok(index) = read_asset("subsystems/catalog.txt", &mut archive) {
        names.extend(
            String::from_utf8_lossy(&index)
                .lines()
                .map(|s| s.trim().to_lowercase()),
        );
    }
    let base = game_window::root()?;
    for folder in [base.join("Data/Subsystems"), base.join("Subsystems")] {
        if folder.is_dir() {
            for entry in fs::read_dir(folder).map_err(|e| e.to_string())?.flatten() {
                if entry.path().is_dir() {
                    names.insert(entry.file_name().to_string_lossy().to_lowercase());
                }
            }
        }
    }
    Ok(names
        .into_iter()
        .filter(|n| definition(n, &mut archive).is_ok_and(|s| usable(n, &s)))
        .collect())
}
pub fn component(name: String) -> Result<HODModel, String> {
    let mut archive = installed_archive()?;
    let def = definition(&name, &mut archive)?;
    let path = def
        .lines()
        .find_map(|s| s.trim().strip_prefix("hull "))
        .ok_or("Subsystem has no hull")?;
    let key = path
        .strip_prefix("@/")
        .map(str::to_owned)
        .unwrap_or_else(|| format!("subsystems/{name}/{path}"));
    HODModel::parse_with_external(&read_asset(&key, &mut archive)?, None, None)
}
fn companion(path: &Path) -> PathBuf {
    if path
        .file_stem()
        .is_some_and(|s| s.eq_ignore_ascii_case("hull"))
    {
        path.with_file_name("ship.owshipdef")
    } else {
        path.with_extension("owshipdef")
    }
}
fn source_text(source: &str, template: &str) -> Result<Option<String>, String> {
    if !source.is_empty() && !source.starts_with("game://") {
        match fs::read(companion(Path::new(source))) {
            Ok(b) => return String::from_utf8(b).map(Some).map_err(|e| e.to_string()),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {}
            Err(e) => return Err(e.to_string()),
        }
    }
    if template.is_empty() {
        return Ok(None);
    }
    let file = format!("{}ship.owshipdef", check_ship(template)?);
    String::from_utf8(read_asset(&file, &mut installed_archive()?)?)
        .map(Some)
        .map_err(|e| e.to_string())
}
fn mounts(def: &str) -> Vec<Mount> {
    let mut list = vec![];
    let head = def.split("[Ship]").next().unwrap_or("");
    for line in head
        .lines()
        .filter(|s| s.trim_start().starts_with("StartShipHardPointConfig("))
    {
        let args: Vec<_> = line.split('"').skip(1).step_by(2).collect();
        if args.len() != 6 {
            continue;
        }
        let authored = args[0].starts_with("OWMount_") && args[1] == format!("HP_{}", args[0]);
        let anchor = head
            .lines()
            .map(|l| l.split_whitespace().collect::<Vec<_>>())
            .find(|w| w.len() == 9 && w[0] == "hardpoint" && w[1] == args[1])
            .map(|w| w[2].to_string())
            .unwrap_or_else(|| format!("{}_Position", args[1]));
        list.push(Mount {
            slot: args[0].into(),
            asset: args[5].into(),
            anchor,
            authored,
        });
    }
    list
}
pub fn read_mounts(source: String) -> Result<Value, String> {
    let normalized = source.replace('\\', "/").to_lowercase();
    let parts: Vec<_> = normalized.split('/').rev().take(4).collect();
    let mut key = parts.iter().rev().copied().collect::<Vec<_>>().join("/");
    if check_ship(&key).is_err() {
        key.clear();
    }
    let text = source_text(&source, &key)?.unwrap_or_default();
    if let Some(saved) = text
        .lines()
        .find_map(|l| l.strip_prefix("; OpenHW HOD template "))
    {
        key = saved.trim().into();
    }
    Ok(json!({"template":key,"mounts":mounts(&text)}))
}
fn compile(root: &str, doc: &str) -> Result<String, String> {
    let fun = COMPLETE.get().ok_or("Gameplay compiler is unavailable")?;
    let root = CString::new(root).map_err(|e| e.to_string())?;
    let mut out = vec![0u8; 1024 * 1024 + 1];
    let mut error = [0i8; 512];
    if unsafe {
        fun(
            root.as_ptr(),
            doc.as_ptr().cast(),
            doc.len(),
            out.as_mut_ptr().cast(),
            out.len(),
            error.as_mut_ptr(),
            error.len(),
        )
    } == 0
    {
        return Err(unsafe { CStr::from_ptr(error.as_ptr()) }
            .to_string_lossy()
            .into_owned());
    }
    let len = out
        .iter()
        .position(|b| *b == 0)
        .ok_or("Invalid gameplay compiler output")?;
    out.truncate(len);
    String::from_utf8(out).map_err(|e| e.to_string())
}
// A new hull borrows the ship's simulation settings. Its geometry and weapons
// come from the new document rather than joints belonging to the template.
fn hull_settings(body: &str) -> String {
    let mut kept = Vec::new();
    let mut in_gun = false;
    let mut depth = 0i32;
    let mut opened = false;
    for row in body.lines() {
        let code = row
            .split(';')
            .next()
            .unwrap_or("")
            .split("//")
            .next()
            .unwrap_or("");
        let tag = code.split_whitespace().next();
        if !in_gun && tag == Some("GUN") {
            in_gun = true;
            opened = false;
        }
        if in_gun {
            for c in code.chars() {
                match c {
                    '{' => {
                        opened = true;
                        depth += 1;
                    }
                    '}' => depth -= 1,
                    _ => {}
                }
            }
            if opened && depth == 0 {
                in_gun = false;
            }
        } else if tag != Some("NUMBER_OF_GUNS") {
            kept.push(row);
        }
    }
    kept.join("\n")
}
pub fn save(
    target: String,
    source: String,
    model: HODModel,
    bindings: Vec<Mount>,
    template: String,
) -> Result<(), String> {
    let path = Path::new(&target);
    let Some(text) = source_text(&source, &template)? else {
        if bindings.iter().any(|m| m.authored) {
            return Err("Choose a gameplay template before saving subsystems".into());
        }
        return document::save(path, &source, model);
    };
    let custom = !source.starts_with("game://")
        && (source.is_empty() || !companion(Path::new(&source)).exists());
    let filename = path
        .file_name()
        .and_then(|n| n.to_str())
        .ok_or("Invalid HOD filename")?;
    if !filename
        .bytes()
        .all(|b| b.is_ascii_alphanumeric() || b"._-".contains(&b))
    {
        return Err(
            "Use letters, numbers, dots, underscores or hyphens in a gameplay HOD filename".into(),
        );
    }
    let normalized = text.replace("\r\n", "\n");
    let (head, body) = normalized
        .split_once("[Ship]\n")
        .ok_or("Unsupported ship definition format")?;
    // Preserve untouched ships, including their native mounts, on Save As.
    let old = mounts(head);
    let managed: BTreeSet<_> = old
        .iter()
        .filter(|m| m.authored)
        .map(|m| m.slot.as_str())
        .collect();
    let mut lines = Vec::new();
    for line in head.lines() {
        let w: Vec<_> = line.split_whitespace().collect();
        if custom
            && !matches!(
                w.first().copied(),
                Some("OPENHW_SHIP_2" | "type" | "scale" | "hull" | "mad")
            )
        {
            continue;
        }
        if w.len() > 1
            && ((["joint", "weaponSlot"].contains(&w[0]) && managed.contains(w[1]))
                || (w[0] == "hardpoint" && managed.contains(w[1].trim_start_matches("HP_")))
                || (line.starts_with("StartShipHardPointConfig(")
                    && line.split('"').nth(1).is_some_and(|s| managed.contains(s))))
        {
            continue;
        }
        if w.first() == Some(&"hull") {
            lines.push(format!("hull {filename}"));
        } else if w.first() == Some(&"mad") {
            let mad = if model.animations.is_empty() {
                "-".to_owned()
            } else {
                format!("{}.mad", path.file_stem().unwrap().to_string_lossy())
            };
            lines.push(format!("mad {mad}"));
        } else {
            lines.push(line.to_string());
        }
    }
    if custom {
        for i in 0..model.materials.len() {
            lines.push(format!("surface 0 {i} m{i}"));
        }
    }
    for mount in bindings.iter().filter(|m| m.authored) {
        if !id(&mount.slot) || !mount.slot.starts_with("OWMount_") || !id(&mount.asset) {
            return Err("Invalid subsystem placement".into());
        }
        if !model.joints.iter().any(|j| j.name == mount.slot) {
            return Err(format!("Missing mount joint {}", mount.slot));
        }
        let slot = &mount.slot;
        let role = if mount.asset.contains("engine") {
            "Engine"
        } else {
            "Weapon"
        };
        lines.push(format!("hardpoint HP_{slot} {slot} 0 0 0 0 0 0"));
        lines.push(format!("StartShipHardPointConfig(NewShipType, \"{slot}\", \"HP_{slot}\", \"{role}\", \"Innate\", \"Ship\", \"{}\");",mount.asset));
        let rig = component(mount.asset.clone())?;
        for (i, original) in rig.joints.iter().enumerate() {
            let name = format!("{slot}_J{i}");
            let j = model
                .joints
                .iter()
                .find(|j| j.name == name)
                .ok_or_else(|| format!("Subsystem joint {name} is missing"))?;
            let parent = match j.parent_name.as_deref() {
                Some(n) if n == slot => "-",
                Some(n) => {
                    let index = n
                        .strip_prefix(&format!("{slot}_J"))
                        .and_then(|v| v.parse::<usize>().ok())
                        .ok_or("Keep subsystem joints under their mount")?;
                    &rig.joints
                        .get(index)
                        .ok_or("Invalid subsystem joint parent")?
                        .name
                }
                None => return Err("Unparented subsystem joint".into()),
            };
            let m = &j.local_transform.m;
            let p = &m[3];
            let rot = j
                .rotation
                .as_ref()
                .map(|r| [r.x.to_degrees(), r.y.to_degrees(), r.z.to_degrees()])
                .unwrap_or([0.; 3]);
            lines.push(format!(
                "joint {slot} {} {parent} {} {} {} {} {} {}",
                original.name, p[0], p[1], p[2], rot[0], rot[1], rot[2]
            ));
        }
    }
    let body = body
        .lines()
        .filter(|l| !l.starts_with("; OpenHW HOD template "))
        .collect::<Vec<_>>()
        .join("\n");
    let body = if custom { hull_settings(&body) } else { body };
    let draft = format!(
        "{}\n[Ship]\n; OpenHW HOD template {template}\n{body}\n",
        lines.join("\n")
    );
    let definition = if bindings.iter().any(|m| m.authored) || !managed.is_empty() {
        compile(check_ship(&template)?, &draft)?
    } else {
        draft
    };
    document::save_with_definition(
        path,
        &source,
        model,
        Some((companion(path), definition.into_bytes())),
    )
}
