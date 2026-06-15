#ifndef FILESEARCHDIALOG_H
#define FILESEARCHDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QSet>

class QLineEdit;
class QListWidget;

class FileSearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileSearchDialog(const QString &rootPath,
                              const QSet<QString> &ignoredAbsolutePaths = {},
                              const QString &initialQuery = {},
                              QWidget *parent = nullptr);
    QStringList selectedFiles() const;

private slots:
    void onTextChanged(const QString &text);
    void onItemDoubleClicked();

private:
    void scanFiles(const QString &dir, const QString &prefix);
    void acceptSelection();
    void populate();
    bool eventFilter(QObject *obj, QEvent *event) override;

    QLineEdit *m_input;
    QListWidget *m_list;
    QString m_rootPath;
    QSet<QString> m_ignoredAbsolute;
    QStringList m_allFiles;
    QStringList m_selectedFiles;
    bool m_populated = false;
};

#endif
