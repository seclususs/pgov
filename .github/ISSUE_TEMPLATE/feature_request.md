---
name: Feature request
about: Something you'd like pgov to do that it doesn't yet
title: "[Feature] "
labels: enhancement
assignees: ""
---

## Describe the feature

A clear description of what you want `pgovd` (or the surrounding
build/module tooling) to do, and what problem or limitation prompted
the request.

## Scope

- **Build variant:** Magisk module / AOSP-Soong vendor build / Termux build (delete two)
- **Component (if known):** e.g. PSI monitor, governor mapping, thermal throttling,
  calibration, config parser, build tooling

## Fit with existing constraints

`pgov` is single-threaded, allocates no heap past startup, and runs its
control loop entirely in Q16.16 fixed-point - no runtime floating point
(see [`ARCHITECTURE`](../../docs/ARCHITECTURE.md)). Its six sysfs
channels also degrade independently per-node rather than all-or-nothing.
Note whether this fits within that as-is or would need one of these
relaxed, and why the trade-off is worth it if so. If this adds a new
kernel node to read or write, please confirm first that it's actually
accessible on your device:

```sh
ls -l /proc/sys/kernel/<the_node>
cat /proc/sys/kernel/<the_node>
```

## Alternatives considered

Other approaches you've tried - an existing `sysfs.<path>=` /
`prop.<name>=` override in `pgovd.conf`, a fork or patch, or a
different tunable entirely - and why they fall short.

## Additional context

Anything else that might help: links to kernel docs for a scheduler
knob, how another governor handles the same problem, or device-specific
behavior this should account for.
