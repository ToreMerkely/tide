#include "CodeEditor.h"
#include <QKeyEvent>
#include <QFont>
#include <QTextBlock>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>

static const int INDENT_WIDTH = 4;
static const QString INDENT_STR = QString(INDENT_WIDTH, ' ');

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    QFont font("JetBrains Mono", 11);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * INDENT_WIDTH);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_lineNumberArea = new LineNumberArea(this);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);

    updateLineNumberAreaWidth(0);

    m_cursorBlinkTimer = new QTimer(this);
    m_cursorBlinkTimer->setInterval(530);
    connect(m_cursorBlinkTimer, &QTimer::timeout, this, [this]() {
        m_cursorVisible = !m_cursorVisible;
        if (!m_extraCursors.isEmpty())
            viewport()->update();
    });
}

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    digits = qMax(digits, 3); // minimum 3 digits wide
    return 8 + fontMetrics().horizontalAdvance('9') * digits + foldColumnWidth();
}

int CodeEditor::foldColumnWidth() const
{
    return 14;
}

int CodeEditor::blockIndentSpaces(const QTextBlock &block) const
{
    QString text = block.text();
    int spaces = 0;
    for (QChar c : text) {
        if (c == ' ') ++spaces;
        else if (c == '\t') spaces += INDENT_WIDTH;
        else break;
    }
    return spaces;
}

static bool isImportLine(const QString &text)
{
    QString trimmed = text.trimmed();
    return trimmed.startsWith("import ")
        || trimmed.startsWith("from ")
        || trimmed.startsWith("#include");
}

static int parenDelta(const QString &text)
{
    int delta = 0;
    for (QChar c : text) {
        if (c == '(' || c == '[') ++delta;
        else if (c == ')' || c == ']') --delta;
    }
    return delta;
}

CodeEditor::ImportBlock CodeEditor::findImportBlock() const
{
    QTextDocument *doc = document();
    int n = doc->blockCount();
    int start = -1;
    int last = -1;
    int parenDepth = 0;

    for (int i = 0; i < n; ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        QString text = block.text();

        if (parenDepth > 0) {
            parenDepth += parenDelta(text);
            last = i;
            continue;
        }

        if (text.trimmed().isEmpty())
            continue;

        if (isImportLine(text)) {
            if (start == -1) start = i;
            last = i;
            parenDepth += parenDelta(text);
        } else {
            // First non-import line at depth 0 ends the import region
            if (start != -1) break;
            return {-1, -1};
        }
    }
    return {start, last};
}

bool CodeEditor::isFoldableLine(int blockNumber) const
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return false;
    if (block.text().trimmed().isEmpty())
        return false;

    // Import / include region: only the first import line in the block is
    // foldable, and only if the block spans more than one line.
    ImportBlock ib = findImportBlock();
    if (ib.start >= 0 && blockNumber == ib.start)
        return ib.end > ib.start;
    if (ib.start >= 0 && blockNumber > ib.start && blockNumber <= ib.end)
        return false;

    // Indent-based: foldable when next non-blank line is more deeply indented
    int myIndent = blockIndentSpaces(block);
    QTextBlock next = block.next();
    while (next.isValid() && next.text().trimmed().isEmpty())
        next = next.next();
    if (!next.isValid())
        return false;
    return blockIndentSpaces(next) > myIndent;
}

bool CodeEditor::isLineFolded(int blockNumber) const
{
    return m_foldedLines.contains(blockNumber);
}

int CodeEditor::foldRangeEnd(int startBlock) const
{
    QTextBlock block = document()->findBlockByNumber(startBlock);
    if (!block.isValid())
        return startBlock;

    // Import-block fold: cover the entire scanned block including
    // multi-line "from X import ( ... )" continuations
    ImportBlock ib = findImportBlock();
    if (ib.start >= 0 && startBlock == ib.start)
        return ib.end;

    // Indent-based fold
    int baseIndent = blockIndentSpaces(block);
    QTextBlock cur = block.next();
    int last = startBlock;
    while (cur.isValid()) {
        if (cur.text().trimmed().isEmpty()) {
            last = cur.blockNumber();
            cur = cur.next();
            continue;
        }
        if (blockIndentSpaces(cur) <= baseIndent)
            break;
        last = cur.blockNumber();
        cur = cur.next();
    }
    return last;
}

void CodeEditor::toggleFold(int blockNumber)
{
    if (!isFoldableLine(blockNumber) && !m_foldedLines.contains(blockNumber))
        return;

    int endLine = foldRangeEnd(blockNumber);
    bool fold = !m_foldedLines.contains(blockNumber);

    QTextBlock cur = document()->findBlockByNumber(blockNumber + 1);
    while (cur.isValid() && cur.blockNumber() <= endLine) {
        cur.setVisible(!fold);
        cur = cur.next();
    }
    if (fold)
        m_foldedLines.insert(blockNumber);
    else
        m_foldedLines.remove(blockNumber);

    document()->markContentsDirty(0, document()->characterCount());
    viewport()->update();
    m_lineNumberArea->update();
}

void CodeEditor::updateLineNumberAreaWidth(int)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        if (delta == 0)
            delta = event->angleDelta().x();
        if (delta != 0)
            emit zoomRequested(delta > 0 ? 1 : -1);
        event->accept();
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        QScrollBar *hbar = horizontalScrollBar();
        int delta = event->angleDelta().y();
        if (delta == 0)
            delta = event->angleDelta().x();
        hbar->setValue(hbar->value() - delta);
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(0x31, 0x33, 0x35));

    const int foldW = foldColumnWidth();
    const int areaW = m_lineNumberArea->width();
    const int numbersRight = areaW - foldW;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(0x60, 0x63, 0x66));
            painter.drawText(0, top, numbersRight - 4,
                             fontMetrics().height(), Qt::AlignRight, number);

            bool folded = m_foldedLines.contains(blockNumber);
            if (folded || isFoldableLine(blockNumber)) {
                painter.setPen(QColor(0x88, 0x8C, 0x90));
                QString marker = folded ? QStringLiteral("▸")
                                        : QStringLiteral("▾");
                painter.drawText(numbersRight, top, foldW,
                                 fontMetrics().height(), Qt::AlignCenter, marker);
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void LineNumberArea::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    int foldStart = m_editor->lineNumberAreaWidth() - m_editor->foldColumnWidth();
    if (event->pos().x() < foldStart) {
        QWidget::mousePressEvent(event);
        return;
    }
    QPoint p(0, event->pos().y());
    QTextCursor cursor = m_editor->cursorForPosition(p);
    int blockNumber = cursor.blockNumber();
    if (m_editor->isFoldableLine(blockNumber) || m_editor->isLineFolded(blockNumber))
        m_editor->toggleFold(blockNumber);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    // Double-Ctrl detection
    if (event->key() == Qt::Key_Control) {
        if (m_ctrlWasReleased && m_lastCtrlPress.isValid() && m_lastCtrlPress.elapsed() < 400) {
            // Double-Ctrl detected — enter multi-cursor mode
            m_multiCursorMode = true;
            QTextCursor cursor = textCursor();
            m_multiCursorCol = cursor.columnNumber();
            m_ctrlWasReleased = false;
            m_cursorBlinkTimer->start();
            m_cursorVisible = true;
            setCursorWidth(0); // Hide native cursor
            return;
        }
        m_lastCtrlPress.start();
        m_ctrlWasReleased = false;
        return;
    }

    // Multi-cursor mode — Ctrl+Up/Down adds cursors
    if (m_multiCursorMode && (event->modifiers() & Qt::ControlModifier)) {
        if (event->key() == Qt::Key_Down) {
            // Add cursor on line below the last extra cursor (or main cursor)
            int lastLine;
            if (m_extraCursors.isEmpty())
                lastLine = textCursor().blockNumber();
            else
                lastLine = m_extraCursors.last().blockNumber();

            QTextBlock block = document()->findBlockByNumber(lastLine + 1);
            if (block.isValid()) {
                QTextCursor cur(block);
                int col = std::min(m_multiCursorCol, (int)block.text().length());
                cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
                m_extraCursors.append(cur);
                viewport()->update();
            }
            return;
        }
        if (event->key() == Qt::Key_Up) {
            // Add cursor on line above the first extra cursor (or main cursor)
            int firstLine;
            if (m_extraCursors.isEmpty())
                firstLine = textCursor().blockNumber();
            else
                firstLine = m_extraCursors.first().blockNumber();

            QTextBlock block = document()->findBlockByNumber(firstLine - 1);
            if (block.isValid()) {
                QTextCursor cur(block);
                int col = std::min(m_multiCursorCol, (int)block.text().length());
                cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
                m_extraCursors.prepend(cur);
                viewport()->update();
            }
            return;
        }
        if (event->key() == Qt::Key_Right) {
            QTextCursor main = textCursor();
            main.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            setTextCursor(main);
            for (QTextCursor &cur : m_extraCursors)
                cur.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            updateMultiCursorSelections();
            return;
        }
        if (event->key() == Qt::Key_Left) {
            QTextCursor main = textCursor();
            main.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
            setTextCursor(main);
            for (QTextCursor &cur : m_extraCursors)
                cur.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
            updateMultiCursorSelections();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            clearMultiCursors();
            return;
        }
    }

    // If we have extra cursors and user types text
    if (!m_extraCursors.isEmpty()) {
        if (event->key() == Qt::Key_Escape) {
            clearMultiCursors();
            return;
        }

        // Delete/Backspace at all cursors
        if (event->key() == Qt::Key_Delete) {
            QTextCursor main = textCursor();
            main.beginEditBlock();
            // Process in reverse order to preserve positions
            for (int i = m_extraCursors.size() - 1; i >= 0; --i) {
                QTextCursor &cur = m_extraCursors[i];
                cur.deleteChar();
            }
            main.deleteChar();
            main.endEditBlock();
            setTextCursor(main);
            viewport()->update();
            return;
        }

        if (event->key() == Qt::Key_Backspace) {
            QTextCursor main = textCursor();
            main.beginEditBlock();
            for (int i = m_extraCursors.size() - 1; i >= 0; --i) {
                QTextCursor &cur = m_extraCursors[i];
                cur.deletePreviousChar();
            }
            main.deletePreviousChar();
            main.endEditBlock();
            setTextCursor(main);
            viewport()->update();
            return;
        }

        // Regular text input at all cursors
        if (!event->text().isEmpty() && event->key() != Qt::Key_Return &&
            event->key() != Qt::Key_Enter &&
            !(event->modifiers() & Qt::ControlModifier)) {
            QString ch = event->text();
            QTextCursor main = textCursor();
            main.beginEditBlock();
            // Insert in reverse to preserve positions
            for (int i = m_extraCursors.size() - 1; i >= 0; --i) {
                QTextCursor &cur = m_extraCursors[i];
                cur.insertText(ch);
            }
            main.insertText(ch);
            main.endEditBlock();
            setTextCursor(main);
            viewport()->update();
            return;
        }

        // Tab at all cursors
        if (event->key() == Qt::Key_Tab && !event->modifiers()) {
            QTextCursor main = textCursor();
            main.beginEditBlock();
            for (int i = m_extraCursors.size() - 1; i >= 0; --i) {
                QTextCursor &cur = m_extraCursors[i];
                cur.insertText(INDENT_STR);
            }
            main.insertText(INDENT_STR);
            main.endEditBlock();
            setTextCursor(main);
            viewport()->update();
            return;
        }

        // Enter clears multi-cursor
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            clearMultiCursors();
            // Fall through to normal Enter handling
        }
    }

    // Normal key handling below
    QTextCursor cursor = textCursor();

    if (event->key() == Qt::Key_Tab && !event->modifiers()) {
        cursor.insertText(INDENT_STR);
        return;
    }

    if (event->key() == Qt::Key_Backtab) {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();
        int spaces = 0;
        while (spaces < INDENT_WIDTH && spaces < line.size() && line[spaces] == ' ')
            ++spaces;
        if (spaces > 0) {
            cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, spaces);
            cursor.removeSelectedText();
        }
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        int indent = 0;
        while (indent < line.size() && line[indent] == ' ')
            ++indent;
        QString indentStr = QString(indent, ' ');

        QString trimmed = line.trimmed();
        if (trimmed.endsWith('{'))
            indentStr += INDENT_STR;
        if (trimmed.endsWith(':'))
            indentStr += INDENT_STR;

        setTextCursor(textCursor());
        QPlainTextEdit::keyPressEvent(event);
        textCursor().insertText(indentStr);
        return;
    }

    if (event->key() == Qt::Key_BraceRight) {
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        if (!line.trimmed().isEmpty()) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        int spaces = std::min(INDENT_WIDTH, (int)line.size());
        if (spaces > 0) {
            QTextCursor c = textCursor();
            c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, spaces);
            c.removeSelectedText();
            setTextCursor(c);
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        emit escapePressed();
        // Fall through so QPlainTextEdit's default handling still runs.
    }
    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        if (m_multiCursorMode) {
            // Exit the "adding cursors" mode but keep the cursors
            m_multiCursorMode = false;
        }
        m_ctrlWasReleased = true;
    }
    QPlainTextEdit::keyReleaseEvent(event);
}

static int leadingIndentSpaces(const QString &line)
{
    int spaces = 0;
    for (QChar c : line) {
        if (c == ' ') ++spaces;
        else if (c == '\t') spaces += INDENT_WIDTH;
        else break;
    }
    return spaces;
}

void CodeEditor::paintEvent(QPaintEvent *event)
{
    QPlainTextEdit::paintEvent(event);

    // Indent guides
    {
        QPainter painter(viewport());
        painter.setPen(QPen(QColor(0x30, 0x33, 0x38), 1));
        const int charWidth = fontMetrics().horizontalAdvance(' ');
        const int xOff = qRound(contentOffset().x());

        // Avoid overpainting the text cursor at indent boundaries
        const int cursorBlockNum = textCursor().blockNumber();
        const QRect cRect = cursorRect();
        const int cursorX = cRect.x();

        QTextBlock block = firstVisibleBlock();
        while (block.isValid()) {
            QRectF g = blockBoundingGeometry(block).translated(contentOffset());
            int top = qRound(g.top());
            int bottom = qRound(g.bottom());
            if (top > event->rect().bottom())
                break;
            if (bottom >= event->rect().top() && block.isVisible()) {
                QString text = block.text();
                int leading;
                if (text.trimmed().isEmpty()) {
                    leading = 0;
                    QTextBlock next = block.next();
                    while (next.isValid() && next.text().trimmed().isEmpty())
                        next = next.next();
                    if (next.isValid())
                        leading = leadingIndentSpaces(next.text());
                } else {
                    leading = leadingIndentSpaces(text);
                }
                int levels = leading / INDENT_WIDTH;
                bool isCursorRow = (block.blockNumber() == cursorBlockNum);
                for (int level = 0; level < levels; ++level) {
                    int x = level * INDENT_WIDTH * charWidth + xOff;
                    if (isCursorRow && qAbs(x - cursorX) <= 1)
                        continue;
                    painter.drawLine(x, top, x, bottom);
                }
            }
            block = block.next();
        }
    }

    drawMultiCursors();
}

void CodeEditor::drawMultiCursors()
{
    if (m_extraCursors.isEmpty() || !m_cursorVisible)
        return;

    QPainter painter(viewport());
    painter.setPen(QPen(QColor(0xA9, 0xB7, 0xC6), 1));

    // Draw the main cursor too
    QVector<QTextCursor> allCursors = m_extraCursors;
    allCursors.prepend(textCursor());

    for (const QTextCursor &cur : allCursors) {
        QTextBlock block = cur.block();
        if (!block.isValid())
            continue;

        QRectF blockRect = blockBoundingGeometry(block).translated(contentOffset());
        int col = cur.positionInBlock();
        qreal x = blockRect.left() + fontMetrics().horizontalAdvance(block.text().left(col));
        qreal y = blockRect.top();
        qreal h = blockRect.height();

        painter.drawLine(QPointF(x, y), QPointF(x, y + h));
    }
}

void CodeEditor::updateMultiCursorSelections()
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat fmt;
    fmt.setBackground(QColor(0x21, 0x42, 0x83));

    // Add selections for extra cursors that have selections
    for (const QTextCursor &cur : m_extraCursors) {
        if (cur.hasSelection()) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = cur;
            sel.format = fmt;
            selections.append(sel);
        }
    }

    setExtraSelections(selections);
    viewport()->update();
}

void CodeEditor::clearMultiCursors()
{
    m_extraCursors.clear();
    m_multiCursorMode = false;
    m_multiCursorCol = -1;
    m_cursorBlinkTimer->stop();
    m_cursorVisible = true;
    setCursorWidth(1); // Restore native cursor
    setExtraSelections({});
    viewport()->update();
}
