#ifndef GOHIGHLIGHTER_H
#define GOHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class GoHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit GoHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QVector<Rule> m_rules;
    QRegularExpression m_blockCommentStart;
    QRegularExpression m_blockCommentEnd;
    QRegularExpression m_rawStringFence;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_stringFormat;
};

#endif
