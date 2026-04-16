#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

class QTimer;

class QTreeView;
class CodeEditor;
class QFileSystemModel;
class QTabWidget;
class LspClient;
class SearchBar;

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
    void showSearch();
    void saveAll();
    void onEditorModified();

private:
    void loadFile(const QString &path, int line = -1);
    void saveTab(int index);
    CodeEditor *currentEditor() const;
    QString tabFilePath(int index) const;
    LspClient *lspForFile(const QString &path) const;
    void ensurePythonLsp();
    static bool isCppFile(const QString &suffix);
    static bool isPythonFile(const QString &suffix);

    bool event(QEvent *event) override;

    QTreeView *m_treeView;
    QTabWidget *m_tabWidget;
    QFileSystemModel *m_fileModel;
    QMap<QString, int> m_openFiles; // path -> tab index
    LspClient *m_cppLsp;
    LspClient *m_pyLsp;
    SearchBar *m_searchBar;
    QTimer *m_autoSaveTimer;
    bool m_pyLspPromptShown = false;
};

#endif
