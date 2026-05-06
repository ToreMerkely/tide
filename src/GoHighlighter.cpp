#include "GoHighlighter.h"

GoHighlighter::GoHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_blockCommentStart("/\\*")
    , m_blockCommentEnd("\\*/")
    , m_rawStringFence("`")
{
    // Strings (double-quoted, single line)
    m_stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), m_stringFormat, 0});
    // Rune literals: 'x'
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), m_stringFormat, 0});

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({
        QRegularExpression("\\b-?\\d[\\d_]*(?:\\.\\d[\\d_]*)?(?:[eE][+-]?\\d+)?i?\\b"),
        numberFormat, 0
    });
    m_rules.append({QRegularExpression("\\b0[xX][0-9a-fA-F_]+\\b"), numberFormat, 0});
    m_rules.append({QRegularExpression("\\b0[oO][0-7_]+\\b"), numberFormat, 0});
    m_rules.append({QRegularExpression("\\b0[bB][01_]+\\b"), numberFormat, 0});

    // Keywords (orange bold)
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString kws[] = {
        "func", "var", "const", "type", "struct", "interface", "map", "chan",
        "package", "import", "return", "if", "else", "for", "range", "switch",
        "case", "default", "break", "continue", "defer", "go", "select",
        "fallthrough", "goto"
    };
    for (const QString &kw : kws)
        m_rules.append({QRegularExpression("\\b" + kw + "\\b"), keywordFormat, 0});

    // Built-in types (lavender)
    QTextCharFormat typeFormat;
    typeFormat.setForeground(QColor(0xB3, 0x89, 0xC5));
    const QString types[] = {
        "bool", "byte", "rune", "string", "error", "any",
        "int", "int8", "int16", "int32", "int64",
        "uint", "uint8", "uint16", "uint32", "uint64", "uintptr",
        "float32", "float64", "complex64", "complex128"
    };
    for (const QString &t : types)
        m_rules.append({QRegularExpression("\\b" + t + "\\b"), typeFormat, 0});

    // Built-in constants and functions
    QTextCharFormat builtinFormat;
    builtinFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    const QString builtins[] = {
        "true", "false", "nil", "iota",
        "make", "new", "len", "cap", "append", "copy", "delete",
        "panic", "recover", "print", "println", "close"
    };
    for (const QString &b : builtins)
        m_rules.append({QRegularExpression("\\b" + b + "\\b"), builtinFormat, 0});

    // Function definitions: `func name(`
    QTextCharFormat fnFormat;
    fnFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("(?<=\\bfunc\\s)[A-Za-z_][\\w]*"),
        fnFormat, 0
    });
    // Function calls: `name(`
    m_rules.append({
        QRegularExpression("\\b[A-Za-z_][\\w]*(?=\\s*\\()"),
        fnFormat, 0
    });

    // Single-line comments
    m_commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    m_commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("//[^\\n]*"), m_commentFormat, 0});
}

void GoHighlighter::highlightBlock(const QString &text)
{
    // State: 0 normal, 1 inside /* ... */, 2 inside backtick raw string
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
    } else if (state == 2) {
        auto m = m_rawStringFence.match(text, 0);
        if (m.hasMatch()) {
            int end = m.capturedEnd();
            setFormat(0, end, m_stringFormat);
            state = 0;
            start = end;
        } else {
            setFormat(0, text.length(), m_stringFormat);
            setCurrentBlockState(2);
            return;
        }
    }

    // Apply single-line rules to the post-fence remainder
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

    // Now scan for new fences (block comment OR raw string) on the rest of
    // this line, in order, picking whichever appears first.
    while (start < text.length()) {
        auto cs = m_blockCommentStart.match(text, start);
        auto rs = m_rawStringFence.match(text, start);
        int cPos = cs.hasMatch() ? cs.capturedStart() : text.length();
        int rPos = rs.hasMatch() ? rs.capturedStart() : text.length();
        if (cPos >= text.length() && rPos >= text.length())
            break;

        if (cPos <= rPos) {
            // /* ... */
            auto em = m_blockCommentEnd.match(text, cPos + 2);
            if (em.hasMatch()) {
                int len = em.capturedEnd() - cPos;
                setFormat(cPos, len, m_commentFormat);
                start = cPos + len;
            } else {
                setFormat(cPos, text.length() - cPos, m_commentFormat);
                setCurrentBlockState(1);
                return;
            }
        } else {
            // raw `...`
            auto em = m_rawStringFence.match(text, rPos + 1);
            if (em.hasMatch()) {
                int len = em.capturedEnd() - rPos;
                setFormat(rPos, len, m_stringFormat);
                start = rPos + len;
            } else {
                setFormat(rPos, text.length() - rPos, m_stringFormat);
                setCurrentBlockState(2);
                return;
            }
        }
    }

    setCurrentBlockState(0);
}
