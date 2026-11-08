//! glTF 2.0 / GLB importer.
//!
//! Supports the subset Zypher needs at cook time: triangle primitives with
//! POSITION, NORMAL and TEXCOORD_0 attributes, indices from a BufferView, and
//! buffers supplied inline (base64 data URI), as a sidecar .bin, or from the
//! GLB binary chunk.

use crate::json::{self, Json};
use crate::mesh::{MeshData, Vertex};
use std::path::Path;

const GLB_MAGIC: u32 = 0x4654_6C67;
const CHUNK_JSON: u32 = 0x4E4F_534A;
const CHUNK_BIN: u32 = 0x004E_4942;

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, String> {
    if offset + 4 > bytes.len() {
        return Err("truncated binary data".into());
    }
    Ok(u32::from_le_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]]))
}

fn base64_decode(input: &str) -> Result<Vec<u8>, String> {
    let mut output = Vec::with_capacity(input.len() * 3 / 4);
    let mut accumulator: u32 = 0;
    let mut bits: u32 = 0;
    for byte in input.bytes() {
        let value = match byte {
            b'A'..=b'Z' => byte - b'A',
            b'a'..=b'z' => byte - b'a' + 26,
            b'0'..=b'9' => byte - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            b'=' => break,
            b'\n' | b'\r' | b' ' | b'\t' => continue,
            _ => return Err("invalid base64 data".into()),
        };
        accumulator = (accumulator << 6) | value as u32;
        bits += 6;
        if bits >= 8 {
            bits -= 8;
            output.push((accumulator >> bits) as u8);
        }
    }
    Ok(output)
}

pub struct GltfDocument {
    pub json: Json,
    pub binary_chunk: Option<Vec<u8>>,
}

/// Parses a `.gltf` (JSON) or `.glb` (binary container) file.
pub fn load(path: &Path) -> Result<GltfDocument, String> {
    let bytes = std::fs::read(path).map_err(|error| format!("cannot read '{}': {}", path.display(), error))?;
    let is_glb = bytes.len() >= 12 && read_u32(&bytes, 0)? == GLB_MAGIC;
    if is_glb {
        let version = read_u32(&bytes, 4)?;
        if version != 2 {
            return Err(format!("unsupported GLB version {}", version));
        }
        let total_length = read_u32(&bytes, 8)? as usize;
        if total_length > bytes.len() {
            return Err("GLB header declares a larger file than provided".into());
        }
        let mut offset = 12;
        let mut json_text: Option<String> = None;
        let mut binary: Option<Vec<u8>> = None;
        while offset + 8 <= total_length {
            let chunk_length = read_u32(&bytes, offset)? as usize;
            let chunk_type = read_u32(&bytes, offset + 4)?;
            let data_start = offset + 8;
            if data_start + chunk_length > total_length {
                return Err("GLB chunk is out of range".into());
            }
            let chunk = &bytes[data_start..data_start + chunk_length];
            match chunk_type {
                CHUNK_JSON => {
                    json_text = Some(
                        String::from_utf8(chunk.to_vec()).map_err(|_| "GLB JSON chunk is not UTF-8")?,
                    );
                }
                CHUNK_BIN => binary = Some(chunk.to_vec()),
                _ => {}
            }
            offset = data_start + chunk_length;
            // Chunks are padded to 4 byte boundaries.
            offset = (offset + 3) & !3;
        }
        let text = json_text.ok_or("GLB is missing its JSON chunk")?;
        let json = json::parse(&text)?;
        Ok(GltfDocument { json, binary_chunk: binary })
    } else {
        let text = String::from_utf8(bytes).map_err(|_| "glTF file is not valid UTF-8")?;
        let json = json::parse(&text)?;
        Ok(GltfDocument { json, binary_chunk: None })
    }
}

fn component_size(component_type: u32) -> Result<usize, String> {
    Ok(match component_type {
        5120 | 5121 => 1,
        5122 | 5123 => 2,
        5125 | 5126 => 4,
        other => return Err(format!("unsupported accessor component type {}", other)),
    })
}

fn component_count(type_name: &str) -> Result<usize, String> {
    Ok(match type_name {
        "SCALAR" => 1,
        "VEC2" => 2,
        "VEC3" => 3,
        "VEC4" => 4,
        other => return Err(format!("unsupported accessor type '{}'", other)),
    })
}

fn load_buffers(document: &Json, base_dir: &Path, binary_chunk: Option<&Vec<u8>>) -> Result<Vec<Vec<u8>>, String> {
    let mut buffers = Vec::new();
    if let Some(list) = document.get_array("buffers") {
        for (index, entry) in list.iter().enumerate() {
            if let Some(uri) = entry.get_str("uri") {
                if let Some(encoded) = uri.strip_prefix("data:") {
                    let payload = encoded
                        .split_once(',')
                        .map(|(_, data)| data)
                        .ok_or("malformed data URI in glTF buffer")?;
                    buffers.push(base64_decode(payload)?);
                } else {
                    let path = base_dir.join(uri);
                    buffers.push(
                        std::fs::read(&path)
                            .map_err(|error| format!("cannot read buffer '{}': {}", path.display(), error))?,
                    );
                }
            } else if index == 0 {
                buffers.push(binary_chunk.cloned().ok_or("glTF buffer has no uri and no GLB chunk")?);
            } else {
                return Err("glTF buffer has no uri".into());
            }
        }
    }
    Ok(buffers)
}

struct Accessor {
    values: Vec<f32>,
    components: usize,
    count: usize,
}

fn read_accessor(document: &Json, buffers: &[Vec<u8>], accessor_index: usize) -> Result<Accessor, String> {
    let accessors = document.get_array("accessors").ok_or("glTF has no accessors")?;
    let accessor = accessors
        .get(accessor_index)
        .ok_or_else(|| format!("accessor {} does not exist", accessor_index))?;
    let component_type = accessor
        .get_f64("componentType")
        .map(|value| value as u32)
        .ok_or("accessor has no componentType")?;
    let count = accessor.get_f64("count").map(|value| value as usize).ok_or("accessor has no count")?;
    let type_name = accessor.get_str("type").ok_or("accessor has no type")?;
    let components = component_count(type_name)?;
    let size = component_size(component_type)?;
    let byte_offset = accessor.get_f64("byteOffset").unwrap_or(0.0) as usize;

    let values_len = count
        .checked_mul(components)
        .ok_or("accessor count overflows")?;
    let mut values = Vec::with_capacity(values_len);

    let Some(buffer_view_index) = accessor
        .get_f64("bufferView")
        .map(|value| value as usize)
    else {
        // Sparse accessors without a buffer view are all zero.
        values.resize(values_len, 0.0);
        return Ok(Accessor { values, components, count });
    };

    let buffer_views = document.get_array("bufferViews").ok_or("glTF has no bufferViews")?;
    let buffer_view = buffer_views
        .get(buffer_view_index)
        .ok_or_else(|| format!("bufferView {} does not exist", buffer_view_index))?;
    let buffer_index = buffer_view
        .get_f64("buffer")
        .map(|value| value as usize)
        .ok_or("bufferView has no buffer")?;
    let buffer = buffers.get(buffer_index).ok_or("bufferView references a missing buffer")?;
    let view_offset = buffer_view.get_f64("byteOffset").unwrap_or(0.0) as usize;
    let stride = buffer_view
        .get_f64("byteStride")
        .map(|value| value as usize)
        .filter(|value| *value > 0)
        .unwrap_or(components * size);
    let start = view_offset + byte_offset;

    for element in 0..count {
        for component in 0..components {
            let offset = start + element * stride + component * size;
            if offset + size > buffer.len() {
                return Err("accessor reads past the end of the buffer".into());
            }
            let value = match component_type {
                5126 => f32::from_le_bytes([buffer[offset], buffer[offset + 1], buffer[offset + 2], buffer[offset + 3]]),
                5123 => u16::from_le_bytes([buffer[offset], buffer[offset + 1]]) as f32,
                5125 => u32::from_le_bytes([
                    buffer[offset],
                    buffer[offset + 1],
                    buffer[offset + 2],
                    buffer[offset + 3],
                ]) as f32,
                5121 => buffer[offset] as f32,
                5120 => buffer[offset] as i8 as f32,
                5122 => i16::from_le_bytes([buffer[offset], buffer[offset + 1]]) as f32,
                other => return Err(format!("unsupported component type {}", other)),
            };
            values.push(value);
        }
    }

    Ok(Accessor { values, components, count })
}

fn read_indices(document: &Json, buffers: &[Vec<u8>], accessor_index: usize) -> Result<Vec<u32>, String> {
    let accessor = read_accessor(document, buffers, accessor_index)?;
    if accessor.components != 1 {
        return Err("index accessor must be SCALAR".into());
    }
    Ok(accessor.values.iter().map(|value| *value as u32).collect())
}

pub fn parse_document(document: &Json, base_dir: &Path, binary_chunk: Option<&Vec<u8>>) -> Result<MeshData, String> {
    let buffers = load_buffers(document, base_dir, binary_chunk)?;
    let meshes = document.get_array("meshes").ok_or("glTF has no meshes")?;

    let mut mesh = MeshData::default();
    let mut saw_normals = false;

    for source_mesh in meshes {
        let Some(primitives) = source_mesh.get_array("primitives") else {
            continue;
        };
        for primitive in primitives {
            let mode = primitive.get_f64("mode").unwrap_or(4.0) as u32;
            if mode != 4 {
                return Err(format!("unsupported primitive mode {} (only triangles are supported)", mode));
            }
            let attributes = primitive.get("attributes").and_then(Json::as_object).ok_or("primitive has no attributes")?;
            let position_index = attributes
                .get("POSITION")
                .and_then(Json::as_usize)
                .ok_or("primitive has no POSITION attribute")?;
            let positions = read_accessor(document, &buffers, position_index)?;
            if positions.components != 3 {
                return Err("POSITION must be VEC3".into());
            }

            let normals = attributes
                .get("NORMAL")
                .and_then(Json::as_usize)
                .map(|index| read_accessor(document, &buffers, index))
                .transpose()?;
            if normals.is_some() {
                saw_normals = true;
            }
            let uvs = attributes
                .get("TEXCOORD_0")
                .and_then(Json::as_usize)
                .map(|index| read_accessor(document, &buffers, index))
                .transpose()?;

            let base = mesh.vertices.len() as u32;
            for vertex_index in 0..positions.count {
                let mut vertex = Vertex::default();
                vertex.position = [
                    positions.values[vertex_index * 3],
                    positions.values[vertex_index * 3 + 1],
                    positions.values[vertex_index * 3 + 2],
                ];
                if let Some(ref normal_data) = normals {
                    vertex.normal = [
                        normal_data.values[vertex_index * 3],
                        normal_data.values[vertex_index * 3 + 1],
                        normal_data.values[vertex_index * 3 + 2],
                    ];
                }
                if let Some(ref uv_data) = uvs {
                    vertex.uv = [uv_data.values[vertex_index * 2], uv_data.values[vertex_index * 2 + 1]];
                }
                mesh.vertices.push(vertex);
            }

            match primitive.get("indices").and_then(Json::as_usize) {
                Some(index_accessor) => {
                    let indices = read_indices(document, &buffers, index_accessor)?;
                    if indices.len() % 3 != 0 {
                        return Err("index count is not a multiple of three".into());
                    }
                    mesh.indices.extend(indices.iter().map(|index| index + base));
                }
                None => {
                    if positions.count % 3 != 0 {
                        return Err("non-indexed primitive vertex count is not a multiple of three".into());
                    }
                    for index in 0..positions.count as u32 {
                        mesh.indices.push(base + index);
                    }
                }
            }
        }
    }

    if mesh.vertices.is_empty() {
        return Err("glTF contains no triangle primitives".into());
    }
    if !saw_normals {
        mesh.compute_normals();
    }
    mesh.validate()?;
    Ok(mesh)
}

/// Imports a `.gltf` or `.glb` file into a cooked-ready mesh.
pub fn import(path: &Path) -> Result<MeshData, String> {
    let loaded = load(path)?;
    let base_dir = path.parent().unwrap_or_else(|| Path::new("."));
    parse_document(&loaded.json, base_dir, loaded.binary_chunk.as_ref())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn triangle_gltf() -> String {
        // Three float positions (36 bytes) followed by three u16 indices (6 bytes)
        // packed into a base64 data URI buffer.
        let mut binary = Vec::new();
        for value in [0.0f32, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0] {
            binary.extend_from_slice(&value.to_le_bytes());
        }
        for index in [0u16, 1, 2] {
            binary.extend_from_slice(&index.to_le_bytes());
        }
        const ALPHABET: &[u8] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        let mut encoded = String::new();
        for chunk in binary.chunks(3) {
            let b0 = chunk[0] as u32;
            let b1 = *chunk.get(1).unwrap_or(&0) as u32;
            let b2 = *chunk.get(2).unwrap_or(&0) as u32;
            let triple = (b0 << 16) | (b1 << 8) | b2;
            encoded.push(ALPHABET[(triple >> 18) as usize & 63] as char);
            encoded.push(ALPHABET[(triple >> 12) as usize & 63] as char);
            encoded.push(if chunk.len() > 1 { ALPHABET[(triple >> 6) as usize & 63] as char } else { '=' });
            encoded.push(if chunk.len() > 2 { ALPHABET[triple as usize & 63] as char } else { '=' });
        }

        format!(
            r#"{{
              "asset": {{ "version": "2.0" }},
              "buffers": [{{ "uri": "data:application/octet-stream;base64,{}", "byteLength": 42 }}],
              "bufferViews": [
                {{ "buffer": 0, "byteOffset": 0, "byteLength": 36 }},
                {{ "buffer": 0, "byteOffset": 36, "byteLength": 6 }}
              ],
              "accessors": [
                {{ "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3" }},
                {{ "bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR" }}
              ],
              "meshes": [{{ "primitives": [{{ "attributes": {{ "POSITION": 0 }}, "indices": 1 }}] }}]
            }}"#,
            encoded
        )
    }

    #[test]
    fn imports_minimal_gltf() {
        let document = json::parse(&triangle_gltf()).expect("valid json");
        let mesh = parse_document(&document, &PathBuf::from("."), None).expect("imports");
        assert_eq!(mesh.vertices.len(), 3);
        assert_eq!(mesh.indices, vec![0, 1, 2]);
        assert_eq!(mesh.triangle_count(), 1);
        // Normals are generated for files without a NORMAL attribute.
        assert!(mesh.vertices[0].normal[2].abs() > 0.5);
    }

    #[test]
    fn rejects_unsupported_modes() {
        let text = r#"{
            "buffers": [{"byteLength": 36, "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}],
            "bufferViews": [{"buffer": 0, "byteLength": 36}],
            "accessors": [{"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"}],
            "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "mode": 1}]}]
        }"#;
        let document = json::parse(text).expect("valid json");
        assert!(parse_document(&document, &PathBuf::from("."), None).is_err());
    }
}
