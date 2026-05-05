#include "DockerfileHighlighter.h"

DockerfileHighlighter::DockerfileHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Strings
    QTextCharFormat stringFormat;
    stringFormat.setForeground(QColor(0x6A, 0x87, 0x59));
    m_rules.append({QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\""), stringFormat, 0});
    m_rules.append({QRegularExpression("'(?:[^'\\\\]|\\\\.)*'"), stringFormat, 0});

    // Numbers
    QTextCharFormat numberFormat;
    numberFormat.setForeground(QColor(0x68, 0x97, 0xBB));
    m_rules.append({QRegularExpression("\\b\\d+(?:\\.\\d+)?\\b"), numberFormat, 0});

    // Variables: $NAME, ${NAME}
    QTextCharFormat varFormat;
    varFormat.setForeground(QColor(0x98, 0x76, 0xAA));
    m_rules.append({
        QRegularExpression("\\$(?:\\{[^}]*\\}|[A-Za-z_]\\w*)"),
        varFormat, 0
    });

    // Instructions at the start of (possibly indented) line
    QTextCharFormat instructionFormat;
    instructionFormat.setForeground(QColor(0xCC, 0x78, 0x32));
    instructionFormat.setFontWeight(QFont::Bold);
    const QString instructions[] = {
        "FROM", "RUN", "CMD", "LABEL", "MAINTAINER", "EXPOSE", "ENV",
        "ADD", "COPY", "ENTRYPOINT", "VOLUME", "USER", "WORKDIR", "ARG",
        "ONBUILD", "STOPSIGNAL", "HEALTHCHECK", "SHELL"
    };
    for (const QString &kw : instructions) {
        m_rules.append({
            QRegularExpression("^\\s*" + kw + "\\b",
                               QRegularExpression::CaseInsensitiveOption),
            instructionFormat, 0
        });
    }

    // Comments (gray italic) — last so they win
    QTextCharFormat commentFormat;
    commentFormat.setForeground(QColor(0x80, 0x80, 0x80));
    commentFormat.setFontItalic(true);
    m_rules.append({QRegularExpression("^\\s*#[^\\n]*"), commentFormat, 0});
}

void DockerfileHighlighter::highlightBlock(const QString &text)
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
