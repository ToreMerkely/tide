#include "SearchBar.h"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QKeyEvent>

SearchBar::SearchBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
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

    m_caseBtn = new QPushButton("Aa");
    m_caseBtn->setCheckable(true);
    m_caseBtn->setFixedWidth(32);
    m_caseBtn->setFocusPolicy(Qt::NoFocus);
    m_caseBtn->setToolTip("Match case");
    m_caseBtn->setStyleSheet(
        "QPushButton { border: 1px solid transparent; padding: 2px 4px; }"
        "QPushButton:hover { border: 1px solid #4D5054; }"
        "QPushButton:checked { background-color: #21426D; "
        "                      border: 1px solid #589DF6; color: #FFFFFF; }");

    auto *closeBtn = new QPushButton("\u2715");
    closeBtn->setFixedWidth(28);
    closeBtn->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(m_input);
    layout->addWidget(m_matchLabel);
    layout->addWidget(m_caseBtn);
    layout->addWidget(prevBtn);
    layout->addWidget(nextBtn);
    layout->addWidget(closeBtn);
    layout->addStretch();

    connect(m_input, &QLineEdit::textChanged, this, &SearchBar::onTextChanged);
    connect(m_caseBtn, &QPushButton::toggled, this, [this](bool) {
        onTextChanged(m_input->text());
    });
    connect(prevBtn, &QPushButton::clicked, this, &SearchBar::findPrevious);
    connect(nextBtn, &QPushButton::clicked, this, &SearchBar::findNext);
    connect(closeBtn, &QPushButton::clicked, this, &SearchBar::close);

    hide();
}

void SearchBar::setEditor(QPlainTextEdit *editor)
{
    m_editor = editor;
}

void SearchBar::activate()
{
    show();
    m_input->setFocus();
    m_input->selectAll();
    onTextChanged(m_input->text());
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
    QTextDocument::FindFlags flags;
    if (m_caseBtn && m_caseBtn->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    while (true) {
        cursor = doc->find(text, cursor, flags);
        if (cursor.isNull())
            break;
        m_matches.append({cursor.selectionStart(), cursor.selectionEnd() - cursor.selectionStart()});
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
    if (m_editor) {
        m_editor->setExtraSelections({});
        m_editor->setFocus();
    }
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

    // Re-highlight so current match stands out
    highlightMatches();

    // Override current match with brighter highlight
    QList<QTextEdit::ExtraSelection> sels = m_editor->extraSelections();
    QTextEdit::ExtraSelection current;
    current.format.setBackground(QColor(0x5A, 0x89, 0x5A));
    current.cursor = cursor;
    sels.append(current);
    m_editor->setExtraSelections(sels);
}
