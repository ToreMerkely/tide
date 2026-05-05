#ifndef GHERKINHIGHLIGHTER_H
#define GHERKINHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class GherkinHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit GherkinHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<Rule> m_rules;
    QTextCharFormat m_docStringFormat;
    QRegularExpression m_docStringFence;
};

#endif
