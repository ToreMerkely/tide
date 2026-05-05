#ifndef CONFIGHIGHLIGHTER_H
#define CONFIGHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QVector>

class ConfigHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit ConfigHighlighter(QTextDocument *parent = nullptr);

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
