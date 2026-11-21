use byteorder::{LittleEndian, ReadBytesExt};
use hwr_hod_parser::iff::IffChunk;
use std::fs::File;
use std::io::{Cursor, Read};

fn read_len_string(r: &mut Cursor<&Vec<u8>>) -> Result<String, String> {
    let len = r.read_u32::<LittleEndian>().map_err(|e| e.to_string())? as usize;
    if len > 10000 {
        return Err(format!("unreasonable string len {}", len));
    }
    let mut buf = vec![0u8; len];
    r.read_exact(&mut buf).map_err(|e| e.to_string())?;
    Ok(String::from_utf8_lossy(&buf).to_string())
}

fn main() {
    let path = std::env::args()
        .nth(1)
        .expect("usage: dump_pool <file.hod>");
    let mut file = File::open(&path).expect("Failed to open HOD");
    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer).expect("Failed to read");

    let mut cursor = Cursor::new(&buffer);
    let mut chunks = Vec::new();
    while (cursor.position() as usize) < buffer.len() {
        match IffChunk::read_chunk(&mut cursor) {
            Ok(c) => chunks.push(c),
            Err(e) => {
                println!("Error reading chunk: {}", e);
                break;
            }
        }
    }

    println!("file: {}", path);
    for chunk in &chunks {
        println!("top: id='{}' type={:?} data_len={} children={}", chunk.id, chunk.chunk_type, chunk.data.len(), chunk.children.len());
    }

    // POOL header
    for chunk in &chunks {
        if chunk.id == "POOL" && chunk.data.len() >= 16 {
            let mut r = Cursor::new(&chunk.data);
            let pool_type = r.read_u32::<LittleEndian>().unwrap_or(0);
            let comp_tex = r.read_u32::<LittleEndian>().unwrap_or(0);
            let decomp_tex = r.read_u32::<LittleEndian>().unwrap_or(0);
            let comp_mesh = r.read_u32::<LittleEndian>().unwrap_or(0);
            let decomp_mesh = r.read_u32::<LittleEndian>().unwrap_or(0);
            println!("\nPOOL: type={} comp_tex={} decomp_tex={} comp_mesh={} decomp_mesh={} (total data {})",
                pool_type, comp_tex, decomp_tex, comp_mesh, decomp_mesh, chunk.data.len());
        }
    }

    // STAT chunks with full raw dump
    for chunk in &chunks {
        let mut stack: Vec<&IffChunk> = vec![chunk];
        while let Some(c) = stack.pop() {
            for sub in &c.children {
                stack.push(sub);
            }
            let id = c.id.trim();
            if id == "STAT" || id == "MATT" {
                println!("\n===== {} ({} bytes) =====", id, c.data.len());
                let data = &c.data;
                for (row, slice) in data.chunks(16).enumerate() {
                    let hex: Vec<String> = slice.iter().map(|b| format!("{:02x}", b)).collect();
                    let ascii: String = slice
                        .iter()
                        .map(|&b| if b >= 32 && b <= 126 { b as char } else { '.' })
                        .collect();
                    println!("  {:04x}: {:<47} {}", row * 16, hex.join(" "), ascii);
                }
                let mut r = Cursor::new(data);
                if let Ok(mat_name) = read_len_string(&mut r) {
                    if let Ok(shader_name) = read_len_string(&mut r) {
                        if let Ok(param_count) = r.read_u32::<LittleEndian>() {
                            println!("  -> mat='{}' shader='{}' param_count={} (cursor at {})",
                                mat_name, shader_name, param_count, r.position());
                            for i in 0..param_count.min(16) {
                                let pos = r.position();
                                let a = r.read_u32::<LittleEndian>().unwrap_or(999);
                                let b = r.read_u32::<LittleEndian>().unwrap_or(999);
                                let idx = r.read_u32::<LittleEndian>().unwrap_or(999);
                                let name = read_len_string(&mut r).unwrap_or("<err>".into());
                                println!("     param[{}] @{}: extra=({}, {}) tex_idx={} name='{}'", i, pos, a, b, idx, name);
                            }
                            println!("     cursor after params: {} of {}", r.position(), data.len());
                        }
                    }
                }
            } else if id == "LMIP" || id == "TEXM" {
                println!("\n===== {} ({} bytes) =====", id, c.data.len());
                let data = &c.data;
                for (row, slice) in data.chunks(16).enumerate() {
                    if row > 6 {
                        println!("  ... ({} more bytes)", data.len() - 16 * 6);
                        break;
                    }
                    let hex: Vec<String> = slice.iter().map(|b| format!("{:02x}", b)).collect();
                    let ascii: String = slice
                        .iter()
                        .map(|&b| if b >= 32 && b <= 126 { b as char } else { '.' })
                        .collect();
                    println!("  {:04x}: {:<47} {}", row * 16, hex.join(" "), ascii);
                }
            }
        }
    }
}
