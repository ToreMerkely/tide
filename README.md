# tide

A lightweight Qt6 code editor.

## Building

```
make build      # configure + build into ./build
make run        # build, then launch
make clean      # remove the build directory
```

## Dependencies

- **Qt 6** — Widgets and PDF (PdfWidgets) modules.

The PDF module is required to build (PDF files open in an embedded viewer).
On Debian/Ubuntu:

```
sudo apt install qt6-base-dev qt6-pdf-dev
```

PNG, SVG and common raster images (jpg, jpeg, gif, bmp, webp) are rendered
via Qt's built-in image plugins, which need no extra package.
