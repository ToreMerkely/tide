#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

class QTreeView;
class CodeEditor;
class QFileSystemModel;
class QTabWidget;
class LspClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openFile(const QModelIndex &index);
    void openFileDialog();
    void saveFile();
    void onTabChanged(int index);
    void closeTab(int index);
    void gotoDefinition();

private:
    void loadFile(const QString &path, int line = -1);
    CodeEditor *currentEditor() const;
    QString tabFilePath(int index) const;

    QTreeView *m_treeView;
    QTabWidget *m_tabWidget;
    QFileSystemModel *m_fileModel;
    QMap<QString, int> m_openFiles; // path -> tab index
    LspClient *m_lsp;
};

#endif
