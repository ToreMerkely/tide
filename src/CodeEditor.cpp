#include "CodeEditor.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QFont>

static const int INDENT_WIDTH = 4;
static const QString INDENT_STR = QString(INDENT_WIDTH, ' ');

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    QFont font("JetBrains Mono", 11);
    font.setStyleHint(QFont::Monospace);
    setFont(font);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * INDENT_WIDTH);
}

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    QTextCursor cursor = textCursor();

    if (event->key() == Qt::Key_Tab && !event->modifiers()) {
        // Insert spaces instead of tab
        cursor.insertText(INDENT_STR);
        return;
    }

    if (event->key() == Qt::Key_Backtab) {
        // Shift+Tab: remove up to 4 leading spaces
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
        // Get current line text
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        // Calculate leading whitespace
        int indent = 0;
        while (indent < line.size() && line[indent] == ' ')
            ++indent;
        QString indentStr = QString(indent, ' ');

        // Extra indent if line ends with {
        QString trimmed = line.trimmed();
        if (trimmed.endsWith('{'))
            indentStr += INDENT_STR;

        // Restore cursor and insert newline + indent
        setTextCursor(textCursor());
        QPlainTextEdit::keyPressEvent(event);
        textCursor().insertText(indentStr);
        return;
    }

    if (event->key() == Qt::Key_BraceRight) {
        // Dedent when typing }
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QString line = cursor.selectedText();

        // Only dedent if line is all whitespace before cursor
        QString beforeCursor = line;
        if (!beforeCursor.trimmed().isEmpty()) {
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        // Remove up to 4 spaces from the end of the whitespace
        int spaces = std::min(INDENT_WIDTH, (int)beforeCursor.size());
        if (spaces > 0) {
            QTextCursor c = textCursor();
            c.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, spaces);
            c.removeSelectedText();
            setTextCursor(c);
        }
        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}
