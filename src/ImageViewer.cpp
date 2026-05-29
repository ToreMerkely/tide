#include "ImageViewer.h"

#include <QImageReader>
#include <QPainter>
#include <QFileInfo>

ImageViewer::ImageViewer(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
}

bool ImageViewer::load(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    // SVGs report a small nominal size; render them large so they stay
    // crisp when the viewport scales them down to fit.
    if (QFileInfo(path).suffix().compare("svg", Qt::CaseInsensitive) == 0) {
        QSize sz = reader.size();
        if (sz.isValid() && !sz.isEmpty()) {
            sz.scale(2048, 2048, Qt::KeepAspectRatio);
            reader.setScaledSize(sz);
        }
    }

    QImage img = reader.read();
    if (img.isNull())
        return false;

    m_image = img;
    update();
    return true;
}

void ImageViewer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(QPalette::Base));

    if (m_image.isNull())
        return;

    // Fit to viewport, preserving aspect ratio; never upscale past the
    // image's own (rendered) resolution.
    QSize target = m_image.size();
    target.scale(size(), Qt::KeepAspectRatio);
    target = target.boundedTo(m_image.size());

    QRect dst(QPoint(0, 0), target);
    dst.moveCenter(rect().center());

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(dst, m_image);
}
