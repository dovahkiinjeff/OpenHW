use hwr_hod_parser::hod::{generate_v2_from_model, serialize_mad_companion, HODModel};
use hwr_hod_parser::iff::IffChunk;
use std::fs;
use std::io::{self, Write};
use std::path::Path;
use std::sync::Mutex;
use tempfile::NamedTempFile;

#[derive(Default)]
pub struct SourceDocument {
    pub name: String,
    pub bytes: Vec<u8>,
    pub chunks: Vec<IffChunk>,
}
pub static SOURCE: Mutex<Option<SourceDocument>> = Mutex::new(None);

fn stage(path: &Path, bytes: &[u8]) -> io::Result<NamedTempFile> {
    let mut file = NamedTempFile::new_in(
        path.parent()
            .ok_or_else(|| io::Error::other("Missing output folder"))?,
    )?;
    file.write_all(bytes)?;
    file.as_file().sync_all()?;
    Ok(file)
}

pub fn save(target: &Path, source: &str, model: HODModel) -> Result<(), String> {
    save_with_definition(target, source, model, None)
}
pub fn save_with_definition(
    target: &Path,
    source: &str,
    mut model: HODModel,
    definition: Option<(std::path::PathBuf, Vec<u8>)>,
) -> Result<(), String> {
    if !target
        .extension()
        .and_then(|v| v.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("hod"))
    {
        return Err("Choose a .hod file for this model".into());
    }
    let guard = SOURCE.lock().map_err(|_| "Reopen the source model")?;
    let bytes = if source.is_empty() {
        &[][..]
    } else {
        let src = guard
            .as_ref()
            .filter(|s| s.name == source)
            .ok_or("The source document changed. Reopen the model.")?;
        // The serializer rebuilds collision geometry and its indices together.
        // Restore metadata that the upstream UI model excludes from JSON.
        model.preserved_chunks = src
            .chunks
            .iter()
            .filter(|c| c.id != "COLD" && c.id != "KDOP")
            .cloned()
            .collect();
        src.bytes.as_slice()
    };
    model.version = 512;
    model.is_v2 = true;
    let hod = generate_v2_from_model(bytes, &model)?;
    let mad = if model.animations.is_empty() {
        None
    } else {
        Some(serialize_mad_companion(&model)?)
    };
    let Some((path, def)) = definition else {
        return commit_files(target, &hod, mad.as_deref()).map_err(|e| e.to_string());
    };
    let previous = match fs::read(&path) {
        Ok(data) => Some(stage(&path, &data).map_err(|e| e.to_string())?),
        Err(e) if e.kind() == io::ErrorKind::NotFound => None,
        Err(e) => return Err(e.to_string()),
    };
    stage(&path, &def)
        .map_err(|e| e.to_string())?
        .persist(&path)
        .map_err(|e| e.to_string())?;
    if let Err(error) = commit_files(target, &hod, mad.as_deref()) {
        if let Some(file) = previous {
            file.persist(&path).map_err(|e| e.to_string())?;
        } else {
            fs::remove_file(&path).map_err(|e| e.to_string())?;
        }
        return Err(error.to_string());
    }
    Ok(())
}

fn commit_files(target: &Path, hod: &[u8], mad: Option<&[u8]>) -> io::Result<()> {
    let hod_file = stage(target, hod)?;
    let companion = target.with_extension("mad");
    let mad_file = mad.map(|data| stage(&companion, data)).transpose()?;
    // Stage both outputs before changing either. Restore the previous companion
    // if the final atomic HOD replacement fails (for example a locked file).
    let previous = match fs::read(&companion) {
        Ok(bytes) => Some(stage(&companion, &bytes)?),
        Err(e) if e.kind() == io::ErrorKind::NotFound => None,
        Err(e) => return Err(e),
    };
    if let Some(file) = mad_file {
        file.persist(&companion)?;
    } else if previous.is_some() {
        fs::remove_file(&companion)?;
    }
    if let Err(failure) = hod_file.persist(target) {
        if let Some(file) = previous {
            file.persist(&companion)?;
        } else if mad.is_some() {
            fs::remove_file(&companion)?;
        }
        return Err(failure.error);
    }
    Ok(())
}
