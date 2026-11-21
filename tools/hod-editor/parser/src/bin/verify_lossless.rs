use hwr_hod_parser::dae::parse_dae;
use hwr_hod_parser::hod::{generate_v2_from_model, save_edits, HODModel};
use std::fs;
use std::path::{Path, PathBuf};

fn main() {
    let [pebble_path, elysium_path, fenris_path, asteroid_path, dae_path, carrier_path] =
        resolve_fixture_paths();

    println!("--- Testing hgn_mothership (HOD) ---");
    let bytes = fs::read(carrier_path).unwrap();
    let model = HODModel::parse(&bytes).unwrap();
    println!("Parsed mothership! Dockpaths: {}", model.dockpaths.len());

    println!("--- Testing Pebble (HOD 2.0) ---");
    test_file(&pebble_path, true);

    println!("\n--- Testing Ter Elysium (HOD 2.0) ---");
    test_file(&elysium_path, true);

    println!("\n--- Testing Ter Fenris (HOD 2.0, With Animation) ---");
    test_file(&fenris_path, true);

    println!("\n--- Testing Asteroid 3 (HOD 1.0) ---");
    test_file(&asteroid_path, false);

    println!("\n--- Testing DAE Fallback ---");
    let dae_bytes = fs::read(dae_path).unwrap();
    let dae_str = String::from_utf8(dae_bytes).unwrap();
    let mut model = parse_dae(&dae_str).unwrap();
    model.is_v2 = true;
    let new_bytes = generate_v2_from_model(&[], &model).unwrap();
    println!(
        "DAE generation succeeded! Output size: {} bytes",
        new_bytes.len()
    );
}

fn test_file(path: &str, is_v2: bool) {
    let bytes = fs::read(path).unwrap();
    println!("Original size: {} bytes", bytes.len());

    let model = HODModel::parse(&bytes).unwrap();

    let new_bytes = if is_v2 {
        generate_v2_from_model(&bytes, &model).unwrap()
    } else {
        save_edits(&bytes, &model).unwrap()
    };

    let out_path = format!("{}_generated.hod", path);
    fs::write(&out_path, &new_bytes).unwrap();
    println!(
        "Generated size: {} bytes, saved to {}",
        new_bytes.len(),
        out_path
    );

    if bytes.len() == new_bytes.len() {
        println!("SUCCESS: Sizes match exactly!");

        let mut diffs = 0;
        for i in 0..bytes.len() {
            if bytes[i] != new_bytes[i] {
                diffs += 1;
            }
        }
        if diffs == 0 {
            println!("SUCCESS: Files are 100% byte-for-byte identical!");
        } else {
            println!("WARNING: Sizes match, but {} bytes differ.", diffs);
        }
    } else {
        println!(
            "FAILED: Sizes do not match! Diff: {}",
            new_bytes.len() as isize - bytes.len() as isize
        );
    }

    println!("Re-parsing generated file to verify structural integrity...");
    match HODModel::parse(&new_bytes) {
        Ok(reparsed) => {
            println!("SUCCESS: Re-parsed generated file!");
            println!(
                "  Original: Meshes={}, Joints={}, NavLights={}, Markers={}, EngineBurns={}",
                model.meshes.len(),
                model.joints.len(),
                model.nav_lights.len(),
                model.markers.len(),
                model.engine_burns.len()
            );
            println!(
                "  Reparsed: Meshes={}, Joints={}, NavLights={}, Markers={}, EngineBurns={}",
                reparsed.meshes.len(),
                reparsed.joints.len(),
                reparsed.nav_lights.len(),
                reparsed.markers.len(),
                reparsed.engine_burns.len()
            );
        }
        Err(e) => {
            println!("FAILED: Could not re-parse generated file! {}", e);
        }
    }
}

fn resolve_fixture_paths() -> [String; 6] {
    let keys = [
        "pebble_0.hod",
        "ter_elysium.hod",
        "ter_fenris.hod",
        "asteroid_3.hod",
        "galaxymapgalaxy.dae",
        "hgn_mothership.hod",
    ];
    let candidates = match std::env::var_os("HODE_LOCAL_ENV") {
        Some(path) => vec![PathBuf::from(path)],
        None => vec![
            PathBuf::from(".omo/local-environment.md"),
            PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../.omo/local-environment.md"),
        ],
    };

    let contents = candidates.iter().find_map(|path| fs::read_to_string(path).ok());
    let paths = contents.as_deref().map(|contents| {
        keys.map(|key| {
            contents.lines().find_map(|line| {
                let suffix = line.split_once(key)?.1;
                let suffix = suffix.strip_prefix('`').unwrap_or(suffix);
                let value = suffix.strip_prefix(':')?.trim().trim_matches('`').trim();
                (!value.is_empty()).then(|| value.to_owned())
            })
        })
    });

    let missing_keys: Vec<_> = keys
        .iter()
        .enumerate()
        .filter_map(|(index, key)| {
            paths
                .as_ref()
                .and_then(|paths| paths[index].as_ref())
                .is_none()
                .then_some(*key)
        })
        .collect();
    if !missing_keys.is_empty() {
        eprintln!("missing local-environment entries: {}", missing_keys.join(", "));
        eprintln!(
            "See docs/local-environment.md for the template (copy to .omo/local-environment.md and fill in your paths)."
        );
        std::process::exit(1);
    }

    let resolved: [String; 6] = match paths {
        Some(paths) => paths.map(|path| match path {
            Some(path) => path,
            None => std::process::exit(1),
        }),
        None => std::process::exit(1),
    };
    let missing_files: Vec<_> = resolved
        .iter()
        .filter(|path| !Path::new(path).exists())
        .collect();
    if !missing_files.is_empty() {
        for path in missing_files {
            eprintln!("fixture not found: {path}");
        }
        std::process::exit(1);
    }

    resolved
}
