use super::*;
use base64::{engine::general_purpose::STANDARD, Engine};
use hwr_hod_parser::hod::HODMaterial;
use image::{imageops, Rgba, RgbaImage};

fn texture(
    model: &HODModel,
    material: &HODMaterial,
    parameter: &str,
) -> Result<Option<RgbaImage>, String> {
    let Some(p) = material
        .params
        .iter()
        .find(|p| p.name.eq_ignore_ascii_case(parameter) && p.param_type == 5)
    else {
        return Ok(None);
    };
    let index = u32::from_le_bytes(
        p.value
            .get(..4)
            .ok_or("Invalid texture parameter")?
            .try_into()
            .unwrap(),
    ) as usize;
    let source = model
        .textures
        .get(index)
        .ok_or("Missing material texture")?;
    let encoded = source
        .png_data
        .as_ref()
        .ok_or_else(|| format!("Full-resolution texture {} is unavailable", source.name))?;
    let payload = encoded.split_once(',').map(|(_, s)| s).unwrap_or(encoded);
    let bytes = STANDARD.decode(payload).map_err(|e| e.to_string())?;
    let image = image::load_from_memory(&bytes)
        .map_err(|e| e.to_string())?
        .to_rgba8();
    Ok(Some(imageops::flip_vertical(&image)))
}
fn aligned(source: Option<RgbaImage>, w: u32, h: u32, fallback: [u8; 4]) -> RgbaImage {
    match source {
        Some(image) if image.dimensions() == (w, h) => image,
        Some(image) => imageops::resize(&image, w, h, imageops::FilterType::Triangle),
        None => RgbaImage::from_pixel(w, h, Rgba(fallback)),
    }
}
fn number(bytes: &mut [u8], offset: usize, value: u32) {
    bytes[offset..offset + 4].copy_from_slice(&value.to_le_bytes())
}
fn dds(image: &RgbaImage) -> Vec<u8> {
    let mut data = vec![0; 128];
    data[..4].copy_from_slice(b"DDS ");
    for (at, value) in [
        (4, 124),
        (8, 0x100f),
        (12, image.height()),
        (16, image.width()),
        (20, image.width() * 4),
        (28, 1),
        (76, 32),
        (80, 0x41),
        (88, 32),
        (92, 0xff),
        (96, 0xff00),
        (100, 0xff0000),
        (104, 0xff000000),
        (108, 0x1000),
    ] {
        number(&mut data, at, value);
    }
    data.extend_from_slice(image.as_raw());
    data
}
fn byte(value: f32) -> u8 {
    (value.clamp(0., 1.) * 255.).round() as u8
}
pub fn stage(model: &HODModel, folder: &Path) -> Result<(), String> {
    for (index, material) in model.materials.iter().enumerate() {
        let shader = material.shader_name.to_lowercase();
        if shader == "innatess" {
            continue;
        }
        for state in if shader == "thruster" {
            vec!["", "On"]
        } else {
            vec![""]
        } {
            let parameter_state = if shader == "thruster" && state.is_empty() {
                "Off"
            } else {
                state
            };
            let mut diffuse = match texture(model, material, &format!("$diffuse{parameter_state}"))?
            {
                Some(image) => image,
                None if shader == "default" => {
                    RgbaImage::from_fn(256, 1, |x, _| Rgba([x as u8, x as u8, x as u8, 255]))
                }
                None => return Err(format!("Material {} has no diffuse texture", material.name)),
            };
            let (w, h) = diffuse.dimensions();
            let glow = aligned(
                texture(model, material, &format!("$glow{parameter_state}"))?,
                w,
                h,
                [0, 0, 0, 255],
            );
            let team = aligned(texture(model, material, "$team")?, w, h, [255; 4]);
            let spec = aligned(texture(model, material, "$spec")?, w, h, [0, 0, 0, 255]);
            let normal = texture(model, material, "$normal")?;
            let mut emission = RgbaImage::new(w, h);
            let mut control = RgbaImage::new(w, h);
            let mut shade = RgbaImage::new(w, h);
            let mut masks = [RgbaImage::new(w, h), RgbaImage::new(w, h)];
            let rgb_glow = ["shipglow", "shipglow_ns", "badgeglow"].contains(&shader.as_str());
            let matte = ["matte", "mattealpha", "mattescissor"].contains(&shader.as_str());
            for y in 0..h {
                for x in 0..w {
                    let d = diffuse.get_pixel_mut(x, y);
                    let g = glow.get_pixel(x, y);
                    let t = team.get_pixel(x, y);
                    let packed = if rgb_glow { spec.get_pixel(x, y) } else { g };
                    if shader == "mattescissor" {
                        d[3] = if t[0] >= 128 { 255 } else { 0 }
                    } else if shader != "mattealpha" {
                        d[3] = 255
                    }
                    let painted = if matte {
                        t[2] as f32 / 255.
                    } else {
                        1. - (t[0] as f32 / 255.) * (t[1] as f32 / 255.) * (1. - t[2] as f32 / 255.)
                    };
                    let sharp = packed[0] as f32 / 255.;
                    let gloss = (sharp.powf(0.1 + 1.8 * painted) * (75. - 115. * painted)).max(1.)
                        + 30.
                        - 25. * painted;
                    let mask = (packed[2] as f32 / 255.
                        - if rgb_glow {
                            1.
                        } else {
                            packed[1] as f32 / 255.
                        })
                    .max(0.);
                    control.put_pixel(
                        x,
                        y,
                        Rgba([
                            packed[0],
                            byte((2. / (gloss + 2.)).sqrt()),
                            byte(mask * (1. - 0.12 * painted)),
                            if shader == "bay" {
                                128
                            } else {
                                129 + (124. * mask).round() as u8
                            },
                        ]),
                    );
                    let rgb = if rgb_glow {
                        [g[0], g[1], g[2]]
                    } else {
                        [
                            byte(d[0] as f32 * g[1] as f32 / 65025.),
                            byte(d[1] as f32 * g[1] as f32 / 65025.),
                            byte(d[2] as f32 * g[1] as f32 / 65025.),
                        ]
                    };
                    emission.put_pixel(
                        x,
                        y,
                        Rgba([
                            rgb[0],
                            rgb[1],
                            rgb[2],
                            if rgb.iter().any(|v| *v > 0) { d[3] } else { 0 },
                        ]),
                    );
                    let paint = 1. - t[0] as f32 * t[1] as f32 / 65025.;
                    let value = if matte {
                        255
                    } else {
                        byte(
                            1. - paint
                                + paint
                                    * ((3. * d[0] as f32 / 255. - 0.5).clamp(0., 1.) * 0.299
                                        + (3. * d[1] as f32 / 255. - 0.5).clamp(0., 1.) * 0.587
                                        + (3. * d[2] as f32 / 255. - 0.5).clamp(0., 1.) * 0.114),
                        )
                    };
                    shade.put_pixel(x, y, Rgba([value, value, value, 255]));
                    for i in 0..2 {
                        let value = if matte { 0 } else { 255 - t[i] };
                        masks[i].put_pixel(x, y, Rgba([value, value, value, 255]));
                    }
                }
            }
            let label = format!(
                "ow_m{index}{}",
                if state.is_empty() { "" } else { "_engineOn" }
            );
            for (suffix, image) in [
                ("", &diffuse),
                ("_emissive", &emission),
                ("_material", &control),
                ("_paintShade", &shade),
            ] {
                fs::write(folder.join(format!("{label}{suffix}.dds")), dds(image))
                    .map_err(|e| e.to_string())?;
            }
            if let Some(normal) = normal {
                fs::write(
                    folder.join(format!("{label}_normal.dds")),
                    dds(&aligned(Some(normal), w, h, [128, 128, 255, 255])),
                )
                .map_err(|e| e.to_string())?;
            }
            let ratio = (512.0 / w.max(h) as f32).min(1.);
            let (bw, bh) = (
                (w as f32 * ratio).max(1.) as u32,
                (h as f32 * ratio).max(1.) as u32,
            );
            let small = imageops::resize(&diffuse, bw, bh, imageops::FilterType::Lanczos3);
            let mut lif = vec![0; 48];
            lif[..8].copy_from_slice(b"Willy 7\0");
            for (at, value) in [
                (8, 0x104),
                (
                    12,
                    0x30 | if small.pixels().any(|p| p[3] < 255) {
                        8
                    } else {
                        0
                    },
                ),
                (16, bw),
                (20, bh),
                (32, 48),
                (40, 48 + bw * bh * 4),
                (44, 48 + bw * bh * 5),
            ] {
                number(&mut lif, at, value)
            }
            lif.extend_from_slice(small.as_raw());
            for (i, mask) in masks.iter().enumerate() {
                let mask = imageops::resize(mask, bw, bh, imageops::FilterType::Triangle);
                lif.extend(mask.pixels().map(|p| p[0]));
                fs::write(
                    folder.join(format!("{label}_teamEffect{i}.dds")),
                    dds(&mask),
                )
                .map_err(|e| e.to_string())?;
            }
            fs::write(folder.join(format!("{label}.lif")), lif).map_err(|e| e.to_string())?;
        }
    }
    Ok(())
}
