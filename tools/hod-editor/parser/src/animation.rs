use crate::iff::IffChunk;
use crate::types::{read_len_string, write_len_string};
use crate::math::{evaluate_curve, euler_to_quaternion, quaternion_to_euler, ParsedCurve, HODKeyframeCurveValue};
use crate::types::{HODAnimation, HODAnimationTrack, HODJoint, HODKeyframe, HODModel, Vector3};
use byteorder::{BigEndian, LittleEndian, ReadBytesExt, WriteBytesExt};
use std::io::{Cursor, Write};
use std::path::{Path, PathBuf};

pub(crate) fn companion_mad_candidates(hod_path: &Path) -> Vec<PathBuf> {
    let mut candidates = Vec::new();
    candidates.push(hod_path.with_extension("mad"));
    candidates.push(hod_path.with_extension("MAD"));

    let parent = match hod_path.parent() {
        Some(parent) => parent,
        None => return candidates,
    };

    let stem = match hod_path.file_stem().and_then(|s| s.to_str()) {
        Some(stem) => stem,
        None => return candidates,
    };

    let mut normalized_stems = Vec::new();
    for suffix in [
        "_2.0_original",
        "_1.0_original",
        "_from_2.0_to_2.0",
        "_from_1.0_to_2.0",
    ] {
        if let Some(base) = stem.strip_suffix(suffix) {
            normalized_stems.push(base.to_string());
        }
    }

    if let Some((base, _)) = stem.split_once("_from_") {
        normalized_stems.push(base.to_string());
    }

    for normalized_stem in normalized_stems {
        let candidate_lower = parent.join(format!("{}.mad", normalized_stem));
        let candidate_upper = parent.join(format!("{}.MAD", normalized_stem));

        if !candidates.iter().any(|existing| existing == &candidate_lower) {
            candidates.push(candidate_lower);
        }
        if !candidates.iter().any(|existing| existing == &candidate_upper) {
            candidates.push(candidate_upper);
        }
    }

    candidates
}
pub(crate) fn parse_keyf_chunk(keyf: &IffChunk) -> Result<Vec<ParsedCurve>, String> {
    let mut curves = Vec::new();
    for anim in &keyf.children {
        if anim.id == "ANIM" {
            let mut r = Cursor::new(&anim.data);
            let name = read_len_string(&mut r)?;
            let keyframe_count = r.read_i32::<LittleEndian>().map_err(|e| e.to_string())? as usize;
            let mut keyframes = Vec::with_capacity(keyframe_count);
            for _ in 0..keyframe_count {
                let time = r.read_f64::<LittleEndian>().map_err(|e| e.to_string())?;
                let value = r.read_f64::<LittleEndian>().map_err(|e| e.to_string())?;
                let in_x = r.read_f32::<LittleEndian>().map_err(|e| e.to_string())?;
                let in_y = r.read_f32::<LittleEndian>().map_err(|e| e.to_string())?;
                let out_x = r.read_f32::<LittleEndian>().map_err(|e| e.to_string())?;
                let out_y = r.read_f32::<LittleEndian>().map_err(|e| e.to_string())?;
                keyframes.push(HODKeyframeCurveValue {
                    time,
                    value,
                    in_tangent: [in_x, in_y],
                    out_tangent: [out_x, out_y],
                });
            }
            keyframes.sort_by(|a, b| a.time.partial_cmp(&b.time).unwrap());

            let pre_infinity = if r.position() + 4 <= anim.data.len() as u64 {
                r.read_i32::<LittleEndian>().unwrap_or(0)
            } else {
                0
            };
            let post_infinity = if r.position() + 4 <= anim.data.len() as u64 {
                r.read_i32::<LittleEndian>().unwrap_or(0)
            } else {
                0
            };
            curves.push(ParsedCurve {
                name,
                keyframes,
                pre_infinity,
                post_infinity,
            });
        }
    }
    Ok(curves)
}

pub(crate) fn serialize_anim_curve(curve_name: &str, values: Vec<(f64, f64)>) -> Result<IffChunk, String> {
    let mut keyframes = Vec::new();
    for (time, val) in values {
        keyframes.push(HODKeyframeCurveValue {
            time,
            value: val,
            in_tangent: [1.0, 0.0],
            out_tangent: [1.0, 0.0],
        });
    }

    let n = keyframes.len();
    if n > 1 {
        for i in 0..n - 1 {
            let dt = keyframes[i + 1].time - keyframes[i].time;
            let dv = keyframes[i + 1].value - keyframes[i].value;
            let len = (dt * dt + dv * dv).sqrt();
            let dir = if len > 0.0 {
                [(dt / len) as f32, (dv / len) as f32]
            } else {
                [1.0, 0.0]
            };
            keyframes[i].out_tangent = dir;
            keyframes[i + 1].in_tangent = dir;
        }
    }

    let mut data = Vec::new();
    write_len_string(&mut data, curve_name)?;
    data.write_i32::<LittleEndian>(keyframes.len() as i32)
        .map_err(|e| e.to_string())?;
    for kf in &keyframes {
        data.write_f64::<LittleEndian>(kf.time)
            .map_err(|e| e.to_string())?;
        data.write_f64::<LittleEndian>(kf.value)
            .map_err(|e| e.to_string())?;
        data.write_f32::<LittleEndian>(kf.in_tangent[0])
            .map_err(|e| e.to_string())?;
        data.write_f32::<LittleEndian>(kf.in_tangent[1])
            .map_err(|e| e.to_string())?;
        data.write_f32::<LittleEndian>(kf.out_tangent[0])
            .map_err(|e| e.to_string())?;
        data.write_f32::<LittleEndian>(kf.out_tangent[1])
            .map_err(|e| e.to_string())?;
    }
    data.write_i32::<LittleEndian>(0)
        .map_err(|e| e.to_string())?;
    data.write_i32::<LittleEndian>(0)
        .map_err(|e| e.to_string())?;

    Ok(IffChunk {
        id: "ANIM".to_string(),
        chunk_type: crate::iff::ChunkType::Normal,
        version: 1,
        data,
        children: Vec::new(),
    })
}

pub fn parse_mad_bytes(bytes: &[u8], joints: &[HODJoint]) -> Result<Vec<HODAnimation>, String> {
    let mut cursor = Cursor::new(bytes);
    let mut root_chunks = Vec::new();
    while cursor.position() < bytes.len() as u64 {
        match IffChunk::read_chunk(&mut cursor) {
            Ok(chunk) => root_chunks.push(chunk),
            Err(e) => return Err(format!("MAD IFF parse error: {}", e)),
        }
    }

    let mad_form = root_chunks
        .iter()
        .find(|c| c.id.trim() == "MAD")
        .ok_or_else(|| "Root MAD chunk not found in companion file".to_string())?;

    let info_chunk = mad_form
        .children
        .iter()
        .find(|c| c.id.trim() == "INFO")
        .ok_or_else(|| "INFO chunk not found in MAD file".to_string())?;
    let mut r_info = Cursor::new(&info_chunk.data);
    let _fps = r_info
        .read_i32::<LittleEndian>()
        .map_err(|e| e.to_string())?;
    let animation_count = r_info
        .read_i32::<LittleEndian>()
        .map_err(|e| e.to_string())? as usize;
    let curve_count = r_info
        .read_i32::<LittleEndian>()
        .map_err(|e| e.to_string())? as usize;

    let stri_chunk = mad_form
        .children
        .iter()
        .find(|c| c.id.trim() == "STRI")
        .ok_or_else(|| "STRI chunk not found in MAD file".to_string())?;
    let stri_bytes = &stri_chunk.data;

    let get_string_from_stri = |pos: usize| -> String {
        if pos >= stri_bytes.len() {
            return "Unknown".to_string();
        }
        let sub = &stri_bytes[pos..];
        if let Some(null_pos) = sub.iter().position(|&b| b == 0) {
            String::from_utf8_lossy(&sub[..null_pos]).to_string()
        } else {
            String::from_utf8_lossy(sub).to_string()
        }
    };

    let curv_chunk = mad_form
        .children
        .iter()
        .find(|c| c.id.trim() == "CURV")
        .ok_or_else(|| "CURV chunk not found in MAD file".to_string())?;
    let mut r_curv = Cursor::new(&curv_chunk.data);
    let mut curves = Vec::with_capacity(curve_count);
    for _ in 0..curve_count {
        let name_pos = r_curv
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())? as usize;
        let name = get_string_from_stri(name_pos);
        let keyframe_count = r_curv
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())? as usize;
        let mut keyframes = Vec::with_capacity(keyframe_count);
        for _ in 0..keyframe_count {
            let time = r_curv
                .read_f64::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            let value = r_curv
                .read_f64::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            let in_x = r_curv
                .read_f32::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            let in_y = r_curv
                .read_f32::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            let out_x = r_curv
                .read_f32::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            let out_y = r_curv
                .read_f32::<LittleEndian>()
                .map_err(|e| e.to_string())?;
            keyframes.push(HODKeyframeCurveValue {
                time,
                value,
                in_tangent: [in_x, in_y],
                out_tangent: [out_x, out_y],
            });
        }
        keyframes.sort_by(|a, b| a.time.partial_cmp(&b.time).unwrap());

        println!(
            "[DEBUG] Curve '{}' has {} keyframes:",
            name,
            keyframes.len()
        );
        for kf in &keyframes {
            println!("  kf time={:.3}s value={:.6}", kf.time, kf.value);
        }

        let pre_infinity = r_curv
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())?;
        let post_infinity = r_curv
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())?;

        curves.push(ParsedCurve {
            name,
            keyframes,
            pre_infinity,
            post_infinity,
        });
    }

    let mark_chunk = mad_form
        .children
        .iter()
        .find(|c| c.id.trim() == "MARK")
        .ok_or_else(|| "MARK chunk not found in MAD file".to_string())?;
    let mut r_mark = Cursor::new(&mark_chunk.data);
    let mut animations = Vec::with_capacity(animation_count);

    for _ in 0..animation_count {
        let name_pos = r_mark
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())? as usize;
        let anim_name = get_string_from_stri(name_pos);

        let start_time = r_mark
            .read_f32::<LittleEndian>()
            .map_err(|e| e.to_string())? as f64;
        let end_time = r_mark
            .read_f32::<LittleEndian>()
            .map_err(|e| e.to_string())? as f64;
        let _loop_start = r_mark
            .read_f32::<LittleEndian>()
            .map_err(|e| e.to_string())?;
        let _loop_end = r_mark
            .read_f32::<LittleEndian>()
            .map_err(|e| e.to_string())?;

        let joint_count = r_mark
            .read_i32::<LittleEndian>()
            .map_err(|e| e.to_string())? as usize;
        let mut tracks = Vec::with_capacity(joint_count);

        for _ in 0..joint_count {
            let joint_name_pos = r_mark
                .read_i32::<LittleEndian>()
                .map_err(|e| e.to_string())? as usize;
            let joint_name = get_string_from_stri(joint_name_pos);
            let channel_count = r_mark
                .read_i32::<LittleEndian>()
                .map_err(|e| e.to_string())? as usize;
            let mut channel_indices = Vec::with_capacity(channel_count);
            for _ in 0..channel_count {
                let curve_idx = r_mark
                    .read_i32::<LittleEndian>()
                    .map_err(|e| e.to_string())? as usize;
                channel_indices.push(curve_idx);
            }

            let joint_obj = joints
                .iter()
                .find(|j| j.name.eq_ignore_ascii_case(&joint_name));
            let default_pos = joint_obj
                .and_then(|j| j.position.clone())
                .unwrap_or(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                });
            let default_euler = joint_obj
                .and_then(|j| j.rotation.clone())
                .unwrap_or(Vector3 {
                    x: 0.0,
                    y: 0.0,
                    z: 0.0,
                });
            let mut default_scale = joint_obj.and_then(|j| j.scale.clone()).unwrap_or(Vector3 {
                x: 1.0,
                y: 1.0,
                z: 1.0,
            });
            if default_scale.x.abs() < 0.0001 {
                default_scale.x = 1.0;
            }
            if default_scale.y.abs() < 0.0001 {
                default_scale.y = 1.0;
            }
            if default_scale.z.abs() < 0.0001 {
                default_scale.z = 1.0;
            }

            let mut tx_curve: Option<&ParsedCurve> = None;
            let mut ty_curve: Option<&ParsedCurve> = None;
            let mut tz_curve: Option<&ParsedCurve> = None;
            let mut rx_curve: Option<&ParsedCurve> = None;
            let mut ry_curve: Option<&ParsedCurve> = None;
            let mut rz_curve: Option<&ParsedCurve> = None;
            let mut sx_curve: Option<&ParsedCurve> = None;
            let mut sy_curve: Option<&ParsedCurve> = None;
            let mut sz_curve: Option<&ParsedCurve> = None;

            for &idx in &channel_indices {
                if idx < curves.len() {
                    let curve = &curves[idx];
                    let name_lower = curve.name.to_lowercase();
                    if name_lower.ends_with("translatex") {
                        tx_curve = Some(curve);
                    } else if name_lower.ends_with("translatey") {
                        ty_curve = Some(curve);
                    } else if name_lower.ends_with("translatez") {
                        tz_curve = Some(curve);
                    } else if name_lower.ends_with("rotatex") {
                        rx_curve = Some(curve);
                    } else if name_lower.ends_with("rotatey") {
                        ry_curve = Some(curve);
                    } else if name_lower.ends_with("rotatez") {
                        rz_curve = Some(curve);
                    } else if name_lower.ends_with("scalex") {
                        sx_curve = Some(curve);
                    } else if name_lower.ends_with("scaley") {
                        sy_curve = Some(curve);
                    } else if name_lower.ends_with("scalez") {
                        sz_curve = Some(curve);
                    }
                }
            }

            let mut unique_times: Vec<f64> = Vec::new();
            let matched_curves = [
                tx_curve, ty_curve, tz_curve, rx_curve, ry_curve, rz_curve, sx_curve, sy_curve,
                sz_curve,
            ];
            for curve_opt in matched_curves.iter().flatten() {
                for kf in &curve_opt.keyframes {
                    if !unique_times.iter().any(|&t| (t - kf.time).abs() < 1e-5) {
                        unique_times.push(kf.time);
                    }
                }
            }
            unique_times.sort_by(|a, b| a.partial_cmp(b).unwrap());

            let mut keyframes = Vec::new();
            for &time in &unique_times {
                let tx = evaluate_curve(
                    tx_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_pos.x as f64,
                );
                let ty = evaluate_curve(
                    ty_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_pos.y as f64,
                );
                let tz = evaluate_curve(
                    tz_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_pos.z as f64,
                );

                let rx = evaluate_curve(
                    rx_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_euler.x as f64,
                );
                let ry = evaluate_curve(
                    ry_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_euler.y as f64,
                );
                let rz = evaluate_curve(
                    rz_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_euler.z as f64,
                );

                let sx = evaluate_curve(
                    sx_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_scale.x as f64,
                );
                let sy = evaluate_curve(
                    sy_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_scale.y as f64,
                );
                let sz = evaluate_curve(
                    sz_curve.map(|c| &c.keyframes[..]).unwrap_or(&[]),
                    time,
                    default_scale.z as f64,
                );

                let pos_vec = Vector3 {
                    x: tx as f32,
                    y: ty as f32,
                    z: tz as f32,
                };
                let rot_euler = Vector3 {
                    x: rx as f32,
                    y: ry as f32,
                    z: rz as f32,
                };
                let rot_quat = euler_to_quaternion(&rot_euler);
                let scale_vec = Vector3 {
                    x: sx as f32,
                    y: sy as f32,
                    z: sz as f32,
                };

                keyframes.push(HODKeyframe {
                    time,
                    position: Some(pos_vec),
                    rotation: Some(rot_quat),
                    rotation_euler: Some(rot_euler),
                    scale: Some(scale_vec),
                });
            }

            tracks.push(HODAnimationTrack {
                joint_name,
                keyframes,
            });
        }

        animations.push(HODAnimation {
            name: anim_name,
            duration: if end_time > 0.0 { end_time } else { 4.0 },
            tracks,
        });
    }

    Ok(animations)
}

pub fn serialize_mad_companion(model: &HODModel) -> Result<Vec<u8>, String> {
    let mut stri_data = Vec::new();
    let mut get_stri_pos = |s: &str| -> i32 {
        let pos = stri_data.len() as i32;
        stri_data.extend_from_slice(s.as_bytes());
        stri_data.push(0);
        pos
    };

    let mut anim_names_pos = Vec::new();
    for anim in &model.animations {
        anim_names_pos.push(get_stri_pos(&anim.name));
    }

    struct TempCurve {
        name_pos: i32,
        keyframes: Vec<HODKeyframeCurveValue>,
    }

    let mut temp_curves = Vec::new();

    struct TempTrack {
        joint_name_pos: i32,
        curve_indices: Vec<i32>,
    }

    struct TempAnim {
        name_pos: i32,
        duration: f64,
        tracks: Vec<TempTrack>,
    }

    let mut temp_anims = Vec::new();

    for (anim_idx, anim) in model.animations.iter().enumerate() {
        let name_pos = anim_names_pos[anim_idx];
        let mut temp_tracks = Vec::new();

        for track in &anim.tracks {
            let joint_name_pos = get_stri_pos(&track.joint_name);
            let mut curve_indices = Vec::new();

            let mut has_pos = false;
            let mut has_rot = false;
            let mut has_scale = false;

            for kf in &track.keyframes {
                if kf.position.is_some() {
                    has_pos = true;
                }
                if kf.rotation.is_some() {
                    has_rot = true;
                }
                if kf.scale.is_some() {
                    has_scale = true;
                }
            }

            let mut add_channel = |suffix: &str, values: Vec<(f64, f64)>| {
                let curve_name = format!("{}_{}", track.joint_name, suffix);
                let name_pos = get_stri_pos(&curve_name);

                let mut keyframes = Vec::new();
                for (time, val) in values {
                    keyframes.push(HODKeyframeCurveValue {
                        time,
                        value: val,
                        in_tangent: [1.0, 0.0],
                        out_tangent: [1.0, 0.0],
                    });
                }

                let n = keyframes.len();
                if n > 1 {
                    for i in 0..n - 1 {
                        let dt = keyframes[i + 1].time - keyframes[i].time;
                        let dv = keyframes[i + 1].value - keyframes[i].value;
                        let len = (dt * dt + dv * dv).sqrt();
                        let dir = if len > 0.0 {
                            [(dt / len) as f32, (dv / len) as f32]
                        } else {
                            [1.0, 0.0]
                        };
                        keyframes[i].out_tangent = dir;
                        keyframes[i + 1].in_tangent = dir;
                    }
                }

                let idx = temp_curves.len() as i32;
                temp_curves.push(TempCurve {
                    name_pos,
                    keyframes,
                });
                curve_indices.push(idx);
            };

            if has_pos {
                let mut tx_vals = Vec::new();
                let mut ty_vals = Vec::new();
                let mut tz_vals = Vec::new();
                for kf in &track.keyframes {
                    if let Some(ref pos) = kf.position {
                        tx_vals.push((kf.time, pos.x as f64));
                        ty_vals.push((kf.time, pos.y as f64));
                        tz_vals.push((kf.time, pos.z as f64));
                    }
                }
                add_channel("translateX", tx_vals);
                add_channel("translateY", ty_vals);
                add_channel("translateZ", tz_vals);
            }

            if has_rot {
                let mut rx_vals = Vec::new();
                let mut ry_vals = Vec::new();
                let mut rz_vals = Vec::new();
                for kf in &track.keyframes {
                    let euler = if let Some(ref euler) = kf.rotation_euler {
                        euler.clone()
                    } else if let Some(ref rot) = kf.rotation {
                        quaternion_to_euler(rot)
                    } else {
                        Vector3 {
                            x: 0.0,
                            y: 0.0,
                            z: 0.0,
                        }
                    };
                    rx_vals.push((kf.time, euler.x as f64));
                    ry_vals.push((kf.time, euler.y as f64));
                    rz_vals.push((kf.time, euler.z as f64));
                }
                add_channel("rotateX", rx_vals);
                add_channel("rotateY", ry_vals);
                add_channel("rotateZ", rz_vals);
            }

            if has_scale {
                let mut sx_vals = Vec::new();
                let mut sy_vals = Vec::new();
                let mut sz_vals = Vec::new();
                for kf in &track.keyframes {
                    if let Some(ref sc) = kf.scale {
                        sx_vals.push((kf.time, sc.x as f64));
                        sy_vals.push((kf.time, sc.y as f64));
                        sz_vals.push((kf.time, sc.z as f64));
                    }
                }
                add_channel("scaleX", sx_vals);
                add_channel("scaleY", sy_vals);
                add_channel("scaleZ", sz_vals);
            }

            temp_tracks.push(TempTrack {
                joint_name_pos,
                curve_indices,
            });
        }

        temp_anims.push(TempAnim {
            name_pos,
            duration: anim.duration,
            tracks: temp_tracks,
        });
    }

    let mut mad_children = Vec::new();

    let mut vers_data = Vec::new();
    let _ = vers_data.write_u32::<BigEndian>(0x104);
    mad_children.push(IffChunk {
        id: "VERS".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: vers_data,
        children: Vec::new(),
    });

    let mut name_data = Vec::new();
    let _ = name_data.write_all(b"Homeworld2 MAD File");
    mad_children.push(IffChunk {
        id: "NAME".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: name_data,
        children: Vec::new(),
    });

    let mut info_data = Vec::new();
    let mut joint_map_count = 0;
    let mut total_channel_count = 0;
    for anim in &temp_anims {
        joint_map_count += anim.tracks.len();
        for track in &anim.tracks {
            total_channel_count += track.curve_indices.len();
        }
    }

    let _ = info_data.write_i32::<LittleEndian>(30);
    let _ = info_data.write_i32::<LittleEndian>(model.animations.len() as i32);
    let _ = info_data.write_i32::<LittleEndian>(temp_curves.len() as i32);
    let _ = info_data.write_i32::<LittleEndian>(joint_map_count as i32);
    let _ = info_data.write_i32::<LittleEndian>(total_channel_count as i32);

    mad_children.push(IffChunk {
        id: "INFO".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: info_data,
        children: Vec::new(),
    });

    mad_children.push(IffChunk {
        id: "STRI".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: stri_data,
        children: Vec::new(),
    });

    let mut mark_data = Vec::new();
    for anim in &temp_anims {
        let _ = mark_data.write_i32::<LittleEndian>(anim.name_pos);
        let _ = mark_data.write_f32::<LittleEndian>(0.0);
        let _ = mark_data.write_f32::<LittleEndian>(anim.duration as f32);
        let _ = mark_data.write_f32::<LittleEndian>(0.0);
        let _ = mark_data.write_f32::<LittleEndian>(anim.duration as f32);
        let _ = mark_data.write_i32::<LittleEndian>(anim.tracks.len() as i32);

        for track in &anim.tracks {
            let _ = mark_data.write_i32::<LittleEndian>(track.joint_name_pos);
            let _ = mark_data.write_i32::<LittleEndian>(track.curve_indices.len() as i32);
            for &idx in &track.curve_indices {
                let _ = mark_data.write_i32::<LittleEndian>(idx);
            }
        }
    }

    mad_children.push(IffChunk {
        id: "MARK".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: mark_data,
        children: Vec::new(),
    });

    let mut curv_data = Vec::new();
    for curve in &temp_curves {
        let _ = curv_data.write_i32::<LittleEndian>(curve.name_pos);
        let _ = curv_data.write_i32::<LittleEndian>(curve.keyframes.len() as i32);
        for kf in &curve.keyframes {
            let _ = curv_data.write_f64::<LittleEndian>(kf.time);
            let _ = curv_data.write_f64::<LittleEndian>(kf.value);
            let _ = curv_data.write_f32::<LittleEndian>(kf.in_tangent[0]);
            let _ = curv_data.write_f32::<LittleEndian>(kf.in_tangent[1]);
            let _ = curv_data.write_f32::<LittleEndian>(kf.out_tangent[0]);
            let _ = curv_data.write_f32::<LittleEndian>(kf.out_tangent[1]);
        }
        let _ = curv_data.write_i32::<LittleEndian>(0);
        let _ = curv_data.write_i32::<LittleEndian>(0);
    }

    mad_children.push(IffChunk {
        id: "CURV".to_string(),
        chunk_type: crate::iff::ChunkType::Default,
        version: 0,
        data: curv_data,
        children: Vec::new(),
    });

    let mad_root = IffChunk {
        id: "MAD ".to_string(),
        chunk_type: crate::iff::ChunkType::Form,
        version: 0,
        data: Vec::new(),
        children: mad_children,
    };

    let mut output = Vec::new();
    mad_root
        .write_chunk(&mut output)
        .map_err(|e| e.to_string())?;
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn animation_curve_evaluation_is_linear() {
        let keyframes = vec![
            HODKeyframeCurveValue {
                time: 0.0,
                value: 2.0,
                in_tangent: [1.0, 0.0],
                out_tangent: [1.0, 0.0],
            },
            HODKeyframeCurveValue {
                time: 1.0,
                value: 10.0,
                in_tangent: [1.0, 0.0],
                out_tangent: [1.0, 0.0],
            },
        ];

        let result = evaluate_curve(&keyframes, 0.5, 0.0);
        assert!((result - 6.0).abs() < 1e-6);
    }

    #[test]
    fn animation_companion_candidates_find_normalized_stem() {
        let temp_dir = std::env::temp_dir().join(format!(
            "hodeditor_animation_test_{}",
            std::process::id()
        ));
        std::fs::create_dir_all(&temp_dir).unwrap();
        let hod_path = temp_dir.join("ter_fenris_2.0_original.hod");
        std::fs::write(&hod_path, []).unwrap();

        let candidates = companion_mad_candidates(&hod_path);
        assert!(candidates.iter().any(|candidate| {
            candidate.file_name().and_then(|name| name.to_str()) == Some("ter_fenris.mad")
        }));

        std::fs::remove_dir_all(temp_dir).unwrap();
    }
}
