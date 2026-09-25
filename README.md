# Adrenalin Linux

An independent early-stage project exploring a native Qt/QML Linux desktop for workflows inspired by AMD Software: Adrenalin Edition. Radeon RX 6000 and newer GPUs are the intended scope, not certified or supported hardware. The visual and behavior target is AMD Software: Adrenalin Edition 26.9.1 Optional.

This project is not affiliated with or endorsed by AMD.

AMD, Radeon, and AMD Software: Adrenalin Edition are trademarks of [Advanced Micro Devices, Inc.](https://www.amd.com/en/legal/trademarks.html).

## Project status

**Early development.** The repository contains a native application shell, a per-user session service, persisted settings and profile data, and read-only Linux hardware inventory for GPUs, CPU packages, and displays. GPU control providers are not implemented, and no Adrenalin screen has passed the project’s visual-parity gate.

This build is not a GPU control utility. Live hardware telemetry, tuning, display changes, hotkey activation, recording, streaming, and other hardware-facing features are not available. It does not transmit product telemetry; the saved preference only stores the user’s consent choice. No release packages or certified GPU/distribution configurations are available.

## Current implementation

- C++20 application shell built with Qt 6 and QML
- Per-user D-Bus session service with SQLite-backed settings, telemetry-consent, and profile records
- Read-only Hardware1 inventory gathered through Linux providers, with capability states that do not claim unavailable controls
- Versioned D-Bus service, settings, hardware, display, notification, profile, and hotkey contracts
- Reference-capture manifest validation and visual-diff tooling
- Candidate capture of the running QML window with component geometry; candidate artifacts are not accepted parity evidence

Hardware1 inventory is read-only and does not provide GPU tuning, telemetry, or display mutation. See the [audited ticket pack](AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md) for the verified acceptance status, open requirements, and evidence limits.

## Build and test

### Requirements

- CMake 3.24 or newer
- Ninja
- A C++20 compiler
- Qt 6.8 or newer: Core, DBus, Gui, Qml, Quick, Quick Controls 2, and Sql
- Catch2 v3 and Qt Test
- pkg-config and systemd development metadata for session-service installation
- D-Bus and `dbus-run-session` for the private-bus integration tests

### Configure and build

Set a final logical installation prefix supplied by your environment. The build and staging directories below are created dynamically.

```sh
: "${INSTALL_PREFIX:?Set the final logical installation prefix}"
build_dir="$(mktemp -d)"
stage_root="$(mktemp -d)"

cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
QT_QPA_PLATFORM=offscreen "$build_dir/adrenalin-shell" --smoke
DESTDIR="$stage_root" cmake --install "$build_dir"
```

`CMAKE_INSTALL_PREFIX` determines the final executable and activation paths. `DESTDIR` stages the installation without changing those paths. The D-Bus activation executable path must not contain whitespace. The generated desktop executable path must not contain `=` or line breaks. CMake rejects these unsupported paths during configuration. Configure with the final prefix instead of overriding it during installation.

## Platform ownership

Kernel, Mesa, firmware, and package changes remain under distribution ownership. The project does not replace those components or install Windows Radeon driver packages.

## Project documents

- [Final audited engineering spec](AMD_Adrenalin_Linux_RX6000plus_ENGINEERING_SPEC_FINAL_AUDITED%281%29.md)
- [Final audited ticket pack](AMD_Adrenalin_Linux_RX6000plus_TICKETS_FINAL_AUDITED.md)
- [Hardware1 contract notes](docs/architecture/hardware1-contract.md)
- [Reference-capture tooling](reference/README.md)

A license file has not yet been added.
