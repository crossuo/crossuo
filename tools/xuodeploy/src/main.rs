// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

use quick_xml::de::from_reader;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::ffi::OsStr;
use std::fs::{self, File};
use std::hash::Hasher;
use std::io::prelude::*;
use std::io::{BufRead, BufReader, Error, ErrorKind, Write};
use std::path::{Path, PathBuf};
use structopt::StructOpt;
use twox_hash::XxHash64;
use walkdir::WalkDir;

fn zip_file(out_path: &Path, in_path: &Path, name: &Path) -> zip::result::ZipResult<()> {
    use zip::write::FileOptions;
    let out_file = File::create(&out_path)?;
    let mut input_file = File::open(in_path)?;
    let mut buffer = Vec::new();
    input_file.read_to_end(&mut buffer)?;

    let mut zip = zip::ZipWriter::new(out_file);
    let options = FileOptions::default()
        .last_modified_time(zip::DateTime::default())
        .compression_method(zip::CompressionMethod::Deflated)
        .unix_permissions(0o755);
    zip.start_file(name.to_string_lossy(), options)?;
    zip.write_all(&*buffer)?;
    zip.finish()?;
    Ok(())
}

fn hash_file(path: &Path) -> Result<String, Error> {
    println!("hashing {}", path.display());
    let file = File::open(path)?;
    let mut buf = BufReader::new(file);
    let mut hasher = XxHash64::with_seed(0x2593);
    loop {
        let consumed = {
            let bytes = buf.fill_buf()?;
            if bytes.is_empty() {
                break;
            }
            hasher.write(bytes);
            bytes.len()
        };
        buf.consume(consumed);
    }
    let r = format!("{:016x}", hasher.finish());
    println!("hashed: {}", r);
    Ok(r)
}

type Cache = HashMap<String, (String, String)>;

fn platform_read_cache(path: &Path) -> Result<Cache, Error> {
    let mut cache = HashMap::new();
    let cache_file = path.join("cache.txt");
    let input = match File::open(cache_file) {
        Ok(f) => f,
        Err(_) => return Ok(cache),
    };
    let buffered = BufReader::new(input);
    for line in buffered.lines() {
        let s = line?;
        let v: Vec<&str> = s.split(',').collect();
        let name = v[0];
        let hash = v[1];
        let datahash = v[2];
        println!("cache {} is {} => {}", name, hash, datahash);
        cache.insert(name.into(), (hash.into(), datahash.into()));
    }
    Ok(cache)
}

fn platform_write_cache(cache: Cache, path: &Path) -> Result<(), Error> {
    let cache_file = path.join("cache.txt");
    let mut output = File::create(cache_file)?;
    for (k, v) in cache {
        output.write_all(format!("{},{},{}\n", k, v.0, v.1).as_bytes())?;
    }
    Ok(())
}

fn cache_hit(cache: &Cache, name: &Path, hash: &str) -> bool {
    let s: String = name.display().to_string();
    match cache.get(&s) {
        Some(v) => v.0 == hash,
        None => false,
    }
}

fn cache_put(cache: &mut Cache, name: &Path, hash: &str, datahash: &str) {
    let s: String = name.display().to_string();
    cache.insert(s, (hash.into(), datahash.into()));
}

#[derive(Debug, Serialize, Deserialize, PartialEq, Default, Clone)]
#[serde(rename = "file", default)]
struct XuoFile {
    name: PathBuf,
    hash: String,
    data: PathBuf,
    datahash: String,
}

#[derive(Debug, Serialize, Deserialize, PartialEq, Default)]
#[serde(rename = "release", default)]
struct XuoRelease {
    name: String,
    version: String,
    latest: bool,
    #[serde(rename = "file", default)]
    files: Vec<XuoFile>,
}

#[derive(Debug, Serialize, Deserialize, PartialEq, Default)]
#[serde(rename = "manifest", default)]
struct XuoManifest {
    #[serde(rename = "release", default)]
    releases: Vec<XuoRelease>,
}

fn platform_read_manifest(path: &Path) -> Result<XuoManifest, Error> {
    let file = match File::open(path) {
        Ok(f) => f,
        Err(_) => return Ok(XuoManifest::default()),
    };
    let reader = BufReader::new(file);
    let manifest: XuoManifest = match from_reader(reader) {
        Ok(data) => data,
        Err(_) => {
            println!("couldn't read manifest {}, creating it.", path.display());
            XuoManifest {
                ..Default::default()
            }
        }
    };
    Ok(manifest)
}

fn platform_write_manifest(manifest: XuoManifest, path: &Path) -> Result<(), Error> {
    let mut output = File::create(path)?;
    //let xml = to_string(&manifest).unwrap();
    //output.write_all(xml.as_bytes())?;
    writeln!(
        output,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<manifest>"
    )?;
    for release in &manifest.releases {
        writeln!(
            output,
            "\t<release name=\"{}\" version=\"{}\" latest=\"{}\">",
            release.name, release.version, release.latest
        )?;
        for file in &release.files {
            writeln!(
                output,
                "\t\t<file name=\"{}\" hash=\"{}\" data=\"{}\" datahash=\"{}\"/>",
                file.name.display(),
                file.hash,
                file.data.display(),
                file.datahash
            )?;
        }
        writeln!(output, "\t</release>")?;
    }
    writeln!(output, "</manifest>")?;
    Ok(())
}

fn manifest_add_release(
    manifest: &mut XuoManifest,
    new_release: XuoRelease,
    create: bool,
) -> Result<(), Error> {
    let mut replace = false;
    let mut update = false;
    for old_release in &manifest.releases {
        if old_release.name == new_release.name {
            update = true;
        }
        if old_release.name == new_release.name && old_release.version == new_release.version {
            replace = true;
            update = false;
        }
    }

    for old_release in &mut manifest.releases {
        if old_release.name != new_release.name {
            continue;
        }

        if replace {
            if old_release.version != new_release.version {
                continue;
            }
            old_release.files = new_release.files;
            return Ok(());
        } else if update {
            old_release.latest = false;
        }
    }

    let create_all = new_release.name == "all";
    if update || create || create_all {
        manifest.releases.push(new_release);
        return Ok(());
    }

    println!(
        "could not find a product \"{}\", maybe you want force create? `--force`",
        new_release.name
    );
    Err(Error::new(ErrorKind::Other, "product not found"))
}

fn manifest_get_datahash(manifest: &XuoManifest, name: &Path, hash: &str) -> Result<String, Error> {
    for release in &manifest.releases {
        for file in &release.files {
            if file.name == name && file.hash == *hash {
                return Ok(file.datahash.clone());
            }
        }
    }
    Err(Error::new(
        ErrorKind::Other,
        "file in cache not found in manifest, please delete the cache to rebuild it",
    ))
}

#[derive(StructOpt, Debug)]
struct Opt {
    /// Platform name (osx, win64, linux, ...)
    #[structopt(short = "o", long)]
    platform: Option<String>,
    /// Product name ("X:UO Launcher", "CrossUO")
    #[structopt(short, long)]
    product: Option<String>,
    /// When updating a product, a version string is required.
    /// If using an existing version, will overwrite an previous update in manifest
    /// otherwise a new release will be created.
    #[structopt(short, long)]
    version: Option<String>,
    /// Beta channel
    #[structopt(short, long)]
    beta: bool,
    /// Initialize a new platform/product
    #[structopt(short, long)]
    init: bool,
    /// Force action, can be to force create a new product and to refresh the cache
    #[structopt(short, long)]
    force: bool,
    /// Open a debug webserver
    #[structopt(short, long)]
    serve: bool,
}

fn fs_create_path(path: &Path) -> Result<(), Error> {
    if let Err(e) = fs::create_dir(path) {
        match e.kind() {
            std::io::ErrorKind::AlreadyExists => {}
            _ => return Err(e),
        }
    }
    Ok(())
}

fn build_artifact_name(name: &str, hash: &str) -> PathBuf {
    let placement = if let Some((i, _)) = hash.char_indices().rev().nth(1) {
        &hash[i..]
    } else {
        panic!("could not obtain hash for file: {}", name);
    };
    PathBuf::from(format!("{}/{}_{}", placement, name, hash).to_lowercase())
}

fn build_all_release(cache: &Cache) -> XuoRelease {
    let mut release = XuoRelease {
        name: "all".into(),
        version: "incremental".into(),
        latest: false,
        ..Default::default()
    };

    for (name, (hash, datahash)) in cache {
        release.files.push(XuoFile {
            name: name.into(),
            hash: hash.clone(),
            data: build_artifact_name(name, hash),
            datahash: datahash.clone(),
        });
    }
    release.files.sort_by(|a, b| a.name.cmp(&b.name));
    release
}

#[tokio::main]
async fn serve_main() {
    use warp::Filter;
    println!("listening to http://localhost:12593");
    let release = warp::path("release").and(warp::fs::dir("./release"));
    let update = warp::path("update").and(warp::fs::dir("./update"));
    let routes = release.or(update);
    warp::serve(routes).run(([127, 0, 0, 1], 12593)).await;
}

fn main() -> Result<(), Error> {
    let opt = Opt::from_args();
    let platform = match opt.platform {
        Some(x) => x,
        None => {
            if opt.serve {
                "".into()
            } else {
                panic!("a '--platform' parameter is required")
            }
        }
    };
    let release_path = PathBuf::from("release");
    let update_path = PathBuf::from("update");
    let beta = opt.beta;
    let platform = if beta { platform + "-beta" } else { platform };
    let platform_path = release_path.join(&platform);
    let mut product_name = None;
    let mut product_update = false;
    let mut product_pretty_name = String::from("all");
    let work_path = if let Some(product) = opt.product {
        product_pretty_name = product.clone();
        let p: String = product
            .to_lowercase()
            .chars()
            .map(|c| match c {
                'a'..='z' => c,
                '0'..='9' => c,
                _ => ' ',
            })
            .collect();
        let p = p.replace(" ", "");
        let product_path = platform_path.join(&p);
        product_name = Some(p);
        product_update = true;
        product_path
    } else {
        platform_path.clone()
    };
    let product_name = product_name;
    let product_update = product_update;
    let product_pretty_name = product_pretty_name;

    if opt.init {
        fs_create_path(&update_path)?;
        fs_create_path(&release_path)?;
        fs_create_path(&platform_path)?;
        fs_create_path(&work_path)?; // may be equal platform_path if product_name is some
        println!("{} initialized.", work_path.display());
    } else if opt.serve {
        serve_main();
    } else {
        let version = if let Some(version) = opt.version {
            version
        } else if product_update {
            println!(
                "when updating/generating a new product update, a version string is required!"
            );
            return Err(Error::new(ErrorKind::Other, "version string is required"));
        } else {
            "incremental".to_owned()
        };
        let manifest_path = release_path.join(platform + ".manifest.xml");
        let cache_file = Some(OsStr::new("cache.txt"));

        let mut cache = platform_read_cache(&platform_path)?;
        let mut manifest = platform_read_manifest(&manifest_path)?;

        let mut release = XuoRelease {
            name: product_pretty_name,
            version,
            latest: product_update,
            ..Default::default()
        };

        let mut current_product = String::new();
        for entry in WalkDir::new(&platform_path) {
            let entry = entry?;
            let file = entry.path();
            if file == platform_path {
                continue;
            }
            if file.is_dir() {
                current_product = file.file_name().unwrap().to_string_lossy().to_string();
                continue;
            }
            if file.file_name() == cache_file {
                continue;
            }
            if let Some(ref expected_product) = product_name {
                if *expected_product != current_product {
                    continue;
                }
            }

            let hash = hash_file(&file)?;
            let product_path = platform_path.join(&current_product);
            let diff = file.strip_prefix(&product_path).unwrap();
            println!("file: {} => {}", diff.display(), hash);
            let placement = if let Some((i, _)) = hash.char_indices().rev().nth(1) {
                &hash[i..]
            } else {
                panic!("could not obtain hash for file: {}", file.display());
            };

            let zip_subpath = build_artifact_name(&*diff.display().to_string(), &*hash);
            let zip_path = update_path.join(&zip_subpath);
            let artifact_path = update_path.join(&placement);
            fs_create_path(&artifact_path)?;
            let datahash = if !cache_hit(&cache, &diff, &hash) {
                println!("zip {}", zip_path.display());
                zip_file(&zip_path, &file, &diff)?;
                hash_file(&zip_path)?
            } else {
                manifest_get_datahash(&manifest, &diff, &hash)?
            };
            cache_put(&mut cache, &diff, &hash, &datahash);

            println!("found {}", file.display());
            let file = XuoFile {
                name: diff.into(),
                hash,
                data: zip_subpath,
                datahash,
            };
            release.files.push(file);
        }
        release.files.sort_by(|a, b| a.name.cmp(&b.name));

        if !release.files.is_empty() {
            if release.name != "all" {
                let all = build_all_release(&cache);
                manifest_add_release(&mut manifest, all, true)?;
            }
            manifest_add_release(&mut manifest, release, opt.force)?;
            manifest.releases.sort_by(|a, b| a.name.cmp(&b.name));
            //println!("{:#?}", manifest);
            //println!("{:#?}", cache);
            platform_write_cache(cache, &platform_path)?;
            platform_write_manifest(manifest, &manifest_path)?;
        }

        println!("updates processed.");
    }
    Ok(())
}
