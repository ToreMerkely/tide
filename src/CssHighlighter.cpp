#include "CssHighlighter.h"

CssHighlighter::CssHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_commentStart("/\\*")
    , m_commentEnd("\\*/")
{
    // Strings
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), stringFormat, 0});

    // Numbers and units (10px, 1.5em, 100%, etc.)
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({
        QRegularExpression("\\b-?\\d+(?:\\.\\d+)?(?:px|em|rem|%|vh|vw|pt|cm|mm|in|s|ms|deg|rad|turn|fr|ch|ex)?\\b"),
        numberFormat, 0
    });

    // Hex colors: #abc, #aabbcc, #aabbccdd
    m_rules.append({
        QRegularExpression("#[0-9a-fA-F]{3,8}\\b"),
        numberFormat, 0
    });

    // Properties (identifier followed by `:` inside a block)
    QTextCharFormat propertyFormat;
    propertyFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("\\b([-a-zA-Z][\\w-]*)(?=\\s*:)"),
        propertyFormat, 1
    });

    // At-rules: @media, @import, @keyframes etc.
    QTextCharFormat atFormat;
    atFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    atFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression("@[A-Za-z-]+"), atFormat, 0});

    // Pseudo-classes / pseudo-elements
    QTextCharFormat pseudoFormat;
    pseudoFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({
        QRegularExpression(":{1,2}[a-zA-Z-]+(?:\\([^)]*\\))?"),
        pseudoFormat, 0
    });

    // Important
    QTextCharFormat importantFormat;
    importantFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    importantFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression("!important\\b"), importantFormat, 0});

    // SCSS / Sass extras: $variables and // line comments
    QTextCharFormat varFormat;
    varFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({QRegularExpression("\\$[A-Za-z_][\\w-]*"), varFormat, 0});

    QTextCharFormat lineCommentFormat;
    lineCommentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    lineCommentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("//[^\\n]*"), lineCommentFormat, 0});

    m_commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    m_commentFormat.setFontItalic(true);
}

void CssHighlighter::highlightBlock(const QString &text)
{
    int prev = previousBlockState();
    if (prev < 0) prev = 0;
    int state = prev;
    int start = 0;

    if (state == 1) {
        auto m = m_commentEnd.match(text, 0);
        if (m.hasMatch()) {
            int end = m.capturedEnd();
            setFormat(0, end, m_commentFormat);
            state = 0;
            start = end;
        } else {
            setFormat(0, text.length(), m_commentFormat);
            setCurrentBlockState(1);
            return;
        }
    }

    QString rest = text.mid(start);
    for (const Rule &rule : m_rules) {
        auto it = rule.pattern.globalMatch(rest);
        while (it.hasNext()) {
            auto m = it.next();
            int s = m.capturedStart(rule.captureGroup);
            int len = m.capturedLength(rule.captureGroup);
            if (len > 0)
                setFormat(start + s, len, rule.format);
        }
    }

    while (start < text.length()) {
        auto sm = m_commentStart.match(text, start);
        if (!sm.hasMatch())
            break;
        int sPos = sm.capturedStart();
        auto em = m_commentEnd.match(text, sPos + 2);
        if (em.hasMatch()) {
            int len = em.capturedEnd() - sPos;
            setFormat(sPos, len, m_commentFormat);
            start = sPos + len;
        } else {
            setFormat(sPos, text.length() - sPos, m_commentFormat);
            setCurrentBlockState(1);
            return;
        }
    }

    setCurrentBlockState(0);
}
