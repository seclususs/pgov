# Changelog

## [v1.1]

### Changed

- Optimize thermal I/O and load math with idle equilibrium
  bypasses to reduce unnecessary polling and computation.

## [v1.0]

### Added

- `pgovd`, a userspace CPU scheduler governor for Android driven by
  `/proc/pressure/cpu`. Adjusts `sched_latency_ns`,
  `sched_min_granularity_ns`, `sched_wakeup_granularity_ns`,
  `sched_migration_cost_ns`, `sched_walt_init_task_load_pct`, and
  `sched_uclamp_util_min` off a running PSI estimate.
- Adaptive Kalman filtering of the PSI signal - measurement and process
  noise adjust themselves from observed innovation, with a
  structural-break check for reacting immediately to a genuine workload
  change instead of smoothing through it.
- Thermal-aware throttling: a PID controller over CPU and battery
  temperature produces a scale factor that pulls the governor's
  responsiveness back as thermal headroom shrinks, with its own setpoint
  tightening as battery temperature approaches its limit.
- Boot-time calibration of tunable ranges against detected core count,
  max CPU frequency, and thermal trip points where available, rather than
  one fixed set of constants for every device.
- Idle-time cache sweep under `/data/data` and
  `/data/media/0/Android/data`, gated on display-off and interruptible by
  backlight or pressure changes.
- Process hardening on startup: `SCHED_FIFO` real-time priority, a
  `uclamp_max` ceiling, OOM-killer immunity, `mlockall`, adjusted file
  descriptor and stack rlimits, and crash handlers that log to stderr
  on fatal signals before re-raising.
- Single-instance enforcement via a `flock()`-based lockfile.
- Optional runtime configuration file for one-shot sysfs writes and
  temporary system-property overrides at startup.
- Three build targets: NDK/CMake for a systemless Magisk module
  (`arm64-v8a` and `armeabi-v7a`), Soong for in-tree AOSP/vendor builds,
  and an on-device Termux build.
- Fixed-point (Q16.16) arithmetic throughout the control loop - no
  runtime floating point, no heap allocation past startup, single
  thread.
