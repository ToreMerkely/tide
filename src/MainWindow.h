#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QTreeView;
class QPlainTextEdit;
class QFileSystemModel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openFile(const QModelIndex &index);
    void openFileDialog();
    void saveFile();

private:
    void loadFile(const QString &path);

    QTreeView *m_treeView;
    QPlainTextEdit *m_editor;
    QFileSystemModel *m_fileModel;
    QString m_currentFilePath;
};

#endif
