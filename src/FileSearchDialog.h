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
                              QWidget *parent = nullptr);
    QString selectedFile() const;

private slots:
    void onTextChanged(const QString &text);
    void onItemDoubleClicked();

private:
    void scanFiles(const QString &dir, const QString &prefix);
    bool eventFilter(QObject *obj, QEvent *event) override;

    QLineEdit *m_input;
    QListWidget *m_list;
    QString m_rootPath;
    QStringList m_allFiles;
    QString m_selectedFile;
};

#endif
