# Building

## Requirements

- CMake 3.21 or newer
- Android NDK
- Ninja - the Android presets hardcode it as the generator, there's no
  Makefiles fallback
- Python 3.9 or newer, for `tools/build.py`, `tools/mkmod.py`, and
  `tools/prepaosp.py`

## Environment

`ANDROID_NDK_HOME` must point at the NDK before configuring any of the
Android presets; the toolchain file is read straight out of it:

```
$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake
```

`tools/build.py` will auto-detect it if unset, checking the default SDK
install location for the platform (`~/Android/Sdk/ndk` on Linux,
`~/Library/Android/sdk/ndk` on macOS, `%LOCALAPPDATA%\Android\Sdk\ndk` on
Windows) and picking the highest version present. It aborts if nothing is
found there either.

## Building for Android

Four presets are defined - `arm64-debug`, `arm64-release`, `arm32-debug`,
`arm32-release` - targeting `arm64-v8a` and `armeabi-v7a` against
`android-30`. Output binaries land at `build/<abi>/<variant>/pgovd`.

### CMake presets

```bash
cmake --preset arm64-release
cmake --build --preset arm64-release
```

Substitute any of the other three preset names for a different ABI or
variant.

### `tools/build.py`

```bash
python3 tools/build.py --abi arm64 --type release
```

`--abi` (`arm64` / `arm32`) and `--type` (`debug` / `release`) are both
required. `--api <N>` overrides the target platform level (default `30`).
`--clean` wipes the entire `build/` directory when passed alone, or just
the selected preset's subdirectory when combined with `--abi`/`--type`.

This calls the same preset commands shown above - it's a wrapper with NDK
auto-detection and cleaner console output, not a separate build system.

## Building on-device (Termux)

```bash
bash tools/termux.sh
```

Installs `clang`, `cmake`, and `ninja` through `pkg` if any are missing,
detects the ABI from `uname -m`, and builds Release directly:

```
cmake -B build/<abi>/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/<abi>/release
```

No NDK toolchain file is involved - this compiles with Termux's own
native `clang`. Output lands at the same `build/<abi>/release/pgovd` path
as the preset builds.

## Packaging the Magisk module

Build the release binaries first, then:

```bash
python3 tools/mkmod.py
```

This stages whichever of `build/arm64-v8a/release/pgovd` and
`build/armeabi-v7a/release/pgovd` exist into
`modules/system/bin/<abi>/pgovd`, and zips `modules/` to
`dist/pgovd-<version>.zip` (version read from `modules/module.prop`,
currently `v1.0`). The staged binaries are removed again once the zip is
written - `modules/system/bin/` holding only `.gitkeep` afterward is
expected, not a failure. Fails if neither binary is present.

## Building for AOSP / vendor trees

`android/Android.bp` builds `libpgov` (`cc_library_static`) and `pgovd`
(`cc_binary`, `vendor: true`, thin-LTO) once this tree sits inside an
AOSP source tree where Soong can see it - no separate build invocation
is needed beyond that.

Two things need to be merged into the target tree alongside it:

- `android/pgovd.rc` as `pgovd`'s `init_rc`.
- `android/file_contexts` and `android/pgovd.te` into the vendor sepolicy - the
  `pgovd_exec` / `pgovd_vendor_data_file` types, the
  `sys_nice`/`sys_resource`/`ipc_lock` capabilities, read-only access to the
  thermal, battery, CPU topology, LED, and backlight sysfs nodes, and
  read/write access to `proc_sched` and `proc_pressure` plus write access
  to `proc_oom_score_adj`.

To extract just this path into its own tree:

```bash
python3 tools/prepaosp.py
```

Moves those four `android/` files to the repository root and removes
everything specific to the CMake/Magisk paths (`CMakeLists.txt`,
`CMakePresets.json`, `modules/`, `.github/`, `tools/`, and the lint/editor
dotfiles), leaving `include/`, `src/`, and the four build files. This is
destructive - run it against a copy, not the working tree.

## Cleaning

```bash
python3 tools/build.py --clean
python3 tools/build.py --abi arm64 --type release --clean
rm -rf build/arm64-v8a/release
```

The first wipes every configured preset. The second wipes and rebuilds
just one. The third is the manual equivalent for the raw CMake path,
where there's no built-in clean flag.
