<p align="center">
  <img src="docs/images/pgov-banner.svg" alt="pgov: Keeping the scheduler in step">
</p>

<div align="center">

[![C](https://img.shields.io/badge/C-00599C?style=flat-square&logo=c&logoColor=white)](<https://en.wikipedia.org/wiki/C_(programming_language)>)
[![Android](https://img.shields.io/badge/Android-3DDC84?style=flat-square&logo=android&logoColor=white)](https://source.android.com/)
[![Linux](https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=black)](https://github.com/torvalds/linux)
[![CMake](https://img.shields.io/badge/CMake-064F8C?style=flat-square&logo=cmake&logoColor=white)](https://cmake.org/)

</div>

pgov is a userspace scheduler daemon for Android that reads
`/proc/pressure/cpu` and maps the metrics into six CFS tunable
writes latency, granularity, wakeup granularity, migration cost,
WALT init-task load, and `uclamp_util_min` gated by thermal and
battery state. It filters kernel PSI accounting through an adaptive
Kalman filter and writes the computed parameters to sysfs to adjust
scheduler behavior based on workload.

The daemon operates as a single thread with zero heap allocation.
Every buffer is a stack array or a member of a single static context
struct, with no calls to `malloc`, `free`, or `pthread`. The control
loop relies exclusively on fixed-point arithmetic without runtime
floating-point operations; `float` is restricted to compile-time
conversion macros.

Execution runs under the `SCHED_FIFO` real-time scheduling policy pinned
to the efficiency CPU cluster. The process caps its own `uclamp_max` at
102/1024 and sets timer slack to 50ms, bounding CPU utilization and
overhead within the scheduler.

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

Flash the zip, reboot, done

For CMake preset commands, on Termux builds, or building
into an AOSP vendor tree via Soong, see [`BUILD`](docs/BUILD.md)

## Documentation

- [`ARCHITECTURE`](docs/ARCHITECTURE.md)
- [`BUILD`](docs/BUILD.md)
- [`CHANGELOG`](docs/CHANGELOG.md)
- [`CONFIGURATION`](docs/CONFIGURATION.md)

## Disclaimer

This runs as root and rewrites scheduler on your device.<br>
Tested on my own hardware, not yours - use it at your own risk.

## License

GPL-3.0. See [LICENSE](LICENSE)
