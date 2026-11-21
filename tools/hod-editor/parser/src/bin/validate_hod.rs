//! validate_hod: structural validator for HOD files.
//!
//! The editor's loader silently repairs several classes of corruption that
//! crash the game engine with an access violation:
//!   * face-pool indices >= the part's vertex count are clamped
//!     (`parse_basic_mesh`: `min(raw, vertex_count - 1)`)
//!   * NaN vertex positions are clamped to 0.0
//!   * STAT texture indices >= the LMIP count are silently dropped
//!
//! This tool re-walks the raw chunks WITHOUT those guards and reports every
//! engine-unsafe construct, so a DAE/1.3-sourced HOD can be diagnosed without
//! launching the game. Exit code: 0 = clean, 1 = errors found,
//! 2 = file could not be read/parsed at all.

use byteorder::{LittleEndian, ReadBytesExt};
use hwr_hod_parser::iff::{ChunkType, IffChunk};
use std::io::{Cursor, Read};

#[derive(Debug, PartialEq, Eq, PartialOrd, Ord)]
enum Severity {
    Error,
    Warning,
    Info,
}

struct Finding {
    severity: Severity,
    location: String,
    message: String,
}

struct Report {
    findings: Vec<Finding>,
}

impl Report {
    fn new() -> Self {
        Self {
            findings: Vec::new(),
        }
    }
    fn error(&mut self, location: impl Into<String>, message: impl Into<String>) {
        self.findings.push(Finding {
            severity: Severity::Error,
            location: location.into(),
            message: message.into(),
        });
    }
    fn warning(&mut self, location: impl Into<String>, message: impl Into<String>) {
        self.findings.push(Finding {
            severity: Severity::Warning,
            location: location.into(),
            message: message.into(),
        });
    }
    fn info(&mut self, location: impl Into<String>, message: impl Into<String>) {
        self.findings.push(Finding {
            severity: Severity::Info,
            location: location.into(),
            message: message.into(),
        });
    }
    fn error_count(&self) -> usize {
        self.findings
            .iter()
            .filter(|f| f.severity == Severity::Error)
            .count()
    }
    fn print(&self) {
        let mut sorted: Vec<&Finding> = self.findings.iter().collect();
        sorted.sort_by(|a, b| {
            a.severity
                .cmp(&b.severity)
                .then(a.location.cmp(&b.location))
        });
        for f in sorted {
            let tag = match f.severity {
                Severity::Error => "ERROR",
                Severity::Warning => "WARN ",
                Severity::Info => "INFO ",
            };
            println!("  [{}] {}: {}", tag, f.location, f.message);
        }
        if self.findings.is_empty() {
            println!("  (clean: no findings)");
        }
    }
}

fn read_len_string(data: &[u8], pos: &mut usize) -> Option<String> {
    if *pos + 4 > data.len() {
        return None;
    }
    let len = u32::from_le_bytes(data[*pos..*pos + 4].try_into().ok()?) as usize;
    *pos += 4;
    if *pos + len > data.len() {
        return None;
    }
    let s = String::from_utf8_lossy(&data[*pos..*pos + len]).to_string();
    *pos += len;
    Some(s)
}

fn vertex_stride(mask: u32, version: u32) -> u32 {
    let mut stride = 0u32;
    if (mask & 0x01) != 0 {
        stride += 16;
    }
    if (mask & 0x02) != 0 {
        stride += 16;
    }
    if (mask & 0x04) != 0 {
        stride += 4;
    }
    if (mask & 0x08) != 0 {
        stride += 8;
    }
    if version == 1401 {
        if (mask & 0x10) != 0 {
            stride += 8;
        }
        if (mask & 0x20) != 0 {
            stride += 8;
        }
    }
    if (mask & 0x2000) != 0 {
        stride += 12;
    }
    if (mask & 0x4000) != 0 {
        stride += 12;
    }
    if stride == 0 {
        stride = 1;
    }
    stride
}

fn dxt_block_bytes(format: &str, w: u32, h: u32) -> Option<usize> {
    let bw = ((w + 3) / 4).max(1) as usize;
    let bh = ((h + 3) / 4).max(1) as usize;
    match format {
        "DXT1" => Some(bw * bh * 8),
        "DXT3" | "DXT5" => Some(bw * bh * 16),
        "8888" | "RGBA" => Some((w as usize) * (h as usize) * 4),
        _ => None,
    }
}

struct Pools {
    tex: Vec<u8>,
    mesh: Vec<u8>,
    face: Vec<u8>,
    is_v2: bool,
}

fn parse_pool(chunk: &IffChunk, report: &mut Report) -> Option<Pools> {
    let mut cursor = Cursor::new(&chunk.data);
    let pool_type = cursor.read_u32::<LittleEndian>().ok()?;
    let comp_tex_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let decomp_tex_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let pos = cursor.position() as usize;
    if pos + comp_tex_len > chunk.data.len() {
        report.error(
            "POOL",
            format!(
                "texture stream overruns POOL data (need {} bytes, have {})",
                comp_tex_len,
                chunk.data.len() - pos
            ),
        );
        return None;
    }
    let comp_tex = chunk.data[pos..pos + comp_tex_len].to_vec();
    cursor.set_position((pos + comp_tex_len) as u64);

    let tex = if comp_tex_len == decomp_tex_len {
        // Stored raw (our writer skips compression below the 10% threshold).
        comp_tex
    } else {
        match hwr_hod_parser::xpress::decompress(&comp_tex, decomp_tex_len) {
            Ok(v) => {
                if v.len() != decomp_tex_len {
                    report.error(
                        "POOL/tex",
                        format!(
                            "decompressed texture stream is {} bytes, header claims {}",
                            v.len(),
                            decomp_tex_len
                        ),
                    );
                }
                v
            }
            Err(e) => {
                report.error("POOL/tex", format!("texture stream decompression failed: {}", e));
                return None;
            }
        }
    };

    let comp_mesh_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let decomp_mesh_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let pos = cursor.position() as usize;
    if pos + comp_mesh_len > chunk.data.len() {
        report.error(
            "POOL",
            format!(
                "mesh stream overruns POOL data (need {} bytes, have {})",
                comp_mesh_len,
                chunk.data.len() - pos
            ),
        );
        return None;
    }
    let comp_mesh = chunk.data[pos..pos + comp_mesh_len].to_vec();
    cursor.set_position((pos + comp_mesh_len) as u64);
    let mesh = if comp_mesh_len == decomp_mesh_len {
        comp_mesh
    } else {
        match hwr_hod_parser::xpress::decompress(&comp_mesh, decomp_mesh_len) {
            Ok(v) => v,
            Err(e) => {
                report.error("POOL/mesh", format!("mesh stream decompression failed: {}", e));
                return None;
            }
        }
    };
    if mesh.len() != decomp_mesh_len {
        report.error(
            "POOL/mesh",
            format!(
                "decompressed mesh stream is {} bytes, header claims {}",
                mesh.len(),
                decomp_mesh_len
            ),
        );
    }

    let comp_face_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let decomp_face_len = cursor.read_u32::<LittleEndian>().ok()? as usize;
    let pos = cursor.position() as usize;
    if pos + comp_face_len > chunk.data.len() {
        report.error(
            "POOL",
            format!(
                "face stream overruns POOL data (need {} bytes, have {})",
                comp_face_len,
                chunk.data.len() - pos
            ),
        );
        return None;
    }
    let comp_face = chunk.data[pos..pos + comp_face_len].to_vec();
    cursor.set_position((pos + comp_face_len) as u64);
    let face = if comp_face_len == decomp_face_len {
        comp_face
    } else {
        match hwr_hod_parser::xpress::decompress(&comp_face, decomp_face_len) {
            Ok(v) => v,
            Err(e) => {
                report.error("POOL/face", format!("face stream decompression failed: {}", e));
                return None;
            }
        }
    };
    if face.len() != decomp_face_len {
        report.error(
            "POOL/face",
            format!(
                "decompressed face stream is {} bytes, header claims {}",
                face.len(),
                decomp_face_len
            ),
        );
    }

    let trailing = chunk.data.len() as u64 - cursor.position();
    if trailing != 0 {
        report.warning(
            "POOL",
            format!("{} trailing bytes after face stream", trailing),
        );
    }
    report.info(
        "POOL",
        format!(
            "type={} tex={}/{} mesh={}/{} face={}/{}",
            pool_type,
            comp_tex_len,
            decomp_tex_len,
            comp_mesh_len,
            decomp_mesh_len,
            comp_face_len,
            decomp_face_len
        ),
    );
    Some(Pools {
        tex,
        mesh,
        face,
        is_v2: true,
    })
}

/// Raw STAT walk without the loader's guards. Returns (shader, texture indices).
fn check_stat(
    chunk: &IffChunk,
    lmip_count: usize,
    report: &mut Report,
    mat_slot: usize,
) -> Option<(String, Vec<usize>)> {
    let data = &chunk.data;
    let mut pos = 0usize;
    let mat_name = match read_len_string(data, &mut pos) {
        Some(s) => s,
        None => {
            report.error(
                format!("STAT[{}]", mat_slot),
                "cannot read material name (truncated chunk)".to_string(),
            );
            return None;
        }
    };
    let shader = match read_len_string(data, &mut pos) {
        Some(s) => s,
        None => {
            report.error(
                format!("STAT '{}'", mat_name),
                "cannot read shader name (truncated chunk)".to_string(),
            );
            return None;
        }
    };
    if pos + 4 > data.len() {
        report.error(
            format!("STAT '{}'", mat_name),
            "cannot read param_count (truncated chunk)".to_string(),
        );
        return None;
    }
    let param_count = u32::from_le_bytes(data[pos..pos + 4].try_into().unwrap()) as usize;
    pos += 4;
    let loc = format!("STAT '{}'/{}", mat_name, shader);
    if shader.trim().is_empty() {
        report.warning(loc.clone(), "empty shader name".to_string());
    }

    // Try named variant first (must consume exactly), then nameless.
    let mut tex_indices = Vec::new();
    let mut ok = false;
    for with_names in [true, false] {
        let mut p = pos;
        let mut indices = Vec::new();
        let mut valid = true;
        for _ in 0..param_count {
            if p + 8 > data.len() {
                valid = false;
                break;
            }
            let param_type = u32::from_le_bytes(data[p..p + 4].try_into().unwrap());
            let value_size = u32::from_le_bytes(data[p + 4..p + 8].try_into().unwrap()) as usize;
            p += 8;
            if p + value_size > data.len() {
                valid = false;
                break;
            }
            let value = &data[p..p + value_size];
            p += value_size;
            if with_names {
                if read_len_string(data, &mut p).is_none() {
                    valid = false;
                    break;
                }
            }
            if param_type == 5 {
                if value.len() < 4 {
                    report.error(
                        loc.clone(),
                        "type-5 param with < 4 value bytes".to_string(),
                    );
                } else {
                    indices.push(u32::from_le_bytes(value[0..4].try_into().unwrap()) as usize);
                }
            }
        }
        if valid && p == data.len() {
            tex_indices = indices;
            ok = true;
            break;
        }
    }
    if !ok {
        report.error(
            loc.clone(),
            format!(
                "param records do not consume the chunk (param_count={}, {} bytes remain or overrun)",
                param_count,
                data.len().saturating_sub(pos)
            ),
        );
        return Some((shader, Vec::new()));
    }

    for (slot, &tex_idx) in tex_indices.iter().enumerate() {
        if tex_idx >= lmip_count {
            report.error(
                loc.clone(),
                format!(
                    "slot {} texture index {} out of range (only {} LMIP textures); the loader silently drops this binding",
                    slot, tex_idx, lmip_count
                ),
            );
        }
    }
    if param_count == 0 {
        report.warning(loc.clone(), "param_count is 0".to_string());
    }
    Some((shader, tex_indices))
}

struct BmshPart {
    material_index: usize,
    mask: u32,
    vertex_count: usize,
    prim_groups: i16,
    indice_count: usize,
}

fn parse_bmsh_header(
    data: &[u8],
    version: u32,
    is_v2: bool,
    loc: &str,
    report: &mut Report,
) -> Option<(i32, Vec<BmshPart>)> {
    let mut r = Cursor::new(data);
    let lod = r.read_i32::<LittleEndian>().ok()?;
    let part_count = r.read_i32::<LittleEndian>().ok()?;
    if part_count < 0 || part_count > 10000 {
        report.error(loc, format!("implausible part_count {}", part_count));
        return None;
    }
    let mut parts = Vec::new();
    for _ in 0..part_count {
        let material_index = r.read_i32::<LittleEndian>().ok()? as usize;
        let mask = r.read_u32::<LittleEndian>().ok()?;
        let vertex_count = r.read_i32::<LittleEndian>().ok()?;
        if !is_v2 {
            // v1 stores vertices inline; header ends here per part.
            parts.push(BmshPart {
                material_index,
                mask,
                vertex_count: vertex_count.max(0) as usize,
                prim_groups: 0,
                indice_count: 0,
            });
            let _ = version;
            continue;
        }
        let prim_groups = r.read_i16::<LittleEndian>().ok()?;
        let indice_count = r.read_i32::<LittleEndian>().ok()?;
        parts.push(BmshPart {
            material_index,
            mask,
            vertex_count: vertex_count.max(0) as usize,
            prim_groups,
            indice_count: indice_count.max(0) as usize,
        });
    }
    Some((lod, parts))
}

#[allow(clippy::too_many_arguments)]
fn check_bmsh_v2(
    mesh_name: &str,
    lod: i32,
    parts: &[BmshPart],
    chunk_version: u32,
    stat_count: usize,
    mesh_pool: &mut Cursor<&Vec<u8>>,
    face_pool: &mut Cursor<&Vec<u8>>,
    report: &mut Report,
) {
    for (pi, part) in parts.iter().enumerate() {
        let loc = format!("BMSH '{}' lod={} part={}", mesh_name, lod, pi);
        if part.material_index >= stat_count {
            report.error(
                loc.clone(),
                format!(
                    "material_index {} out of range ({} STAT materials); engine material lookup reads out of bounds",
                    part.material_index, stat_count
                ),
            );
        }
        if (part.mask & 0x1) == 0 {
            report.warning(loc.clone(), format!("vertex_mask 0x{:X} has no position bit", part.mask));
        }
        if part.prim_groups != -1 {
            report.warning(
                loc.clone(),
                format!(
                    "prim_group_count is {} (HODOR v2 writes -1); unexpected index-block layout",
                    part.prim_groups
                ),
            );
        }
        if part.vertex_count == 0 {
            if part.indice_count > 0 {
                report.error(
                    loc.clone(),
                    format!(
                        "0 vertices but {} indices; engine draws from an empty vertex buffer",
                        part.indice_count
                    ),
                );
            }
            continue;
        }
        let stride = vertex_stride(part.mask, chunk_version) as u64;
        let need = stride * part.vertex_count as u64;
        let remaining = mesh_pool.get_ref().len() as u64 - mesh_pool.position();
        if need > remaining {
            report.error(
                loc.clone(),
                format!(
                    "vertex block overruns mesh pool (need {} bytes, {} remain); pool desync corrupts every later part",
                    need, remaining
                ),
            );
            return;
        }
        // Scan positions for NaN/Inf (the loader clamps NaN to 0 and hides this).
        if (part.mask & 0x1) != 0 {
            let base = mesh_pool.position() as usize;
            let bytes = mesh_pool.get_ref();
            let mut bad = 0usize;
            for v in 0..part.vertex_count {
                let off = base + v * stride as usize;
                if off + 12 > bytes.len() {
                    break;
                }
                for c in 0..3 {
                    let f = f32::from_le_bytes(bytes[off + c * 4..off + c * 4 + 4].try_into().unwrap());
                    if !f.is_finite() {
                        bad += 1;
                        break;
                    }
                }
            }
            if bad > 0 {
                report.error(
                    loc.clone(),
                    format!(
                        "{}/{} vertices have NaN/Inf positions; the loader clamps these to 0 but the engine does not",
                        bad, part.vertex_count
                    ),
                );
            }
        }
        mesh_pool.set_position(mesh_pool.position() + need);

        let ineed = part.indice_count as u64 * 2;
        let iremaining = face_pool.get_ref().len() as u64 - face_pool.position();
        if ineed > iremaining {
            report.error(
                loc.clone(),
                format!(
                    "index block overruns face pool (need {} bytes, {} remain)",
                    ineed, iremaining
                ),
            );
            return;
        }
        let base = face_pool.position() as usize;
        let bytes = face_pool.get_ref();
        let mut oob = 0usize;
        let mut first_oob = Vec::new();
        for i in 0..part.indice_count {
            let raw = u16::from_le_bytes(bytes[base + i * 2..base + i * 2 + 2].try_into().unwrap()) as usize;
            if raw >= part.vertex_count {
                oob += 1;
                if first_oob.len() < 8 {
                    first_oob.push(raw);
                }
            }
        }
        if oob > 0 {
            report.error(
                loc.clone(),
                format!(
                    "{}/{} face indices out of range for {} vertices (e.g. {:?}); the loader clamps these but the engine reads out of bounds -> access violation",
                    oob, part.indice_count, part.vertex_count, first_oob
                ),
            );
        }
        face_pool.set_position(face_pool.position() + ineed);
    }
}

fn collect_bmsh_from_mult(data: &[u8], report: &mut Report, loc: &str) -> Vec<(Vec<u8>, u32)> {
    // MULT payload: len-string name, len-string parent, u32 lod_count, then chunks.
    let mut pos = 0usize;
    let name = read_len_string(data, &mut pos);
    if name.is_none() {
        report.warning(loc, "cannot read MULT mesh name".to_string());
        return Vec::new();
    }
    if read_len_string(data, &mut pos).is_none() {
        report.warning(loc, "cannot read MULT parent name".to_string());
        return Vec::new();
    }
    if pos + 4 > data.len() {
        return Vec::new();
    }
    pos += 4; // lod_count
    let mut out = Vec::new();
    let mut cursor = Cursor::new(&data[pos..]);
    while cursor.position() < (data.len() - pos) as u64 {
        match IffChunk::read_chunk(&mut cursor) {
            Ok(c) => {
                if c.id.trim() == "BMSH" {
                    let version = if c.chunk_type == ChunkType::Normal { 0 } else { 0 };
                    let _ = version;
                    out.push((c.data.clone(), c.version));
                } else if c.id.trim() == "NRML" && c.data.starts_with(b"BMSH") {
                    // NRML wrapper loses its version once unwrapped (parser sets 0).
                    out.push((c.data[4..].to_vec(), 0));
                }
            }
            Err(e) => {
                report.warning(loc, format!("embedded chunk read failed inside MULT: {}", e));
                break;
            }
        }
    }
    out
}

fn parse_hier_names(data: &[u8]) -> Vec<(String, String)> {
    let mut r = Cursor::new(data);
    let mut out = Vec::new();
    let Ok(first) = r.read_u32::<LittleEndian>() else {
        return out;
    };
    if (first as i32) < 0 {
        // v2: records until exhaustion.
        while r.position() + 4 <= data.len() as u64 {
            let start = r.position();
            let mut tmp = Vec::new();
            let mut ok = true;
            for _ in 0..2 {
                let Ok(len) = r.read_u32::<LittleEndian>() else { ok = false; break; };
                let mut buf = vec![0u8; len as usize];
                if r.read_exact(&mut buf).is_err() { ok = false; break; }
                tmp.push(String::from_utf8_lossy(&buf).to_string());
            }
            if !ok {
                break;
            }
            // 9 x f32
            let mut floats = [0u8; 36];
            if r.read_exact(&mut floats).is_err() {
                let _ = start;
                break;
            }
            if tmp[0].is_empty() {
                break;
            }
            out.push((tmp[0].clone(), tmp[1].clone()));
            if r.position() + 44 > data.len() as u64 && r.position() < data.len() as u64 {
                // trailing partial record; stop.
                if data.len() as u64 - r.position() < 44 {
                    break;
                }
            }
        }
    } else {
        for _ in 0..first {
            let mut tmp = Vec::new();
            let mut ok = true;
            for _ in 0..2 {
                let Ok(len) = r.read_u32::<LittleEndian>() else { ok = false; break; };
                let mut buf = vec![0u8; len as usize];
                if r.read_exact(&mut buf).is_err() { ok = false; break; }
                tmp.push(String::from_utf8_lossy(&buf).to_string());
            }
            if !ok {
                break;
            }
            // v1: 9 f32 + 3 f32 + 3 bytes dof
            let mut skip = [0u8; 51];
            if r.read_exact(&mut skip).is_err() {
                break;
            }
            out.push((tmp[0].clone(), tmp[1].clone()));
        }
    }
    out
}

fn validate_file(path: &str, verbose: bool) -> i32 {
    println!("== {}", path);
    let mut report = Report::new();
    let bytes = match std::fs::read(path) {
        Ok(b) => b,
        Err(e) => {
            println!("  [ERROR] cannot read file: {}", e);
            return 2;
        }
    };

    // Top-level IFF walk.
    let mut cursor = Cursor::new(&bytes);
    let mut chunks = Vec::new();
    loop {
        if cursor.position() as usize >= bytes.len() {
            break;
        }
        match IffChunk::read_chunk(&mut cursor) {
            Ok(c) => chunks.push(c),
            Err(e) => {
                report.error(
                    "IFF",
                    format!(
                        "top-level chunk walk failed at byte {}: {}",
                        cursor.position(),
                        e
                    ),
                );
                break;
            }
        }
    }
    if cursor.position() as usize != bytes.len() {
        report.warning(
            "IFF",
            format!(
                "{} trailing bytes after top-level walk",
                bytes.len() - cursor.position() as usize
            ),
        );
    }
    let ids: Vec<String> = chunks.iter().map(|c| c.id.clone()).collect();
    report.info("IFF", format!("top-level chunks: {:?}", ids));
    for required in ["VERS", "NAME", "POOL", "HVMD", "DTRM"] {
        if !chunks.iter().any(|c| c.id == required) {
            let sev_is_error = required == "HVMD" || required == "DTRM";
            if sev_is_error {
                report.error("IFF", format!("missing top-level chunk '{}'", required));
            } else {
                report.warning("IFF", format!("missing top-level chunk '{}'", required));
            }
        }
    }
    let is_v2 = chunks.iter().any(|c| c.id == "POOL");

    // POOL.
    let mut pools: Option<Pools> = None;
    for chunk in &chunks {
        if chunk.id == "POOL" {
            pools = parse_pool(chunk, &mut report);
        }
    }

    // HVMD: LMIP inventory then STAT raw checks.
    let mut lmip_names: Vec<String> = Vec::new();
    let mut stat_shaders: Vec<(String, Vec<usize>)> = Vec::new();
    if let Some(hvmd) = chunks.iter().find(|c| c.id == "HVMD") {
        for sub in &hvmd.children {
            match sub.id.trim() {
                "LMIP" | "TEXM" => {
                    let mut pos = 0usize;
                    match read_len_string(&sub.data, &mut pos) {
                        Some(name) => {
                            if name.is_empty() {
                                report.warning(
                                    sub.id.trim(),
                                    "empty texture name".to_string(),
                                );
                            }
                            lmip_names.push(name);
                        }
                        None => report.error(
                            sub.id.trim(),
                            "cannot read texture name (truncated LMIP)".to_string(),
                        ),
                    }
                }
                _ => {}
            }
        }
        report.info("HVMD", format!("{} LMIP/TEXM textures", lmip_names.len()));
        let mut slot = 0usize;
        for sub in &hvmd.children {
            match sub.id.trim() {
                "STAT" | "MATT" => {
                    if let Some((shader, indices)) =
                        check_stat(sub, lmip_names.len(), &mut report, slot)
                    {
                        stat_shaders.push((shader, indices));
                    }
                    slot += 1;
                }
                _ => {}
            }
        }
        report.info("HVMD", format!("{} STAT/MATT materials", stat_shaders.len()));
    }

    // Texture-pool consumption check (v2): replay LMIP reads in file order.
    if let Some(pools) = &pools {
        if let Some(hvmd) = chunks.iter().find(|c| c.id == "HVMD") {
            let mut tex_pos = 0usize;
            for sub in &hvmd.children {
                if !matches!(sub.id.trim(), "LMIP" | "TEXM") {
                    continue;
                }
                let mut pos = 0usize;
                let name = read_len_string(&sub.data, &mut pos).unwrap_or_default();
                if pos + 4 > sub.data.len() {
                    continue;
                }
                let format = String::from_utf8_lossy(&sub.data[pos..pos + 4])
                    .trim()
                    .to_string();
                pos += 4;
                if pos + 4 > sub.data.len() {
                    continue;
                }
                let mip_count =
                    u32::from_le_bytes(sub.data[pos..pos + 4].try_into().unwrap()) as usize;
                pos += 4;
                let mut need = 0usize;
                let mut dims_ok = true;
                for _ in 0..mip_count {
                    if pos + 8 > sub.data.len() {
                        dims_ok = false;
                        break;
                    }
                    let w = u32::from_le_bytes(sub.data[pos..pos + 4].try_into().unwrap());
                    let h = u32::from_le_bytes(sub.data[pos + 4..pos + 8].try_into().unwrap());
                    pos += 8;
                    match dxt_block_bytes(&format, w, h) {
                        Some(b) => need += b,
                        None => {
                            report.warning(
                                format!("LMIP '{}'", name),
                                format!(
                                    "unknown texture format '{}'; cannot verify pool consumption",
                                    format
                                ),
                            );
                        }
                    }
                }
                if !dims_ok {
                    report.error(
                        format!("LMIP '{}'", name),
                        "mip dimension table overruns LMIP chunk".to_string(),
                    );
                    continue;
                }
                if w_h_zero(sub) {
                    report.error(
                        format!("LMIP '{}'", name),
                        "zero base dimensions".to_string(),
                    );
                }
                if tex_pos + need > pools.tex.len() {
                    report.error(
                        format!("LMIP '{}'", name),
                        format!(
                            "needs {} texture-pool bytes, only {} remain (pool desync)",
                            need,
                            pools.tex.len().saturating_sub(tex_pos)
                        ),
                    );
                    break;
                }
                tex_pos += need;
            }
            let leftover = pools.tex.len().saturating_sub(tex_pos);
            if leftover > 0 {
                report.warning(
                    "POOL/tex",
                    format!("{} texture-pool bytes never consumed by any LMIP", leftover),
                );
            }
            if verbose {
                report.info(
                    "POOL/tex",
                    format!("consumed {}/{} texture-pool bytes", tex_pos, pools.tex.len()),
                );
            }
        }
    }

    // DTRM: joints, markers, BMSH walk with pool cursors.
    let mut joint_names: Vec<String> = Vec::new();
    let mut mesh_parents: Vec<(String, String)> = Vec::new();
    let mut marker_parents: Vec<(String, String)> = Vec::new();
    let mut mesh_pool_cursor: Option<Cursor<&Vec<u8>>> = None;
    let mut face_pool_cursor: Option<Cursor<&Vec<u8>>> = None;
    if let Some(p) = &pools {
        mesh_pool_cursor = Some(Cursor::new(&p.mesh));
        face_pool_cursor = Some(Cursor::new(&p.face));
    }
    if let Some(dtrm) = chunks.iter().find(|c| c.id == "DTRM") {
        for sub in &dtrm.children {
            // COLD must be HODOR-layout: data = len-string name only, with
            // DEFAULT children BBOX (24 B), BSPH (16 B), TRIS (8 B, counts).
            if sub.id.trim() == "COLD" {
                let mut pos = 0usize;
                match read_len_string(&sub.data, &mut pos) {
                    Some(_) if pos != sub.data.len() => report.error(
                        "DTRM/COLD",
                        format!(
                            "data section is {} bytes but the name string ends at {}; HODOR stores the name only and the engine misreads trailing bounds",
                            sub.data.len(),
                            pos
                        ),
                    ),
                    None => report.error(
                        "DTRM/COLD",
                        "cannot read collision name (truncated COLD)".to_string(),
                    ),
                    _ => {}
                }
                let mut seen = Vec::new();
                for child in &sub.children {
                    seen.push(child.id.clone());
                    let want = match child.id.trim() {
                        "BBOX" => Some(24),
                        "BSPH" => Some(16),
                        "TRIS" => Some(8),
                        _ => None,
                    };
                    match want {
                        Some(n) if child.data.len() != n => report.error(
                            "DTRM/COLD",
                            format!(
                                "child '{}' is {} bytes, HODOR writes {}",
                                child.id,
                                child.data.len(),
                                n
                            ),
                        ),
                        None => report.error(
                            "DTRM/COLD",
                            format!(
                                "unexpected child '{}'; HODOR COLD holds BBOX/BSPH/TRIS only (a FORM-wrapped TRIS desyncs the engine parser)",
                                child.id
                            ),
                        ),
                        _ => {}
                    }
                }
                report.info("DTRM/COLD", format!("children: {:?}", seen));
            }
            // KDOP: mirror parse_kdop_collision_mesh strictly. The engine
            // builds its collision tree from these exact fields.
            if sub.id.trim() == "KDOP" {
                let d = &sub.data;
                const KDOP_HEADER: usize = 28 + 13 * 32;
                if d.len() < KDOP_HEADER + 8 {
                    report.error(
                        "DTRM/KDOP",
                        format!(
                            "truncated KDOP ({} bytes, need {})",
                            d.len(),
                            KDOP_HEADER + 8
                        ),
                    );
                } else {
                    let get_f = |o: usize| f32::from_le_bytes(d[o..o + 4].try_into().unwrap());
                    let radius = get_f(0);
                    let min_e = [get_f(4), get_f(8), get_f(12)];
                    let max_e = [get_f(16), get_f(20), get_f(24)];
                    for (label, v) in [
                        ("radius", radius),
                        ("min.x", min_e[0]),
                        ("min.y", min_e[1]),
                        ("min.z", min_e[2]),
                        ("max.x", max_e[0]),
                        ("max.y", max_e[1]),
                        ("max.z", max_e[2]),
                    ] {
                        if !v.is_finite() {
                            report.error(
                                "DTRM/KDOP",
                                format!("non-finite bound {}={}", label, v),
                            );
                        }
                    }
                    for i in 0..3 {
                        if min_e[i] > max_e[i] {
                            report.error(
                                "DTRM/KDOP",
                                format!(
                                    "min extents exceed max on axis {} ({} > {})",
                                    i, min_e[i], max_e[i]
                                ),
                            );
                        }
                    }
                    let mut pos = KDOP_HEADER;
                    let vcount =
                        u32::from_le_bytes(d[pos..pos + 4].try_into().unwrap()) as usize;
                    pos += 4;
                    if vcount > u16::MAX as usize + 1 {
                        report.error(
                            "DTRM/KDOP",
                            format!("vertex count {} exceeds u16 range", vcount),
                        );
                    } else if pos + vcount * 12 + 4 > d.len() {
                        report.error(
                            "DTRM/KDOP",
                            format!(
                                "vertex buffer overruns KDOP (need {} bytes, have {})",
                                vcount * 12 + 4,
                                d.len() - pos
                            ),
                        );
                    } else {
                        pos += vcount * 12;
                        let fcount =
                            u32::from_le_bytes(d[pos..pos + 4].try_into().unwrap()) as usize;
                        pos += 4;
                        if pos + fcount * 3 * 2 > d.len() {
                            report.error(
                                "DTRM/KDOP",
                                format!(
                                    "index buffer overruns KDOP (need {} bytes, have {})",
                                    fcount * 3 * 2,
                                    d.len() - pos
                                ),
                            );
                        } else {
                            let mut bad = 0usize;
                            for i in 0..fcount * 3 {
                                let idx = u16::from_le_bytes(
                                    d[pos + i * 2..pos + i * 2 + 2].try_into().unwrap(),
                                ) as usize;
                                if vcount == 0 || idx >= vcount {
                                    bad += 1;
                                }
                            }
                            if bad > 0 {
                                report.error(
                                    "DTRM/KDOP",
                                    format!(
                                        "{}/{} KDOP indices out of range for {} vertices",
                                        bad,
                                        fcount * 3,
                                        vcount
                                    ),
                                );
                            }
                            pos += fcount * 3 * 2;
                        }
                        if pos != d.len() {
                            report.info(
                                "DTRM/KDOP",
                                format!("{} trailing bytes after index buffer", d.len() - pos),
                            );
                        }
                    }
                }
            }
            match sub.id.trim() {
                "HIER" => {
                    for (name, parent) in parse_hier_names(&sub.data) {
                        if !parent.is_empty()
                            && !joint_names.iter().any(|j| j == &parent)
                            && joint_names.iter().any(|_| true)
                        {
                            // parent must have been defined earlier; HODOR writes parents first.
                            report.warning(
                                "HIER",
                                format!(
                                    "joint '{}' references parent '{}' before its definition",
                                    name, parent
                                ),
                            );
                        }
                        joint_names.push(name);
                    }
                    report.info("HIER", format!("{} joints", joint_names.len()));
                }
                "MRKS" => {
                    let mut r = Cursor::new(&sub.data);
                    if let Ok(count) = r.read_u32::<LittleEndian>() {
                        for _ in 0..count.min(100000) {
                            let start = r.position() as usize;
                            let mut pos = start;
                            let mname = read_len_string(&sub.data, &mut pos);
                            let mparent = read_len_string(&sub.data, &mut pos);
                            match (mname, mparent) {
                                (Some(n), Some(p)) => {
                                    pos += 48; // 6 x f64
                                    if pos > sub.data.len() {
                                        break;
                                    }
                                    r.set_position(pos as u64);
                                    marker_parents.push((n, p));
                                }
                                _ => break,
                            }
                        }
                    }
                }
                "MRKR" => {
                    let mut pos = 0usize;
                    if let (Some(n), Some(p)) = (
                        read_len_string(&sub.data, &mut pos),
                        read_len_string(&sub.data, &mut pos),
                    ) {
                        marker_parents.push((n, p));
                    }
                }
                _ => {}
            }
        }
        // BMSH walk lives in HVMD (MSHL/MULT), but parents resolve against HIER.
        let _ = &joint_names;
    }

    // HVMD mesh walk (MSHL + MULT) in file order, sharing pool cursors.
    if let Some(hvmd) = chunks.iter().find(|c| c.id == "HVMD") {
        for sub in &hvmd.children {
            match sub.id.trim() {
                "MSHL" => {
                    for mesh_chunk in &sub.children {
                        if mesh_chunk.id.trim() != "BMSH" {
                            continue;
                        }
                        let loc = "MSHL/BMSH".to_string();
                        match parse_bmsh_header(
                            &mesh_chunk.data,
                            mesh_chunk.version,
                            is_v2,
                            &loc,
                            &mut report,
                        ) {
                            Some((lod, parts)) => {
                                if is_v2 {
                                    if let (Some(mp), Some(fp)) =
                                        (mesh_pool_cursor.as_mut(), face_pool_cursor.as_mut())
                                    {
                                        check_bmsh_v2(
                                            "MSHL",
                                            lod,
                                            &parts,
                                            mesh_chunk.version,
                                            stat_shaders.len(),
                                            mp,
                                            fp,
                                            &mut report,
                                        );
                                    }
                                    mesh_parents.push(("MSHL".to_string(), "Root".to_string()));
                                }
                            }
                            None => report.error(loc, "cannot parse BMSH header".to_string()),
                        }
                    }
                }
                "MULT" | "GOBG" => {
                    let loc = format!("{}/MULT", sub.id.trim());
                    // Recover mesh/parent names for ref checks.
                    let mut pos = 0usize;
                    let mname = read_len_string(&sub.data, &mut pos).unwrap_or_default();
                    let mparent = read_len_string(&sub.data, &mut pos).unwrap_or_default();
                    mesh_parents.push((mname.clone(), mparent.clone()));
                    for (bmsh_data, version) in collect_bmsh_from_mult(&sub.data, &mut report, &loc)
                    {
                        match parse_bmsh_header(&bmsh_data, version, is_v2, &loc, &mut report) {
                            Some((lod, parts)) => {
                                if is_v2 {
                                    if let (Some(mp), Some(fp)) =
                                        (mesh_pool_cursor.as_mut(), face_pool_cursor.as_mut())
                                    {
                                        check_bmsh_v2(
                                            &mname,
                                            lod,
                                            &parts,
                                            version,
                                            stat_shaders.len(),
                                            mp,
                                            fp,
                                            &mut report,
                                        );
                                    }
                                }
                            }
                            None => report.error(loc.clone(), "cannot parse BMSH header".to_string()),
                        }
                    }
                }
                _ => {}
            }
        }
    }

    // Pool leftovers: any significant unconsumed mesh/face bytes indicate a
    // desync between BMSH headers and pool contents.
    if let (Some(mp), Some(fp), Some(p)) = (
        mesh_pool_cursor.as_ref(),
        face_pool_cursor.as_ref(),
        pools.as_ref(),
    ) {
        let mesh_left = p.mesh.len() as u64 - mp.position();
        let face_left = p.face.len() as u64 - fp.position();
        if mesh_left > 0 {
            report.warning(
                "POOL/mesh",
                format!("{} mesh-pool bytes never consumed by any BMSH part", mesh_left),
            );
        }
        if face_left > 0 {
            report.warning(
                "POOL/face",
                format!("{} face-pool bytes never consumed by any BMSH part", face_left),
            );
        }
        if verbose {
            report.info(
                "POOL",
                format!(
                    "consumed mesh {}/{} face {}/{}",
                    p.mesh.len() as u64 - mesh_left,
                    p.mesh.len(),
                    p.face.len() as u64 - face_left,
                    p.face.len()
                ),
            );
        }
    }

    // Reference checks.
    for (mesh, parent) in &mesh_parents {
        if parent.is_empty() || parent == "Root" || parent == "MSHL" {
            continue;
        }
        if !joint_names.iter().any(|j| j == parent) {
            report.error(
                format!("mesh '{}'", mesh),
                format!(
                    "parent joint '{}' not found in HIER; engine joint lookup fails",
                    parent
                ),
            );
        }
    }
    for (marker, parent) in &marker_parents {
        if !joint_names.iter().any(|j| j == parent) {
            report.error(
                format!("marker '{}'", marker),
                format!("parent joint '{}' not found in HIER", parent),
            );
        }
    }

    report.print();
    let errors = report.error_count();
    println!(
        "  -> {} error(s), {} total finding(s)",
        errors,
        report.findings.len()
    );
    if errors > 0 {
        1
    } else {
        0
    }
}

fn w_h_zero(sub: &IffChunk) -> bool {
    let mut pos = 0usize;
    if read_len_string(&sub.data, &mut pos).is_none() {
        return false;
    }
    pos += 4; // format
    if pos + 4 > sub.data.len() {
        return false;
    }
    pos += 4; // mip count
    if pos + 8 > sub.data.len() {
        return false;
    }
    let w = u32::from_le_bytes(sub.data[pos..pos + 4].try_into().unwrap());
    let h = u32::from_le_bytes(sub.data[pos + 4..pos + 8].try_into().unwrap());
    w == 0 || h == 0
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 2 {
        eprintln!("usage: validate_hod [--verbose] <file.hod> [file.hod ...]");
        std::process::exit(2);
    }
    let verbose = args.iter().any(|a| a == "--verbose");
    let files: Vec<&String> = args[1..].iter().filter(|a| *a != "--verbose").collect();
    if files.is_empty() {
        eprintln!("usage: validate_hod [--verbose] <file.hod> [file.hod ...]");
        std::process::exit(2);
    }
    let mut worst = 0;
    for path in files {
        let code = validate_file(path, verbose);
        worst = worst.max(code);
    }
    std::process::exit(worst);
}
