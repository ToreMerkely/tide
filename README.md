# tide

A lightweight Qt6 code editor.

## Building

```
make build      # configure + build into ./build
make run        # build, then launch
make clean      # remove the build directory
```

## Dependencies

- **CMake** 3.16+ and a C++17 compiler.
- **Qt 6** — Widgets and PDF (PdfWidgets), required to build.
- **Qt SVG** — provides the `qsvg` image plugin, loaded at runtime for the
  bundled icons and for opening `.svg` files in the image viewer.
- **Qt image format plugins** (optional) — only needed for `.webp`; png, jpg,
  gif and bmp are handled by qtbase.

| Platform | Install |
| --- | --- |
| Debian/Ubuntu | `sudo apt install cmake qt6-base-dev qt6-pdf-dev libqt6svg6 qt6-image-formats-plugins` |
| Fedora | `sudo dnf install cmake qt6-qtbase-devel qt6-qtpdf-devel qt6-qtsvg qt6-qtimageformats` |
| Arch | `sudo pacman -S cmake qt6-base qt6-webengine qt6-svg qt6-imageformats` |
| macOS | `brew install cmake qt` |

Arch has no standalone Qt PDF package; `Qt6::PdfWidgets` ships inside
`qt6-webengine`. On macOS the Homebrew `qt` formula is full Qt 6 and covers
everything above; if CMake cannot find it, pass
`-DCMAKE_PREFIX_PATH=$(brew --prefix qt)`.

## License

MIT — see [LICENSE](LICENSE).
