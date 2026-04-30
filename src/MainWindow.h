#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QStack>
#include <QElapsedTimer>

class QTimer;
class QLabel;
class QCloseEvent;

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
    void showFileSearch();
    void showSymbolSearch();
    void saveAll();
    void onEditorModified();
    void showTabContextMenu(const QPoint &pos);
    void onEditorZoom(int delta);

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
    static bool isJsonFile(const QString &suffix);
    static bool isYamlFile(const QString &suffix);
    static bool isShellFile(const QString &suffix);
    static bool isMakefile(const QString &fileName, const QString &suffix);

    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    void saveSession();
    void restoreSession();
    void collectExpandedDirs(const QModelIndex &parent, QStringList &out) const;
    void onDirectoryLoaded(const QString &path);

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
    QElapsedTimer m_lastShiftPress;
    bool m_shiftWasReleased = false;
    QSet<QString> m_pendingExpand;
};

#endif
