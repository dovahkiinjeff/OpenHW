use crate::types::{base64_encode, HODTexture};

pub fn compress_dxt1(rgba: &[u8], width: usize, height: usize) -> Vec<u8> {
    let out_size = ((width.max(1) + 3) / 4) * ((height.max(1) + 3) / 4) * 8;
    let mut out = vec![0u8; out_size];
    texpresso::Format::Bc1.compress(rgba, width, height, texpresso::Params::default(), &mut out);
    out
}

pub fn compress_dxt5(rgba: &[u8], width: usize, height: usize) -> Vec<u8> {
    let out_size = ((width.max(1) + 3) / 4) * ((height.max(1) + 3) / 4) * 16;
    let mut out = vec![0u8; out_size];
    texpresso::Format::Bc3.compress(rgba, width, height, texpresso::Params::default(), &mut out);
    out
}

pub fn compress_dxt3(rgba: &[u8], width: usize, height: usize) -> Vec<u8> {
    let out_size = ((width.max(1) + 3) / 4) * ((height.max(1) + 3) / 4) * 16;
    let mut out = vec![0u8; out_size];
    texpresso::Format::Bc2.compress(rgba, width, height, texpresso::Params::default(), &mut out);
    out
}

pub fn generate_mip_chain(
    rgba: &[u8],
    width: usize,
    height: usize,
    max_mips: usize,
) -> Vec<(Vec<u8>, usize, usize)> {
    let mut mips = Vec::new();
    let mut cur_rgba = rgba.to_vec();
    let mut cur_w = width;
    let mut cur_h = height;

    for _ in 0..max_mips {
        mips.push((cur_rgba.clone(), cur_w, cur_h));
        if cur_w <= 1 && cur_h <= 1 {
            break;
        }
        let next_w = std::cmp::max(1, cur_w / 2);
        let next_h = std::cmp::max(1, cur_h / 2);

        if let Some(img) = image::ImageBuffer::<image::Rgba<u8>, _>::from_raw(cur_w as u32, cur_h as u32, cur_rgba.clone()) {
            let resized = image::imageops::resize(
                &img,
                next_w as u32,
                next_h as u32,
                image::imageops::FilterType::Lanczos3,
            );
            cur_rgba = resized.into_raw();
        }

        cur_w = next_w;
        cur_h = next_h;
    }
    mips
}

pub fn decompress_dxt1(data: &[u8], width: usize, height: usize) -> Vec<u8> {
    let mut rgba = vec![0u8; width * height * 4];
    let blocks_x = (width + 3) / 4;
    let blocks_y = (height + 3) / 4;
    let mut offset = 0;

    for by in 0..blocks_y {
        for bx in 0..blocks_x {
            if offset + 8 > data.len() {
                break;
            }
            let color0 = u16::from_le_bytes([data[offset], data[offset + 1]]);
            let color1 = u16::from_le_bytes([data[offset + 2], data[offset + 3]]);
            let code = u32::from_le_bytes([
                data[offset + 4],
                data[offset + 5],
                data[offset + 6],
                data[offset + 7],
            ]);
            offset += 8;

            let mut r = [0u8; 4];
            let mut g = [0u8; 4];
            let mut b = [0u8; 4];
            let mut a = [255u8; 4];

            // Decode RGB565
            r[0] = (((color0 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[0] = (((color0 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[0] = ((color0 & 0x1F) as u32 * 255 / 31) as u8;

            r[1] = (((color1 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[1] = (((color1 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[1] = ((color1 & 0x1F) as u32 * 255 / 31) as u8;

            if color0 > color1 {
                r[2] = ((r[0] as u32 * 2 + r[1] as u32) / 3) as u8;
                g[2] = ((g[0] as u32 * 2 + g[1] as u32) / 3) as u8;
                b[2] = ((b[0] as u32 * 2 + b[1] as u32) / 3) as u8;

                r[3] = ((r[0] as u32 + r[1] as u32 * 2) / 3) as u8;
                g[3] = ((g[0] as u32 + g[1] as u32 * 2) / 3) as u8;
                b[3] = ((b[0] as u32 + b[1] as u32 * 2) / 3) as u8;
            } else {
                r[2] = ((r[0] as u32 + r[1] as u32) / 2) as u8;
                g[2] = ((g[0] as u32 + g[1] as u32) / 2) as u8;
                b[2] = ((b[0] as u32 + b[1] as u32) / 2) as u8;

                r[3] = 0;
                g[3] = 0;
                b[3] = 0;
                a[3] = 0;
            }

            for y in 0..4 {
                for x in 0..4 {
                    let px = bx * 4 + x;
                    let py = by * 4 + y;
                    if px < width && py < height {
                        let i = y * 4 + x;
                        let select = ((code >> (2 * i)) & 0x03) as usize;
                        let pixel_offset = (py * width + px) * 4;
                        rgba[pixel_offset] = r[select];
                        rgba[pixel_offset + 1] = g[select];
                        rgba[pixel_offset + 2] = b[select];
                        rgba[pixel_offset + 3] = a[select];
                    }
                }
            }
        }
    }
    rgba
}

pub fn decompress_dxt5(data: &[u8], width: usize, height: usize) -> Vec<u8> {
    let mut rgba = vec![0u8; width * height * 4];
    let blocks_x = (width + 3) / 4;
    let blocks_y = (height + 3) / 4;
    let mut offset = 0;

    for by in 0..blocks_y {
        for bx in 0..blocks_x {
            if offset + 16 > data.len() {
                break;
            }

            // 1. Decode Alpha Block (8 bytes)
            let alpha0 = data[offset];
            let alpha1 = data[offset + 1];

            // Extract 48-bit value for 16 3-bit indices
            let mut alpha_mask = 0u64;
            for i in 0..6 {
                alpha_mask |= (data[offset + 2 + i] as u64) << (i * 8);
            }
            offset += 8;

            let mut alphas = [0u8; 8];
            alphas[0] = alpha0;
            alphas[1] = alpha1;
            if alpha0 > alpha1 {
                for i in 1..7 {
                    alphas[i + 1] =
                        (((7 - i) as u32 * alpha0 as u32 + i as u32 * alpha1 as u32) / 7) as u8;
                }
            } else {
                for i in 1..5 {
                    alphas[i + 1] =
                        (((5 - i) as u32 * alpha0 as u32 + i as u32 * alpha1 as u32) / 5) as u8;
                }
                alphas[6] = 0;
                alphas[7] = 255;
            }

            // 2. Decode Color Block (8 bytes, like DXT1 but color 2/3 are always interpolated)
            let color0 = u16::from_le_bytes([data[offset], data[offset + 1]]);
            let color1 = u16::from_le_bytes([data[offset + 2], data[offset + 3]]);
            let code = u32::from_le_bytes([
                data[offset + 4],
                data[offset + 5],
                data[offset + 6],
                data[offset + 7],
            ]);
            offset += 8;

            let mut r = [0u8; 4];
            let mut g = [0u8; 4];
            let mut b = [0u8; 4];

            r[0] = (((color0 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[0] = (((color0 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[0] = ((color0 & 0x1F) as u32 * 255 / 31) as u8;

            r[1] = (((color1 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[1] = (((color1 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[1] = ((color1 & 0x1F) as u32 * 255 / 31) as u8;

            r[2] = ((r[0] as u32 * 2 + r[1] as u32) / 3) as u8;
            g[2] = ((g[0] as u32 * 2 + g[1] as u32) / 3) as u8;
            b[2] = ((b[0] as u32 * 2 + b[1] as u32) / 3) as u8;

            r[3] = ((r[0] as u32 + r[1] as u32 * 2) / 3) as u8;
            g[3] = ((g[0] as u32 + g[1] as u32 * 2) / 3) as u8;
            b[3] = ((b[0] as u32 + b[1] as u32 * 2) / 3) as u8;

            // 3. Write pixels
            for y in 0..4 {
                for x in 0..4 {
                    let px = bx * 4 + x;
                    let py = by * 4 + y;
                    if px < width && py < height {
                        let i = y * 4 + x;
                        let select = ((code >> (2 * i)) & 0x03) as usize;
                        let alpha_select = ((alpha_mask >> (3 * i)) & 0x07) as usize;

                        let pixel_offset = (py * width + px) * 4;
                        rgba[pixel_offset] = r[select];
                        rgba[pixel_offset + 1] = g[select];
                        rgba[pixel_offset + 2] = b[select];
                        rgba[pixel_offset + 3] = alphas[alpha_select];
                    }
                }
            }
        }
    }
    rgba
}

pub fn decompress_dxt3(data: &[u8], width: usize, height: usize) -> Vec<u8> {
    let mut rgba = vec![0u8; width * height * 4];
    let blocks_x = (width + 3) / 4;
    let blocks_y = (height + 3) / 4;
    let mut offset = 0;

    for by in 0..blocks_y {
        for bx in 0..blocks_x {
            if offset + 16 > data.len() {
                break;
            }

            let alpha_offset = offset;
            let color_offset = offset + 8;
            offset += 16;

            let color0 = u16::from_le_bytes([data[color_offset], data[color_offset + 1]]);
            let color1 = u16::from_le_bytes([data[color_offset + 2], data[color_offset + 3]]);
            let code = u32::from_le_bytes([
                data[color_offset + 4],
                data[color_offset + 5],
                data[color_offset + 6],
                data[color_offset + 7],
            ]);

            let mut r = [0u8; 4];
            let mut g = [0u8; 4];
            let mut b = [0u8; 4];

            r[0] = (((color0 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[0] = (((color0 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[0] = ((color0 & 0x1F) as u32 * 255 / 31) as u8;

            r[1] = (((color1 >> 11) & 0x1F) as u32 * 255 / 31) as u8;
            g[1] = (((color1 >> 5) & 0x3F) as u32 * 255 / 63) as u8;
            b[1] = ((color1 & 0x1F) as u32 * 255 / 31) as u8;

            r[2] = ((r[0] as u32 * 2 + r[1] as u32) / 3) as u8;
            g[2] = ((g[0] as u32 * 2 + g[1] as u32) / 3) as u8;
            b[2] = ((b[0] as u32 * 2 + b[1] as u32) / 3) as u8;

            r[3] = ((r[0] as u32 + r[1] as u32 * 2) / 3) as u8;
            g[3] = ((g[0] as u32 + g[1] as u32 * 2) / 3) as u8;
            b[3] = ((b[0] as u32 + b[1] as u32 * 2) / 3) as u8;

            for y in 0..4 {
                for x in 0..4 {
                    let px = bx * 4 + x;
                    let py = by * 4 + y;
                    if px < width && py < height {
                        let i = y * 4 + x;
                        let alpha_byte = data[alpha_offset + i / 2];
                        let alpha4 = if i % 2 == 0 {
                            alpha_byte & 0x0F
                        } else {
                            alpha_byte >> 4
                        };
                        let select = ((code >> (2 * i)) & 0x03) as usize;

                        let pixel_offset = (py * width + px) * 4;
                        rgba[pixel_offset] = r[select];
                        rgba[pixel_offset + 1] = g[select];
                        rgba[pixel_offset + 2] = b[select];
                        rgba[pixel_offset + 3] = alpha4 * 17;
                    }
                }
            }
        }
    }
    rgba
}

pub(crate) fn compressed_texture_mip_size(format: &str, width: usize, height: usize) -> usize {
    let blocks_x = (width + 3) / 4;
    let blocks_y = (height + 3) / 4;
    match format {
        "DXT1" => blocks_x * blocks_y * 8,
        "DXT3" | "DXT5" => blocks_x * blocks_y * 16,
        _ => width * height * 4,
    }
}

pub(crate) fn encode_b64_png_thumbnail(rgba: &[u8], width: u32, height: u32, max_dim: u32) -> Option<String> {
    if rgba.is_empty() || width == 0 || height == 0 {
        return None;
    }
    let (target_w, target_h) = if width > max_dim || height > max_dim {
        let aspect = width as f32 / height as f32;
        if width > height {
            (max_dim, ((max_dim as f32 / aspect) as u32).max(1))
        } else {
            (((max_dim as f32 * aspect) as u32).max(1), max_dim)
        }
    } else {
        (width, height)
    };

    let rgba_to_encode = if target_w != width || target_h != height {
        if let Some(img) =
            image::ImageBuffer::<image::Rgba<u8>, _>::from_raw(width, height, rgba.to_vec())
        {
            let resized = image::imageops::resize(
                &img,
                target_w,
                target_h,
                image::imageops::FilterType::Lanczos3,
            );
            resized.into_raw()
        } else {
            rgba.to_vec()
        }
    } else {
        rgba.to_vec()
    };

    let mut png_bytes = Vec::new();
    if let Ok(_) = image::codecs::png::PngEncoder::new(&mut png_bytes).encode(
        &rgba_to_encode,
        target_w,
        target_h,
        image::ColorType::Rgba8,
    ) {
        Some(format!(
            "data:image/png;base64,{}",
            base64_encode(&png_bytes)
        ))
    } else {
        None
    }
}

pub(crate) fn texture_name_key(name: &str) -> String {
    name.to_lowercase()
        .trim_end_matches(".tga")
        .trim_end_matches(".png")
        .trim_end_matches(".dds")
        .trim_end_matches(".bmp")
        .trim_end_matches(".jpg")
        .trim_end_matches(".jpeg")
        .trim()
        .to_string()
}

pub(crate) fn decode_texture_png_rgba(texture: &HODTexture) -> Result<Option<(Vec<u8>, u32, u32)>, String> {
    use base64::{engine::general_purpose, Engine as _};

    let Some(encoded) = texture.png_data.as_ref().or(texture.png_preview.as_ref()) else {
        return Ok(None);
    };
    let payload = encoded
        .split_once(',')
        .map(|(_, data)| data)
        .unwrap_or(encoded.as_str());
    let bytes = general_purpose::STANDARD
        .decode(payload)
        .map_err(|e| format!("Failed to decode texture '{}': {}", texture.name, e))?;
    let img = image::load_from_memory(&bytes)
        .map_err(|e| format!("Failed to decode texture image '{}': {}", texture.name, e))?
        .to_rgba8();
    let (width, height) = img.dimensions();

    Ok(Some((img.into_raw(), width, height)))
}

pub(crate) fn flip_rgba_vertical_in_place(rgba: &mut [u8], width: u32, height: u32) {
    let row_len = width as usize * 4;
    if row_len == 0 || rgba.len() < row_len * height as usize {
        return;
    }
    for y in 0..(height as usize / 2) {
        let top = y * row_len;
        let bottom = (height as usize - 1 - y) * row_len;
        for x in 0..row_len {
            rgba.swap(top + x, bottom + x);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn decompress_dxt3_expands_explicit_alpha() {
        let mut block = [0u8; 16];
        for i in 0..16 {
            let alpha = i as u8;
            if i % 2 == 0 {
                block[i / 2] |= alpha;
            } else {
                block[i / 2] |= alpha << 4;
            }
        }

        let red565 = 0xF800u16;
        block[8..10].copy_from_slice(&red565.to_le_bytes());
        block[10..12].copy_from_slice(&red565.to_le_bytes());

        let rgba = decompress_dxt3(&block, 4, 4);

        for i in 0..16 {
            assert_eq!(rgba[i * 4], 255);
            assert_eq!(rgba[i * 4 + 1], 0);
            assert_eq!(rgba[i * 4 + 2], 0);
            assert_eq!(rgba[i * 4 + 3], i as u8 * 17);
        }
    }

    #[test]
    fn texture_codec_mip_size_matches_layout() {
        for &(width, height, blocks_x, blocks_y) in &[(4, 4, 1, 1), (8, 8, 2, 2), (512, 512, 128, 128)] {
            assert_eq!(compressed_texture_mip_size("DXT1", width, height), blocks_x * blocks_y * 8);
            assert_eq!(compressed_texture_mip_size("DXT3", width, height), blocks_x * blocks_y * 16);
            assert_eq!(compressed_texture_mip_size("DXT5", width, height), blocks_x * blocks_y * 16);
        }
    }

    #[test]
    fn texture_codec_flip_is_involution() {
        let original: Vec<u8> = (0..32).collect();
        let mut flipped = original.clone();
        flip_rgba_vertical_in_place(&mut flipped, 2, 4);
        flip_rgba_vertical_in_place(&mut flipped, 2, 4);
        assert_eq!(flipped, original);
    }
}
