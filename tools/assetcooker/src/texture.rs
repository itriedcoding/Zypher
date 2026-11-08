//! Texture handling.
//!
//! Textures are stored in the runtime pack with a small header that records
//! dimensions and the GPU format so the renderer can create the resource
//! without decoding on the CPU. PNG/WebP/JPEG/EXR/KTX2 payloads are passed
//! through unchanged; the runtime decodes them when the GPU backend needs it.

pub const TEXTURE_MAGIC: &[u8; 8] = b"ZYTEX001";

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TextureFormat {
    Unknown = 0,
    Rgba8 = 1,
    Bc7 = 2,
    Astc4x4 = 3,
    Rgba16Float = 4,
}

#[derive(Clone, Debug, PartialEq)]
pub struct TextureInfo {
    pub width: u32,
    pub height: u32,
    pub format: TextureFormat,
}

fn u32_le(bytes: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]])
}

fn u32_be(bytes: &[u8], offset: usize) -> u32 {
    u32::from_be_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]])
}

fn png_size(data: &[u8]) -> Result<TextureInfo, String> {
    const SIGNATURE: [u8; 8] = [0x89, b'P', b'N', b'G', 0x0D, 0x0A, 0x1A, 0x0A];
    if data.len() < 33 || data[0..8] != SIGNATURE {
        return Err("not a PNG file".into());
    }
    if &data[12..16] != b"IHDR" {
        return Err("PNG is missing its IHDR chunk".into());
    }
    let width = u32_be(data, 16);
    let height = u32_be(data, 20);
    if width == 0 || height == 0 {
        return Err("PNG has zero dimensions".into());
    }
    Ok(TextureInfo { width, height, format: TextureFormat::Rgba8 })
}

fn jpeg_size(data: &[u8]) -> Result<TextureInfo, String> {
    if data.len() < 4 || data[0] != 0xFF || data[1] != 0xD8 {
        return Err("not a JPEG file".into());
    }
    let mut offset = 2;
    while offset + 9 < data.len() {
        if data[offset] != 0xFF {
            offset += 1;
            continue;
        }
        let marker = data[offset + 1];
        offset += 2;
        if marker == 0xD8 || marker == 0xD9 {
            continue;
        }
        if offset + 2 > data.len() {
            break;
        }
        let segment_length = u16::from_be_bytes([data[offset], data[offset + 1]]) as usize;
        if matches!(marker, 0xC0 | 0xC1 | 0xC2) {
            if offset + 7 > data.len() {
                break;
            }
            let height = u16::from_be_bytes([data[offset + 3], data[offset + 4]]) as u32;
            let width = u16::from_be_bytes([data[offset + 5], data[offset + 6]]) as u32;
            return Ok(TextureInfo { width, height, format: TextureFormat::Rgba8 });
        }
        if segment_length < 2 {
            break;
        }
        offset += segment_length;
    }
    Err("JPEG has no supported start-of-frame marker".into())
}

fn webp_size(data: &[u8]) -> Result<TextureInfo, String> {
    if data.len() < 30 || &data[0..4] != b"RIFF" || &data[8..12] != b"WEBP" {
        return Err("not a WebP file".into());
    }
    let chunk = &data[12..16];
    if chunk == b"VP8X" {
        let width = 1 + (data[24] as u32 | (data[25] as u32) << 8 | (data[26] as u32) << 16);
        let height = 1 + (data[27] as u32 | (data[28] as u32) << 8 | (data[29] as u32) << 16);
        return Ok(TextureInfo { width, height, format: TextureFormat::Rgba8 });
    }
    if chunk == b"VP8L" && data.len() >= 25 {
        let bits = u32_le(data, 21);
        let width = 1 + (bits & 0x3FFF);
        let height = 1 + ((bits >> 14) & 0x3FFF);
        return Ok(TextureInfo { width, height, format: TextureFormat::Rgba8 });
    }
    // Lossy VP8 without an extended header: dimensions live in the frame header.
    if chunk == b"VP8 " && data.len() >= 30 {
        let width = u16::from_le_bytes([data[26], data[27]]) as u32 & 0x3FFF;
        let height = u16::from_le_bytes([data[28], data[29]]) as u32 & 0x3FFF;
        return Ok(TextureInfo { width, height, format: TextureFormat::Rgba8 });
    }
    Err("unsupported WebP sub-format".into())
}

fn exr_size(data: &[u8]) -> Result<TextureInfo, String> {
    if data.len() < 16 || data[0..4] != [0x76, 0x2F, 0x31, 0x01] {
        return Err("not an OpenEXR file".into());
    }
    // Walk the attribute list looking for dataWindow (type "box2i").
    let mut offset = 8;
    while offset + 2 <= data.len() {
        let name_end = data[offset..].iter().position(|byte| *byte == 0).map(|value| offset + value);
        let Some(name_end) = name_end else { break };
        let name = std::str::from_utf8(&data[offset..name_end]).unwrap_or("");
        if name.is_empty() {
            break;
        }
        offset = name_end + 1;
        let type_end = data[offset..].iter().position(|byte| *byte == 0).map(|value| offset + value);
        let Some(type_end) = type_end else { break };
        let type_name = std::str::from_utf8(&data[offset..type_end]).unwrap_or("");
        offset = type_end + 1;
        if offset + 4 > data.len() {
            break;
        }
        let size = u32_le(data, offset) as usize;
        offset += 4;
        if offset + size > data.len() {
            break;
        }
        if name == "dataWindow" && type_name == "box2i" && size >= 16 {
            let x_min = u32_le(data, offset) as i32;
            let y_min = u32_le(data, offset + 4) as i32;
            let x_max = u32_le(data, offset + 8) as i32;
            let y_max = u32_le(data, offset + 12) as i32;
            let width = (x_max - x_min + 1).max(0) as u32;
            let height = (y_max - y_min + 1).max(0) as u32;
            return Ok(TextureInfo { width, height, format: TextureFormat::Rgba16Float });
        }
        offset += size;
    }
    Err("OpenEXR is missing a dataWindow attribute".into())
}

fn ktx2_size(data: &[u8]) -> Result<TextureInfo, String> {
    const IDENTIFIER: [u8; 12] = [0xAB, b'K', b'T', b'X', b' ', b'2', b'0', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A];
    if data.len() < 68 || data[0..12] != IDENTIFIER {
        return Err("not a KTX2 file".into());
    }
    // supercompressionScheme at 16, then pixelWidth at 20, pixelHeight at 24.
    let width = u32_le(data, 20);
    let height = u32_le(data, 24);
    let format = match u32_le(data, 12) {
        // VK_FORMAT_BC7_UNORM_BLOCK / SRGB_BLOCK
        145 | 146 => TextureFormat::Bc7,
        // VK_FORMAT_ASTC_4x4_UNORM_BLOCK / SRGB_BLOCK
        157 | 158 => TextureFormat::Astc4x4,
        109 | 110 => TextureFormat::Rgba16Float,
        _ => TextureFormat::Rgba8,
    };
    Ok(TextureInfo { width, height, format })
}

pub fn inspect(extension: &str, data: &[u8]) -> Result<TextureInfo, String> {
    match extension.to_ascii_lowercase().as_str() {
        ".png" => png_size(data),
        ".jpg" | ".jpeg" => jpeg_size(data),
        ".webp" => webp_size(data),
        ".exr" => exr_size(data),
        ".ktx2" | ".ktx" => ktx2_size(data),
        other => Err(format!("unsupported texture extension '{}'", other)),
    }
}

/// Builds the runtime texture blob: magic, width, height, format, mip count,
/// then the untouched encoded payload.
pub fn encode_blob(extension: &str, data: &[u8]) -> Result<Vec<u8>, String> {
    let info = inspect(extension, data)?;
    let mut buffer = Vec::with_capacity(32 + data.len());
    buffer.extend_from_slice(TEXTURE_MAGIC);
    buffer.extend_from_slice(&info.width.to_le_bytes());
    buffer.extend_from_slice(&info.height.to_le_bytes());
    buffer.extend_from_slice(&(info.format as u32).to_le_bytes());
    buffer.extend_from_slice(&1u32.to_le_bytes()); // mip count
    buffer.extend_from_slice(&(data.len() as u32).to_le_bytes());
    buffer.extend_from_slice(&0u32.to_le_bytes()); // reserved
    buffer.extend_from_slice(data);
    Ok(buffer)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tiny_png() -> Vec<u8> {
        let mut data = vec![0x89, b'P', b'N', b'G', 0x0D, 0x0A, 0x1A, 0x0A];
        data.extend_from_slice(&13u32.to_be_bytes());
        data.extend_from_slice(b"IHDR");
        data.extend_from_slice(&4u32.to_be_bytes());
        data.extend_from_slice(&3u32.to_be_bytes());
        data.extend_from_slice(&[8, 6, 0, 0, 0]);
        data.extend_from_slice(&[0, 0, 0, 0]); // fake CRC (not validated here)
        data
    }

    #[test]
    fn reads_png_dimensions() {
        let info = inspect(".png", &tiny_png()).expect("valid png header");
        assert_eq!(info.width, 4);
        assert_eq!(info.height, 3);
    }

    #[test]
    fn encodes_texture_blob() {
        let blob = encode_blob(".png", &tiny_png()).expect("encodes");
        assert_eq!(&blob[0..8], TEXTURE_MAGIC);
        assert_eq!(u32::from_le_bytes([blob[8], blob[9], blob[10], blob[11]]), 4);
        assert_eq!(u32::from_le_bytes([blob[12], blob[13], blob[14], blob[15]]), 3);
    }

    #[test]
    fn rejects_wrong_signature() {
        assert!(inspect(".png", b"nope").is_err());
        assert!(inspect(".tga", b"whatever").is_err());
    }
}
