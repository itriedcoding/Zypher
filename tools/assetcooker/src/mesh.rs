//! Cooked mesh blob ("ZYMESH01") shared with the runtime decoder.

pub const MESH_MAGIC: &[u8; 8] = b"ZYMESH01";
pub const MESH_VERSION: u32 = 1;
pub const VERTEX_STRIDE: usize = 32;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Vertex {
    pub position: [f32; 3],
    pub normal: [f32; 3],
    pub uv: [f32; 2],
}

impl Default for Vertex {
    fn default() -> Self {
        Self { position: [0.0; 3], normal: [0.0, 1.0, 0.0], uv: [0.0; 2] }
    }
}

#[derive(Clone, Debug, Default)]
pub struct MeshData {
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
}

impl MeshData {
    pub fn triangle_count(&self) -> usize {
        self.indices.len() / 3
    }

    pub fn bounds(&self) -> ([f32; 3], [f32; 3]) {
        if self.vertices.is_empty() {
            return ([0.0; 3], [0.0; 3]);
        }
        let mut min = [f32::INFINITY; 3];
        let mut max = [f32::NEG_INFINITY; 3];
        for vertex in &self.vertices {
            for axis in 0..3 {
                min[axis] = min[axis].min(vertex.position[axis]);
                max[axis] = max[axis].max(vertex.position[axis]);
            }
        }
        (min, max)
    }

    /// Recomputes smooth vertex normals, used when the source format has none.
    pub fn compute_normals(&mut self) {
        if self.indices.len() < 3 {
            return;
        }
        let mut accumulated = vec![[0.0f32; 3]; self.vertices.len()];
        for triangle in self.indices.chunks_exact(3) {
            let a = self.vertices[triangle[0] as usize].position;
            let b = self.vertices[triangle[1] as usize].position;
            let c = self.vertices[triangle[2] as usize].position;
            let ab = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
            let ac = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
            let normal = [
                ab[1] * ac[2] - ab[2] * ac[1],
                ab[2] * ac[0] - ab[0] * ac[2],
                ab[0] * ac[1] - ab[1] * ac[0],
            ];
            for index in triangle {
                let slot = &mut accumulated[*index as usize];
                slot[0] += normal[0];
                slot[1] += normal[1];
                slot[2] += normal[2];
            }
        }
        for (vertex, normal) in self.vertices.iter_mut().zip(accumulated.iter()) {
            let length = (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]).sqrt();
            if length > 1.0e-8 {
                vertex.normal = [normal[0] / length, normal[1] / length, normal[2] / length];
            } else {
                vertex.normal = [0.0, 1.0, 0.0];
            }
        }
    }

    pub fn validate(&self) -> Result<(), String> {
        if self.vertices.is_empty() {
            return Err("mesh has no vertices".into());
        }
        if self.indices.is_empty() {
            return Err("mesh has no indices".into());
        }
        if self.indices.len() % 3 != 0 {
            return Err("index count is not a multiple of three".into());
        }
        let vertex_count = self.vertices.len() as u32;
        if let Some(bad) = self.indices.iter().find(|index| **index >= vertex_count) {
            return Err(format!("index {} is out of range", bad));
        }
        Ok(())
    }

    /// Serialises to the runtime blob layout documented in MeshBlob.h.
    pub fn encode(&self) -> Vec<u8> {
        let (min, max) = self.bounds();
        let mut buffer = Vec::with_capacity(48 + self.vertices.len() * VERTEX_STRIDE + self.indices.len() * 4);
        buffer.extend_from_slice(MESH_MAGIC);
        buffer.extend_from_slice(&MESH_VERSION.to_le_bytes());
        buffer.extend_from_slice(&(self.vertices.len() as u32).to_le_bytes());
        buffer.extend_from_slice(&(self.indices.len() as u32).to_le_bytes());
        buffer.extend_from_slice(&3u32.to_le_bytes()); // has normals + uvs
        for value in min.iter().chain(max.iter()) {
            buffer.extend_from_slice(&value.to_le_bytes());
        }
        for vertex in &self.vertices {
            for value in vertex.position.iter().chain(vertex.normal.iter()).chain(vertex.uv.iter()) {
                buffer.extend_from_slice(&value.to_le_bytes());
            }
        }
        for index in &self.indices {
            buffer.extend_from_slice(&index.to_le_bytes());
        }
        buffer
    }

    pub fn decode(data: &[u8]) -> Result<MeshData, String> {
        if data.len() < 48 || &data[0..8] != MESH_MAGIC {
            return Err("invalid mesh blob header".into());
        }
        let read_u32 = |offset: usize| -> u32 {
            u32::from_le_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]])
        };
        let version = read_u32(8);
        if version != MESH_VERSION {
            return Err(format!("unsupported mesh version {}", version));
        }
        let vertex_count = read_u32(12) as usize;
        let index_count = read_u32(16) as usize;
        let expected = 48 + vertex_count * VERTEX_STRIDE + index_count * 4;
        if data.len() != expected {
            return Err(format!("mesh size mismatch: expected {}, got {}", expected, data.len()));
        }
        let mut vertices = Vec::with_capacity(vertex_count);
        let mut offset = 48;
        for _ in 0..vertex_count {
            let read_f32 = |at: usize| -> f32 {
                f32::from_le_bytes([data[at], data[at + 1], data[at + 2], data[at + 3]])
            };
            vertices.push(Vertex {
                position: [read_f32(offset), read_f32(offset + 4), read_f32(offset + 8)],
                normal: [read_f32(offset + 12), read_f32(offset + 16), read_f32(offset + 20)],
                uv: [read_f32(offset + 24), read_f32(offset + 28)],
            });
            offset += VERTEX_STRIDE;
        }
        let mut indices = Vec::with_capacity(index_count);
        for _ in 0..index_count {
            indices.push(read_u32(offset));
            offset += 4;
        }
        Ok(MeshData { vertices, indices })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn triangle() -> MeshData {
        MeshData {
            vertices: vec![
                Vertex { position: [0.0, 0.0, 0.0], normal: [0.0, 0.0, 1.0], uv: [0.0, 0.0] },
                Vertex { position: [1.0, 0.0, 0.0], normal: [0.0, 0.0, 1.0], uv: [1.0, 0.0] },
                Vertex { position: [0.0, 1.0, 0.0], normal: [0.0, 0.0, 1.0], uv: [0.0, 1.0] },
            ],
            indices: vec![0, 1, 2],
        }
    }

    #[test]
    fn round_trips() {
        let mesh = triangle();
        let encoded = mesh.encode();
        assert_eq!(encoded.len(), 48 + 3 * VERTEX_STRIDE + 3 * 4);
        let decoded = MeshData::decode(&encoded).expect("decodes");
        assert_eq!(decoded.vertices.len(), 3);
        assert_eq!(decoded.indices, vec![0, 1, 2]);
        assert!((decoded.vertices[1].position[0] - 1.0).abs() < 1e-6);
    }

    #[test]
    fn computes_normals() {
        let mut mesh = triangle();
        for vertex in &mut mesh.vertices {
            vertex.normal = [0.0, 0.0, 0.0];
        }
        mesh.compute_normals();
        for vertex in &mesh.vertices {
            assert!((vertex.normal[2] - 1.0).abs() < 1e-5);
        }
    }

    #[test]
    fn validates_indices() {
        let mut mesh = triangle();
        mesh.indices.push(9);
        mesh.indices.push(0);
        mesh.indices.push(0);
        assert!(mesh.validate().is_err());
    }
}
