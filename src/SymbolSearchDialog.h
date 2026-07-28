#ifndef SYMBOLSEARCHDIALOG_H
#define SYMBOLSEARCHDIALOG_H

#include <QDialog>
#include <QSet>
#include <QVector>

class QLineEdit;
class QListWidget;

class SymbolSearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SymbolSearchDialog(const QString &rootPath,
                                const QSet<QString> &ignoredAbsolutePaths = {},
                                QWidget *parent = nullptr);
    QString selectedFile() const;
    int selectedLine() const;

    struct Match {
        QString fullPath;
        int line;
    };
    QVector<Match> exactMatches(const QString &name) const;
    void setInitialQuery(const QString &query);

private slots:
    void onTextChanged(const QString &text);
    void onItemDoubleClicked();

private:
    void scanSymbols();
    bool eventFilter(QObject *obj, QEvent *event) override;

    struct Symbol {
        QString name;
        QString file;      // relative path
        QString fullPath;
        int line;
        QString kind;      // "function", "class", "method", etc.
    };

    QLineEdit *m_input;
    QListWidget *m_list;
    QString m_rootPath;
    QSet<QString> m_ignoredAbsolute;
    QVector<Symbol> m_allSymbols;
    QString m_selectedFile;
    int m_selectedLine = -1;
};

#endif
