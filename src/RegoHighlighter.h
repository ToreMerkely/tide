#ifndef REGOHIGHLIGHTER_H
#define REGOHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class RegoHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit RegoHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<Rule> m_rules;
    QRegularExpression m_rawStringFence;
    QTextCharFormat m_stringFormat;
};

#endif
