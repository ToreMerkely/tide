#ifndef DOCKERFILEHIGHLIGHTER_H
#define DOCKERFILEHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class DockerfileHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit DockerfileHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<Rule> m_rules;
};

#endif
