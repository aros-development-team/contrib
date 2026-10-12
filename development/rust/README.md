# Rust crates for AROS programs

Rust programs in the tree are built with the Rust crosstool
(`configure --enable-rust`, `tools/crosstools/rust`). The crates.io crates
that need AROS changes are kept in two stores, fetched and patched once and
put in place of the crates.io versions through generated cargo config files
(cargo merges the `[patch.crates-io]` tables of every `--config` file it is
given):

* `tools/crosstools/rust/crates`: what every Rust program needs (`libc` with
  its AROS module, `getrandom`, `parking_lot_core`) and the crates mesa's NAK
  compiler is built from. Target `rust-crates-config` (fetches them and
  writes `$(RUST_CRATES_CONFIG)`), cargo runs with `$(RUST_CARGO)`
  (`tools/crosstools/rust.cfg`).
* `contrib/development/rust/crates` (this directory): application crates -
  `tokio`/`mio`/`socket2`/`errno` (an async runtime without an I/O driver:
  posixc has no `poll()` and its `select()` is a stub), `libloading`, `iced`
  and `iced_tiny_skia`, `fontique` - and the AROS-only crates in
  `crates/aros`. Target `contrib-rust-crates-config` (fetches them and
  writes `$(CONTRIB_RUST_CRATES_CONFIG)`), cargo runs with
  `$(CONTRIB_RUST_CARGO)` (`contrib-rust.cfg`). Cargo refuses a `[patch]`
  entry whose path does not exist, so a config target fetches every crate of
  its store; a program that does not use one only gets a warning.

Both unpack into `$(PORTSDIR)/rust-crates`; the archives are kept in
`$(PORTSSOURCEDIR)/rust-crates`, and the rest of a program's dependencies in
the shared cargo home `$(PORTSSOURCEDIR)/cargo-home` (one `cargo fetch` per
program snapshot, the one step that needs the network; builds then run with
`--offline`).

## The AROS-only crates (`crates/aros`)

* `iced_aros`: a shell for iced 0.14 programs - one Intuition window
  (`c/aros_shell.c`: IDCMP input, keymap.library, cybergraphics blits, the
  FTXT clipboard, timer.device), the tiny-skia renderer through its headless
  path, iced's program loop with tasks and subscriptions on the program's
  tokio executor. The patched `iced` crate uses it in place of `iced_winit`
  on AROS, so an iced program needs no source changes for the window itself.
  The program loop runs on a thread with a 64 MB stack. Test hooks:
  `ICED_AROS_SCREENSHOT=<file.png>` (with `ICED_AROS_SCREENSHOT_AFTER=<ms>`)
  writes the window and asks the program to close; `ICED_AROS_AUTOCLICK=
  <ms>:<x>,<y>[+...]` clicks; `ICED_AROS_TRACE=1` logs the loop's waits.
* `rfd-aros`: `rfd`'s `AsyncFileDialog`/`FileDialog`/`AsyncMessageDialog`/
  `MessageDialog` on asl.library file and drawer requesters and
  intuition.library's EasyRequest.

## Writing a port

A program's mmakefile fetches its upstream snapshot with `%fetch`, depends
on `rust-crates-config` and `contrib-rust-crates-config`, writes its own cargo config with `%rust_cargo_config`
(its own patches, the target linker, its target directory, `AROS_CC`/
`AROS_AR` for build scripts), fetches its dependencies once with
`%rust_cargo_fetch` (which first moves the program's `Cargo.lock` to the
stores' crate versions, since cargo only takes a `[patch]` entry whose
version is the locked one), runs
`cargo build --offline --release --target $(RUST_TARGET_TRIPLE)` with
`$(CONTRIB_RUST_CARGO) --config <its config>`, and copies the result.
`ports/graphics/photocraft` and `ports/audio/maolan` are the
examples; what still has to live with a program are its own upstream diff and
the crates only it uses.

Two things every port runs into: a process may only call `exit()` on its
main task (posixc's `exit()` returns into the startup context on that task's
stack), and pthread.library's exit handler waits for every thread, so a
program must end its thread pools before `main` returns.
