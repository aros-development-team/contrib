//! Compiles the C side of the shell (`c/aros_shell.c`) with the AROS C cross compiler and links
//! it in. The compiler and archiver come from `AROS_CC` / `AROS_AR` (the AROS build system sets
//! them), else `<arch>-aros-cc` / `<arch>-aros-ar` on the PATH (what the Rust crosstool installs).
//! On any other target the file is skipped, so the workspace still type-checks on the host.

use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=c/aros_shell.c");
    println!("cargo:rerun-if-env-changed=AROS_CC");
    println!("cargo:rerun-if-env-changed=AROS_AR");
    let os = std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    if os != "aros" {
        return;
    }
    let arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap_or_default();
    let cc = std::env::var("AROS_CC").unwrap_or_else(|_| format!("{arch}-aros-cc"));
    let ar = std::env::var("AROS_AR").unwrap_or_else(|_| format!("{arch}-aros-ar"));
    let out = PathBuf::from(std::env::var("OUT_DIR").unwrap_or_default());
    let obj = out.join("aros_shell.o");
    let lib = out.join("libiced_aros_shell.a");
    let status = Command::new(&cc).args(["-O2", "-Wall", "-Wno-unused-function", "-c", "c/aros_shell.c", "-o"]).arg(&obj).status();
    match status {
        Ok(s) if s.success() => {}
        Ok(s) => panic_build(&format!("{cc} failed with {s}")),
        Err(e) => panic_build(&format!("cannot run {cc}: {e}")),
    }
    let _ = std::fs::remove_file(&lib);
    let status = Command::new(&ar).arg("crs").arg(&lib).arg(&obj).status();
    match status {
        Ok(s) if s.success() => {}
        Ok(s) => panic_build(&format!("{ar} failed with {s}")),
        Err(e) => panic_build(&format!("cannot run {ar}: {e}")),
    }
    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=iced_aros_shell");
}

/// A build script reports its failure through its exit status; this is the one place a panic
/// is the right tool.
#[allow(clippy::panic)]
fn panic_build(msg: &str) -> ! {
    panic!("iced_aros build.rs: {msg}");
}
