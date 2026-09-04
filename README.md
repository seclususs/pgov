<p align="center">
  <img src="docs/images/pgov-banner.svg" alt="pgov: Keeping the scheduler in step">
</p>

<div align="center">

[![C](https://img.shields.io/badge/C-00599C?style=flat-square&logo=c&logoColor=white)](<https://en.wikipedia.org/wiki/C_(programming_language)>)
[![Android](https://img.shields.io/badge/Android-3DDC84?style=flat-square&logo=android&logoColor=white)](https://source.android.com/)
[![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=black)](https://github.com/torvalds/linux)
[![CMake](https://img.shields.io/badge/CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)

</div>

# pgov

A userspace scheduler daemon for Android that reads
`/proc/pressure/cpu` and turns it into six CFS tunable writes -
latency, granularity, wakeup granularity, migration cost, WALT
init-task load, and `uclamp_util_min` - gated by thermal and battery
state. It doesn't invent its own load metric; it filters the kernel's
own PSI accounting through an adaptive Kalman filter and maps the
result to sysfs, so the scheduler's behavior tracks what the system is
actually doing instead of sitting on one static configuration.

A few constraints I settled on early and don't plan to revisit:

- **Single thread, no heap.** Every buffer is a stack array or a field
  of one static context struct. No `malloc`, no `free`, no `pthread`.
- **No runtime floating point.** The entire control loop is Q16.16
  fixed-point. `float` only appears inside compile-time conversion
  macros.
- **Invisible by default.** The process runs `SCHED_FIFO` but pins
  itself to the efficiency cluster, caps its own `uclamp_max` at
  102/1024, and relaxes timer slack to 50ms - it outranks most things
  on the system but goes out of its way not to show up in battery
  stats.

## Requirements

- Android 11 or newer (API 30)
- Kernel 4.14 or newer with `CONFIG_PSI` enabled
- `/proc/pressure/cpu` readable and writable
- Root - Magisk, KernelSU, or a vendor build running as uid 0

## Quick start

Grab the latest zip from
[Releases](https://github.com/seclususs/pgov/releases), or
build it yourself:

```bash
export ANDROID_NDK_HOME=/path/to/ndk   # or let build.py find it

python3 tools/build.py --abi arm64 --type release
python3 tools/build.py --abi arm32 --type release
python3 tools/mkmod.py
```

Flash the zip, reboot, done - `service.sh` starts the daemon
automatically.

For raw CMake preset commands, on-device Termux builds, or building
into an AOSP vendor tree via Soong, see [`BUILD`](docs/BUILD.md).

## Configuration

An optional config file at
`/data/adb/modules/pgovd/system/etc/pgovd.conf` (only present when
built with `NDK_BUILD`) supports two kinds of directives:

```
sysfs./proc/sys/vm/swappiness=60
prop.persist.sys.example=value
```

`sysfs.*` writes are fire-and-forget at startup. `prop.*` overrides
capture the original value first and restore it when the daemon exits,
so nothing outlives the process. Blank lines and `#` comments are
fine, anything past the line buffer (256 bytes) gets silently skipped
rather than misparsed.

## Documentation

- [`ARCHITECTURE`](docs/ARCHITECTURE.md) - start here if
  you want to understand what the code is doing and why.
- [`BUILD`](docs/BUILD.md) - detailed build instructions.
- [`CHANGELOG`](docs/CHANGELOG.md) - release history.

## License

GPL-3.0. See [LICENSE](LICENSE).

This runs as root and rewrites scheduler on your device.
Tested on my own hardware, not yours - use it at your own risk.
