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

## Alternatives considered

Other approaches you've tried - an existing `node.<path>=` /
`prop.<name>=` override in `pgovd.conf`, a fork or patch, or a
different tunable entirely - and why they fall short.

## Additional context

Anything else that might help: links to kernel docs for a scheduler
knob, how another governor handles the same problem, or device-specific
behavior this should account for.
