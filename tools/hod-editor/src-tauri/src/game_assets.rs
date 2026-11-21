use byteorder::{LittleEndian, ReadBytesExt};
use std::collections::BTreeMap;
use std::fs::File;
use std::io::{self, BufReader, Read, Seek, SeekFrom};
use std::path::Path;

struct Entry {
    offset: u64,
    size: u32,
    checksum: u32,
}

pub struct GameAssets {
    archive: File,
    entries: BTreeMap<String, Entry>,
}

impl GameAssets {
    pub fn open(path: &Path) -> io::Result<Self> {
        let archive = File::open(path)?;
        let total = archive.metadata()?.len();
        let mut reader = BufReader::new(&archive);
        let mut magic = [0; 8];
        reader.read_exact(&mut magic)?;
        let version = reader.read_u32::<LittleEndian>()?;
        let count = reader.read_u32::<LittleEndian>()?;
        let data = reader.read_u64::<LittleEndian>()?;
        if magic != *b"OBIGPK01" || version != 1 || count > 1_000_000
            || data != 24 + u64::from(count) * 272 || data > total {
            return Err(io::Error::other("Invalid OpenHW archive directory"));
        }
        let mut entries = BTreeMap::new();
        for _ in 0..count {
            let mut name = [0; 256];
            reader.read_exact(&mut name)?;
            let offset = reader.read_u64::<LittleEndian>()?;
            let size = reader.read_u32::<LittleEndian>()?;
            let checksum = reader.read_u32::<LittleEndian>()?;
            let end = name.iter().position(|&x| x == 0).ok_or_else(|| io::Error::other("Invalid archive name"))?;
            let key = std::str::from_utf8(&name[..end]).map_err(io::Error::other)?;
            if key.ends_with(".hod") || key.ends_with(".mad") || key.ends_with(".owshipdef") || key.ends_with(".owsubsystem") || key.ends_with(".owevents") || key.ends_with(".wf") || key == "subsystems/catalog.txt" {
                if offset < data || offset > total || u64::from(size) > total-offset {
                    return Err(io::Error::other("Invalid model offset in game archive"));
                }
                entries.insert(key.to_owned(), Entry {offset, size, checksum});
            }
        }
        let aliases:&[(&str,&str)]=include!(concat!(env!("OUT_DIR"),"/archive_aliases.rs"));
        for &(alias,target) in aliases {
            if let Some(entry)=entries.get(target) {
                let copy=Entry{offset:entry.offset,size:entry.size,checksum:entry.checksum};
                entries.entry(alias.to_owned()).or_insert(copy);
            }
        }
        Ok(Self {archive, entries})
    }
    pub fn subsystem_names(&self) -> Vec<String> {
        self.entries.keys().filter_map(|p| {
            let name=p.strip_prefix("subsystems/")?.strip_suffix("/subsystem.owsubsystem")?;
            if name.contains('/') {None} else {Some(name.to_owned())}
        }).collect()
    }
    pub fn names(&self) -> Vec<String> {
        self.entries.keys().filter(|p| p.ends_with(".hod")).cloned().collect()
    }
    pub fn read(&mut self, name: &str) -> io::Result<Vec<u8>> {
        let entry = self.entries.get(name).ok_or_else(|| io::Error::other("Model is absent from the game archive"))?;
        self.archive.seek(SeekFrom::Start(entry.offset))?;
        let mut data = Vec::new();
        data.try_reserve_exact(entry.size as usize).map_err(io::Error::other)?;
        (&mut self.archive).take(u64::from(entry.size)).read_to_end(&mut data)?;
        if data.len() != entry.size as usize || crc32fast::hash(&data) != entry.checksum {
            return Err(io::Error::other("Model checksum failed"));
        }
        Ok(data)
    }

}
