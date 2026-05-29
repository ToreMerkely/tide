#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QWidget>
#include <QImage>

// Read-only viewer for raster and SVG images, shown in an editor tab in
// place of a CodeEditor. Scales the image to fit the viewport while
// preserving aspect ratio and never upscaling raster images past native.
class ImageViewer : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewer(QWidget *parent = nullptr);

    // Loads the image at path. Returns false if it could not be decoded.
    bool load(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;
};

#endif
