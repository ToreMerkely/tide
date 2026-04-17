#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QElapsedTimer>
#include <QVector>
#include <QTextCursor>
#include <QTimer>

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void clearMultiCursors();
    void drawMultiCursors();
    void updateMultiCursorSelections();

    // Multi-cursor state
    QVector<QTextCursor> m_extraCursors;
    int m_multiCursorCol = -1;
    bool m_multiCursorMode = false;
    QTimer *m_cursorBlinkTimer;
    bool m_cursorVisible = true;

    // Double-Ctrl detection
    QElapsedTimer m_lastCtrlPress;
    bool m_ctrlWasReleased = false;
};

#endif
