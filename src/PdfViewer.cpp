#include "PdfViewer.h"

#include <QPdfDocument>
#include <QPdfView>
#include <QVBoxLayout>

PdfViewer::PdfViewer(QWidget *parent)
    : QWidget(parent)
    , m_doc(new QPdfDocument(this))
    , m_view(new QPdfView(this))
{
    m_view->setDocument(m_doc);
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);
}

bool PdfViewer::load(const QString &path)
{
    return m_doc->load(path) == QPdfDocument::Error::None;
}
