#include "SearchBar.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTimer>

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *findRow = new QWidget;
    auto *layout = new QHBoxLayout(findRow);
    layout->setContentsMargins(4, 2, 4, 2);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Search...");
    m_input->setMinimumWidth(200);
    m_input->installEventFilter(this);

    m_matchLabel = new QLabel;
    m_matchLabel->setMinimumWidth(60);

    auto *prevBtn = new QPushButton("\u25B2");
    prevBtn->setFixedWidth(28);
    prevBtn->setFocusPolicy(Qt::NoFocus);

    auto *nextBtn = new QPushButton("\u25BC");
    nextBtn->setFixedWidth(28);
    nextBtn->setFocusPolicy(Qt::NoFocus);

    const QString toggleStyle =
        "QPushButton { border: 1px solid transparent; padding: 2px 4px; }"
        "QPushButton:hover { border: 1px solid #4D5054; }"
        "QPushButton:checked { background-color: #21426D; "
        "                      border: 1px solid #589DF6; color: #FFFFFF; }";

    m_caseBtn = new QPushButton("Aa");
    m_caseBtn->setCheckable(true);
    m_caseBtn->setFixedWidth(32);
    m_caseBtn->setFocusPolicy(Qt::NoFocus);
    m_caseBtn->setToolTip("Match case");
    m_caseBtn->setStyleSheet(toggleStyle);

    m_regexBtn = new QPushButton(".*");
    m_regexBtn->setCheckable(true);
    m_regexBtn->setFixedWidth(32);
    m_regexBtn->setFocusPolicy(Qt::NoFocus);
    m_regexBtn->setToolTip("Regular expression");
    m_regexBtn->setStyleSheet(toggleStyle);

    auto *closeBtn = new QPushButton("\u2715");
    closeBtn->setFixedWidth(28);
    closeBtn->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(m_input);
    layout->addWidget(m_matchLabel);
    layout->addWidget(m_caseBtn);
    layout->addWidget(m_regexBtn);
    layout->addWidget(prevBtn);
    layout->addWidget(nextBtn);
    layout->addWidget(closeBtn);
    layout->addStretch();

    m_replaceRow = new QWidget;
    auto *rlayout = new QHBoxLayout(m_replaceRow);
    rlayout->setContentsMargins(4, 0, 4, 2);

    m_replaceInput = new QLineEdit;
    m_replaceInput->setPlaceholderText("Replace...");
    m_replaceInput->setMinimumWidth(200);
    m_replaceInput->installEventFilter(this);

    auto *replaceBtn = new QPushButton("Replace");
    replaceBtn->setFocusPolicy(Qt::NoFocus);
    replaceBtn->setToolTip("Replace current match (Enter)");

    auto *replaceAllBtn = new QPushButton("Replace All");
    replaceAllBtn->setFocusPolicy(Qt::NoFocus);
    replaceAllBtn->setToolTip("Replace all matches (Shift+Enter)");

    rlayout->addWidget(m_replaceInput);
    rlayout->addWidget(replaceBtn);
    rlayout->addWidget(replaceAllBtn);
    rlayout->addStretch();

    m_replaceRow->hide();

    outer->addWidget(findRow);
    outer->addWidget(m_replaceRow);

    connect(m_input, &QLineEdit::textChanged, this, &SearchBar::onTextChanged);
    auto rerun = [this](bool) { onTextChanged(m_input->text()); };
    connect(m_caseBtn, &QPushButton::toggled, this, rerun);
    connect(m_regexBtn, &QPushButton::toggled, this, rerun);
    connect(prevBtn, &QPushButton::clicked, this, &SearchBar::findPrevious);
    connect(nextBtn, &QPushButton::clicked, this, &SearchBar::findNext);
    connect(closeBtn, &QPushButton::clicked, this, &SearchBar::close);
    connect(replaceBtn, &QPushButton::clicked, this, &SearchBar::replaceCurrent);
    connect(replaceAllBtn, &QPushButton::clicked, this, &SearchBar::replaceAll);

    hide();
}

void SearchBar::setEditor(QPlainTextEdit *editor)
{
    m_editor = editor;
}

void SearchBar::activate(bool withReplace)
{
    show();
    if (m_editor) {
        QString selection = m_editor->textCursor().selectedText();
        if (!selection.isEmpty() && !selection.contains(QChar::ParagraphSeparator))
            m_input->setText(selection);
    }
    if (withReplace) {
        setReplaceVisible(true);
        if (!m_input->text().isEmpty() && m_matches.isEmpty())
            onTextChanged(m_input->text());
        if (m_input->text().isEmpty()) {
            m_input->setFocus();
            m_input->selectAll();
        } else {
            m_replaceInput->setFocus();
            m_replaceInput->selectAll();
            onTextChanged(m_input->text());
        }
    } else {
        m_input->setFocus();
        m_input->selectAll();
        onTextChanged(m_input->text());
    }
}

bool SearchBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            close();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                findPrevious();
            else
                findNext();
            return true;
        }
    }
    if (obj == m_replaceInput && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            close();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                replaceAll();
            else
                replaceCurrent();
            return true;
        }
    }
    if ((obj == m_input || obj == m_replaceInput)
        && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        QTimer::singleShot(0, this, [this]() { highlightMatches(); });
    }
    return QWidget::eventFilter(obj, event);
}

void SearchBar::onTextChanged(const QString &text)
{
    m_matches.clear();
    m_currentMatch = -1;

    if (!m_editor || text.isEmpty()) {
        m_matchLabel->setText("");
        if (m_editor)
            m_editor->setExtraSelections({});
        return;
    }

    // Find all matches
    QTextDocument *doc = m_editor->document();
    QTextCursor cursor(doc);
    const bool caseSensitive = m_caseBtn && m_caseBtn->isChecked();
    const bool regex = m_regexBtn && m_regexBtn->isChecked();

    if (regex) {
        QRegularExpression::PatternOptions opts;
        if (!caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(text, opts);
        if (!re.isValid()) {
            m_matchLabel->setText("(bad regex)");
            m_editor->setExtraSelections({});
            return;
        }
        while (true) {
            cursor = doc->find(re, cursor);
            if (cursor.isNull())
                break;
            m_matches.append({cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart()});
        }
    } else {
        QTextDocument::FindFlags flags;
        if (caseSensitive)
            flags |= QTextDocument::FindCaseSensitively;
        while (true) {
            cursor = doc->find(text, cursor, flags);
            if (cursor.isNull())
                break;
            m_matches.append({cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart()});
        }
    }

    highlightMatches();

    if (!m_matches.isEmpty()) {
        // Find the match closest to current cursor position
        int cursorPos = m_editor->textCursor().position();
        m_currentMatch = 0;
        for (int i = 0; i < m_matches.size(); ++i) {
            if (m_matches[i].start >= cursorPos) {
                m_currentMatch = i;
                break;
            }
        }
        goToMatch(m_currentMatch);
    } else {
        m_matchLabel->setText("0 of 0");
    }
}

void SearchBar::findNext()
{
    if (m_matches.isEmpty())
        return;
    m_currentMatch = (m_currentMatch + 1) % m_matches.size();
    goToMatch(m_currentMatch);
}

void SearchBar::findPrevious()
{
    if (m_matches.isEmpty())
        return;
    m_currentMatch = (m_currentMatch - 1 + m_matches.size()) % m_matches.size();
    goToMatch(m_currentMatch);
}

void SearchBar::close()
{
    hide();
    setReplaceVisible(false);
    if (m_editor) {
        m_editor->setExtraSelections({});
        m_editor->setFocus();
    }
}

void SearchBar::setReplaceVisible(bool visible)
{
    if (m_replaceRow)
        m_replaceRow->setVisible(visible);
}

void SearchBar::replaceCurrent()
{
    if (!m_editor || m_matches.isEmpty() || m_currentMatch < 0)
        return;

    const Match m = m_matches[m_currentMatch];
    const QString replacement = m_replaceInput->text();
    const bool regex = m_regexBtn && m_regexBtn->isChecked();
    const bool caseSensitive = m_caseBtn && m_caseBtn->isChecked();

    QTextCursor cursor(m_editor->document());
    cursor.setPosition(m.start);
    cursor.setPosition(m.start + m.length, QTextCursor::KeepAnchor);

    QString output = replacement;
    if (regex) {
        QRegularExpression::PatternOptions opts;
        if (!caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression re(m_input->text(), opts);
        if (re.isValid()) {
            QString matched = cursor.selectedText();
            output = matched;
            output.replace(re, replacement);
        }
    }

    cursor.insertText(output);

    int nextIdx = m_currentMatch;
    onTextChanged(m_input->text());
    if (!m_matches.isEmpty()) {
        if (nextIdx >= m_matches.size())
            nextIdx = 0;
        m_currentMatch = nextIdx;
        goToMatch(m_currentMatch);
    }
    m_replaceInput->setFocus();
}

void SearchBar::replaceAll()
{
    if (!m_editor || m_matches.isEmpty())
        return;

    const QString replacement = m_replaceInput->text();
    const bool regex = m_regexBtn && m_regexBtn->isChecked();
    const bool caseSensitive = m_caseBtn && m_caseBtn->isChecked();

    QRegularExpression re;
    if (regex) {
        QRegularExpression::PatternOptions opts;
        if (!caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        re = QRegularExpression(m_input->text(), opts);
        if (!re.isValid())
            return;
    }

    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();
    for (int i = m_matches.size() - 1; i >= 0; --i) {
        const Match &m = m_matches[i];
        cursor.setPosition(m.start);
        cursor.setPosition(m.start + m.length, QTextCursor::KeepAnchor);
        QString output = replacement;
        if (regex) {
            QString matched = cursor.selectedText();
            output = matched;
            output.replace(re, replacement);
        }
        cursor.insertText(output);
    }
    cursor.endEditBlock();

    onTextChanged(m_input->text());
    m_replaceInput->setFocus();
}

void SearchBar::highlightMatches()
{
    if (!m_editor)
        return;

    QList<QTextEdit::ExtraSelection> selections;
    QTextCharFormat format;
    format.setBackground(QColor(0x32, 0x59, 0x3D));

    for (const Match &m : m_matches) {
        QTextEdit::ExtraSelection sel;
        sel.format = format;
        QTextCursor cursor(m_editor->document());
        cursor.setPosition(m.start);
        cursor.setPosition(m.start + m.length, QTextCursor::KeepAnchor);
        sel.cursor = cursor;
        selections.append(sel);
    }

    const bool barHasFocus = (m_input && m_input->hasFocus())
        || (m_replaceInput && m_replaceInput->hasFocus());
    if (barHasFocus && m_currentMatch >= 0 && m_currentMatch < m_matches.size()) {
        const Match &m = m_matches[m_currentMatch];
        QTextEdit::ExtraSelection current;
        current.format.setBackground(QColor(0x5A, 0x89, 0x5A));
        QTextCursor cursor(m_editor->document());
        cursor.setPosition(m.start);
        cursor.setPosition(m.start + m.length, QTextCursor::KeepAnchor);
        current.cursor = cursor;
        selections.append(current);
    }

    m_editor->setExtraSelections(selections);
}

void SearchBar::goToMatch(int index)
{
    if (index < 0 || index >= m_matches.size())
        return;

    const Match &m = m_matches[index];
    QTextCursor cursor(m_editor->document());
    cursor.setPosition(m.start);
    cursor.setPosition(m.start + m.length, QTextCursor::KeepAnchor);
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();

    m_matchLabel->setText(QString("%1 of %2").arg(index + 1).arg(m_matches.size()));

    highlightMatches();
}
