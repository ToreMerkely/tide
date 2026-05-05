#ifndef TERRAFORMHIGHLIGHTER_H
#define TERRAFORMHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class TerraformHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit TerraformHighlighter(QTextDocument *parent = nullptr);

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
    QTextCharFormat m_commentFormat;
};

#endif
