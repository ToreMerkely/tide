#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QElapsedTimer>
#include <QVector>
#include <QSet>
#include <QTextCursor>
#include <QTimer>

class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    int foldColumnWidth() const;
    bool isFoldableLine(int blockNumber) const;
    bool isLineFolded(int blockNumber) const;
    void toggleFold(int blockNumber);

signals:
    void zoomRequested(int delta);
    void escapePressed();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    void clearMultiCursors();
    void drawMultiCursors();
    void updateMultiCursorSelections();

    LineNumberArea *m_lineNumberArea;

    // Multi-cursor state
    QVector<QTextCursor> m_extraCursors;
    int m_multiCursorCol = -1;
    bool m_multiCursorMode = false;
    QTimer *m_cursorBlinkTimer;
    bool m_cursorVisible = true;

    // Double-Ctrl detection
    QElapsedTimer m_lastCtrlPress;
    bool m_ctrlWasReleased = false;

    // Code folding
    QSet<int> m_foldedLines;
    int blockIndentSpaces(const QTextBlock &block) const;
    int foldRangeEnd(int startBlock) const;

    struct ImportBlock { int start = -1; int end = -1; };
    ImportBlock findImportBlock() const;
};

class LineNumberArea : public QWidget {
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), m_editor(editor) {}

    QSize sizeHint() const override {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        m_editor->lineNumberAreaPaintEvent(event);
    }
    void mousePressEvent(QMouseEvent *event) override;

private:
    CodeEditor *m_editor;
};

#endif
