#include "GherkinHighlighter.h"

GherkinHighlighter::GherkinHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_docStringFence("\"\"\"|'''")
{
    // Step / structure keywords at start of line
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    m_rules.append({
        QRegularExpression(
            "^\\s*(Feature|Background|Scenario Outline|Scenario Template"
            "|Scenario|Examples|Scenarios|Rule"
            "|Given|When|Then|And|But|\\*)\\b:?"),
        keywordFormat, 1
    });

    // Tags
    QTextCharFormat tagFormat;
    tagFormat.setForeground(QColor(0xFF, 0xC6, 0x6D));
    m_rules.append({QRegularExpression("@[A-Za-z_][\\w.\\-]*"), tagFormat, 0});

    // Quoted strings inside steps
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});

    // <parameter> placeholders inside scenario outlines
    QTextCharFormat paramFormat;
    paramFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({QRegularExpression("<[^>\\n]+>"), paramFormat, 0});

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({QRegularExpression("\\b-?\\d+(?:\\.\\d+)?\\b"), numberFormat, 0});

    // Table pipe markers
    QTextCharFormat pipeFormat;
    pipeFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({QRegularExpression("\\|"), pipeFormat, 0});

    // Comments — last so they win
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("^\\s*#[^\\n]*"), commentFormat, 0});

    m_docStringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
}

void GherkinHighlighter::highlightBlock(const QString &text)
{
    int prev = previousBlockState();
    if (prev < 0) prev = 0;
    int state = prev;

    if (state == 1) {
        setFormat(0, text.length(), m_docStringFormat);
        if (text.trimmed() == "\"\"\"" || text.trimmed() == "'''")
            state = 0;
        setCurrentBlockState(state);
        return;
    }

    if (text.trimmed() == "\"\"\"" || text.trimmed() == "'''") {
        setFormat(0, text.length(), m_docStringFormat);
        setCurrentBlockState(1);
        return;
    }

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

    setCurrentBlockState(0);
}
