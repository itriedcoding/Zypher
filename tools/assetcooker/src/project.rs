//! Project manifests and the cook pipeline.

use crate::audio;
use crate::crc::crc32;
use crate::gltf;
use crate::json;
use crate::mesh::MeshData;
use crate::obj;
use crate::pack::PackBuilder;
use crate::texture;
use crate::{asset_id, normalize_path, AssetType};
use std::path::{Path, PathBuf};

pub const SCENE_MAGIC: &[u8; 8] = b"ZYSCN001";

#[derive(Clone, Debug)]
pub struct AssetSpec {
    pub path: String,
    pub asset_type: Option<AssetType>,
}

#[derive(Clone, Debug)]
pub struct Project {
    pub name: String,
    pub directory: PathBuf,
    pub asset_root: PathBuf,
    pub output: PathBuf,
    pub scene: Option<String>,
    pub assets: Vec<AssetSpec>,
}

pub struct CookedAsset {
    pub path: String,
    pub asset_type: AssetType,
    pub bytes: usize,
    pub id: u64,
}

pub struct CookResult {
    pub output: PathBuf,
    pub manifest: PathBuf,
    pub assets: Vec<CookedAsset>,
    pub total_bytes: usize,
}

fn extension_type(extension: &str) -> AssetType {
    match extension {
        ".obj" | ".gltf" | ".glb" | ".fbx" => AssetType::Mesh,
        ".png" | ".jpg" | ".jpeg" | ".webp" | ".exr" | ".ktx2" | ".ktx" => AssetType::Texture,
        ".wav" | ".ogg" | ".opus" | ".flac" => AssetType::Audio,
        ".zscene" => AssetType::Scene,
        ".vert" | ".frag" | ".comp" | ".glsl" | ".hlsl" | ".metal" | ".slang" => AssetType::Shader,
        ".json" => AssetType::Json,
        ".ttf" | ".otf" => AssetType::Font,
        _ => AssetType::Raw,
    }
}

pub fn load(path: &Path) -> Result<Project, String> {
    let text = std::fs::read_to_string(path)
        .map_err(|error| format!("cannot read project '{}': {}", path.display(), error))?;
    let document = json::parse(&text)
        .map_err(|error| format!("'{}': {}", path.display(), error))?;
    if document.as_object().is_none() {
        return Err("project file root must be an object".into());
    }
    let directory = path.parent().unwrap_or_else(|| Path::new(".")).to_path_buf();
    let asset_root = document
        .get_str("assetRoot")
        .map(|value| directory.join(normalize_path(value)))
        .unwrap_or_else(|| directory.join("assets"));

    let mut assets = Vec::new();
    if let Some(list) = document.get_array("assets") {
        for (index, entry) in list.iter().enumerate() {
            if let Some(asset_path) = entry.as_str() {
                assets.push(AssetSpec { path: normalize_path(asset_path), asset_type: None });
                continue;
            }
            let asset_path = entry
                .get_str("path")
                .ok_or_else(|| format!("asset {} has no path", index))?;
            let asset_type = match entry.get_str("type") {
                Some(name) => Some(
                    AssetType::parse(name)
                        .ok_or_else(|| format!("asset {} has unknown type '{}'", index, name))?,
                ),
                None => None,
            };
            assets.push(AssetSpec { path: normalize_path(asset_path), asset_type });
        }
    }

    let project = Project {
        name: document.get_str("name").unwrap_or("Zypher Project").to_string(),
        directory: directory.clone(),
        asset_root,
        output: document
            .get_str("output")
            .map(|value| directory.join(normalize_path(value)))
            .unwrap_or_else(|| directory.join("dist").join("project.zpkg")),
        scene: document.get_str("scene").map(normalize_path),
        assets,
    };
    Ok(project)
}

fn cook_mesh(extension: &str, path: &Path) -> Result<Vec<u8>, String> {
    let mesh: MeshData = match extension {
        ".obj" => {
            let text = std::fs::read_to_string(path)
                .map_err(|error| format!("cannot read '{}': {}", path.display(), error))?;
            obj::parse(&text)?
        }
        ".gltf" | ".glb" => gltf::import(path)?,
        ".fbx" => {
            return Err(
                "FBX cooking requires Assimp support (configure the cooker with --features assimp)"
                    .into(),
            )
        }
        other => return Err(format!("unsupported mesh extension '{}'", other)),
    };
    Ok(mesh.encode())
}

fn cook_asset(spec: &AssetSpec, asset_root: &Path) -> Result<(AssetType, Vec<u8>), String> {
    let source = asset_root.join(&spec.path);
    let data = std::fs::read(&source)
        .map_err(|error| format!("cannot read '{}': {}", source.display(), error))?;
    let extension = source
        .extension()
        .map(|value| format!(".{}", value.to_string_lossy().to_ascii_lowercase()))
        .unwrap_or_default();
    let asset_type = spec.asset_type.unwrap_or_else(|| extension_type(&extension));

    let payload = match asset_type {
        AssetType::Mesh => cook_mesh(&extension, &source)?,
        AssetType::Texture => texture::encode_blob(&extension, &data)?,
        AssetType::Audio => audio::encode_blob(&extension, &data)?,
        AssetType::Scene => {
            // Validate that the scene is parseable JSON, then embed it verbatim.
            let text = String::from_utf8(data.clone())
                .map_err(|_| format!("'{}' is not valid UTF-8", spec.path))?;
            json::parse(&text).map_err(|error| format!("'{}': {}", spec.path, error))?;
            let mut blob = Vec::with_capacity(8 + data.len());
            blob.extend_from_slice(SCENE_MAGIC);
            blob.extend_from_slice(&data);
            blob
        }
        _ => data,
    };
    Ok((asset_type, payload))
}

fn escape_json(text: &str) -> String {
    let mut output = String::with_capacity(text.len() + 2);
    for character in text.chars() {
        match character {
            '"' => output.push_str("\\\""),
            '\\' => output.push_str("\\\\"),
            '\n' => output.push_str("\\n"),
            '\r' => output.push_str("\\r"),
            '\t' => output.push_str("\\t"),
            _ => output.push(character),
        }
    }
    output
}

pub fn cook(project: &Project, output_override: Option<&Path>, verbose: bool) -> Result<CookResult, String> {
    let mut builder = PackBuilder::new();
    let mut cooked: Vec<CookedAsset> = Vec::new();

    if project.assets.is_empty() {
        return Err("project lists no assets".into());
    }

    for spec in &project.assets {
        let (asset_type, payload) = cook_asset(spec, &project.asset_root)?;
        let id = asset_id(&spec.path);
        if verbose {
            println!(
                "  cook  {:<10} {:>8} bytes  {}",
                asset_type.name(),
                payload.len(),
                spec.path
            );
        }
        cooked.push(CookedAsset { path: spec.path.clone(), asset_type, bytes: payload.len(), id });
        builder.add(id, &spec.path, asset_type, payload)?;
    }

    // Scenes are always embedded so a pack is self contained.
    if let Some(scene) = &project.scene {
        let scene_path = project.directory.join(scene);
        if scene_path.exists() && !cooked.iter().any(|asset| asset.path == *scene) {
            let text = std::fs::read_to_string(&scene_path)
                .map_err(|error| format!("cannot read scene '{}': {}", scene_path.display(), error))?;
            json::parse(&text).map_err(|error| format!("'{}': {}", scene, error))?;
            let mut blob = Vec::new();
            blob.extend_from_slice(SCENE_MAGIC);
            blob.extend_from_slice(text.as_bytes());
            let id = asset_id(scene);
            if verbose {
                println!("  cook  {:<10} {:>8} bytes  {}", "scene", blob.len(), scene);
            }
            cooked.push(CookedAsset { path: scene.clone(), asset_type: AssetType::Scene, bytes: blob.len(), id });
            builder.add(id, scene, AssetType::Scene, blob)?;
        }
    }

    let output = output_override.map(|path| path.to_path_buf()).unwrap_or_else(|| project.output.clone());
    let total_bytes = builder.write_to(&output)?;

    let manifest = output.with_extension("zpkg.json");
    let mut manifest_text = String::new();
    manifest_text.push_str("{\n");
    manifest_text.push_str(&format!("  \"project\": \"{}\",\n", escape_json(&project.name)));
    manifest_text.push_str(&format!("  \"version\": 1,\n"));
    manifest_text.push_str(&format!("  \"totalBytes\": {},\n", total_bytes));
    manifest_text.push_str(&format!("  \"packCrc32\": \"{:08x}\",\n", {
        let bytes = std::fs::read(&output).map_err(|error| format!("cannot re-read pack: {}", error))?;
        crc32(&bytes)
    }));
    manifest_text.push_str("  \"assets\": [\n");
    for (index, asset) in cooked.iter().enumerate() {
        manifest_text.push_str(&format!(
            "    {{ \"path\": \"{}\", \"type\": \"{}\", \"id\": \"{:016x}\", \"bytes\": {} }}{}\n",
            escape_json(&asset.path),
            asset.asset_type.name(),
            asset.id,
            asset.bytes,
            if index + 1 < cooked.len() { "," } else { "" }
        ));
    }
    manifest_text.push_str("  ]\n}\n");
    if let Some(parent) = manifest.parent() {
        std::fs::create_dir_all(parent)
            .map_err(|error| format!("cannot create '{}': {}", parent.display(), error))?;
    }
    std::fs::write(&manifest, manifest_text)
        .map_err(|error| format!("cannot write manifest '{}': {}", manifest.display(), error))?;

    Ok(CookResult { output, manifest, assets: cooked, total_bytes })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    fn temp_directory(name: &str) -> PathBuf {
        let path = std::env::temp_dir().join(format!("zypher-cooker-{}-{}", name, std::process::id()));
        let _ = fs::remove_dir_all(&path);
        fs::create_dir_all(&path).unwrap();
        path
    }

    #[test]
    fn cooks_obj_project_end_to_end() {
        let directory = temp_directory("obj");
        fs::create_dir_all(directory.join("assets/models")).unwrap();
        fs::write(
            directory.join("assets/models/tri.obj"),
            "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n",
        )
        .unwrap();
        let project_text = r#"{
            "name": "Test",
            "assets": [{ "path": "models/tri.obj", "type": "mesh" }]
        }"#;
        fs::write(directory.join("project.json"), project_text).unwrap();

        let project = load(&directory.join("project.json")).expect("loads project");
        assert_eq!(project.name, "Test");
        let result = cook(&project, None, false).expect("cooks");
        assert_eq!(result.assets.len(), 1);
        assert!(result.output.exists());
        assert!(result.manifest.exists());

        let pack = fs::read(&result.output).unwrap();
        assert_eq!(&pack[0..8], crate::pack::MAGIC);
        let _ = fs::remove_dir_all(&directory);
    }

    #[test]
    fn rejects_missing_assets() {
        let project = Project {
            name: "Missing".into(),
            directory: PathBuf::from("."),
            asset_root: PathBuf::from("definitely-not-here"),
            output: PathBuf::from("unused.zpkg"),
            scene: None,
            assets: vec![AssetSpec { path: "nope.obj".into(), asset_type: None }],
        };
        assert!(cook(&project, None, false).is_err());
    }
}
