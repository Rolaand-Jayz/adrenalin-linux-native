# AMD Software for Linux

Native Qt 6/QML desktop application for Linux. The shipping application is a
C++20 process and has no Python runtime dependency.

## Build

Install CMake 3.24+, Ninja, a C++20 compiler, Qt 6.8+ (Core, Gui, Qml, Quick,
Quick Controls 2 and Test), and Catch2 v3. Then run:

```sh
export ADRENALIN_BUILD_DIR="$(mktemp -d)"
: "${ADRENALIN_INSTALL_PREFIX:?Set this to the caller-selected staging destination}"
cmake -S . -B "$ADRENALIN_BUILD_DIR" -G Ninja \
  -DCMAKE_INSTALL_PREFIX="$ADRENALIN_INSTALL_PREFIX"
cmake --build "$ADRENALIN_BUILD_DIR"
ctest --test-dir "$ADRENALIN_BUILD_DIR" --output-on-failure
cmake --install "$ADRENALIN_BUILD_DIR"
```

The installed desktop entry starts `adrenalin-shell`. For a headless startup
check, run `QT_QPA_PLATFORM=offscreen adrenalin-shell --smoke`. CMake chooses
platform-specific install directories beneath the caller-selected prefix.
`ADRENALIN_INSTALL_PREFIX` must be set to the packaging or staging destination;
the application and build configuration do not assume a machine-specific path.
