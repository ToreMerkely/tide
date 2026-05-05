#ifndef JSHIGHLIGHTER_H
#define JSHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class JsHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    enum Flavor { JavaScript, TypeScript };
    explicit JsHighlighter(QTextDocument *parent = nullptr,
                           Flavor flavor = JavaScript);

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
