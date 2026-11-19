# colibry-notes

Plain-text notes, one file per note. Part of Colibry OS, written in C++17 on [Raven](https://github.com/oscar-soirey/Raven)
(Apache-2.0; Vulkan rendering, XML layouts). Binary: `colibry-notes`. Licence: MIT (see `LICENSE`).

## Features

- Search across notes, new and delete.
- Notes are plain `.txt` files in `$XDG_DATA_HOME/colibry/notes` (override: `COLIBRY_NOTES_DIR`).

## Build and install

    ./install.sh                                  # build, install to /usr/local (downloads Raven at a pinned commit)
    ./install.sh --raven-dir /path/to/Raven       # use your own Raven checkout
    ./install.sh --uninstall
    cmake -S . -B build && cmake --build build && ctest --test-dir build

Needs `cmake`, a C11/C++17 compiler, `libx11`, `libxi` and the Vulkan headers and loader.
Raven is found as `-DRAVEN_DIR=<checkout>`, then an installed package, then a download of commit `89197438a59e`
(`RAVEN_FETCH_COMMIT`); the download needs network access while configuring.
The layout file is installed to `<prefix>/share/colibry/notes/` and found relative to the binary; `COLIBRY_SHARE` overrides.

Packaging: `PKGBUILD` (AUR), `default.nix`, CI in `.github/workflows/ci.yml`.

## Layout

- `app/`: the application and its XML layout
- `core/`: shared base (start-up, XML loading, JSON, processes), the same small copy in every Colibry repo
- `tests/`: headless tests on Raven's null backend (no window, no GPU)

## Limits

- No real Vulkan window was run: the tests use the null backend only.
- Search is case-insensitive for Latin and basic Cyrillic only.
- Raven is pre-1.0 and says its API may change; Wayland is through XWayland.
- `PKGBUILD` has `sha256sums=SKIP` and `default.nix` has `lib.fakeHash`: fill both in at release time; neither was built.
