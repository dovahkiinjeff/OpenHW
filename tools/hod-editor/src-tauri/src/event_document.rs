use byteorder::{LittleEndian, ReadBytesExt, WriteBytesExt};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};
use std::io::Cursor;
#[derive(Serialize, Deserialize)]
pub struct Timeline {
    pub name: String,
    pub parent: Option<u32>,
    pub duration: f32,
    pub minimum: f32,
    pub maximum: f32,
    pub looping: bool,
}
#[derive(Serialize, Deserialize)]
pub struct Event {
    pub timeline: u32,
    pub time: f32,
    pub scale: f32,
    pub flags: u32,
    pub markers: String,
    pub fx: String,
    pub sound: String,
    pub game_event: String,
    pub attachment: u32,
    pub length: f32,
    pub other: u32,
    pub transform: u32,
    pub other_marker: String,
    pub orphan_text: String,
}
#[derive(Serialize, Deserialize)]
pub struct Document {
    pub timelines: Vec<Timeline>,
    pub events: Vec<Event>,
}
type Result<T> = std::result::Result<T, String>;
fn word(r: &mut Cursor<&[u8]>) -> Result<u32> {
    r.read_u32::<LittleEndian>().map_err(|e| e.to_string())
}
fn float(r: &mut Cursor<&[u8]>) -> Result<f32> {
    let v = f32::from_bits(word(r)?);
    if !v.is_finite() {
        return Err("Nonfinite event number".into());
    }
    Ok(v)
}
pub fn decode(bytes: &[u8]) -> Result<Document> {
    if bytes.len() < 20 || &bytes[..8] != b"OWEVENT2" {
        return Err("Invalid events file".into());
    }
    let mut r = Cursor::new(bytes);
    r.set_position(8);
    let (nt, ne, ns) = (
        word(&mut r)? as usize,
        word(&mut r)? as usize,
        word(&mut r)? as usize,
    );
    if nt > 4096
        || ne > 4096
        || ns == 0
        || ns > 1024 * 1024
        || bytes.len() != 20 + nt * 24 + ne * 56 + ns
    {
        return Err("Invalid events storage".into());
    }
    let pool = &bytes[bytes.len() - ns..];
    let text = |r: &mut Cursor<&[u8]>| -> Result<String> {
        let off = word(r)? as usize;
        if off >= pool.len() || (off > 0 && pool[off - 1] != 0) {
            return Err("Invalid event string".into());
        }
        let end = pool[off..]
            .iter()
            .position(|v| *v == 0)
            .ok_or("Unterminated event string")?;
        String::from_utf8(pool[off..off + end].to_vec()).map_err(|e| e.to_string())
    };
    let mut timelines = vec![];
    let mut events = vec![];
    for _ in 0..nt {
        let name = text(&mut r)?;
        let parent = word(&mut r)?;
        timelines.push(Timeline {
            name,
            parent: if parent == u32::MAX {
                None
            } else {
                Some(parent)
            },
            duration: float(&mut r)?,
            minimum: float(&mut r)?,
            maximum: float(&mut r)?,
            looping: word(&mut r)? != 0,
        });
    }
    for _ in 0..ne {
        events.push(Event {
            timeline: word(&mut r)?,
            time: float(&mut r)?,
            scale: float(&mut r)?,
            flags: word(&mut r)?,
            markers: text(&mut r)?,
            fx: text(&mut r)?,
            sound: text(&mut r)?,
            game_event: text(&mut r)?,
            attachment: word(&mut r)?,
            length: float(&mut r)?,
            other: word(&mut r)?,
            transform: word(&mut r)?,
            other_marker: text(&mut r)?,
            orphan_text: text(&mut r)?,
        });
    }
    Ok(Document { timelines, events })
}
pub fn encode(doc: &Document) -> Result<Vec<u8>> {
    if doc.timelines.len() > 4096 || doc.events.len() > 4096 {
        return Err("Too many events".into());
    }
    let mut names = BTreeSet::new();
    for (i, t) in doc.timelines.iter().enumerate() {
        if t.name.is_empty()
            || !names.insert(t.name.to_lowercase())
            || !t.duration.is_finite()
            || t.duration <= 0.
            || t.duration > 3600.
            || !t.minimum.is_finite()
            || !t.maximum.is_finite()
        {
            return Err("Invalid timeline".into());
        }
        let mut chain = BTreeSet::new();
        let mut next = Some(i as u32);
        while let Some(index) = next {
            if !chain.insert(index) {
                return Err("Cyclic event timelines".into());
            }
            next = doc
                .timelines
                .get(index as usize)
                .ok_or("Invalid timeline parent")?
                .parent;
        }
    }
    let mut pool = vec![0];
    let mut strings = BTreeMap::new();
    strings.insert(String::new(), 0u32);
    let mut put_text = |out: &mut Vec<u8>, s: &str| -> Result<()> {
        if s.len() > 4096 || !s.bytes().all(|b| (32..=126).contains(&b)) {
            return Err("Event text must contain printable ASCII".into());
        }
        let offset = *strings.entry(s.to_owned()).or_insert_with(|| {
            let offset = pool.len() as u32;
            pool.extend_from_slice(s.as_bytes());
            pool.push(0);
            offset
        });
        out.write_u32::<LittleEndian>(offset)
            .map_err(|e| e.to_string())
    };
    let mut table = vec![];
    let mut records = vec![];
    for t in &doc.timelines {
        put_text(&mut table, &t.name)?;
        for v in [
            t.parent.unwrap_or(u32::MAX),
            t.duration.to_bits(),
            t.minimum.to_bits(),
            t.maximum.to_bits(),
            u32::from(t.looping),
        ] {
            table.extend_from_slice(&v.to_le_bytes());
        }
    }
    for e in &doc.events {
        if e.timeline as usize >= doc.timelines.len()
            || !e.time.is_finite()
            || (e.time < 0. && e.time != -1.)
            || e.time > 1.
            || !e.scale.is_finite()
            || !(0. ..=100000.).contains(&e.scale)
            || e.flags & !15 != 0
            || e.flags & 9 == 9
            || e.attachment > 7
            || !e.length.is_finite()
            || !(0. ..=100000.).contains(&e.length)
            || e.other > 2
            || e.transform > 5
        {
            return Err("Invalid presentation event".into());
        }
        for v in [e.timeline, e.time.to_bits(), e.scale.to_bits(), e.flags] {
            records.extend_from_slice(&v.to_le_bytes());
        }
        for s in [&e.markers, &e.fx, &e.sound, &e.game_event] {
            put_text(&mut records, s)?;
        }
        for v in [e.attachment, e.length.to_bits(), e.other, e.transform] {
            records.extend_from_slice(&v.to_le_bytes());
        }
        for s in [&e.other_marker, &e.orphan_text] {
            put_text(&mut records, s)?;
        }
    }
    if pool.len() > 1024 * 1024 {
        return Err("Event string pool is too large".into());
    }
    let mut out = b"OWEVENT2".to_vec();
    for v in [
        doc.timelines.len() as u32,
        doc.events.len() as u32,
        pool.len() as u32,
    ] {
        out.extend_from_slice(&v.to_le_bytes());
    }
    out.extend(table);
    out.extend(records);
    out.extend(pool);
    Ok(out)
}
