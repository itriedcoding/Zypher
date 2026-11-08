//! zypher-assetcooker command line interface.

use std::path::PathBuf;
use std::process::ExitCode;
use zypher_assetcooker::project;

const VERSION: &str = env!("CARGO_PKG_VERSION");

fn print_help() {
    println!(
        "zypher-assetcooker {VERSION}
Zypher asset cooker - builds runtime .zpkg packs from source assets.

Usage:
  zypher-assetcooker [options]

Options:
  -p, --project <path>   Project manifest (default: samples/demo/zypher.project.json)
  -o, --out <path>       Override the pack output path
      --list             List the assets that would be cooked and exit
  -v, --verbose          Print every cooked asset
  -h, --help             Print this message
  -V, --version          Print the version
"
    );
}

fn main() -> ExitCode {
    let mut project_path = PathBuf::from("samples/demo/zypher.project.json");
    let mut output_override: Option<PathBuf> = None;
    let mut verbose = false;
    let mut list_only = false;

    let mut arguments = std::env::args().skip(1);
    while let Some(argument) = arguments.next() {
        match argument.as_str() {
            "-h" | "--help" => {
                print_help();
                return ExitCode::SUCCESS;
            }
            "-V" | "--version" => {
                println!("zypher-assetcooker {VERSION}");
                return ExitCode::SUCCESS;
            }
            "-p" | "--project" => {
                let Some(value) = arguments.next() else {
                    eprintln!("error: --project requires a path");
                    return ExitCode::from(2);
                };
                project_path = PathBuf::from(value);
            }
            "-o" | "--out" => {
                let Some(value) = arguments.next() else {
                    eprintln!("error: --out requires a path");
                    return ExitCode::from(2);
                };
                output_override = Some(PathBuf::from(value));
            }
            "--list" => list_only = true,
            "-v" | "--verbose" => verbose = true,
            other => {
                eprintln!("error: unknown argument '{other}' (try --help)");
                return ExitCode::from(2);
            }
        }
    }

    let project = match project::load(&project_path) {
        Ok(project) => project,
        Err(error) => {
            eprintln!("error: {error}");
            return ExitCode::FAILURE;
        }
    };

    if list_only {
        println!("project '{}' ({})", project.name, project_path.display());
        println!("  asset root: {}", project.asset_root.display());
        for asset in &project.assets {
            let exists = project.asset_root.join(&asset.path).exists();
            println!("  {:<10} {} {}", if exists { "ok" } else { "missing" }, asset.path,
                     asset.asset_type.map(|kind| kind.name()).unwrap_or("auto"));
        }
        return ExitCode::SUCCESS;
    }

    println!("cooking '{}' -> {}", project.name, project.output.display());
    match project::cook(&project, output_override.as_deref(), verbose) {
        Ok(result) => {
            println!(
                "cooked {} asset(s), {} bytes -> {}",
                result.assets.len(),
                result.total_bytes,
                result.output.display()
            );
            println!("manifest: {}", result.manifest.display());
            ExitCode::SUCCESS
        }
        Err(error) => {
            eprintln!("error: {error}");
            ExitCode::FAILURE
        }
    }
}
