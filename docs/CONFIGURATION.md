# Configuration

`pgovd` reads an optional config file at
`/data/adb/modules/pgovd/system/etc/pgovd.conf` once, at startup. This
applies only to the Magisk/KernelSU module build (`NDK_BUILD`); a
vendor/AOSP build doesn't compile this feature in at all. The file
itself is optional - if it isn't there, startup just continues without
it. The module ships one at that exact path, pre-populated with comments
describing the format below and no live directives, ready to be edited
or replaced outright.

## Syntax

One directive per line:

```
<prefix>.<key>=<value>
```

- Leading and trailing whitespace around both the key and the value is
  trimmed, so spaces or tabs around `=` are fine.
- `#` starts a comment that runs to the end of the line, whether it's
  the whole line or trailing after a directive.
- Blank lines are ignored.
- A line has to fit in 255 characters; anything longer is dropped
  whole rather than truncated or misparsed.
- A line with no `=`, or an empty key once whitespace is trimmed off
  it, is ignored.
- A key that doesn't start with `sysfs.` or `prop.` is ignored.

## Directives

### `sysfs.*`

Writes `<value>` to `<path>` once, in the order the lines appear in the
file. `<path>` is used exactly as written, so it needs its own leading `/`:

```
sysfs./proc/sys/kernel/sched_child_runs_first=0
```

A failed write - bad path, no permission - is logged and skipped; it
doesn't stop the rest of the file from being processed. There's no limit
on how many `sysfs.*` lines a file can have.

### `prop.*`

Sets an Android system property. The first time a given property name is
touched, its current value is captured; that original value is restored
automatically when the daemon exits normally, so an override never
outlives the process:

```
prop.debug.sf.latch_unsignaled=1
```

Up to 64 properties can be overridden at once. Past that, additional
`prop.*` lines are logged and skipped.
