use byteorder::{BigEndian, ReadBytesExt, WriteBytesExt};
use std::io::{self, Cursor, Read, Seek, SeekFrom, Write};

#[derive(Debug, Clone, PartialEq)]
pub enum ChunkType {
    Form,
    Normal,
    Default,
}

#[derive(Debug, Clone)]
pub struct IffChunk {
    pub id: String, // 4-char identifier (e.g. "HVMD", "BMSH", "POOL")
    pub chunk_type: ChunkType,
    pub version: u32,            // Only applicable to Normal chunks
    pub data: Vec<u8>,           // Raw payload if Normal or Default, empty if Form
    pub children: Vec<IffChunk>, // Child chunks if Form
}

impl IffChunk {
    /// Resolves a chunk's payload size from its raw 4-byte size field.
    ///
    /// HOD files normally store chunk sizes big-endian, but `NRML` BMSH
    /// children of `MULT` have been observed with little-endian sizes.
    /// Only reinterpret a size as little-endian when the big-endian value
    /// cannot possibly fit in the bytes remaining in the stream while the
    /// little-endian value can.
    ///
    /// Do NOT reintroduce a fixed "big-endian size looks unreasonably large"
    /// threshold (the old code used `> 0x00FFFFFF`): legitimate big-endian
    /// sizes above 16MB whose low byte is `0x00` (e.g. a 32MB `POOL` of size
    /// `0x01E90E00`) read as a "reasonable" little-endian value and were
    /// silently corrupted, blowing the IFF boundary.
    fn resolve_chunk_size(size_be: u32, size_le: u32, remaining: u64) -> (u32, bool) {
        if u64::from(size_be) > remaining && u64::from(size_le) <= remaining {
            (size_le, true)
        } else {
            (size_be, false)
        }
    }

    /// Reads a single chunk from a stream (which can be a Cursor over memory)
    pub fn read_chunk<R: Read + Seek>(reader: &mut R) -> io::Result<Self> {
        let mut id_bytes = [0u8; 4];
        if let Err(e) = reader.read_exact(&mut id_bytes) {
            return Err(e);
        }
        let raw_id = String::from_utf8_lossy(&id_bytes).to_string();

        let mut size_bytes = [0u8; 4];
        reader.read_exact(&mut size_bytes)?;

        // Bytes left in the stream after this size field. A chunk's size
        // covers everything that follows the size field (real id + payload,
        // plus the version word for NRML), so a size larger than this can
        // only be satisfied by running past the end of the stream.
        let after_size = reader.stream_position()?;
        let remaining = reader.seek(SeekFrom::End(0))?.saturating_sub(after_size);
        reader.seek(SeekFrom::Start(after_size))?;

        let (size, _) = Self::resolve_chunk_size(
            u32::from_be_bytes(size_bytes),
            u32::from_le_bytes(size_bytes),
            remaining,
        );

        match raw_id.as_str() {
            "FORM" => {
                let mut real_id_bytes = [0u8; 4];
                reader.read_exact(&mut real_id_bytes)?;
                let real_id = String::from_utf8_lossy(&real_id_bytes).to_string();

                let payload_size = size.saturating_sub(4) as usize;
                let mut payload = vec![0u8; payload_size];
                reader.read_exact(&mut payload)?;

                // Only recursively parse children if this FORM is a known container chunk
                let id_trimmed = real_id.trim();
                let is_container = matches!(
                    id_trimmed,
                    "HVMD"
                        | "DTRM"
                        | "BGMS"
                        | "BSRM"
                        | "COLD"
                        | "GLOW"
                        | "MRKR"
                        | "KEYF"
                        | "MSHL"
                        | "MAD"
                );

                let mut children = Vec::new();
                let mut data = Vec::new();

                if is_container {
                    if id_trimmed == "COLD" {
                        if payload.len() >= 4 {
                            let mut len_bytes = [0u8; 4];
                            len_bytes.copy_from_slice(&payload[0..4]);
                            let len = u32::from_le_bytes(len_bytes) as usize;

                            let has_extents = if payload.len() >= 4 + len + 4 {
                                let id_at_short = &payload[4 + len..4 + len + 4];
                                let is_valid_short =
                                    id_at_short.iter().all(|&b| b >= 32 && b <= 126);
                                !is_valid_short
                            } else {
                                false
                            };

                            let prefix_len = if has_extents && payload.len() >= 4 + len + 40 {
                                4 + len + 40
                            } else {
                                4 + len
                            };

                            if payload.len() >= prefix_len {
                                data = payload[0..prefix_len].to_vec();
                                let mut cursor = Cursor::new(payload[prefix_len..].to_vec());
                                let sub_limit = payload_size.saturating_sub(prefix_len);
                                while cursor.position() < sub_limit as u64 {
                                    children.push(Self::read_chunk(&mut cursor)?);
                                }
                            } else {
                                data = payload;
                            }
                        } else {
                            data = payload;
                        }
                    } else {
                        let mut cursor = Cursor::new(payload);
                        while cursor.position() < payload_size as u64 {
                            children.push(Self::read_chunk(&mut cursor)?);
                        }
                    }
                } else {
                    data = payload;
                }

                Ok(Self {
                    id: real_id,
                    chunk_type: ChunkType::Form,
                    version: 0,
                    data,
                    children,
                })
            }
            "NRML" => {
                let mut real_id_bytes = [0u8; 4];
                reader.read_exact(&mut real_id_bytes)?;
                let real_id = String::from_utf8_lossy(&real_id_bytes).to_string();

                let version = reader.read_u32::<BigEndian>()?;

                let payload_size = size.saturating_sub(8) as usize;
                let mut data = vec![0u8; payload_size];
                reader.read_exact(&mut data)?;

                Ok(Self {
                    id: real_id,
                    chunk_type: ChunkType::Normal,
                    version,
                    data,
                    children: Vec::new(),
                })
            }
            _ => {
                // DEFAULT chunks (like POOL, BGLT)
                let mut data = vec![0u8; size as usize];
                reader.read_exact(&mut data)?;

                Ok(Self {
                    id: raw_id,
                    chunk_type: ChunkType::Default,
                    version: 0,
                    data,
                    children: Vec::new(),
                })
            }
        }
    }

    /// Serializes the chunk into a writer
    pub fn write_chunk<W: Write>(&self, writer: &mut W) -> io::Result<()> {
        match self.chunk_type {
            ChunkType::Form => {
                let mut payload_buffer = self.data.clone();
                if !self.children.is_empty() {
                    for child in &self.children {
                        child.write_chunk(&mut payload_buffer)?;
                    }
                }

                // Write FORM tag
                writer.write_all(b"FORM")?;

                // Write total size (payload + 4 bytes real ID)
                let total_size = (payload_buffer.len() + 4) as u32;
                writer.write_u32::<BigEndian>(total_size)?;

                // Write real ID
                let mut id_bytes = [b' '; 4];
                let src_bytes = self.id.as_bytes();
                let len = src_bytes.len().min(4);
                id_bytes[..len].copy_from_slice(&src_bytes[..len]);
                writer.write_all(&id_bytes)?;

                // Write child payload
                writer.write_all(&payload_buffer)?;
            }
            ChunkType::Normal => {
                // Write NRML tag
                writer.write_all(b"NRML")?;

                // Write total size (data + 4 bytes real ID + 4 bytes version)
                let total_size = (self.data.len() + 8) as u32;
                writer.write_u32::<BigEndian>(total_size)?;

                // Write real ID
                let mut id_bytes = [b' '; 4];
                let src_bytes = self.id.as_bytes();
                let len = src_bytes.len().min(4);
                id_bytes[..len].copy_from_slice(&src_bytes[..len]);
                writer.write_all(&id_bytes)?;

                // Write version
                writer.write_u32::<BigEndian>(self.version)?;

                // Write data payload
                writer.write_all(&self.data)?;
            }
            ChunkType::Default => {
                // Write the raw tag ID directly
                let mut id_bytes = [b' '; 4];
                let src_bytes = self.id.as_bytes();
                let len = src_bytes.len().min(4);
                id_bytes[..len].copy_from_slice(&src_bytes[..len]);
                writer.write_all(&id_bytes)?;

                // Write size
                writer.write_u32::<BigEndian>(self.data.len() as u32)?;

                // Write data
                writer.write_all(&self.data)?;
            }
        }
        Ok(())
    }

    /// Recursively find a child chunk by its ID
    pub fn find_child(&self, id: &str) -> Option<&IffChunk> {
        if self.id == id {
            return Some(self);
        }
        for child in &self.children {
            if let Some(found) = child.find_child(id) {
                return Some(found);
            }
        }
        None
    }

    /// Recursively find all children by their ID
    pub fn find_all_children(&self, id: &str, matches: &mut Vec<IffChunk>) {
        if self.id == id {
            matches.push(self.clone());
        }
        for child in &self.children {
            child.find_all_children(id, matches);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use byteorder::LittleEndian;

    #[test]
    fn keeps_large_big_endian_size_when_low_byte_is_zero() {
        // Regression: the POOL in testing/bst_mothership/bst_mothership.hod is
        // 0x01E90E00 bytes. The old "> 16MB => little-endian" heuristic read
        // the size as 0x000EE901 and desynchronized the IFF walk.
        assert_eq!(
            IffChunk::resolve_chunk_size(0x01E90E00, 0x000EE901, 32_382_540),
            (0x01E90E00, false)
        );
        // Exactly-fitting big-endian sizes are also kept.
        assert_eq!(
            IffChunk::resolve_chunk_size(0x01E90E00, 0x000EE901, 0x01E90E00),
            (0x01E90E00, false)
        );
    }

    #[test]
    fn falls_back_to_little_endian_only_when_big_endian_cannot_fit() {
        // MULT child NRML BMSH sizes may be little-endian: here size 100000 is
        // stored as bytes [A0 86 01 00], i.e. big-endian 0xA0860100.
        assert_eq!(
            IffChunk::resolve_chunk_size(0xA0860100, 100_000, 100_000),
            (100_000, true)
        );
        // A small big-endian size that fits is never reinterpreted.
        assert_eq!(IffChunk::resolve_chunk_size(52, 0x34000000, 52), (52, false));
        // If neither reading fits, keep big-endian and let the read fail loudly.
        assert_eq!(
            IffChunk::resolve_chunk_size(0xFFFFFFFF, 0xFFFFFFFF, 100),
            (0xFFFFFFFF, false)
        );
    }

    #[test]
    fn reads_little_endian_nrml_bmsh_child() {
        let mut buf = Vec::new();
        buf.extend_from_slice(b"NRML");
        buf.write_u32::<LittleEndian>(52).unwrap(); // size: 4 realid + 4 version + 44 data
        buf.extend_from_slice(b"BMSH");
        buf.write_u32::<BigEndian>(1400).unwrap();
        buf.extend_from_slice(&[0xAB; 44]);

        let mut cursor = Cursor::new(buf);
        let chunk = IffChunk::read_chunk(&mut cursor).unwrap();
        assert_eq!(chunk.id, "BMSH");
        assert_eq!(chunk.chunk_type, ChunkType::Normal);
        assert_eq!(chunk.version, 1400);
        assert_eq!(chunk.data.len(), 44);
        assert_eq!(cursor.position(), 8 + 52);
    }

    #[test]
    fn reads_large_big_endian_default_chunk() {
        let mut buf = Vec::new();
        buf.extend_from_slice(b"POOL");
        buf.write_u32::<BigEndian>(0x01000000).unwrap(); // > 16MB with low byte 0x00
        buf.resize(buf.len() + 0x01000000, 0);

        let mut cursor = Cursor::new(buf);
        let chunk = IffChunk::read_chunk(&mut cursor).unwrap();
        assert_eq!(chunk.id, "POOL");
        assert_eq!(chunk.chunk_type, ChunkType::Default);
        assert_eq!(chunk.data.len(), 0x01000000);
        assert_eq!(cursor.position(), 8 + 0x01000000);
    }
}
