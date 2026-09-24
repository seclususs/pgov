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

Please attach or paste the contents of the persistent error log. Depending on your
build variant, it is located at:

- NDK/Magisk: `/data/adb/modules/pgovd/pgovd.log`
- AOSP/Soong: `/data/vendor/pgovd/pgovd.log`

If the file does not exist, you may also include relevant `logcat` output:

```sh
logcat -d | grep -i pgov
```

## Steps to reproduce

1.
2.
3.

## Additional context

Anything else that might be relevant (custom kernel, other Magisk
modules active, OEM-specific power management, etc.).
