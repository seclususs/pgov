# Changelog

## [v1.1]

### Added

- Persistent error logging to `pgovd.log` via `stderr` redirection.
- Multi-panel backlight monitoring supporting up to 4 concurrent
  display panels on foldable devices.
- Support debugfs scheduler fallback paths, governor dormant mode,
  and automatic sysfs baseline restoration on shutdown.

### Changed

- Optimize thermal I/O and load math with idle equilibrium
  bypasses to reduce unnecessary polling and computation.
- Transition Kalman Filter sensitivity and structural break
  thresholds from static boot-time constants to dynamic
  runtime parameters driven by live 5-minute PSI load.
- Throttle backlight sysfs polling to at most once per second while
  the display is known to be active, reducing system call frequency.
- Drop redundant userspace HZ detection in favor of a constant 50ms
  transition poll baseline, avoiding synthetic tick computations.
- Drop dead `r_meas` seed assignment in thermal filter initialization.
- Decouple transition gating decisions from hardware sysfs actuation
  in governor update loop.
- Optimize integer square root with CLZ bit positioning.
- Optimize scheduler timing quantization with bitwise rounding.
- Optimize Q16 decimal parser with scalar integer accumulation.
- Drop redundant covariance computations in Kalman filter.
- Decompose monolithic `calc_demand` in `gov.c` by extracting
  battery and thermal metrics polling into `poll_battery_metrics`
  and `poll_thermal_metrics` static helpers.
- Decompose `epoll_events` in `epoll.c` by isolating signalfd draining
  and governor trigger callback execution into `handle_signal_event`
  and `handle_trigger_event`.
- Deduplicate non-whitespace token skipping in `psi.c` via static inline
  helper `skip_token`, reducing nested parser loops.
- Modularize `pg_conf_parse` in `conf.c` by extracting buffer refilling
  into `refill_buffer` and line dispatching into `dispatch_line`, removing
  duplicate trailing line handling while preserving single-exit cleanup.
- Split monolithic configuration tuning routine into modular topology
  and subsystem helpers.
- Consolidate trailing whitespace trimming logic in configuration parser.
- Add fail-fast input validation across sysfs, scan, and fs APIs.
- Ensure battery level input is clamped within legal bounds in
  CPU integrator.
- Add input validation and division-by-zero guards in sensor routines.

### Fixed

- Prevent `-EINVAL` rejection on strict kernel `sysctl` handlers
  (e.g., `printk_devkmsg`) by enforcing a trailing newline on
  string writes in `pg_sysfs_write`.
- Fix false-positive reactor resets in `pg_poll_calc_next`
  by tracking actual wait time instead of base interval.
- Fix thermal trip-point detection by validating zone `type`
  content instead of directory names.
- Fix governor lockup on devices without CPU thermal sensors
  by omitting the static 65°C fallback.
- Fix idle cache sweep bypass where the busy-system guard
  was skipped on initial invocation.
- Stabilize load demand integration and prevent mapping gate lockup
  by adopting semi-implicit Euler damping and tracking `psi_ref`.
- Fix `-EINVAL` syscall rejection during daemon startup and cache
  sweep by passing `sched_util_min = 0` whenever clamping task
  utilization under `SCHED_FIFO`.
- Prevent prolonged thermal throttling on display wake by resetting
  stale Kalman filter velocity and PID integrator states across
  extended suspend intervals.
- Prevent reactor busy-spinning and log saturation on sysfs write
  or PSI read failures by adding exponential retry backoff.
- Preserve Android `debuggerd` crash tombstones on fatal signals
  by forwarding to previous handlers after restoring sysfs baseline.
- Fix inverted sched_setattr fallback syscall number on arm32.
- Fix core affinity fallback traversal on offline cores.
- Fix infinite epoll timeout corruption upon signal EINTR.
- Suppress governor trigger execution during shutdown.
- Fix unhandled EINTR interruptions during getdents64 traversals.
- Fix potential integer overflow in exponential decay calculation
  for large inputs.
- Enforce lower frequency clamp during thermal battery throttling.

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
