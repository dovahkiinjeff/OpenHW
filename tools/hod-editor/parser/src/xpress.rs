use byteorder::{LittleEndian, WriteBytesExt};

// HODOR's decompressor lookup tables (extracted from HODOR.exe via Ghidra)
// Match decode table: 8 entries, each is (mask, shift_lo, mask2, shift_hi, consumed_bytes)
// Indexed by (word & 7) — the low 3 bits of the match word
const MATCH_TABLE: [(u32, u32, u32, u32, usize); 8] = [
    (0x000000FF, 2, 0x00, 0, 1),  // Type 0: 1-byte, offset 6 bits, length 3
    (0x0000FFFF, 2, 0x00, 0, 2),  // Type 1: 2-byte, offset 14 bits, length 3
    (0x0000FFFF, 6, 0x0F, 2, 2),  // Type 2: 2-byte, offset 12 bits, length 3-18
    (0x00FFFFFF, 8, 0x1F, 3, 3),  // Type 3: 3-byte, offset 21 bits, length 3-34
    (0x000000FF, 2, 0x00, 0, 1),  // Type 4: same as Type 0
    (0x0000FFFF, 2, 0x00, 0, 2),  // Type 5: same as Type 1
    (0x0000FFFF, 6, 0x0F, 2, 2),  // Type 6: same as Type 2
    (0xFFFFFFFF, 11, 0xFF, 3, 4), // Type 7: 4-byte, offset 21 bits, length 3-258
];

// Literal count table: indexed by (indicator & 0xF)
// Maps the low 4 bits of the shifted indicator to how many literal bytes to process.
// The pattern: the number of trailing 0 bits in the low nibble determines the count.
// Index 0 (0000) = 4 literals (4 trailing zeros)
// Index 2 (0010) = 1 literal (1 trailing zero, then a 1)
// Index 4 (0100) = 2 literals (2 trailing zeros, then a 1)
// Index 6 (0110) = 1 literal (1 trailing zero, then a 1)
// Index 8 (1000) = 3 literals (3 trailing zeros, then a 1)
// Odd indices = 0 (shouldn't occur in literal path; means "match")
const LITERAL_TABLE: [u8; 16] = [4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0];

/// Decompresses a buffer using HODOR's MS XPress LZ77 algorithm.
///
/// This matches the decompressor found in HODOR.exe at FUN_00448600 (316 bytes).
/// Uses lookup tables for match decoding and batch literal processing.
pub fn decompress(input: &[u8], output_size: usize) -> Result<Vec<u8>, String> {
    let mut output = vec![0u8; output_size];
    let mut output_idx = 0;
    let mut input_idx = 0;
    let mut indicator = 1u32; // Starts at 1 to trigger read on first iteration
    let input_len = input.len();

    while output_idx < output_size {
        // Read new indicator word when exhausted
        if indicator == 1 {
            if input_idx + 4 > input_len {
                break;
            }
            indicator = u32::from_le_bytes([
                input[input_idx],
                input[input_idx + 1],
                input[input_idx + 2],
                input[input_idx + 3],
            ]);
            input_idx += 4;
        }

        if (indicator & 1) == 0 {
            // Literal(s) — batch process using lookup table
            let table_idx = (indicator & 0xF) as usize;
            let mut lit_count = LITERAL_TABLE[table_idx] as usize;
            if lit_count == 0 {
                lit_count = 1;
            }

            // Copy lit_count bytes from input to output
            for _ in 0..lit_count {
                if output_idx >= output_size || input_idx >= input_len {
                    break;
                }
                output[output_idx] = input[input_idx];
                output_idx += 1;
                input_idx += 1;
            }
            indicator >>= lit_count;
        } else {
            // Match
            if input_idx + 4 > input_len {
                break;
            }

            let word = u32::from_le_bytes([
                input[input_idx],
                input[input_idx + 1],
                input[input_idx + 2],
                input[input_idx + 3],
            ]);

            let match_type = (word & 7) as usize;
            let (mask, shift_lo, mask2, shift_hi, consumed) = MATCH_TABLE[match_type];

            let offset = ((word & mask) >> shift_lo) as usize;
            let length = (((word >> shift_hi) & mask2) as usize) + 3;
            input_idx += consumed;

            // Copy from sliding window
            if offset == 0 {
                // Zero offset: fill with zeros
                for _ in 0..length {
                    if output_idx >= output_size {
                        break;
                    }
                    output[output_idx] = 0;
                    output_idx += 1;
                }
            } else if offset < 4 {
                // Small offset: HODOR's special case
                // Byte-by-byte copy first 3 bytes, then adjust source
                let src_start = output_idx.wrapping_sub(offset);
                let mut i = 0;
                while i < 3 && i < length {
                    if output_idx >= output_size {
                        break;
                    }
                    let src = src_start.wrapping_add(i);
                    if src < output.len() {
                        output[output_idx] = output[src];
                    }
                    output_idx += 1;
                    i += 1;
                }
                // Adjust source: subtract (2 + (offset & 1))
                let adjusted_src = src_start.wrapping_sub(2 + (offset & 1));
                while i < length {
                    if output_idx >= output_size {
                        break;
                    }
                    let src = adjusted_src.wrapping_add(i);
                    if src < output.len() {
                        output[output_idx] = output[src];
                    }
                    output_idx += 1;
                    i += 1;
                }
            } else {
                // Normal copy (offset >= 4)
                let src_start = output_idx.wrapping_sub(offset);
                for i in 0..length {
                    if output_idx >= output_size {
                        break;
                    }
                    let src = src_start.wrapping_add(i);
                    if src < output.len() {
                        output[output_idx] = output[src];
                    }
                    output_idx += 1;
                }
            }

            indicator >>= 1;
        }
    }

    if output_idx < output_size {
        output.truncate(output_idx);
    }
    Ok(output)
}

/// Compresses a buffer using HODOR-compatible MS XPress LZ77 algorithm.
///
/// Matches HODOR's compressor (FUN_004482a0, 843 bytes):
/// - Indicator starts at 0x80000000 (bit 31 = sentinel)
/// - Each operation (literal or match) consumes 1 indicator bit
/// - Indicator written when 31 data bits consumed (bit 31 always set as sentinel)
/// - Literal bytes written one at a time; decompressor batches naturally
/// - Match encoding uses HODOR's FUN_004481d0 format
pub fn compress(input: &[u8]) -> Vec<u8> {
    let input_len = input.len();
    if input_len == 0 {
        return Vec::new();
    }

    let mut compressed = Vec::with_capacity(input_len / 2);
    let mut input_idx = 0;

    // HODOR starts with indicator = 0x80000000 (bit 31 = sentinel)
    // No initial zero word — indicator written only after 31 data bits consumed
    let mut indicator_pos = compressed.len();
    compressed.write_u32::<LittleEndian>(0).unwrap();
    let mut indicator = 0x80000000u32;
    let mut indicator_bit: u32 = 0;

    // Hash table for match finding (same as before)
    let mut head = vec![-1i32; 65536];
    let mut prev = vec![-1i32; input_len];

    // Pre-compute match positions
    let mut match_at = vec![None::<(usize, usize, usize)>; input_len]; // (offset, length, type)
    for pos in 0..input_len {
        if pos + 3 > input_len {
            continue;
        }
        let hash = (((input[pos] as usize) << 10)
            ^ ((input[pos + 1] as usize) << 5)
            ^ (input[pos + 2] as usize))
            & 0xFFFF;

        let mut start = head[hash];
        let mut chain_len = 0;
        let mut best_length = 0usize;
        let mut best_offset = 0usize;

        let max_chain = if input_len > 100_000 { 32 } else { 256 };
        while start != -1 && chain_len < max_chain {
            let start_idx = start as usize;
            let offset = pos - start_idx;
            if offset > 0x1FFFFF {
                break;
            }

            let mut len = 0;
            while pos + len < input_len
                && start_idx + len < input_len
                && input[start_idx + len] == input[pos + len]
            {
                len += 1;
                if len >= 258 {
                    break;
                }
            }

            if len >= 3 && len > best_length {
                if find_best_match_type(offset, len).is_some() {
                    best_length = len;
                    best_offset = offset;
                }
            }

            start = prev[start_idx];
            chain_len += 1;
        }

        if best_length >= 3 {
            let mt = find_best_match_type(best_offset, best_length).unwrap();
            match_at[pos] = Some((best_offset, best_length, mt));
        }

        prev[pos] = head[hash];
        head[hash] = pos as i32;
    }

    // Process input — each operation (literal or match) consumes 1 indicator bit
    while input_idx < input_len {
        // Flush indicator word if 31 data bits consumed
        if indicator_bit >= 31 {
            indicator |= 1u32 << 31; // Ensure sentinel is set
            let bytes = indicator.to_le_bytes();
            compressed[indicator_pos..indicator_pos + 4].copy_from_slice(&bytes);

            indicator_pos = compressed.len();
            compressed.write_u32::<LittleEndian>(0).unwrap();
            indicator = 0x80000000; // New word starts with sentinel
            indicator_bit = 0;
        }

        // Lazy matching: if we have a match here, check if the NEXT position has a BETTER match!
        if let Some((offset, length, mt)) = match_at[input_idx] {
            let mut take_current = true;
            if input_idx + 1 < input_len {
                if let Some((next_offset, next_length, _)) = match_at[input_idx + 1] {
                    // If next match is strictly better (longer and compensates for the 1 literal we'd have to write)
                    if next_length > length + 1 {
                        take_current = false;
                    }
                }
            }

            if take_current {
                indicator |= 1 << indicator_bit;
                indicator_bit += 1;
                encode_match(&mut compressed, mt, offset, length);
                input_idx += length;
            } else {
                indicator_bit += 1;
                compressed.push(input[input_idx]);
                input_idx += 1;
            }
        } else {
            // Literal — bit stays 0, write one byte
            indicator_bit += 1;
            compressed.push(input[input_idx]);
            input_idx += 1;
        }
    }

    // Write final indicator word (with sentinel bit)
    indicator |= 1u32 << 31;
    let bytes = indicator.to_le_bytes();
    compressed[indicator_pos..indicator_pos + 4].copy_from_slice(&bytes);

    // The decompressor processes 31 bits per indicator word. If the final word
    // has fewer than 31 data bits, the remaining zero bits are "literal"
    // operations that consume bytes from the input stream. We must provide
    // enough padding bytes so the decompressor doesn't read past our output
    // into the next chunk's data. HODOR writes 4 zero bytes; we write 32
    // to cover the worst case (31 literal operations in a partial final word).
    compressed.extend_from_slice(&[0u8; 32]);

    compressed
}

/// Finds the best match type for a given offset and length.
/// Priority order matches HODOR's FUN_004481d0: 0 → 1 → 2 → 3 → 7
fn find_best_match_type(offset: usize, length: usize) -> Option<usize> {
    if length == 3 {
        // Type 0: 1-byte, offset 0-63
        if offset <= 0x3F {
            return Some(0);
        }
        // Type 1: 2-byte, offset 0-16383
        if offset <= 0x3FFF {
            return Some(1);
        }
    }

    let len_code = length - 3;

    // Type 2: 2-byte, offset 0-1023, len_code 0-15 (length 3-18)
    if len_code < 0x10 && offset <= 0x3FF {
        return Some(2);
    }

    // Type 3: 3-byte, len_code 0-31 (length 3-34), offset 0-65535
    if len_code < 0x20 && offset <= 0xFFFF {
        return Some(3);
    }

    // Type 7: 4-byte, len_code 0-255 (length 3-258), offset 0-2097151
    if len_code < 0x100 && offset <= 0x1FFFFF {
        // Prevent bloat: do not encode a 3-byte match as a 4-byte token!
        if length >= 4 {
            return Some(7);
        }
    }

    None
}

/// Encodes a match using HODOR's format (FUN_004481d0).
fn encode_match(compressed: &mut Vec<u8>, match_type: usize, offset: usize, length: usize) {
    let len_code = (length - 3) as u32;
    let offset_code = offset as u32;

    match match_type {
        0 => {
            // Type 0: 1-byte, word = offset << 2
            compressed.push(((offset_code << 2) & 0xFF) as u8);
        }
        1 => {
            // Type 1: 2-byte, word = (offset << 2) | 1
            let word = ((offset_code << 2) | 1) as u16;
            compressed.push((word & 0xFF) as u8);
            compressed.push(((word >> 8) & 0xFF) as u8);
        }
        2 => {
            // Type 2: 2-byte, word = ((offset << 4) | len_code) << 2 | 2
            let word = (((offset_code << 4) | len_code) << 2 | 2) as u16;
            compressed.push((word & 0xFF) as u8);
            compressed.push(((word >> 8) & 0xFF) as u8);
        }
        3 => {
            // Type 3: 3-byte, word = ((offset << 5) | len_code) << 3 | 3
            let word = ((offset_code << 5) | len_code) << 3 | 3;
            compressed.push((word & 0xFF) as u8);
            compressed.push(((word >> 8) & 0xFF) as u8);
            compressed.push(((word >> 16) & 0xFF) as u8);
        }
        7 => {
            // Type 7: 4-byte, word = ((offset << 8) | len_code) << 3 | 7
            let word = ((offset_code << 8) | len_code) << 3 | 7;
            compressed.push((word & 0xFF) as u8);
            compressed.push(((word >> 8) & 0xFF) as u8);
            compressed.push(((word >> 16) & 0xFF) as u8);
            compressed.push(((word >> 24) & 0xFF) as u8);
        }
        _ => unreachable!("Invalid match type: {}", match_type),
    }
}

/// Compresses when useful, otherwise returns the input unchanged.
pub fn compress_or_raw(input: &[u8]) -> Vec<u8> {
    let compressed = compress(input);
    let ratio = compressed.len() as f64 / input.len() as f64;

    // Only use compression if it actually reduces size (at least 10% savings)
    if ratio < 0.9 {
        compressed
    } else {
        input.to_vec()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_xpress_roundtrip_basic() {
        let original =
            b"Hello World! Hello World! Hello World! This is MS XPress LZ77 algorithm test!"
                .to_vec();
        let compressed = compress(&original);
        let decompressed = decompress(&compressed, original.len()).unwrap();
        assert_eq!(original.as_slice(), decompressed.as_slice());
    }

    #[test]
    fn test_xpress_roundtrip_repetitive() {
        let mut original = Vec::new();
        for _ in 0..100 {
            original.extend_from_slice(b"ABCDEFGHIJ");
        }
        let compressed = compress(&original);
        let decompressed = decompress(&compressed, original.len()).unwrap();
        assert_eq!(original.as_slice(), decompressed.as_slice());
        assert!(
            compressed.len() < original.len(),
            "Compression should reduce size for repetitive data"
        );
    }

    #[test]
    fn test_xpress_roundtrip_random() {
        let original: Vec<u8> = (0..1000).map(|i| ((i * 7 + 13) & 0xFF) as u8).collect();
        let compressed = compress(&original);
        let decompressed = decompress(&compressed, original.len()).unwrap();
        assert_eq!(original.as_slice(), decompressed.as_slice());
    }

    #[test]
    fn test_xpress_empty() {
        let original = Vec::new();
        let compressed = compress(&original);
        assert!(compressed.is_empty());
    }

    #[test]
    fn test_xpress_single_byte() {
        let original = vec![42u8];
        let compressed = compress(&original);
        let decompressed = decompress(&compressed, original.len()).unwrap();
        assert_eq!(original.as_slice(), decompressed.as_slice());
    }

    #[test]
    fn test_xpress_match_type_3() {
        let mut original = vec![0u8; 10000];
        for i in 0..10 {
            original[5000 + i] = (i + 1) as u8;
        }
        for i in 0..10 {
            original[8000 + i] = (i + 1) as u8;
        }
        let compressed = compress(&original);
        let decompressed = decompress(&compressed, original.len()).unwrap();
        assert_eq!(original.as_slice(), decompressed.as_slice());
    }

    #[test]
    fn test_xpress_debug_roundtrip() {
        let original = b"Hello World! Hello World! Hello World!".to_vec();
        let compressed = compress(&original);

        // Print compressed hex
        let hex: Vec<String> = compressed.iter().map(|b| format!("{:02X}", b)).collect();
        eprintln!("Compressed ({} bytes): {}", compressed.len(), hex.join(" "));

        let decompressed = decompress(&compressed, original.len()).unwrap();

        let hex2: Vec<String> = decompressed.iter().map(|b| format!("{:02X}", b)).collect();
        eprintln!(
            "Decompressed ({} bytes): {}",
            decompressed.len(),
            hex2.join(" ")
        );

        // Compare
        for i in 0..original.len().min(decompressed.len()) {
            if original[i] != decompressed[i] {
                eprintln!(
                    "MISMATCH at {}: expected 0x{:02X}, got 0x{:02X}",
                    i, original[i], decompressed[i]
                );
            }
        }

        assert_eq!(original.as_slice(), decompressed.as_slice());
    }

    #[test]
    fn test_xpress_hodor_mesh_pool() {
        let fixture_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("../testing/ter_centaur/rtl_test");
        let hodor_comp_path = fixture_dir.join("hodor_comp_mesh.bin");
        let expected_decomp_path = fixture_dir.join("decomp_mesh.bin");
        if !hodor_comp_path.exists() || !expected_decomp_path.exists() {
            eprintln!(
                "skipping test_xpress_hodor_mesh_pool: fixtures not present under testing/ter_centaur/rtl_test"
            );
            return;
        }

        let hodor_comp = std::fs::read(hodor_comp_path).unwrap();
        let expected_decomp = std::fs::read(expected_decomp_path).unwrap();

        let our_decomp = decompress(&hodor_comp, expected_decomp.len()).unwrap();
        assert_eq!(
            our_decomp, expected_decomp,
            "Our decompressor should handle HODOR's compressed data"
        );
    }
}
