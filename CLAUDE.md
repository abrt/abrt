# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ABRT (Automatic Bug Reporting Tool) is a Fedora/RHEL system service that detects application crashes and helps users report them. It consists of a daemon, crash hooks, D-Bus API, analysis plugins, CLI, and desktop applet. Part of a larger ecosystem including libreport, satyr, gnome-abrt, and retrace-server.

## Build Commands

**Install build dependencies:**
```
./autogen.sh sysdeps --install
```

**Build from source for development** (no-args autogen.sh already sets debug flags and `--enable-native-unwinder --enable-dump-time-unwind --enable-debug`):
```
./autogen.sh
make
```

**Build RPMs (requires committed changes):**
```
tito build --rpm --test
```

**Run all unit tests:**
```
make check
```

**Run specific autotest tests by keyword or number:**
```
make check TESTSUITEFLAGS='-k koops'
make check TESTSUITEFLAGS='1 2 3'
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
- **`src/hooks/`** — OS-level crash interceptors: C/C++ core dump handler (kernel `core_pattern`), Python 3 `sys.excepthook`, kernel vmcore harvester, pstore oops collector. Hook configs (`CCpp.conf`, `python3.conf`, `oops.conf`, `vmcore.conf`) configure the interceptors themselves, separate from event configs.
- **`src/dbus/`** — Problems2 D-Bus API at `org.freedesktop.problems` with PolicyKit authorization. The `abrt_problems2_service.c` file (~91K) is the largest single file.
- **`src/plugins/`** — All plugin source files are C. Shell logic lives embedded inside `*_event.conf` files (not standalone scripts). Covers backtrace generation, kernel oops analysis, Xorg crash analysis, Bodhi update finder, uReport sender, exploitability assessment.
- **`src/lib/`** — Shared `libabrt` library: configuration management, kernel oops parsing, hook utilities, D-Bus client API. Headers in `src/include/` — key ones: `libabrt.h` (umbrella), `internal_libabrt.h` (internal API), `hooklib.h`, `problem_api.h`, `abrt-dbus.h`.
- **`src/cli/`** — Python package `abrtcli`. Entry point is `src/cli/abrt` (argparse + argcomplete). Subcommands in `src/cli/abrtcli/cli/` loaded dynamically: list, info, backtrace, gdb, remove, report, retrace, status.
- **`src/applet/`** — GTK3 desktop notification applet.
- **`src/configuration-gui/`** — `system-config-abrt` GTK3 configuration UI.
- **`src/python-problem/`** — Python library for ABRT problem database access, includes a C extension module.

### Event System

Events are defined in `.conf` files under `src/plugins/` and `src/daemon/` (installed to `/etc/libreport/events.d/`). Each file contains inline shell code triggered by conditions:

```
EVENT=post-create type=CCpp remote!=1
    # shell code here
```

Condition syntax: `=` (equals), `!=` (not equals), `~=` (regex match). Nonzero exit kills event processing; the last stderr line becomes the error message.

Event naming conventions:
- `post-create` — runs when a new problem appears
- `notify` / `notify-dup` — after post-create
- `analyze_*` / `collect_*` — deeper analysis
- `report_*` — reporting to Bugzilla, uReport, email, etc. (defined by libreport)

### Python Exception Hook

`abrt_exception_handler3.py` runs on every Python startup via a `.pth` file. It is performance-critical — avoids importing anything except `sys` at module level, using lazy imports. There's a separate container variant.

## Key Dependencies

libreport (>= 2.17.13), satyr (>= 0.24), GLib2 (>= 2.73.3), GTK3, libsoup3, json-c, systemd, polkit, rpm, augeas.

## libreport Integration

libreport (source: `https://github.com/abrt/libreport.git`) provides the core infrastructure that abrt builds on. Almost all abrt C code includes `<libreport/internal_libreport.h>` which pulls in the entire libreport API. Config paths: `CONF_DIR=/etc/libreport`, `EVENTS_CONF_DIR=/etc/libreport/events`, `PLUGINS_CONF_DIR=/etc/libreport/plugins`.

**Dump directories** (`dump_dir.h`) are the central data format. A problem is stored as a regular directory (under `/var/spool/abrt/`) with one file per element. abrt creates these; libreport reporters consume them. Core API:
- Open/create: `dd_opendir(dir, flags)`, `dd_create(dir, uid, mode)`, or two-phase `dd_create_skeleton()` + `dd_reset_ownership()`
- Read/write elements: `dd_save_text()`, `dd_save_binary()`, `dd_load_text()`, `dd_load_text_ext(dd, name, flags)`, `dd_load_int32()`, `dd_delete_item()`
- Lifecycle: `dd_delete()`, `dd_rename()`, `dd_close()`
- File iteration: `dd_init_next_file()` → `dd_get_next_file()` → `dd_clear_next_file()`
- Copy in: `dd_copy_file()`, `dd_copy_fd()` (with size limits)

**problem_data** (`problem_data.h`) is the in-memory representation — a `GHashTable` wrapping dump dir contents. Key API: `problem_data_new()`, `problem_data_add()`, `problem_data_get_content_or_NULL()`, `create_problem_data_from_dump_dir(dd)`, `create_dump_dir_from_problem_data(pd, base_dir)`, `problem_data_send_to_abrt(pd)`.

**Standard element names** are defined as `FILENAME_*` constants (~77 total) in libreport's `internal_libreport.h`. The most important:
- `type` — problem type dispatch key: `"CCpp"`, `"Python3"`, `"Kerneloops"`, `"vmcore"`, `"xorg"`
- `analyzer`, `executable`, `cmdline`, `pid`, `reason`, `backtrace`, `coredump`
- `package`, `component`, `pkg_name`, `pkg_version`, `pkg_release`, `pkg_arch`
- `uuid`, `duphash` — deduplication; `reported_to` — tracks reporting destinations
- `not-reportable` — if present, blocks reporting; `count` — occurrence count
- `event_log` — log of event executions; `remote` — set for non-local problems

**Event execution** (`run_event.h`): `struct run_event_state` with callbacks for user interaction (alert, ask, ask_yes_no, ask_password). Created via `new_run_event_state()`. Run events via `run_event_on_dir_name(state, dump_dir, event_name)` or `run_event_on_problem_data()`. Event configs are loaded from `/etc/libreport/report_event.conf` which `include`s event.d files.

**Shared conventions:**
- Initialization: `INITIALIZE_LIBREPORT()` is a one-shot guard calling `libreport_init()`. abrtd uses `abrt_init(argv)` which wraps it, loads global config, reads `ABRT_VERBOSE` env var, and sets `libreport_g_progname`.
- Logging (routes through syslog levels): `log_debug()`, `log_info()`, `log_notice()`, `log_warning()`, `error_msg()`, `error_msg_and_die()`, `log_perror()`
- Option parsing uses libreport's `OPT_BOOL`/`OPT_STRING`/`libreport_parse_opts()` framework
- Verbosity controlled by `libreport_g_verbose`

**Reporters** in libreport: bugzilla, kerneloops, mailx, mantisbt, print, systemd-journal, upload, ureport.

## satyr Integration

satyr (source: `https://github.com/abrt/satyr.git`) provides backtrace parsing, normalization, deduplication hashing, and micro-report generation.

**Report types** (`sr_report_type` enum): `SR_REPORT_CORE`, `SR_REPORT_PYTHON`, `SR_REPORT_KERNELOOPS`, `SR_REPORT_JAVA`, `SR_REPORT_GDB`, `SR_REPORT_RUBY`, `SR_REPORT_JAVASCRIPT`. Each has per-type stacktrace/thread/frame structs. Single-threaded types (Python, koops, Ruby, JS) use the stacktrace struct directly as a thread via cast.

**Key APIs abrt uses:**
- `sr_abrt_type_from_type(type_str)` — converts abrt type string (`"CCpp"`, `"Python3"`, etc.) to `sr_report_type`
- `sr_abrt_create_core_stacktrace()` / `sr_abrt_create_core_stacktrace_from_gdb()` — creates core backtrace from a dump dir
- `sr_stacktrace_parse(type, text, &location)` — parses backtrace text into structured form
- `sr_stacktrace_find_crash_thread()` — locates the crashing thread
- `sr_stacktrace_get_reason()` — brief human-readable crash explanation
- `sr_thread_get_duphash(thread, frames, prefix, flags)` — generates deduplication hash (SHA-1 of normalized top N frames with optional component prefix)
- `sr_normalize_koops_stacktrace()` — normalizes kernel oops traces for dedup

**uReport (micro-report):** `sr_report` struct bundles report type, OS info, RPM packages, stacktrace, and auth entries. `sr_abrt_report_from_dir()` builds a report from a dump directory; `sr_report_to_json()` serializes it for server submission.

**Headers:** `<satyr/stacktrace.h>`, `<satyr/thread.h>`, `<satyr/core/stacktrace.h>`, `<satyr/python/stacktrace.h>`, `<satyr/normalize.h>`, `<satyr/abrt.h>`

## Code Style

**C:** BSD style, 4-space indentation, no tabs. Braces on new line. `if`/`while`/`for` always use braces. Compiled with `-std=gnu99 -Wall -Wwrite-strings`.

**Python:** snake_case, 4-space indent, max 120 chars per line. Pylint config in `pylintrc` (disables `invalid-name` and `missing-docstring`).

**Commits:** `COMPONENT: Subject` format. Use keywords for components (not exact filenames). Subject ≤52 chars, body ≤72 chars wide. See `.git-commit-template`. Sign off commits with `git commit -s`.

## Testing

Unit tests use GNU Autotest (`tests/testsuite.at`). Test groups: `koops-parser.at`, `xorg-utils.at`, `pyhook.at`, `hooklib.at`, `abrt_conf.at`. Tests use `AT_TESTFUN` (C-based, compiled and run) and `AT_SETUP`/`AT_CHECK`. Test data lives in `tests/examples/`.

Python tests in `src/cli/tests/` and `src/python-problem/tests/` use pytest.

Integration tests in `tests/runtests/` use BeakerLib (~80+ test dirs). These modify system state — run only in VMs. Shared helpers in `tests/runtests/aux/lib.sh`.

## Packaging

The spec file (`abrt.spec`) produces ~20 subpackages. Tito manages releases (`.tito/` config with custom `AbrtVersionTagger`). Version string lives in `abrt-version` (generated by `./gen-version`).
