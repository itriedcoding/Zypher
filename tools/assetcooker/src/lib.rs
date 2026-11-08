//! Zypher asset cooker.
//!
//! Converts a project's source assets (OBJ, glTF/GLB, textures, audio, scenes)
//! into a single deterministic `.zpkg` pack that the C++ runtime reads.
//!
//! The crate is dependency free on purpose: the cooker is part of the build
//! infrastructure and must build offline on a clean machine.

pub mod audio;
pub mod crc;
pub mod gltf;
pub mod json;
pub mod mesh;
pub mod obj;
pub mod pack;
pub mod project;
pub mod texture;

/// Asset type tags shared with the C++ runtime (`zypher::AssetType`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum AssetType {
    Unknown = 0,
    Mesh = 1,
    Texture = 2,
    Audio = 3,
    Scene = 4,
    Shader = 5,
    Json = 6,
    Font = 7,
    Raw = 8,
}

impl AssetType {
    pub fn as_u8(self) -> u8 {
        self as u8
    }

    pub fn name(self) -> &'static str {
        match self {
            AssetType::Mesh => "mesh",
            AssetType::Texture => "texture",
            AssetType::Audio => "audio",
            AssetType::Scene => "scene",
            AssetType::Shader => "shader",
            AssetType::Json => "json",
            AssetType::Font => "font",
            AssetType::Raw => "raw",
            AssetType::Unknown => "unknown",
        }
    }

    pub fn parse(text: &str) -> Option<AssetType> {
        Some(match text {
            "mesh" => AssetType::Mesh,
            "texture" => AssetType::Texture,
            "audio" => AssetType::Audio,
            "scene" => AssetType::Scene,
            "shader" => AssetType::Shader,
            "json" => AssetType::Json,
            "font" => AssetType::Font,
            "raw" => AssetType::Raw,
            _ => return None,
        })
    }
}

/// Stable asset id: FNV-1a 64 of the project relative path. The C++ side uses
/// `zy::assetIdFromPath` with the exact same algorithm.
pub fn asset_id(path: &str) -> u64 {
    const OFFSET_BASIS: u64 = 0xcbf2_9ce4_8422_2325;
    const PRIME: u64 = 1099511628211;
    let mut hash = OFFSET_BASIS;
    for byte in path.as_bytes() {
        hash ^= *byte as u64;
        hash = hash.wrapping_mul(PRIME);
    }
    hash
}

/// Normalises a path to the canonical form used for asset ids: forward slashes,
/// no leading "./" and no duplicate separators.
pub fn normalize_path(path: &str) -> String {
    let unified = path.replace('\\', "/");
    let mut parts: Vec<&str> = Vec::new();
    for part in unified.split('/') {
        match part {
            "" | "." => continue,
            ".." => {
                parts.pop();
            }
            other => parts.push(other),
        }
    }
    parts.join("/")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn asset_ids_are_stable_and_path_sensitive() {
        assert_eq!(asset_id("models/cube.obj"), asset_id("models/cube.obj"));
        assert_ne!(asset_id("models/cube.obj"), asset_id("Models/cube.obj"));
        // The value is part of the on-disk format: changing it breaks assets.
        assert_eq!(asset_id("models/cube.obj"), 0x4761_0562_BDFD_77F2);
    }

    #[test]
    fn paths_are_normalised() {
        assert_eq!(normalize_path("./models/../models/cube.obj"), "models/cube.obj");
        assert_eq!(normalize_path("models\\cube.obj"), "models/cube.obj");
        assert_eq!(normalize_path("a//b///c.txt"), "a/b/c.txt");
    }
}
