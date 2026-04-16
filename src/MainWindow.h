#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QStack>

class QTimer;
class QLabel;

class QTreeView;
class CodeEditor;
class QFileSystemModel;
class QTabWidget;
class LspClient;
class SearchBar;
class Settings;

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
    void navigateBack();
    void navigateForward();
    void showSearch();
    void saveAll();
    void onEditorModified();

private:
    struct NavLocation {
        QString filePath;
        int line;
    };

    void loadFile(const QString &path, int line = -1);
    void navigateTo(const QString &path, int line);
    void pushCurrentLocation();
    void saveTab(int index);
    CodeEditor *currentEditor() const;
    QString tabFilePath(int index) const;
    LspClient *lspForFile(const QString &path) const;
    void ensurePythonLsp();
    void requestSemanticHighlight(const QString &path, CodeEditor *editor);
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
    QLabel *m_pathLabel;
    QTimer *m_autoSaveTimer;
    Settings *m_settings;
    QStack<NavLocation> m_backStack;
    QStack<NavLocation> m_forwardStack;
    bool m_navigating = false;
    bool m_pyLspPromptShown = false;
};

#endif
