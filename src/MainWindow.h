#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QStack>
#include <QElapsedTimer>

class QTimer;
class QLabel;
class QCloseEvent;
class QTextBrowser;
class QSplitter;
class QAction;
class QToolButton;
class QFileSystemWatcher;

class QTreeView;
class CodeEditor;
class IgnoreAwareModel;
class QTabBar;
class QStackedWidget;
class LspClient;
class SearchBar;
class Settings;
class EditorGroup;

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
    void gotoLine();
    void navigateBack();
    void navigateForward();
    void showSearch();
    void showSearchReplace();
    void showFileSearch();
    void showSymbolSearch();
    void saveAll();
    void onEditorModified();
    void onFileChangedOnDisk(const QString &path);
    void showTabContextMenu(const QPoint &pos);
    void onEditorZoom(int delta);
    void revealCurrentFileInTree();
    void showTreeContextMenu(const QPoint &pos);
    void openInOtherSplit(const QString &path);

private:
    struct NavLocation {
        QString filePath;
        int line;
    };

    void loadFile(const QString &path, int line = -1);
    void navigateTo(const QString &path, int line);
    void pushCurrentLocation();
    void saveTab(int index);
    QString projectTitlePrefix();
    void updateWindowTitle();
    void queryPrForBranch(const QString &branch);
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
    static bool isMarkdownFile(const QString &suffix);
    static bool isHtmlFile(const QString &suffix);
    static bool isCssFile(const QString &suffix);
    static bool isJsFile(const QString &suffix);
    static bool isTsFile(const QString &suffix);
    static bool isXmlFile(const QString &suffix);
    static bool isConfigFile(const QString &fileName, const QString &suffix);
    static bool isGherkinFile(const QString &suffix);
    static bool isDockerfile(const QString &fileName, const QString &suffix);
    static bool isTerraformFile(const QString &suffix);
    static bool isGoFile(const QString &fileName, const QString &suffix);
    static bool isImageFile(const QString &suffix);
    void ensureGoLsp();

    void applyMarkdownMode();
    void renderMarkdownPreview();
    void setMarkdownMode(const QString &mode);
    void syncPreviewFromEditor();
    void syncEditorFromPreview();
    void rebuildMarkdownScrollMap();

    bool event(QEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    void saveSession();
    void restoreSession();
    void collectExpandedDirs(const QModelIndex &parent, QStringList &out) const;
    void onDirectoryLoaded(const QString &path);

    QTreeView *m_treeView;
    QList<EditorGroup *> m_groups;
    EditorGroup *m_activeGroup = nullptr;
    QSplitter *m_groupSplitter = nullptr;
    QWidget *m_previewTabHolder = nullptr;
    EditorGroup *m_previewBorrowedFrom = nullptr;
    void setupGroup(EditorGroup *group);
    void splitRight();
    void closeSplit();
    void removeGroup(EditorGroup *group);
    EditorGroup *findGroupForPath(const QString &path) const;
    IgnoreAwareModel *m_fileModel;
    QSet<QString> m_ignoredAbsolute;
    void loadIgnoredFromSettings();
    void saveIgnoredToSettings();
    void toggleIgnored(const QString &absolutePath);
    // (was m_openFiles cache; now derived by walking groups via findGroupForPath)
    LspClient *m_cppLsp;
    LspClient *m_pyLsp;
    LspClient *m_goLsp = nullptr;
    bool m_goLspPromptShown = false;
    QFileSystemWatcher *m_fileWatcher = nullptr;
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
    QString m_pendingTreeSelection;
    int m_pendingTreeScroll = -1;
    QTextBrowser *m_mdPreview;
    QSplitter *m_editorSplitter;
    QSplitter *m_mainSplitter;
    QTimer *m_mdRenderTimer;
    QAction *m_mdSourceAct = nullptr;
    QAction *m_mdSplitAct = nullptr;
    QAction *m_mdPreviewAct = nullptr;
    QAction *m_splitRightAct = nullptr;
    QAction *m_closeSplitAct = nullptr;
    void updateSplitActions();
    QWidget *m_mdButtonsContainer = nullptr;
    QToolButton *m_mdSourceBtn = nullptr;
    QToolButton *m_mdSplitBtn = nullptr;
    QToolButton *m_mdPreviewBtn = nullptr;
    QHash<QString, int> m_prNumberByBranch; // branch -> PR number, -1 = none
    QString m_prQueryInFlight;
    bool m_syncingScroll = false;
    struct ScrollAnchor { int sourceLine; int previewY; };
    QList<ScrollAnchor> m_mdScrollAnchors;
};

#endif
