#include "RegoHighlighter.h"

RegoHighlighter::RegoHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_rawStringFence("`")
{
    // Later rules override earlier ones, so strings and comments come last.

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({
        QRegularExpression("\\b-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"),
        numberFormat, 0
    });

    // Rule heads start in column 0: `allow if`, `deny contains msg if`,
    // `default allow := false`.
    QTextCharFormat fnFormat;
    fnFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("^(?:default\\s+)?([A-Za-z_]\\w*)"),
        fnFormat, 1
    });
    // Function calls, including dotted builtins: `count(`, `object.get(`
    m_rules.append({
        QRegularExpression("\\b[A-Za-z_][\\w.]*(?=\\()"),
        fnFormat, 0
    });

    // Keywords (orange bold)
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString kws[] = {
        "package", "import", "default", "else", "not", "some", "every",
        "with", "as", "in", "if", "contains"
    };
    for (const QString &kw : kws)
        m_rules.append({QRegularExpression("\\b" + kw + "\\b"), keywordFormat, 0});

    // Constants and the root documents
    QTextCharFormat builtinFormat;
    builtinFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    const QString builtins[] = {"true", "false", "null", "input", "data"};
    for (const QString &b : builtins)
        m_rules.append({QRegularExpression("\\b" + b + "\\b"), builtinFormat, 0});

    // Strings (double-quoted, single line)
    m_stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), m_stringFormat, 0});

    // Comments
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("#[^\\n]*"), commentFormat, 0});
}

void RegoHighlighter::highlightBlock(const QString &text)
{
    // State: 0 normal, 1 inside a backtick raw string
    int start = 0;

    if (previousBlockState() == 1) {
        auto m = m_rawStringFence.match(text, 0);
        if (!m.hasMatch()) {
            setFormat(0, text.length(), m_stringFormat);
            setCurrentBlockState(1);
            return;
        }
        start = m.capturedEnd();
        setFormat(0, start, m_stringFormat);
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

    // Raw strings may span lines; scan for them after the single-line rules.
    while (start < text.length()) {
        auto open = m_rawStringFence.match(text, start);
        if (!open.hasMatch())
            break;
        int pos = open.capturedStart();
        auto close = m_rawStringFence.match(text, pos + 1);
        if (!close.hasMatch()) {
            setFormat(pos, text.length() - pos, m_stringFormat);
            setCurrentBlockState(1);
            return;
        }
        setFormat(pos, close.capturedEnd() - pos, m_stringFormat);
        start = close.capturedEnd();
    }

    setCurrentBlockState(0);
}
