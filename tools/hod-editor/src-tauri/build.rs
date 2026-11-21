use std::{env, fs, path::Path};

fn assets(dir: &Path, root: &Path, arms: &mut String) {
    for entry in fs::read_dir(dir).expect("Build the editor interface with npm run build first") {
        let path = entry.unwrap().path();
        if path.is_dir() {
            assets(&path, root, arms);
            continue;
        }
        let key = path
            .strip_prefix(root)
            .unwrap()
            .to_string_lossy()
            .replace('\\', "/");
        let mime = match path.extension().and_then(|v| v.to_str()) {
            Some("html") => "text/html",
            Some("js") => "application/javascript",
            Some("css") => "text/css",
            Some("svg") => "image/svg+xml",
            _ => "application/octet-stream",
        };
        arms.push_str(&format!(
            "{:?} => Some(({:?}, include_bytes!({:?}))),\n",
            key, mime, path
        ));
    }
}
fn main() {
    println!("cargo:rerun-if-changed=../../../src/Modern/ArchiveAssetAliases.inc");
    let aliases=fs::read_to_string("../../../src/Modern/ArchiveAssetAliases.inc").expect("Archive asset aliases");
    let mut rows=String::from("&[");
    for line in aliases.lines().filter(|s|s.starts_with('{')){
        let fields:Vec<_>=line.split('"').collect();
        assert_eq!(fields.len(),5);
        rows.push_str(&format!("({:?},{:?}),",fields[1],fields[3]));
    }
    rows.push(']');
    fs::write(Path::new(&env::var("OUT_DIR").unwrap()).join("archive_aliases.rs"),rows).unwrap();
    let root = Path::new("../dist")
        .canonicalize()
        .expect("Build the editor interface first");
    println!("cargo:rerun-if-changed=../dist");
    let mut arms = String::from(
        "fn asset(key: &str) -> Option<(&'static str, &'static [u8])> { match key {\n",
    );
    assets(&root, &root, &mut arms);
    arms.push_str("_ => None } }");
    fs::write(
        Path::new(&env::var("OUT_DIR").unwrap()).join("assets.rs"),
        arms,
    )
    .unwrap();
}
