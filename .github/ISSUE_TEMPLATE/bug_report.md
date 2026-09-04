---
name: Bug report
about: Something isn't working as expected on your device
title: "[Bug] "
labels: bug
assignees: ""
---

## Describe the bug

A clear description of what's wrong and what you expected instead.

## Environment

- **Build variant:** Magisk module / AOSP-Soong vendor build / Termux build (delete two)
- **Device & chipset:**
- **Android version:**
- **Kernel version** (`uname -r`):
- **Root method / Magisk (or fork) version, if applicable:**
- **SELinux mode** (`getenforce`): enforcing / permissive

## PSI & sysfs availability

`pgovd` refuses to start without a readable+writable `/proc/pressure/cpu`,
and several tunables degrade gracefully per-node if unavailable. Please
check and paste the output of:

```sh
cat /proc/pressure/cpu
ls -l /proc/sys/kernel/sched_latency_ns /proc/sys/kernel/sched_wakeup_granularity_ns 2>&1
```

## Logs

Relevant `logcat` output filtered to this daemon, e.g.:

```sh
logcat -d | grep -i pgov
```

If the daemon crashed, please also include the crash line(s), if any
show up. `pgovd`'s own handler only writes a generic line to stderr
before resetting the signal to its default disposition and re-raising -
on the Magisk build, `service.sh` sends that to `/dev/null` anyway. It
also replaces whatever handler was registered before it without
chaining to it, so don't assume the usual Android crash report shows up
in logcat either.

## Steps to reproduce

1.
2.
3.

## Additional context

Anything else that might be relevant (custom kernel, other Magisk
modules active, OEM-specific power management, etc.).
