# AMD Software for Linux

Native Qt 6/QML desktop application for Linux. The shipping application is a
C++20 process and has no Python runtime dependency.

## Build

Install CMake 3.24+, Ninja, a C++20 compiler, Qt 6.8+ (Core, Gui, Qml, Quick,
Quick Controls 2 and Test), and Catch2 v3. Then run:

```sh
export ADRENALIN_BUILD_DIR="$(mktemp -d)"
: "${ADRENALIN_INSTALL_PREFIX:?Set this to the final logical install prefix}"
export ADRENALIN_STAGE_ROOT="$(mktemp -d)"
cmake -S . -B "$ADRENALIN_BUILD_DIR" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ADRENALIN_INSTALL_PREFIX"
cmake --build "$ADRENALIN_BUILD_DIR"
ctest --test-dir "$ADRENALIN_BUILD_DIR" --output-on-failure
DESTDIR="$ADRENALIN_STAGE_ROOT" cmake --install "$ADRENALIN_BUILD_DIR"
```

The installed desktop entry launches the native shell. For a headless startup
check, run `QT_QPA_PLATFORM=offscreen adrenalin-shell --smoke`. CMake chooses
platform-specific install directories beneath the caller-selected logical
prefix. `DESTDIR` stages those files without changing the activation paths
embedded for the final installation. For a custom prefix, make its installed
data directory available to session D-Bus service discovery and its executable
directory available to the desktop launcher environment. The session D-Bus
activation executable path must not contain whitespace because the D-Bus
service-file `Exec` value has no portable quoting rule; CMake rejects that
configuration. Configure with the final logical prefix and use `DESTDIR` for
staging; an install-time `cmake --install --prefix` override is unsupported
because activation paths are generated at configure time. The application and
build configuration do not assume a machine-specific filesystem path.
