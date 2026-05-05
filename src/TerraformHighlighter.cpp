#include "TerraformHighlighter.h"

TerraformHighlighter::TerraformHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_blockCommentStart("/\\*")
    , m_blockCommentEnd("\\*/")
{
    // Strings and heredocs: simple "..." for now
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({QRegularExpression("\\b-?\\d+(?:\\.\\d+)?\\b"), numberFormat, 0});

    // Block keywords
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    keywordFormat.setFontWeight(QFont::Bold);
    const QString kws[] = {
        "resource", "module", "variable", "output", "provider", "data",
        "locals", "terraform", "backend", "provisioner", "lifecycle",
        "dynamic", "for_each", "count", "depends_on", "true", "false", "null"
    };
    for (const QString &kw : kws)
        m_rules.append({QRegularExpression("\\b" + kw + "\\b"), keywordFormat, 0});

    // Type/identifier prefixes inside expressions: var.x, local.x, module.x, data.x
    QTextCharFormat refFormat;
    refFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({
        QRegularExpression("\\b(?:var|local|module|data|each|count|self|path)\\."),
        refFormat, 0
    });

    // Interpolation start ${ and end } highlighted in lavender
    m_rules.append({QRegularExpression("\\$\\{"), refFormat, 0});

    // Single-line comments (# and //)
    m_commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    m_commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("(?:^|\\s)#[^\\n]*"), m_commentFormat, 0});
    m_rules.append({QRegularExpression("//[^\\n]*"), m_commentFormat, 0});
}

void TerraformHighlighter::highlightBlock(const QString &text)
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
