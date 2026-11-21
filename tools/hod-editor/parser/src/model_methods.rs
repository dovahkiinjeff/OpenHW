use crate::math::*;
use crate::types::*;

const THRUSTER_SLOT_ALIASES: [&str; 6] = [
    "$diffuseOn",
    "$diffuseOff",
    "$glowOn",
    "$glowOff",
    "$team",
    "$normal",
];

/// Rebuild a thruster material's texture slots from its parsed type-5 aliases.
/// The STAT alias order is authoritative even when two slots intentionally
/// share one texture (e.g. Team reusing the DIFX map). Returns None when the
/// names are stale/invalid or the texture list does not line up, in which case
/// the caller falls back to filename-role matching.
fn canonical_thruster_texture_maps(mat: &HODMaterial) -> Option<Vec<String>> {
    let parameter_names: Vec<&str> = mat
        .params
        .iter()
        .filter(|param| param.param_type == 5)
        .map(|param| param.name.as_str())
        .collect();
    if parameter_names.is_empty() || parameter_names.len() != mat.texture_maps.len() {
        return None;
    }

    let mut mapped = vec![String::new(); THRUSTER_SLOT_ALIASES.len()];
    let mut previous_index: Option<usize> = None;
    for (name, texture_name) in parameter_names
        .iter()
        .zip(mat.texture_maps.iter())
    {
        let index = THRUSTER_SLOT_ALIASES
            .iter()
            .position(|alias| alias == name)?;
        if previous_index.map_or(false, |previous| index <= previous) {
            return None;
        }
        previous_index = Some(index);
        mapped[index] = texture_name.clone();
    }
    Some(mapped)
}

pub fn normalize_collision_meshes(model: &mut HODModel) {
    if model.collision_meshes.is_empty() {
        return;
    }

    model.collision_meshes.truncate(1);
    let collision = &mut model.collision_meshes[0];
    collision.name = "Root".to_string();
    collision.mesh.name = "CollisionMesh".to_string();
    collision.mesh.parent_name = "Root".to_string();
}

impl HODModel {
    pub fn auto_repair_assembly_names(&mut self) {
        let mut renames: std::collections::HashMap<String, String> =
            std::collections::HashMap::new();
        // Weapon/hardpoint assemblies use `_Position`/`_Direction`/`_Rest`.
        // Latch points (CapturePoint/RepairPoint/SalvagePoint, any name passed
        // to loadLatchPointList) are the opposite: the engine resolves
        // `<base>Heading`/`<base>Left`/`<base>Up` with NO underscore, and no
        // vanilla HOD has `_Heading`/`_Left`/`_Up`. Inserting the underscore
        // there made the game ignore every repair/salvage/capture point.
        let suffixes = ["Position", "Direction", "Rest"];
        let latch_suffixes = ["Heading", "Left", "Up"];
        let joint_names: std::collections::HashSet<String> =
            self.joints.iter().map(|j| j.name.to_lowercase()).collect();

        for joint in &self.joints {
            let name = &joint.name;
            // Undo the underscore older editor builds wrote on latch children.
            // Only for joints parented to their own `<base>` joint, so weapon
            // turrets (`Weapon_X_Heading` under `Weapon_X_Position`) and
            // unrelated names are left alone.
            for suffix in &latch_suffixes {
                let underscored = format!("_{}", suffix);
                if !name.ends_with(&underscored) {
                    continue;
                }
                let base = &name[..name.len() - underscored.len()];
                let parent_is_base = joint
                    .parent_name
                    .as_deref()
                    .map_or(false, |parent| parent.eq_ignore_ascii_case(base));
                let fixed = format!("{}{}", base, suffix);
                if !base.is_empty()
                    && parent_is_base
                    && !base.to_lowercase().starts_with("weapon_")
                    && !joint_names.contains(&fixed.to_lowercase())
                {
                    renames.insert(name.clone(), fixed);
                }
            }
            for suffix in &suffixes {
                if name.ends_with(suffix) && !name.ends_with(&format!("_{}", suffix)) {
                    let base = &name[..name.len() - suffix.len()];
                    if !base.is_empty() && !base.ends_with('_') {
                        let new_name = format!("{}_{}", base, suffix);
                        renames.insert(name.clone(), new_name);
                    }
                }
            }
            if let Some(idx) = name.find("Muzzle") {
                if idx > 0 && name.as_bytes()[idx - 1] != b'_' {
                    let base = &name[..idx];
                    let suffix = &name[idx..];
                    if !base.is_empty() && !base.ends_with('_') {
                        let new_name = format!("{}_{}", base, suffix);
                        renames.insert(name.clone(), new_name);
                    }
                }
            }
        }

        if renames.is_empty() {
            return;
        }

        for joint in &mut self.joints {
            if let Some(new_name) = renames.get(&joint.name) {
                joint.name = new_name.clone();
            }
            if let Some(parent_name) = &joint.parent_name {
                if let Some(new_parent) = renames.get(parent_name) {
                    joint.parent_name = Some(new_parent.clone());
                }
            }
        }

        for mesh in &mut self.meshes {
            if let Some(new_name) = renames.get(&mesh.parent_name) {
                mesh.parent_name = new_name.clone();
            }
        }

        for marker in &mut self.markers {
            if let Some(new_parent) = renames.get(&marker.parent_joint) {
                marker.parent_joint = new_parent.clone();
            }
        }

        for dockpath in &mut self.dockpaths {
            if let Some(new_parent) = renames.get(&dockpath.parent_name) {
                dockpath.parent_name = new_parent.clone();
            }
        }

        for anim in &mut self.animations {
            for track in &mut anim.tracks {
                if let Some(new_name) = renames.get(&track.joint_name) {
                    track.joint_name = new_name.clone();
                }
            }
        }
    }

    pub fn convert_weapon_to_turret(&mut self, base_name: &str) -> Result<(), String> {
        let pos_name = format!("{}_Position", base_name);

        let pos_exists = self
            .joints
            .iter()
            .any(|j| j.name.eq_ignore_ascii_case(&pos_name));
        if !pos_exists {
            return Err("Weapon assembly missing _Position joint, cannot convert.".to_string());
        }

        let dir_name = format!("{}_Direction", base_name);
        let head_name = format!("{}_Heading", base_name);

        let mut has_heading = self
            .joints
            .iter()
            .any(|j| j.name.eq_ignore_ascii_case(&head_name));
        if !has_heading {
            // Rename _Direction to _Heading
            if let Some(dir_joint) = self
                .joints
                .iter_mut()
                .find(|j| j.name.eq_ignore_ascii_case(&dir_name))
            {
                dir_joint.name = head_name.clone();
                has_heading = true;
            }
        }

        if !has_heading {
            let new_heading = HODJoint {
                name: head_name.clone(),
                parent_name: Some(pos_name.clone()),
                local_transform: compose_transform_matrix(
                    Vector3 {
                        x: 0.0,
                        y: 0.0,
                        z: 5.0,
                    },
                    Vector3 {
                        x: 0.0,
                        y: 0.0,
                        z: 0.0,
                    },
                    Vector3 {
                        x: 1.0,
                        y: 1.0,
                        z: 1.0,
                    },
                ),
                position: None,
                rotation: None,
                scale: None,
            };
            self.joints.push(new_heading);
        }

        // Make sure any children that used Direction as parent now use Heading
        for joint in &mut self.joints {
            if let Some(parent) = &joint.parent_name {
                if parent.eq_ignore_ascii_case(&dir_name) {
                    joint.parent_name = Some(head_name.clone());
                }
            }
        }
        for mesh in &mut self.meshes {
            if mesh.parent_name.eq_ignore_ascii_case(&dir_name) {
                mesh.parent_name = head_name.clone();
            }
        }

        let muzzle_name = format!("{}_Muzzle", base_name);
        let has_muzzle = self
            .joints
            .iter()
            .any(|j| j.name.eq_ignore_ascii_case(&muzzle_name));
        if !has_muzzle {
            let new_muzzle = HODJoint {
                name: muzzle_name,
                parent_name: Some(head_name), // Parented to Heading
                local_transform: compose_transform_matrix(
                    Vector3 {
                        x: 0.0,
                        y: 0.0,
                        z: 5.0,
                    },
                    Vector3 {
                        x: 0.0,
                        y: 0.0,
                        z: 0.0,
                    },
                    Vector3 {
                        x: 1.0,
                        y: 1.0,
                        z: 1.0,
                    },
                ),
                position: None,
                rotation: None,
                scale: None,
            };
            self.joints.push(new_muzzle);
        }

        Ok(())
    }

    pub fn upgrade_v1_to_v2(&mut self) {
        if self.is_v2 {
            return;
        }

        // HOD 1.0 (VERS 1000) and HOD 2.0 (VERS 1001) both use meters.
        // We do NOT need to scale positions!
        // The only incompatibility is that HOD 2.0 uses joint scale vectors as gimbal limits.
        // HW2 Classic exported (1.0, 1.0, 1.0) for scale, which HWRM interprets as a 1.0 radian gimbal limit,
        // causing severe coordinate space distortion and rotation.

        for joint in &mut self.joints {
            // In HOD 2.0, the scale field acts as gimbal limits, not geometry scale.
            // Using (1,1,1) in HOD 2.0 causes the engine to rotate the ship sideways.
            joint.scale = Some(Vector3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            });

            // Recompose matrix with the zeroed scale
            if let (Some(ref pos), Some(ref rot), Some(ref scale)) =
                (&joint.position, &joint.rotation, &joint.scale)
            {
                joint.local_transform =
                    compose_transform_matrix(pos.clone(), rot.clone(), scale.clone());
            }
        }

        self.is_v2 = true;
    }

    pub fn clean_hierarchy(&mut self) {
        // Special prefixes that shouldn't have arbitrary children
        let special_prefixes = ["NAVL[", "BURN[", "MARK[", "MULT[", "COL[", "SHAP[", "GLOW["];

        // Loop multiple times in case of deep invalid nesting
        let mut changed = true;
        while changed {
            changed = false;
            let mut updates = Vec::new(); // (child_idx, new_parent_name, new_transform)

            for (i, joint) in self.joints.iter().enumerate() {
                if let Some(parent_name) = &joint.parent_name {
                    // Check if parent name indicates a special node
                    let is_invalid_parent = special_prefixes
                        .iter()
                        .any(|&p| parent_name.starts_with(p) || parent_name == &p[..p.len() - 1]);

                    // Flame joints are allowed under BURN
                    let is_allowed_flame =
                        parent_name.starts_with("BURN[") && joint.name.starts_with("Flame[");

                    if is_invalid_parent && !is_allowed_flame {
                        // Find the parent joint to extract its transform and its parent
                        if let Some(parent_joint) =
                            self.joints.iter().find(|j| j.name == *parent_name)
                        {
                            let new_parent_name = parent_joint.parent_name.clone();
                            // Multiply parent's local transform by child's local transform
                            // In Homeworld matrices, transform = parent * child
                            let p_mat = &parent_joint.local_transform.m;
                            let c_mat = &joint.local_transform.m;

                            let mut new_transform = Matrix4 { m: [[0.0; 4]; 4] };
                            for r in 0..4 {
                                for c in 0..4 {
                                    new_transform.m[r][c] = p_mat[r][0] * c_mat[0][c]
                                        + p_mat[r][1] * c_mat[1][c]
                                        + p_mat[r][2] * c_mat[2][c]
                                        + p_mat[r][3] * c_mat[3][c];
                                }
                            }

                            updates.push((i, new_parent_name, new_transform));
                            changed = true;
                        }
                    }
                }
            }

            // Apply updates
            for (idx, new_parent, new_transform) in updates {
                self.joints[idx].parent_name = new_parent;
                self.joints[idx].local_transform = new_transform.clone();
                // Update stored position/rotation/scale to match the cleaned transform
                let (pos, rot, scale) = decompose_matrix(new_transform);
                self.joints[idx].position = Some(pos);
                self.joints[idx].rotation = Some(rot);
                self.joints[idx].scale = Some(scale);
            }

            // --- FUNCTIONAL NODES CLEANUP ---
            // If any functional node has its parent_name pointing to a special endpoint, we un-nest it.
            // Functional nodes do not carry a full hierarchical local_transform matrix to adapt in the same way,
            // they simply inherit their parent's absolute space.

            let mut check_and_update = |parent_name_ref: &mut String| {
                if special_prefixes.iter().any(|&p| {
                    parent_name_ref.starts_with(p) || parent_name_ref == &p[..p.len() - 1]
                }) {
                    if let Some(parent_joint) =
                        self.joints.iter().find(|j| &j.name == parent_name_ref)
                    {
                        if let Some(new_p) = &parent_joint.parent_name {
                            *parent_name_ref = new_p.clone();
                            changed = true;
                        }
                    }
                }
            };

            for burn in &mut self.engine_burns {
                check_and_update(&mut burn.parent_name);
            }
            for glow in &mut self.engine_glows {
                check_and_update(&mut glow.parent_name);
            }
            for shape in &mut self.engine_shapes {
                check_and_update(&mut shape.parent_name);
            }
            for path in &mut self.dockpaths {
                check_and_update(&mut path.parent_name);
            }
            for mesh in &mut self.meshes {
                check_and_update(&mut mesh.parent_name);
            }
            for marker in &mut self.markers {
                check_and_update(&mut marker.parent_joint);
            }
        }

        // Remove self-parented identity joints that duplicate a real joint name.
        // These are NavLight endpoint artifacts in some HOD 2.0 files (e.g., EngineNozzle1-4).
        let mut names_with_real_joint = std::collections::HashSet::new();
        for joint in &self.joints {
            let is_self_parented = joint.parent_name.as_deref() == Some(&joint.name);
            if !is_self_parented {
                names_with_real_joint.insert(joint.name.clone());
            }
        }
        self.joints.retain(|j| {
            let is_self_parented = j.parent_name.as_deref() == Some(&j.name);
            if is_self_parented && names_with_real_joint.contains(&j.name) {
                println!(
                    "[RUST] clean_hierarchy: Removing duplicate self-parented joint '{}'",
                    j.name
                );
                return false;
            }
            true
        });
    }

    pub fn deduplicate_names(&mut self) {
        use std::collections::HashMap;

        let mut joint_names = HashMap::new();
        let mut renames = HashMap::new(); // Old Name -> New Name

        // First pass: Rename duplicate joints
        for joint in &mut self.joints {
            let count = joint_names.entry(joint.name.clone()).or_insert(0);
            *count += 1;
            if *count > 1 {
                let new_name = format!("{}_{}", joint.name, count);
                renames.insert(joint.name.clone(), new_name.clone());
                println!(
                    "[RUST] WARNING: Duplicate joint name '{}' found. Renamed to '{}'.",
                    joint.name, new_name
                );
                joint.name = new_name;
            }
        }

        let mut mesh_names = HashMap::new();
        for mesh in &mut self.meshes {
            // Use (name, lod) as key — LOD variants with the same name
            // are not duplicates, they're separate LODs of the same mesh.
            let key = format!("{}_{}", mesh.name, mesh.lod);
            let count = mesh_names.entry(key).or_insert(0);
            *count += 1;
            if *count > 1 {
                let new_name = format!("{}_{}", mesh.name, count);
                println!(
                    "[RUST] WARNING: Duplicate mesh name '{}' lod {} found. Renamed to '{}'.",
                    mesh.name, mesh.lod, new_name
                );
                mesh.name = new_name;
            }
        }
    }

    pub fn auto_assign_and_resize_textures(&mut self) {
        use base64::{engine::general_purpose, Engine as _};
        use image::imageops::FilterType;
        use image::io::Reader as ImageReader;
        use std::collections::HashMap;
        use std::io::Cursor;

        let mut textures_by_name = HashMap::new();
        for t in &self.textures {
            let lower = t
                .name
                .to_lowercase()
                .replace(".tga", "")
                .replace(".png", "")
                .replace(".dds", "");
            textures_by_name.insert(lower, t.clone());
        }

        let mut updated_textures = Vec::new();
        let mut texture_names_used = Vec::new();

        for mat in &mut self.materials {
            let shader = mat.shader_name.to_lowercase();
            let mut expected_slots = vec![
                "Diffuse Map (DIFF)",
                "Glow Map (GLOW)",
                "Team Paint Map (TEAM)",
                "Normal Map (NORM)",
            ];

            // `badge` is the ship shader plus a decal pass: the badge image
            // comes from the player's team colour at runtime, never from the
            // HOD, and the decal is only blended where $team marks paint. It
            // needs the full ship slot set; trimming it to $diffuse made every
            // saved badge invisible in game. Keep these aligned with
            // `shader_slot_table` (serialize.rs) and `SHADER_SLOTS`
            // (src/texture_utils.ts). Vanilla sweep: 97 `badge` STATs use
            // $diffuse/$glow/$team/$normal; the one `badgeglow`
            // (kus_mothership) uses $diffuse/$spec/$glow/$team/$normal.
            if shader.contains("badgeglow") {
                expected_slots = vec![
                    "Diffuse Map (DIFF)",
                    "Specular Map (SPEC)",
                    "Glow Map (GLOW)",
                    "Team Paint Map (TEAM)",
                    "Normal Map (NORM)",
                ];
            } else if shader.contains("badge") {
                expected_slots = vec![
                    "Diffuse Map (DIFF)",
                    "Glow Map (GLOW)",
                    "Team Paint Map (TEAM)",
                    "Normal Map (NORM)",
                ];
            } else if shader.contains("thruster") {
                // Canonical STAT slot order: on-state diffuse, off-state
                // diffuse, on-state glow, off-state glow, team, normal.
                // Keep this aligned with `SHADER_SLOTS.thruster` in
                // `src/texture_utils.ts`; the Inspector indexes texture_maps
                // by this order and the exporter names by the same index.
                expected_slots = vec![
                    "Diffuse On (DIFF)",
                    "Diffuse Off (DIFX)",
                    "Glow On (GLOW)",
                    "Glow Off (GLOX)",
                    "Team Paint Map (TEAM)",
                    "Normal Map (NORM)",
                ];
            } else if shader.contains("ship") {
                expected_slots = vec![
                    "Diffuse Map (DIFF)",
                    "Glow Map (GLOW)",
                    "Team Paint Map (TEAM)",
                    "Normal Map (NORM)",
                    "Specular Map (SPEC)",
                ];
            } else if shader.contains("asteroid") {
                expected_slots = vec![
                    "Diffuse Map (DIFF)",
                    "Normal Map (NORM)",
                    "Specular Map (SPEC)",
                ];
            } else if shader.contains("cloud") || shader.contains("background") {
                expected_slots = vec!["Diffuse Map (DIFF)"];
            } else if shader.contains("resource") {
                expected_slots = vec![
                    "Diffuse Map (DIFF)",
                    "Glow Map (GLOW)",
                    "Normal Map (NORM)",
                    "Specular Map (SPEC)",
                ];
            }

            let canonical_thruster_maps = if shader.contains("thruster") {
                canonical_thruster_texture_maps(mat)
            } else {
                None
            };

            let mut mapped = Vec::new();
            let mut base_width = 0;
            let mut base_height = 0;

            for (idx, slot) in expected_slots.iter().enumerate() {
                let mut best_match = canonical_thruster_maps
                    .as_ref()
                    .and_then(|canonical| canonical.get(idx))
                    .cloned()
                    .unwrap_or_default();

                let is_match = |t_name: &str, s_name: &str| -> bool {
                    let mut tn = t_name.to_lowercase();
                    if tn.ends_with(".tga") {
                        tn.truncate(tn.len() - 4);
                    }
                    if tn.ends_with(".png") {
                        tn.truncate(tn.len() - 4);
                    }
                    if tn.ends_with(".dds") {
                        tn.truncate(tn.len() - 4);
                    }

                    // Off-state thruster roles must be matched before the
                    // generic DIFF/GLOW roles, otherwise `_DIFX`/`_GLOX`
                    // textures are never selected and the on-state texture
                    // is assigned twice.
                    if s_name.contains("DIFX")
                        && (tn.contains("_difx")
                            || tn.contains("difx")
                            || tn.contains("diff_off")
                            || tn.contains("diffoff"))
                    {
                        return true;
                    }
                    if s_name.contains("GLOX")
                        && (tn.contains("_glox")
                            || tn.contains("glox")
                            || tn.contains("glow_off")
                            || tn.contains("glowoff")
                            || tn.contains("glowx"))
                    {
                        return true;
                    }
                    if s_name.contains("GLOW")
                        && !tn.contains("glox")
                        && !tn.contains("glow_off")
                        && !tn.contains("glowoff")
                        && !tn.contains("glowx")
                        && (tn.contains("_glow") || tn.contains("glow"))
                    {
                        return true;
                    }
                    if s_name.contains("TEAM") && (tn.contains("_team") || tn.contains("team")) {
                        return true;
                    }
                    if s_name.contains("NORM") && (tn.contains("_norm") || tn.contains("norm")) {
                        return true;
                    }
                    if s_name.contains("SPEC") && (tn.contains("_spec") || tn.contains("spec")) {
                        return true;
                    }
                    if s_name.contains("DIFF")
                        && !tn.contains("difx")
                        && !tn.contains("diff_off")
                        && !tn.contains("diffoff")
                        && !tn.contains("glow")
                        && !tn.contains("team")
                        && !tn.contains("norm")
                        && !tn.contains("spec")
                    {
                        return true;
                    }
                    false
                };

                if canonical_thruster_maps.is_none() {
                    for t_name in &mat.texture_maps {
                        if is_match(t_name, slot) {
                            best_match = t_name.clone();
                            break;
                        }
                    }
                }

                if canonical_thruster_maps.is_none() && best_match.is_empty() {
                    let mut expected_suffix = "";
                    if slot.contains("DIFX") {
                        expected_suffix = "_difx";
                    }
                    if slot.contains("GLOX") {
                        expected_suffix = "_glox";
                    }
                    if slot.contains("GLOW") {
                        expected_suffix = "_glow";
                    }
                    if slot.contains("TEAM") {
                        expected_suffix = "_team";
                    }
                    if slot.contains("NORM") {
                        expected_suffix = "_norm";
                    }
                    if slot.contains("SPEC") {
                        expected_suffix = "_spec";
                    }
                    if slot.contains("DIFF") {
                        expected_suffix = "_diff";
                    }

                    if !expected_suffix.is_empty() {
                        let potential_name = format!("{}{}", mat.name, expected_suffix);
                        if textures_by_name.contains_key(&potential_name.to_lowercase()) {
                            best_match = textures_by_name[&potential_name.to_lowercase()]
                                .name
                                .clone();
                        }
                    }
                }

                mapped.push(best_match.clone());

                if !best_match.is_empty() {
                    let tn_lower = best_match
                        .to_lowercase()
                        .replace(".tga", "")
                        .replace(".png", "");
                    if let Some(tex) = textures_by_name.get(&tn_lower) {
                        if idx == 0 || (base_width == 0 && base_height == 0) {
                            base_width = tex.width;
                            base_height = tex.height;
                        }

                        let mut new_tex = tex.clone();
                        if (new_tex.width != base_width || new_tex.height != base_height)
                            && base_width > 0
                            && base_height > 0
                        {
                            println!(
                                "[RUST] Autofixing texture '{}' dimensions from {}x{} to {}x{}",
                                new_tex.name,
                                new_tex.width,
                                new_tex.height,
                                base_width,
                                base_height
                            );
                            if let Some(png_data) = &new_tex.png_data {
                                if let Ok(decoded) = general_purpose::STANDARD.decode(png_data) {
                                    if let Ok(img) = ImageReader::new(Cursor::new(decoded))
                                        .with_guessed_format()
                                        .unwrap()
                                        .decode()
                                    {
                                        let resized = img.resize_exact(
                                            base_width,
                                            base_height,
                                            FilterType::Lanczos3,
                                        );
                                        let mut out_bytes = Vec::new();
                                        if resized
                                            .write_to(
                                                &mut Cursor::new(&mut out_bytes),
                                                image::ImageFormat::Png,
                                            )
                                            .is_ok()
                                        {
                                            new_tex.png_data =
                                                Some(general_purpose::STANDARD.encode(&out_bytes));

                                            let prev_resized = img.resize_exact(
                                                128.min(base_width),
                                                128.min(base_height),
                                                FilterType::Lanczos3,
                                            );
                                            let mut prev_bytes = Vec::new();
                                            if prev_resized
                                                .write_to(
                                                    &mut Cursor::new(&mut prev_bytes),
                                                    image::ImageFormat::Png,
                                                )
                                                .is_ok()
                                            {
                                                new_tex.png_preview = Some(
                                                    general_purpose::STANDARD.encode(&prev_bytes),
                                                );
                                            }

                                            new_tex.width = base_width;
                                            new_tex.height = base_height;
                                        }
                                    }
                                }
                            }
                        }

                        if !texture_names_used.contains(&new_tex.name) {
                            texture_names_used.push(new_tex.name.clone());
                            updated_textures.push(new_tex);
                        }
                    }
                }
            }

            mat.texture_maps = mapped;
        }

        for t in &self.textures {
            if !texture_names_used.contains(&t.name) {
                updated_textures.push(t.clone());
            }
        }

        self.textures = updated_textures;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn identity_matrix() -> Matrix4 {
        Matrix4 {
            m: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        }
    }

    #[test]
    fn model_methods_deduplicate_names_makes_unique() {
        let mut model = HODModel::new();
        model.joints = vec![
            HODJoint {
                name: "A".to_string(),
                parent_name: None,
                local_transform: identity_matrix(),
                position: None,
                rotation: None,
                scale: None,
            },
            HODJoint {
                name: "A".to_string(),
                parent_name: None,
                local_transform: identity_matrix(),
                position: None,
                rotation: None,
                scale: None,
            },
        ];

        model.deduplicate_names();

        assert_eq!(model.joints[0].name, "A");
        assert_eq!(model.joints[1].name, "A_2");
    }

    #[test]
    fn model_methods_clean_hierarchy_unnests_special_prefixes() {
        let mut model = HODModel::new();
        model.joints = vec![
            HODJoint {
                name: "Root".to_string(),
                parent_name: None,
                local_transform: identity_matrix(),
                position: None,
                rotation: None,
                scale: None,
            },
            HODJoint {
                name: "COL[0]".to_string(),
                parent_name: Some("Root".to_string()),
                local_transform: identity_matrix(),
                position: None,
                rotation: None,
                scale: None,
            },
            HODJoint {
                name: "NAVL[0]".to_string(),
                parent_name: Some("COL[0]".to_string()),
                local_transform: identity_matrix(),
                position: None,
                rotation: None,
                scale: None,
            },
        ];

        model.clean_hierarchy();

        assert_eq!(model.joints[2].parent_name.as_deref(), Some("Root"));
    }

    #[test]
    fn auto_assign_preserves_canonical_thruster_slot_order() {
        // The loader runs auto_assign on every HOD open. Thruster materials
        // must come back in STAT order (DIFF, DIFX, GLOW, GLOX, TEAM, NORM);
        // the old table duplicated DIFF/GLOW and dropped the off-state maps,
        // which then made the exporter pair values with the wrong aliases.
        let names = [
            "Engine_DIFF",
            "Engine_DIFX",
            "Engine_GLOW",
            "Engine_GLOX",
            "Engine_TEAM",
            "Engine_NORM",
        ];
        let mut model = HODModel::new();
        model.textures = names
            .iter()
            .map(|name| HODTexture {
                name: name.to_string(),
                width: 1,
                height: 1,
                format: "DXT1".to_string(),
                png_preview: None,
                png_data: None,
                source_path: None,
            })
            .collect();
        model.materials = vec![HODMaterial {
            name: "Engine".to_string(),
            shader_name: "thruster".to_string(),
            // Deliberately scrambled input: role matching, not input order,
            // must produce the canonical layout.
            texture_maps: vec![
                "Engine_NORM".to_string(),
                "Engine_DIFF".to_string(),
                "Engine_GLOX".to_string(),
                "Engine_TEAM".to_string(),
                "Engine_DIFX".to_string(),
                "Engine_GLOW".to_string(),
            ],
            parameters: Vec::new(),
            params: Vec::new(),
        }];

        model.auto_assign_and_resize_textures();

        assert_eq!(
            model.materials[0].texture_maps,
            names.iter().map(|name| name.to_string()).collect::<Vec<_>>()
        );
    }

    #[test]
    fn auto_assign_preserves_shared_thruster_bindings() {
        // Some thruster materials intentionally point Team at the DIFX map
        // and GlowOff at the DIFF map. The canonical alias order still tells
        // us which slot each texture belongs to, so auto_assign must not
        // drop the duplicate bindings nor reorder them by filename role.
        let texture_names = ["Engine_DIFF", "Engine_DIFX", "Engine_GLOW"];
        let slot_names = [
            "$diffuseOn",
            "$diffuseOff",
            "$glowOn",
            "$glowOff",
            "$team",
        ];
        let slot_values = [0u32, 1, 2, 0, 1];
        let mut model = HODModel::new();
        model.textures = texture_names
            .iter()
            .map(|name| HODTexture {
                name: name.to_string(),
                width: 1,
                height: 1,
                format: "DXT1".to_string(),
                png_preview: None,
                png_data: None,
                source_path: None,
            })
            .collect();
        model.materials = vec![HODMaterial {
            name: "Engine".to_string(),
            shader_name: "thruster".to_string(),
            texture_maps: vec![
                "Engine_DIFF".to_string(),
                "Engine_DIFX".to_string(),
                "Engine_GLOW".to_string(),
                "Engine_DIFF".to_string(),
                "Engine_DIFX".to_string(),
            ],
            parameters: Vec::new(),
            params: slot_names
                .iter()
                .zip(slot_values.iter())
                .map(|(name, value)| HODMaterialParam {
                    param_type: 5,
                    value: value.to_le_bytes().to_vec(),
                    name: name.to_string(),
                })
                .collect(),
        }];

        model.auto_assign_and_resize_textures();

        assert_eq!(
            model.materials[0].texture_maps,
            vec![
                "Engine_DIFF".to_string(),
                "Engine_DIFX".to_string(),
                "Engine_GLOW".to_string(),
                "Engine_DIFF".to_string(),
                "Engine_DIFX".to_string(),
                String::new(),
            ]
        );
    }

    fn joint(name: &str, parent: Option<&str>) -> HODJoint {
        HODJoint {
            name: name.to_string(),
            parent_name: parent.map(|p| p.to_string()),
            local_transform: identity_matrix(),
            position: None,
            rotation: None,
            scale: None,
        }
    }

    #[test]
    fn auto_repair_keeps_engine_latch_point_names() {
        // loadLatchPointList resolves `<base>Heading/Left/Up` without an
        // underscore; vanilla hgn_destroyer uses exactly these names.
        let mut model = HODModel::new();
        model.joints = vec![
            joint("Root", None),
            joint("RepairPoint0", Some("Root")),
            joint("RepairPoint0Heading", Some("RepairPoint0")),
            joint("RepairPoint0Left", Some("RepairPoint0")),
            joint("RepairPoint0Up", Some("RepairPoint0")),
            joint("Weapon_GunPosition", Some("Root")),
        ];

        model.auto_repair_assembly_names();

        let names: Vec<&str> = model.joints.iter().map(|j| j.name.as_str()).collect();
        assert_eq!(
            names,
            vec![
                "Root",
                "RepairPoint0",
                "RepairPoint0Heading",
                "RepairPoint0Left",
                "RepairPoint0Up",
                "Weapon_Gun_Position",
            ]
        );
    }

    #[test]
    fn auto_repair_restores_underscored_latch_point_names() {
        // Files saved by older builds carry `<base>_Heading`; migrate them
        // back, but leave weapon turret joints alone.
        let mut model = HODModel::new();
        model.joints = vec![
            joint("Root", None),
            joint("SalvagePoint1", Some("Root")),
            joint("SalvagePoint1_Heading", Some("SalvagePoint1")),
            joint("SalvagePoint1_Left", Some("SalvagePoint1")),
            joint("SalvagePoint1_Up", Some("SalvagePoint1")),
            joint("Weapon_Gun_Position", Some("Root")),
            joint("Weapon_Gun_Heading", Some("Weapon_Gun_Position")),
        ];
        model.markers = vec![HODMarker {
            name: "Marker".to_string(),
            parent_joint: "SalvagePoint1_Up".to_string(),
            position: Vector3 { x: 0.0, y: 0.0, z: 0.0 },
            rotation: identity_matrix(),
            rotation_euler: None,
        }];

        model.auto_repair_assembly_names();

        let names: Vec<&str> = model.joints.iter().map(|j| j.name.as_str()).collect();
        assert_eq!(
            names,
            vec![
                "Root",
                "SalvagePoint1",
                "SalvagePoint1Heading",
                "SalvagePoint1Left",
                "SalvagePoint1Up",
                "Weapon_Gun_Position",
                "Weapon_Gun_Heading",
            ]
        );
        assert_eq!(model.markers[0].parent_joint, "SalvagePoint1Up");
    }

    #[test]
    fn auto_assign_keeps_full_badge_slot_set() {
        // A `badge` material is the ship shader plus a decal pass; it needs
        // $diffuse/$glow/$team/$normal (97 vanilla STATs), not $diffuse only.
        let names = [
            "Hull_DIFF",
            "Hull_GLOW",
            "Hull_TEAM",
            "Hull_NORM",
        ];
        let mut model = HODModel::new();
        model.textures = names
            .iter()
            .map(|name| HODTexture {
                name: name.to_string(),
                width: 1,
                height: 1,
                format: "DXT1".to_string(),
                png_preview: None,
                png_data: None,
                source_path: None,
            })
            .collect();
        model.materials = vec![HODMaterial {
            name: "badge".to_string(),
            shader_name: "badge".to_string(),
            texture_maps: names.iter().map(|name| name.to_string()).collect(),
            parameters: Vec::new(),
            params: Vec::new(),
        }];

        model.auto_assign_and_resize_textures();

        assert_eq!(
            model.materials[0].texture_maps,
            names.iter().map(|name| name.to_string()).collect::<Vec<_>>()
        );
    }
}
