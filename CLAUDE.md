# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ABRT (Automatic Bug Reporting Tool) is a Fedora/RHEL system service that detects application crashes and helps users report them. It consists of a daemon, crash hooks, D-Bus API, analysis plugins, CLI, and desktop applet. Part of a larger ecosystem including libreport, satyr, gnome-abrt, and retrace-server.

## Build Commands

**Install build dependencies:**
```
./autogen.sh sysdeps --install
```

**Build RPMs (requires committed changes):**
```
tito build --rpm --test
```

**Build from source for development:**
```
CFLAGS="-g -g3 -ggdb -ggdb3 -O0" ./autogen.sh --prefix=/usr \
    --sysconfdir=/etc --localstatedir=/var --sharedstatedir=/var/lib --enable-debug
make
```

**Run unit tests:**
```
make check
```

**Run unit tests with Valgrind:**
```
make maintainer-check
```

**Run Python CLI tests:**
```
pytest-3 src/cli/tests/
```

**Integration tests (BeakerLib, run in VMs only):**
```
tmt run -vv
```

## Architecture

The system follows a pipeline: crash hooks capture crash data → `abrtd` daemon processes it → analysis plugins enrich it → reporters send it upstream.

- **`src/daemon/`** — Core daemon (`abrtd`) monitors `/var/spool/abrt/` via inotify, processes crashes through event pipelines. Also contains the server component and RPM integration.
- **`src/hooks/`** — OS-level crash interceptors: C/C++ core dump handler (kernel `core_pattern`), Python 3 `sys.excepthook`, kernel vmcore harvester, pstore oops collector.
- **`src/dbus/`** — Problems2 D-Bus API at `org.freedesktop.problems` with PolicyKit authorization. The `abrt_problems2_service.c` file (~91K) is the largest single file.
- **`src/plugins/`** — ~90 files (C + Python + shell): backtrace generation, kernel oops analysis, Xorg crash analysis, Bodhi update finder, uReport sender, exploitability assessment.
- **`src/lib/`** — Shared `libabrt` library: configuration management, kernel oops parsing, hook utilities, D-Bus client API. Headers in `src/include/`.
- **`src/cli/`** — Python package `abrtcli` with subcommands (list, info, backtrace, gdb, remove, report, retrace, status).
- **`src/applet/`** — GTK3 desktop notification applet.
- **`src/configuration-gui/`** — `system-config-abrt` GTK3 configuration UI.
- **`src/python-problem/`** — Python library for ABRT problem database access, includes a C extension module.

## Key Dependencies

libreport (>= 2.17.13), satyr (>= 0.24), GLib2 (>= 2.73.3), GTK3, libsoup3, json-c, systemd, polkit, rpm, augeas.

## libreport Integration

libreport (source at ``https://github.com/abrt/libreport.git) provides the core infrastructure that abrt builds on. Almost all abrt C code includes `<libreport/internal_libreport.h>` which pulls in the entire libreport API.

**Dump directories** are the central data format. A problem is stored as a regular directory (under `/var/spool/abrt/`) with one file per element. abrt creates these; libreport reporters consume them. Key API: `dd_opendir()`, `dd_create()`, `dd_save_text()`, `dd_load_text()`, `dd_delete()`, `dd_close()`.

**Standard element names** are defined as `FILENAME_*` constants in libreport's `internal_libreport.h`. The most important:
- `type` — problem type dispatch key: `"CCpp"`, `"Python3"`, `"Kerneloops"`, `"vmcore"`, `"xorg"`
- `analyzer`, `executable`, `cmdline`, `pid`, `reason`, `backtrace`, `coredump`
- `package`, `component`, `pkg_name`, `pkg_version`, `pkg_release`, `pkg_arch`
- `uuid`, `duphash` — deduplication; `reported_to` — tracks reporting destinations
- `not-reportable` — if present, blocks reporting; `count` — occurrence count
- `event_log` — log of event executions

**Event system** drives the processing pipeline. Events are defined in `.conf` files under `/etc/libreport/events.d/` with rules like `EVENT=post-create type=CCpp`. abrt installs its own event configs there. Event naming conventions:
- `post-create` — runs when a new problem appears
- `notify` / `notify-dup` — after post-create
- `analyze_*` / `collect_*` — deeper analysis (abrt defines these)
- `report_*` — actual reporting to Bugzilla, uReport, email, etc. (libreport defines these)
- `workflow_*` — orchestrated reporting sequences per distro/type

**Shared conventions:**
- All programs must call `INITIALIZE_LIBREPORT()` early
- Logging uses libreport macros: `log_warning()`, `log_debug()`, `error_msg()`, `error_msg_and_die()`
- Option parsing uses libreport's `OPT_BOOL`/`OPT_STRING`/`libreport_parse_opts()` framework
- Verbosity controlled by `libreport_g_verbose`

## Code Style

**C:** BSD style, 4-space indentation, no tabs. Braces on new line. `if`/`while`/`for` always use braces. Compiled with `-std=gnu99 -Wall -Wwrite-strings`.

**Python:** snake_case, 4-space indent, max 120 chars per line. Pylint config in `pylintrc` (disables `invalid-name` and `missing-docstring`).

**Commits:** `COMPONENT: Subject` format. Use keywords for components (not exact filenames). Subject ≤52 chars, body ≤72 chars wide. See `.git-commit-template`.

## Testing

Unit tests use GNU Autotest (`tests/testsuite.at` and friends) — primarily test kernel oops parsing, Xorg utilities, Python hooks, hook library, and config parsing. Test data lives in `tests/examples/`.

Python tests in `src/cli/tests/` and `src/python-problem/tests/` use pytest.

Integration tests in `tests/runtests/` use BeakerLib (~80+ test dirs). These modify system state — run only in VMs. Shared helpers in `tests/runtests/aux/lib.sh`.

## Packaging

The spec file (`abrt.spec`) produces ~20 subpackages. Tito manages releases (`.tito/` config with custom `AbrtVersionTagger`). Version string lives in `abrt-version`.
