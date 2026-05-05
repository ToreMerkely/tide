#include "ConfigHighlighter.h"

ConfigHighlighter::ConfigHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Section headers: [name]
    QTextCharFormat sectionFormat;
    sectionFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    sectionFormat.setFontWeight(QFont::Bold);
    m_rules.append({QRegularExpression("^\\s*\\[[^\\]\\n]+\\]"), sectionFormat, 0});

    // Strings (single, double, triple-double for TOML)
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"\"\"(?:[^\"]|\"(?!\"\"))*\"\"\""), stringFormat, 0});
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), stringFormat, 0});

    // Numbers (incl. hex, octal, binary, floats)
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({
        QRegularExpression("\\b-?\\d+(?:_\\d+)*(?:\\.\\d+(?:_\\d+)*)?(?:[eE][+-]?\\d+)?\\b"),
        numberFormat, 0
    });
    m_rules.append({QRegularExpression("\\b0[xX][0-9a-fA-F_]+\\b"), numberFormat, 0});

    // Booleans / null-likes
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    m_rules.append({
        QRegularExpression("\\b(?:true|false|yes|no|on|off|null|none"
                           "|True|False|Yes|No|On|Off|Null|None"
                           "|TRUE|FALSE|YES|NO|ON|OFF|NULL|NONE)\\b"),
        keywordFormat, 0
    });

    // Keys: identifier at start of (possibly indented) line, before = or :
    QTextCharFormat keyFormat;
    keyFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({
        QRegularExpression("^\\s*([A-Za-z_][\\w.\\-]*)(?=\\s*[:=])"),
        keyFormat, 1
    });

    // Variable interpolation: ${NAME} or $NAME
    QTextCharFormat varFormat;
    varFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({
        QRegularExpression("\\$(?:\\{[^}]*\\}|[A-Za-z_]\\w*)"),
        varFormat, 0
    });

    // Comments — last so they win against earlier rules
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("(?:^|\\s)#[^\\n]*"), commentFormat, 0});
    m_rules.append({QRegularExpression("(?:^|\\s);[^\\n]*"), commentFormat, 0});
}

void ConfigHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            int s = m.capturedStart(rule.captureGroup);
            int len = m.capturedLength(rule.captureGroup);
            if (len > 0)
                setFormat(s, len, rule.format);
        }
    }
}
