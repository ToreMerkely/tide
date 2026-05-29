#ifndef PDFVIEWER_H
#define PDFVIEWER_H

#include <QWidget>

class QPdfDocument;
class QPdfView;

// Read-only PDF viewer shown in an editor tab in place of a CodeEditor.
// Wraps Qt's QPdfView (scrolling, multi-page) and QPdfDocument.
class PdfViewer : public QWidget {
    Q_OBJECT

public:
    explicit PdfViewer(QWidget *parent = nullptr);

    // Loads the PDF at path. Returns false if it could not be opened.
    bool load(const QString &path);

private:
    QPdfDocument *m_doc;
    QPdfView *m_view;
};

#endif
