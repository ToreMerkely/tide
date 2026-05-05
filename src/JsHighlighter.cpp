#include "JsHighlighter.h"

JsHighlighter::JsHighlighter(QTextDocument *parent, Flavor flavor)
    : QSyntaxHighlighter(parent)
    , m_blockCommentStart("/\\*")
    , m_blockCommentEnd("\\*/")
{
    // Strings (single, double, backtick template). Backtick template strings
    // can span multiple lines but we keep it simple for v1.
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), stringFormat, 0});
    m_rules.append({QRegularExpression("`(?:[^`\\\\]|\\\\.)*`"), stringFormat, 0});

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({
        QRegularExpression("\\b-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"),
        numberFormat, 0
    });
    m_rules.append({QRegularExpression("\\b0[xX][0-9a-fA-F]+\\b"), numberFormat, 0});

    // Keywords
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString kws[] = {
        "var", "let", "const", "function", "return", "if", "else", "for",
        "while", "do", "switch", "case", "break", "continue", "default",
        "throw", "try", "catch", "finally", "new", "delete", "typeof",
        "instanceof", "in", "of", "this", "super", "class", "extends",
        "import", "export", "from", "as", "async", "await", "yield",
        "true", "false", "null", "undefined", "void", "static", "get", "set"
    };
    for (const QString &kw : kws)
        m_rules.append({QRegularExpression("\\b" + kw + "\\b"), keywordFormat, 0});

    if (flavor == TypeScript) {
        const QString tsKws[] = {
            "interface", "type", "enum", "implements", "abstract", "readonly",
            "public", "private", "protected", "namespace", "declare", "keyof",
            "infer", "is", "any", "unknown", "never", "number", "string",
            "boolean", "object", "bigint", "symbol"
        };
        for (const QString &kw : tsKws)
            m_rules.append({QRegularExpression("\\b" + kw + "\\b"), keywordFormat, 0});

        // Type annotations after `:` — color the identifier lavender
        QTextCharFormat typeFormat;
        typeFormat.setForeground(QColor(0xB3, 0x89, 0xC5));
        m_rules.append({
            QRegularExpression("(?<=:\\s)[A-Z][A-Za-z0-9_<>\\[\\],\\s|]*"),
            typeFormat, 0
        });
    }

    // Function definitions: `function foo(` or `foo = function(`
    QTextCharFormat fnFormat;
    fnFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("(?<=\\bfunction\\s)[A-Za-z_$][\\w$]*"),
        fnFormat, 0
    });
    // Function calls: name(
    m_rules.append({
        QRegularExpression("\\b[A-Za-z_$][\\w$]*(?=\\s*\\()"),
        fnFormat, 0
    });

    // Property access: .name (lavender)
    QTextCharFormat propFormat;
    propFormat.setForeground(QColor(0xA9, 0xB7, 0xC6));
    m_rules.append({
        QRegularExpression("(?<=\\.)[A-Za-z_$][\\w$]*"),
        propFormat, 0
    });

    // Single-line comments (// ...)
    m_commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    m_commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("//[^\\n]*"), m_commentFormat, 0});
}

void JsHighlighter::highlightBlock(const QString &text)
{
    int prev = previousBlockState();
    if (prev < 0) prev = 0;
    int state = prev;
    int start = 0;

    if (state == 1) {
        auto m = m_blockCommentEnd.match(text, 0);
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
        auto sm = m_blockCommentStart.match(text, start);
        if (!sm.hasMatch())
            break;
        int sPos = sm.capturedStart();
        auto em = m_blockCommentEnd.match(text, sPos + 2);
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
