//! Wavefront OBJ importer.
//!
//! Handles `v`, `vt`, `vn` and polygonal `f` records (triangulated with a fan),
//! positive and negative indices, and generates normals when the file has none.

use crate::mesh::{MeshData, Vertex};
use std::collections::HashMap;

#[derive(Clone, Copy, PartialEq, Eq, Hash)]
enum VertexKey {
    PositionOnly(u32),
    PositionUv(u32, u32),
    PositionNormal(u32, u32),
    Full(u32, u32, u32),
}

fn parse_index(token: &str, count: usize, label: &str) -> Result<usize, String> {
    let value: i64 = token
        .parse()
        .map_err(|_| format!("invalid {} index '{}'", label, token))?;
    let resolved = if value > 0 {
        value - 1
    } else if value < 0 {
        count as i64 + value
    } else {
        return Err(format!("{} index 0 is not valid (OBJ indices are 1-based)", label));
    };
    if resolved < 0 || resolved >= count as i64 {
        return Err(format!("{} index '{}' is out of range", label, token));
    }
    Ok(resolved as usize)
}

pub fn parse(text: &str) -> Result<MeshData, String> {
    let mut positions: Vec<[f32; 3]> = Vec::new();
    let mut texcoords: Vec<[f32; 2]> = Vec::new();
    let mut normals: Vec<[f32; 3]> = Vec::new();

    let mut vertices: Vec<Vertex> = Vec::new();
    let mut indices: Vec<u32> = Vec::new();
    let mut lookup: HashMap<VertexKey, u32> = HashMap::new();
    let mut saw_normals = false;

    for (line_number, raw_line) in text.lines().enumerate() {
        let line = raw_line.split('#').next().unwrap_or("").trim();
        if line.is_empty() {
            continue;
        }
        let mut parts = line.split_whitespace();
        let keyword = parts.next().unwrap_or("");
        match keyword {
            "v" => {
                let values: Vec<f32> = parts
                    .take(3)
                    .map(|value| value.parse::<f32>())
                    .collect::<Result<_, _>>()
                    .map_err(|_| format!("line {}: invalid vertex position", line_number + 1))?;
                if values.len() != 3 {
                    return Err(format!("line {}: vertex needs three components", line_number + 1));
                }
                positions.push([values[0], values[1], values[2]]);
            }
            "vt" => {
                let values: Vec<f32> = parts
                    .take(2)
                    .map(|value| value.parse::<f32>())
                    .collect::<Result<_, _>>()
                    .map_err(|_| format!("line {}: invalid texture coordinate", line_number + 1))?;
                if values.len() != 2 {
                    return Err(format!("line {}: texture coordinate needs two components", line_number + 1));
                }
                texcoords.push([values[0], values[1]]);
            }
            "vn" => {
                let values: Vec<f32> = parts
                    .take(3)
                    .map(|value| value.parse::<f32>())
                    .collect::<Result<_, _>>()
                    .map_err(|_| format!("line {}: invalid normal", line_number + 1))?;
                if values.len() != 3 {
                    return Err(format!("line {}: normal needs three components", line_number + 1));
                }
                let length = (values[0] * values[0] + values[1] * values[1] + values[2] * values[2]).sqrt();
                let normal = if length > 1.0e-8 {
                    [values[0] / length, values[1] / length, values[2] / length]
                } else {
                    [0.0, 1.0, 0.0]
                };
                normals.push(normal);
                saw_normals = true;
            }
            "f" => {
                let mut face: Vec<u32> = Vec::new();
                for token in parts {
                    let mut fields = token.split('/');
                    let position_token = fields.next().unwrap_or("");
                    let uv_token = fields.next().unwrap_or("");
                    let normal_token = fields.next().unwrap_or("");

                    let position_index = parse_index(position_token, positions.len(), "position")?;
                    let uv_index = if uv_token.is_empty() {
                        None
                    } else {
                        Some(parse_index(uv_token, texcoords.len(), "texture")?)
                    };
                    let normal_index = if normal_token.is_empty() {
                        None
                    } else {
                        Some(parse_index(normal_token, normals.len(), "normal")?)
                    };

                    let key = match (uv_index, normal_index) {
                        (None, None) => VertexKey::PositionOnly(position_index as u32),
                        (Some(uv), None) => VertexKey::PositionUv(position_index as u32, uv as u32),
                        (None, Some(normal)) => {
                            VertexKey::PositionNormal(position_index as u32, normal as u32)
                        }
                        (Some(uv), Some(normal)) => {
                            VertexKey::Full(position_index as u32, uv as u32, normal as u32)
                        }
                    };

                    let vertex_index = match lookup.get(&key) {
                        Some(index) => *index,
                        None => {
                            let mut vertex = Vertex { position: positions[position_index], ..Vertex::default() };
                            if let Some(uv) = uv_index {
                                vertex.uv = texcoords[uv];
                            }
                            if let Some(normal) = normal_index {
                                vertex.normal = normals[normal];
                            } else {
                                vertex.normal = [0.0, 0.0, 0.0];
                            }
                            let index = vertices.len() as u32;
                            vertices.push(vertex);
                            lookup.insert(key, index);
                            index
                        }
                    };
                    face.push(vertex_index);
                }
                if face.len() < 3 {
                    return Err(format!("line {}: face has fewer than three vertices", line_number + 1));
                }
                for triangle in 1..face.len() - 1 {
                    indices.push(face[0]);
                    indices.push(face[triangle]);
                    indices.push(face[triangle + 1]);
                }
            }
            _ => {}
        }
    }

    if positions.is_empty() {
        return Err("OBJ contains no vertices".into());
    }
    if indices.is_empty() {
        return Err("OBJ contains no faces".into());
    }

    let mut mesh = MeshData { vertices, indices };
    if !saw_normals {
        mesh.compute_normals();
    }
    mesh.validate()?;
    Ok(mesh)
}

#[cfg(test)]
mod tests {
    use super::*;

    const CUBE: &str = r#"
o Cube
v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
vt 0 0
vt 1 0
vt 1 1
vt 0 1
vn 0 0 1
f 5/1/1 6/2/1 7/3/1
f 5/1/1 7/3/1 8/4/1
"#;

    #[test]
    fn parses_quads_and_triangles() {
        let mesh = parse(
            "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\nf -4 -3 -2\n",
        )
        .expect("parses quad");
        assert_eq!(mesh.vertices.len(), 4);
        assert_eq!(mesh.indices.len(), 9); // quad -> 2 triangles + 1 triangle
        assert_eq!(mesh.triangle_count(), 3);
    }

    #[test]
    fn generates_missing_normals() {
        let mesh = parse(CUBE).expect("parses cube");
        assert_eq!(mesh.indices.len(), 6);
        for vertex in &mesh.vertices {
            let length = (vertex.normal[0] * vertex.normal[0]
                + vertex.normal[1] * vertex.normal[1]
                + vertex.normal[2] * vertex.normal[2])
                .sqrt();
            assert!((length - 1.0).abs() < 1e-4);
        }
    }

    #[test]
    fn rejects_out_of_range_indices() {
        assert!(parse("v 0 0 0\nv 1 0 0\nf 1 2 3\n").is_err());
        assert!(parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 0 1 2\n").is_err());
    }
}
