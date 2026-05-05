#include "HtmlHighlighter.h"

HtmlHighlighter::HtmlHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_commentStart("<!--")
    , m_commentEnd("-->")
{
    // Tag delimiters and tag names: capture the name after </ or <
    QTextCharFormat tagDelimFormat;
    tagDelimFormat.setForeground(QColor(0xA9, 0xB7, 0xC6));
    m_rules.append({QRegularExpression("</?|/?>"), tagDelimFormat, 0});

    QTextCharFormat tagFormat;
    tagFormat.setForeground(QColor(0x56, 0xA8, 0xF5));
    tagFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression("(?<=<|</)[A-Za-z][\\w:-]*"), tagFormat, 0});

    // Attribute names
    QTextCharFormat attrFormat;
    attrFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("\\b([a-zA-Z_:][\\w:.-]*)(?=\\s*=)"),
        attrFormat, 1
    });

    // Attribute values (quoted strings)
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), stringFormat, 0});

    // Entities: &amp; &lt; &#123; etc.
    QTextCharFormat entityFormat;
    entityFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({QRegularExpression("&[#A-Za-z0-9]+;"), entityFormat, 0});

    // DOCTYPE
    QTextCharFormat doctypeFormat;
    doctypeFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    doctypeFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression("<!DOCTYPE\\b[^>]*>",
                                       QRegularExpression::CaseInsensitiveOption),
                    doctypeFormat, 0});

    m_commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    m_commentFormat.setFontItalic(true);
}

void HtmlHighlighter::highlightBlock(const QString &text)
{
    // Multi-line HTML comments: <!-- ... -->
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

    // Apply single-line rules over the post-comment range
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

    // Look for new comment starts on this line
    while (start < text.length()) {
        auto sm = m_commentStart.match(text, start);
        if (!sm.hasMatch())
            break;
        int sPos = sm.capturedStart();
        auto em = m_commentEnd.match(text, sPos + 4);
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
