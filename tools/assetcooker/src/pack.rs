//! Deterministic `.zpkg` writer. The layout is documented in
//! `engine/include/zypher/assets/PackFile.h` and validated by the runtime.

use crate::crc::crc32;
use crate::AssetType;
use std::collections::HashSet;

pub const MAGIC: &[u8; 8] = b"ZYPKG001";
pub const VERSION: u32 = 1;
pub const HEADER_SIZE: usize = 56;
pub const ENTRY_SIZE: usize = 32;

#[derive(Clone, Debug)]
pub struct PackAsset {
    pub id: u64,
    pub path: String,
    pub asset_type: AssetType,
    pub data: Vec<u8>,
}

#[derive(Default)]
pub struct PackBuilder {
    assets: Vec<PackAsset>,
    seen: HashSet<u64>,
}

impl PackBuilder {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add(&mut self, id: u64, path: &str, asset_type: AssetType, data: Vec<u8>) -> Result<(), String> {
        if !self.seen.insert(id) {
            return Err(format!("duplicate asset id {:016x} for '{}'", id, path));
        }
        if data.len() > u32::MAX as usize {
            return Err(format!("asset '{}' is too large", path));
        }
        self.assets.push(PackAsset { id, path: path.to_string(), asset_type, data });
        Ok(())
    }

    pub fn asset_count(&self) -> usize {
        self.assets.len()
    }

    /// Serialises the pack. Assets are sorted by id so the same inputs always
    /// produce byte identical output (important for reproducible releases).
    pub fn build(&self) -> Result<Vec<u8>, String> {
        let mut ordered: Vec<&PackAsset> = self.assets.iter().collect();
        ordered.sort_by_key(|asset| asset.id);

        let mut string_table = String::new();
        let mut name_offsets = Vec::with_capacity(ordered.len());
        for asset in &ordered {
            name_offsets.push(string_table.len() as u32);
            string_table.push_str(&asset.path);
        }

        let index_offset = HEADER_SIZE;
        let string_offset = index_offset + ordered.len() * ENTRY_SIZE;
        let data_start = string_offset + string_table.len();

        let mut data_offsets = Vec::with_capacity(ordered.len());
        let mut cursor = data_start;
        for asset in &ordered {
            data_offsets.push(cursor as u32);
            cursor += asset.data.len();
        }
        let total_size = cursor;

        let mut output = Vec::with_capacity(total_size);
        output.extend_from_slice(MAGIC);
        output.extend_from_slice(&VERSION.to_le_bytes());
        output.extend_from_slice(&0u32.to_le_bytes()); // flags
        output.extend_from_slice(&(ordered.len() as u32).to_le_bytes());
        output.extend_from_slice(&0u32.to_le_bytes()); // reserved
        output.extend_from_slice(&(index_offset as u64).to_le_bytes());
        output.extend_from_slice(&(string_offset as u64).to_le_bytes());
        output.extend_from_slice(&(string_table.len() as u64).to_le_bytes());
        output.extend_from_slice(&(total_size as u64).to_le_bytes());
        debug_assert_eq!(output.len(), HEADER_SIZE);

        for (index, asset) in ordered.iter().enumerate() {
            output.extend_from_slice(&asset.id.to_le_bytes());
            output.extend_from_slice(&name_offsets[index].to_le_bytes());
            output.extend_from_slice(&(asset.path.len() as u32).to_le_bytes());
            output.extend_from_slice(&data_offsets[index].to_le_bytes());
            output.extend_from_slice(&(asset.data.len() as u32).to_le_bytes());
            output.push(asset.asset_type.as_u8());
            output.extend_from_slice(&[0u8, 0u8, 0u8]);
            output.extend_from_slice(&crc32(&asset.data).to_le_bytes());
        }
        debug_assert_eq!(output.len(), string_offset);

        output.extend_from_slice(string_table.as_bytes());
        debug_assert_eq!(output.len(), data_start);

        for asset in &ordered {
            output.extend_from_slice(&asset.data);
        }
        debug_assert_eq!(output.len(), total_size);
        Ok(output)
    }

    pub fn write_to(&self, path: &std::path::Path) -> Result<usize, String> {
        let bytes = self.build()?;
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)
                .map_err(|error| format!("cannot create '{}': {}", parent.display(), error))?;
        }
        std::fs::write(path, &bytes)
            .map_err(|error| format!("cannot write '{}': {}", path.display(), error))?;
        Ok(bytes.len())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn read_u32(bytes: &[u8], offset: usize) -> u32 {
        u32::from_le_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]])
    }

    fn read_u64(bytes: &[u8], offset: usize) -> u64 {
        let mut value = [0u8; 8];
        value.copy_from_slice(&bytes[offset..offset + 8]);
        u64::from_le_bytes(value)
    }

    #[test]
    fn writes_a_valid_header() {
        let mut builder = PackBuilder::new();
        builder.add(0x10, "data/b.txt", AssetType::Raw, b"hello".to_vec()).unwrap();
        builder.add(0x20, "data/a.txt", AssetType::Raw, b"world!".to_vec()).unwrap();
        let bytes = builder.build().expect("builds");

        assert_eq!(&bytes[0..8], MAGIC);
        assert_eq!(read_u32(&bytes, 8), VERSION);
        assert_eq!(read_u32(&bytes, 16), 2);
        assert_eq!(read_u64(&bytes, 48), bytes.len() as u64);
        let index_offset = read_u64(&bytes, 24) as usize;
        let string_offset = read_u64(&bytes, 32) as usize;
        let string_size = read_u64(&bytes, 40) as usize;

        // Entries must be sorted by id and point inside the file.
        assert_eq!(read_u64(&bytes, index_offset), 0x10);
        assert_eq!(read_u64(&bytes, index_offset + ENTRY_SIZE), 0x20);
        for index in 0..2 {
            let entry = index_offset + index * ENTRY_SIZE;
            let name_offset = read_u32(&bytes, entry + 8) as usize;
            let name_length = read_u32(&bytes, entry + 12) as usize;
            assert!(name_offset + name_length <= string_size);
            let data_offset = read_u32(&bytes, entry + 16) as usize;
            let data_size = read_u32(&bytes, entry + 20) as usize;
            assert!(data_offset + data_size <= bytes.len());
            let checksum = read_u32(&bytes, entry + 28);
            assert_eq!(checksum, crc32(&bytes[data_offset..data_offset + data_size]));
        }
        // Strings follow the id-sorted entry order.
        assert_eq!(&bytes[string_offset..string_offset + string_size], b"data/b.txtdata/a.txt".as_slice());
    }

    #[test]
    fn output_is_deterministic() {
        let build = || {
            let mut builder = PackBuilder::new();
            builder.add(1, "a", AssetType::Scene, b"scene".to_vec()).unwrap();
            builder.add(2, "b", AssetType::Mesh, vec![1, 2, 3]).unwrap();
            builder.build().unwrap()
        };
        assert_eq!(build(), build());
    }

    #[test]
    fn rejects_duplicates() {
        let mut builder = PackBuilder::new();
        builder.add(1, "a", AssetType::Raw, vec![1]).unwrap();
        assert!(builder.add(1, "b", AssetType::Raw, vec![2]).is_err());
    }
}
