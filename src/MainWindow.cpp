#include "MainWindow.h"
#include "CppHighlighter.h"
#include "PythonHighlighter.h"
#include "JsonHighlighter.h"
#include "YamlHighlighter.h"
#include "ShellHighlighter.h"
#include "MakefileHighlighter.h"
#include "MarkdownHighlighter.h"
#include "EditorGroup.h"
#include "LspClient.h"
#include "SearchBar.h"
#include "Settings.h"
#include "FileSearchDialog.h"
#include "SymbolSearchDialog.h"
#include "FileIconProvider.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QSplitter>
#include <QTreeView>
#include "CodeEditor.h"
#include <QFileSystemModel>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QTabBar>
#include <QStackedWidget>
#include <QShortcut>
#include <QTextBlock>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QProgressDialog>
#include <QCloseEvent>
#include <QJsonObject>
#include <QJsonArray>
#include <QPlainTextDocumentLayout>
#include <QScrollBar>
#include <QMenu>
#include <QClipboard>
#include <QToolButton>
#include <QWheelEvent>
#include <QTextBrowser>
#include <QActionGroup>
#include <QTextTable>
#include <QTextFrame>

static QString findJediLsp(const QString &rootPath, Settings *settings)
{
    static const QString BIN = "jedi-language-server";

    // Check saved setting first
    QString saved = settings->value("python_venv_path");
    if (!saved.isEmpty()) {
        QString path = saved + "/bin/" + BIN;
        if (QFile::exists(path))
            return path;
    }

    // Look in .venv
    QString venvPath = rootPath + "/.venv/bin/" + BIN;
    if (QFile::exists(venvPath))
        return venvPath;

    // Try common venv names
    for (const QString &dir : {"venv", ".env", "env"}) {
        QString path = rootPath + "/" + dir + "/bin/" + BIN;
        if (QFile::exists(path))
            return path;
    }

    // Check system PATH
    QProcess which;
    which.start("which", {BIN});
    if (which.waitForFinished(2000) && which.exitCode() == 0)
        return BIN;

    return {};
}

static bool isShellByShebang(const QString &content)
{
    if (!content.startsWith("#!"))
        return false;
    int eol = content.indexOf('\n');
    QString line = content.left(eol < 0 ? content.size() : eol);

    static const QRegularExpression re("^#!\\s*(\\S+)(?:\\s+(\\S+))?");
    auto m = re.match(line);
    if (!m.hasMatch())
        return false;

    QString interp = m.captured(1);
    QString arg = m.captured(2);
    QString last = interp.endsWith("/env") ? arg : interp;
    if (last.isEmpty())
        return false;

    int slash = last.lastIndexOf('/');
    if (slash >= 0)
        last = last.mid(slash + 1);

    return last == "sh" || last == "bash" || last == "zsh"
        || last == "ksh" || last == "dash" || last == "fish";
}

static QString findExistingVenv(const QString &rootPath, Settings *settings)
{
    auto isVenv = [](const QString &dir) {
        return QFile::exists(dir + "/pyvenv.cfg")
            || QFile::exists(dir + "/bin/python")
            || QFile::exists(dir + "/bin/python3");
    };

    QString saved = settings->value("python_venv_path");
    if (!saved.isEmpty() && isVenv(saved))
        return saved;

    for (const QString &dir : {".venv", "venv", ".env", "env"}) {
        QString path = rootPath + "/" + dir;
        if (isVenv(path))
            return path;
    }
    return {};
}

static QStringList findPythonPath(const QString &rootPath)
{
    QStringList paths;
    // Auto-detect src/ directory as a source root
    if (QDir(rootPath + "/src").exists())
        paths.append(rootPath + "/src");
    // Also check for lib/
    if (QDir(rootPath + "/lib").exists())
        paths.append(rootPath + "/lib");
    return paths;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("tide");
    resize(1024, 768);

    m_settings = new Settings(QDir::currentPath());

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Open...", QKeySequence::Open, this, &MainWindow::openFileDialog);
    fileMenu->addAction("&Save", QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", QKeySequence::Quit, QApplication::instance(), &QApplication::quit);

    QMenu *viewMenu = menuBar()->addMenu("&View");
    QMenu *mdViewMenu = viewMenu->addMenu("Markdown View");
    auto *mdGroup = new QActionGroup(this);
    m_mdSourceAct = mdViewMenu->addAction("Source");
    m_mdSplitAct = mdViewMenu->addAction("Split");
    m_mdPreviewAct = mdViewMenu->addAction("Preview");
    for (QAction *a : {m_mdSourceAct, m_mdSplitAct, m_mdPreviewAct}) {
        a->setCheckable(true);
        mdGroup->addAction(a);
    }
    QString savedMode = m_settings->value("markdown_view_mode", "source");
    if (savedMode == "source")        m_mdSourceAct->setChecked(true);
    else if (savedMode == "preview")  m_mdPreviewAct->setChecked(true);
    else                              m_mdSplitAct->setChecked(true);
    connect(m_mdSourceAct, &QAction::triggered, this, [this]() { setMarkdownMode("source"); });
    connect(m_mdSplitAct, &QAction::triggered, this, [this]() { setMarkdownMode("split"); });
    connect(m_mdPreviewAct, &QAction::triggered, this, [this]() { setMarkdownMode("preview"); });

    viewMenu->addSeparator();
    m_splitRightAct = viewMenu->addAction("Split &Right",
                          QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Right),
                          this, &MainWindow::splitRight);
    m_closeSplitAct = viewMenu->addAction("&Close Split",
                          QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Backslash),
                          this, &MainWindow::closeSplit);
    updateSplitActions();

    auto makeMdBtn = [](const QString &icon, const QString &tip) {
        auto *b = new QToolButton;
        b->setIcon(QIcon(icon));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(24, 22);
        b->setCheckable(true);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        return b;
    };
    m_mdSourceBtn  = makeMdBtn(":/icons/icons/view-source.svg",  "Source");
    m_mdSplitBtn   = makeMdBtn(":/icons/icons/view-split.svg",   "Split");
    m_mdPreviewBtn = makeMdBtn(":/icons/icons/view-preview.svg", "Preview");

    m_mdButtonsContainer = new QWidget;
    auto *btnRow = new QHBoxLayout(m_mdButtonsContainer);
    btnRow->setContentsMargins(2, 0, 2, 0);
    btnRow->setSpacing(0);
    btnRow->addWidget(m_mdSourceBtn);
    btnRow->addWidget(m_mdSplitBtn);
    btnRow->addWidget(m_mdPreviewBtn);
    m_mdButtonsContainer->hide();

    connect(m_mdSourceBtn, &QToolButton::clicked, this, [this]() { setMarkdownMode("source"); });
    connect(m_mdSplitBtn, &QToolButton::clicked, this, [this]() { setMarkdownMode("split"); });
    connect(m_mdPreviewBtn, &QToolButton::clicked, this, [this]() { setMarkdownMode("preview"); });

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setIconProvider(new FileIconProvider());
    m_fileModel->setRootPath(QDir::currentPath());

    m_treeView = new QTreeView;
    m_treeView->setModel(m_fileModel);
    m_treeView->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_treeView->hideColumn(1); // size
    m_treeView->hideColumn(2); // type
    m_treeView->hideColumn(3); // date modified
    m_treeView->header()->hide();
    {
        QFont tf = m_treeView->font();
        int saved = m_settings->valueInt("tree_font_size", tf.pointSize() > 0 ? tf.pointSize() : 10);
        tf.setPointSize(saved);
        m_treeView->setFont(tf);
    }
    m_treeView->viewport()->installEventFilter(this);

    m_groupSplitter = new QSplitter(Qt::Horizontal);
    auto *firstGroup = new EditorGroup;
    setupGroup(firstGroup);
    m_groups.append(firstGroup);
    m_activeGroup = firstGroup;
    firstGroup->setActiveLook(true);
    m_groupSplitter->addWidget(firstGroup);

    m_searchBar = new SearchBar;

    m_pathLabel = new QLabel;
    m_pathLabel->setContentsMargins(6, 2, 6, 2);
    m_pathLabel->setStyleSheet("color: #808080; font-size: 11px;");
    m_pathLabel->hide();

    m_mdPreview = new QTextBrowser;
    m_mdPreview->setOpenExternalLinks(false);
    m_mdPreview->setOpenLinks(false);
    {
        QFont f = m_mdPreview->font();
        f.setFamily("Segoe UI, Helvetica, Arial, sans-serif");
        f.setPointSize(11);
        m_mdPreview->setFont(f);
        m_mdPreview->document()->setDocumentMargin(18);
        m_mdPreview->document()->setDefaultStyleSheet(R"(
            h1 { font-size: 22pt; margin-top: 24px; margin-bottom: 12px; }
            h2 { font-size: 17pt; margin-top: 20px; margin-bottom: 10px; }
            h3 { font-size: 14pt; margin-top: 18px; margin-bottom: 8px; }
            h4 { font-size: 12pt; margin-top: 14px; margin-bottom: 6px; }
            p, li { line-height: 150%; }
            ul, ol { margin: 8px 0; }
            code { background-color: #3C3F41; padding: 1px 5px; font-family: "JetBrains Mono", monospace; }
            pre { background-color: #2B2D30; padding: 12px; }
            pre, pre * { font-family: "JetBrains Mono", monospace; }
            th { background-color: #2B2D30; font-weight: bold; text-align: left; }
            blockquote { border-left: 3px solid #4D5054; padding-left: 12px; color: #888; }
            a { color: #589DF6; }
        )");
    }
    m_mdPreview->hide();

    m_editorSplitter = new QSplitter(Qt::Horizontal);
    m_editorSplitter->addWidget(m_groupSplitter);
    m_editorSplitter->addWidget(m_mdPreview);
    m_editorSplitter->setStretchFactor(0, 1);
    m_editorSplitter->setStretchFactor(1, 1);

    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    auto *pathBar = new QWidget;
    pathBar->setFixedHeight(24);
    auto *pathBarLayout = new QHBoxLayout(pathBar);
    pathBarLayout->setContentsMargins(0, 0, 0, 0);
    pathBarLayout->setSpacing(0);
    pathBarLayout->addWidget(m_pathLabel, 1);
    pathBarLayout->addWidget(m_mdButtonsContainer);

    m_previewTabHolder = new QWidget;
    auto *holderLayout = new QHBoxLayout(m_previewTabHolder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->setSpacing(0);
    m_previewTabHolder->hide();

    rightLayout->addWidget(m_searchBar);
    rightLayout->addWidget(pathBar);
    rightLayout->addWidget(m_previewTabHolder);
    rightLayout->addWidget(m_editorSplitter, 1);

    m_mdRenderTimer = new QTimer(this);
    m_mdRenderTimer->setSingleShot(true);
    m_mdRenderTimer->setInterval(300);
    connect(m_mdRenderTimer, &QTimer::timeout, this, &MainWindow::renderMarkdownPreview);

    auto *selectFileBtn = new QToolButton;
    selectFileBtn->setIcon(QIcon(":/icons/icons/select-file.svg"));
    selectFileBtn->setIconSize(QSize(18, 18));
    selectFileBtn->setFixedSize(26, 26);
    selectFileBtn->setAutoRaise(true);
    selectFileBtn->setToolTip("Select Opened File");
    connect(selectFileBtn, &QToolButton::clicked, this, &MainWindow::revealCurrentFileInTree);

    auto *treeToolbar = new QWidget;
    auto *treeToolbarLayout = new QHBoxLayout(treeToolbar);
    treeToolbarLayout->setContentsMargins(2, 2, 2, 2);
    treeToolbarLayout->setSpacing(0);
    treeToolbarLayout->addWidget(selectFileBtn);
    treeToolbarLayout->addStretch();

    auto *leftPane = new QWidget;
    auto *leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftLayout->addWidget(treeToolbar);
    leftLayout->addWidget(m_treeView, 1);

    m_mainSplitter = new QSplitter;
    m_mainSplitter->addWidget(leftPane);
    m_mainSplitter->addWidget(rightPane);
    m_mainSplitter->setSizes({200, 824});
    setCentralWidget(m_mainSplitter);

    connect(m_treeView, &QTreeView::doubleClicked, this, &MainWindow::openFile);

    auto *prevTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), this);
    connect(prevTab, &QShortcut::activated, this, [this]() {
        int count = m_activeGroup->count();
        if (count > 1)
            m_activeGroup->setCurrentIndex((m_activeGroup->currentIndex() - 1 + count) % count);
    });

    auto *nextTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), this);
    connect(nextTab, &QShortcut::activated, this, [this]() {
        int count = m_activeGroup->count();
        if (count > 1)
            m_activeGroup->setCurrentIndex((m_activeGroup->currentIndex() + 1) % count);
    });

    auto *gotoDef = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    connect(gotoDef, &QShortcut::activated, this, &MainWindow::gotoDefinition);

    auto *navBack = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this);
    connect(navBack, &QShortcut::activated, this, &MainWindow::navigateBack);

    auto *navForward = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this);
    connect(navForward, &QShortcut::activated, this, &MainWindow::navigateForward);

    auto *closeTabShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_W), this);
    connect(closeTabShortcut, &QShortcut::activated, this, [this]() {
        int index = m_activeGroup->currentIndex();
        if (index >= 0)
            closeTab(index);
    });

    auto *moveTabLeft = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_PageUp), this);
    connect(moveTabLeft, &QShortcut::activated, this, [this]() {
        int index = m_activeGroup->currentIndex();
        if (index > 0)
            m_activeGroup->tabBar()->moveTab(index, index - 1);
    });

    auto *moveTabRight = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_PageDown), this);
    connect(moveTabRight, &QShortcut::activated, this, [this]() {
        int index = m_activeGroup->currentIndex();
        if (index >= 0 && index < m_activeGroup->count() - 1)
            m_activeGroup->tabBar()->moveTab(index, index + 1);
    });

    auto *findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(findShortcut, &QShortcut::activated, this, &MainWindow::showSearch);

    auto *fileSearchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    connect(fileSearchShortcut, &QShortcut::activated, this, &MainWindow::showFileSearch);

    // Start C++ LSP
    QString root = QDir::currentPath();
    m_cppLsp = new LspClient("clangd", {"--compile-commands-dir=" + root}, root, this);
    m_cppLsp->start();

    m_pyLsp = nullptr;

    // Auto-save on idle
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(3000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::saveAll);

    connect(m_fileModel, &QFileSystemModel::directoryLoaded,
            this, &MainWindow::onDirectoryLoaded);

    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *, QWidget *now) {
        if (!now)
            return;
        for (QWidget *w = now; w; w = w->parentWidget()) {
            for (EditorGroup *g : m_groups) {
                if (w == g) {
                    if (m_activeGroup != g) {
                        if (m_activeGroup) m_activeGroup->setActiveLook(false);
                        m_activeGroup = g;
                        g->setActiveLook(true);
                    }
                    return;
                }
            }
        }
    });

    restoreSession();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveAll();
    saveSession();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveSession()
{
    QStringList paths;
    QJsonObject editorState;
    QJsonArray groupFiles;
    QJsonArray groupActives;
    for (EditorGroup *g : m_groups) {
        QJsonArray gPaths;
        QString gActive;
        for (int i = 0; i < g->count(); ++i) {
            QWidget *w = g->widget(i);
            QString path = w ? w->property("filePath").toString() : QString();
            if (path.isEmpty())
                continue;
            gPaths.append(path);
            if (i == g->currentIndex())
                gActive = path;
            if (paths.contains(path))
                continue;
            paths.append(path);

            auto *editor = qobject_cast<CodeEditor *>(w);
            if (!editor)
                continue;
            QJsonObject st;
            st["line"] = editor->textCursor().blockNumber();
            st["col"] = editor->textCursor().columnNumber();
            st["scroll"] = editor->verticalScrollBar()->value();
            editorState[path] = st;
        }
        groupFiles.append(gPaths);
        groupActives.append(gActive);
    }

    QString activePath = tabFilePath(m_activeGroup->currentIndex());

    QJsonObject groupsObj;
    groupsObj["files"] = groupFiles;
    groupsObj["activeFiles"] = groupActives;
    groupsObj["activeGroup"] = m_groups.indexOf(m_activeGroup);
    m_settings->setValueObject("session.groups", groupsObj);
    if (m_groups.size() > 1) {
        m_settings->setValue("session.groupSplitterState",
            QString::fromLatin1(m_groupSplitter->saveState().toBase64()));
    } else {
        m_settings->setValue("session.groupSplitterState", QString());
    }

    QStringList expanded;
    collectExpandedDirs(m_treeView->rootIndex(), expanded);

    m_settings->setValueList("session.openFiles", paths);
    m_settings->setValue("session.activeFile", activePath);
    m_settings->setValueObject("session.editorState", editorState);
    m_settings->setValueList("session.expandedDirs", expanded);

    QString treeSelected = m_fileModel->filePath(m_treeView->currentIndex());
    m_settings->setValue("session.treeSelected", treeSelected);
    m_settings->setValueInt("session.treeScroll",
                            m_treeView->verticalScrollBar()->value());

    m_settings->setValue("session.windowGeometry",
                         QString::fromLatin1(saveGeometry().toBase64()));
    m_settings->setValue("session.mainSplitterState",
                         QString::fromLatin1(m_mainSplitter->saveState().toBase64()));
    m_settings->setValue("session.editorSplitterState",
                         QString::fromLatin1(m_editorSplitter->saveState().toBase64()));
}

void MainWindow::collectExpandedDirs(const QModelIndex &parent, QStringList &out) const
{
    int rows = m_fileModel->rowCount(parent);
    for (int i = 0; i < rows; ++i) {
        QModelIndex child = m_fileModel->index(i, 0, parent);
        if (m_treeView->isExpanded(child)) {
            out.append(m_fileModel->filePath(child));
            collectExpandedDirs(child, out);
        }
    }
}

void MainWindow::restoreSession()
{
    QString geom = m_settings->value("session.windowGeometry");
    if (!geom.isEmpty())
        restoreGeometry(QByteArray::fromBase64(geom.toLatin1()));
    QString mainSplit = m_settings->value("session.mainSplitterState");
    if (!mainSplit.isEmpty())
        m_mainSplitter->restoreState(QByteArray::fromBase64(mainSplit.toLatin1()));
    QString editorSplit = m_settings->value("session.editorSplitterState");
    if (!editorSplit.isEmpty())
        m_editorSplitter->restoreState(QByteArray::fromBase64(editorSplit.toLatin1()));

    QString activePath = m_settings->value("session.activeFile");
    QJsonObject editorState = m_settings->valueObject("session.editorState");

    auto applyEditorState = [&](const QString &path) {
        auto *editor = qobject_cast<CodeEditor *>(
            m_activeGroup->widget(m_activeGroup->count() - 1));
        if (!editor || !editorState.contains(path))
            return;
        QJsonObject st = editorState[path].toObject();
        int line = st.value("line").toInt();
        int col = st.value("col").toInt();
        int scroll = st.value("scroll").toInt();
        QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                qMin(col, block.length() - 1));
            editor->setTextCursor(cursor);
        }
        QTimer::singleShot(0, editor, [editor, scroll]() {
            editor->verticalScrollBar()->setValue(scroll);
        });
    };

    QJsonObject groupsObj = m_settings->valueObject("session.groups");
    QJsonArray groupFiles = groupsObj["files"].toArray();
    QJsonArray groupActives = groupsObj["activeFiles"].toArray();

    if (!groupFiles.isEmpty()) {
        for (int gi = 0; gi < groupFiles.size(); ++gi) {
            if (gi > 0)
                splitRight();
            // Make m_groups[gi] active so loadFile() lands in the right group
            EditorGroup *g = m_groups[gi];
            if (m_activeGroup != g) {
                if (m_activeGroup) m_activeGroup->setActiveLook(false);
                m_activeGroup = g;
                g->setActiveLook(true);
            }
            QJsonArray paths = groupFiles[gi].toArray();
            for (const auto &v : paths) {
                QString path = v.toString();
                if (!QFile::exists(path))
                    continue;
                if (findGroupForPath(path))  // already in another group
                    continue;
                loadFile(path);
                applyEditorState(path);
            }
            QString gActive = (gi < groupActives.size())
                                  ? groupActives[gi].toString() : QString();
            if (!gActive.isEmpty()) {
                int idx = g->indexOfPath(gActive);
                if (idx >= 0) g->setCurrentIndex(idx);
            }
        }
        QString gss = m_settings->value("session.groupSplitterState");
        if (!gss.isEmpty() && m_groups.size() > 1)
            m_groupSplitter->restoreState(QByteArray::fromBase64(gss.toLatin1()));
        int activeGroupIdx = groupsObj.value("activeGroup").toInt(0);
        if (activeGroupIdx >= 0 && activeGroupIdx < m_groups.size()) {
            EditorGroup *target = m_groups[activeGroupIdx];
            if (m_activeGroup != target) {
                if (m_activeGroup) m_activeGroup->setActiveLook(false);
                m_activeGroup = target;
                target->setActiveLook(true);
            }
        }
    } else {
        // Backward-compatible single-group restore
        QStringList paths = m_settings->valueList("session.openFiles");
        for (const QString &path : paths) {
            if (!QFile::exists(path))
                continue;
            loadFile(path);
            applyEditorState(path);
        }
        if (!activePath.isEmpty()) {
            EditorGroup *g = findGroupForPath(activePath);
            if (g) {
                int idx = g->indexOfPath(activePath);
                if (idx >= 0) g->setCurrentIndex(idx);
            }
        }
    }

    updateSplitActions();
    QStringList expanded = m_settings->valueList("session.expandedDirs");
    for (const QString &dir : expanded)
        m_pendingExpand.insert(dir);
    m_pendingTreeSelection = m_settings->value("session.treeSelected");
    m_pendingTreeScroll = m_settings->valueInt("session.treeScroll", -1);
    // Trigger expansion for already-loaded directories (e.g. the root)
    onDirectoryLoaded(QDir::currentPath());
}

void MainWindow::onDirectoryLoaded(const QString &path)
{
    // Expand any pending dirs whose parent is `path` (now populated)
    if (!m_pendingExpand.isEmpty()) {
        QModelIndex parentIdx = m_fileModel->index(path);
        int rows = m_fileModel->rowCount(parentIdx);
        for (int i = 0; i < rows; ++i) {
            QModelIndex child = m_fileModel->index(i, 0, parentIdx);
            QString childPath = m_fileModel->filePath(child);
            if (m_pendingExpand.contains(childPath)) {
                m_pendingExpand.remove(childPath);
                m_treeView->expand(child);
                // Triggers lazy load; remaining nested dirs handled when
                // their directoryLoaded fires.
            }
        }
    }

    // Restore the previously selected tree row once its parent is loaded
    if (!m_pendingTreeSelection.isEmpty()) {
        QModelIndex idx = m_fileModel->index(m_pendingTreeSelection);
        if (idx.isValid()) {
            m_treeView->setCurrentIndex(idx);
            m_pendingTreeSelection.clear();
        }
    }

    // Restore tree scroll value once all previously-expanded dirs are loaded
    if (m_pendingExpand.isEmpty() && m_pendingTreeScroll >= 0) {
        int saved = m_pendingTreeScroll;
        m_pendingTreeScroll = -1;
        QTimer::singleShot(0, this, [this, saved]() {
            m_treeView->verticalScrollBar()->setValue(saved);
        });
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeView->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            int d = wheel->angleDelta().y();
            if (d == 0)
                d = wheel->angleDelta().x();
            if (d != 0) {
                QFont f = m_treeView->font();
                int cur = f.pointSize() > 0 ? f.pointSize() : 10;
                int size = qBound(6, cur + (d > 0 ? 1 : -1), 30);
                f.setPointSize(size);
                m_treeView->setFont(f);
                m_settings->setValueInt("tree_font_size", size);
                statusBar()->showMessage(QString("Tree font: %1 pt").arg(size), 2000);
            }
            return true;
        }
        return false;
    }

    {
        EditorGroup *hitGroup = nullptr;
        for (EditorGroup *g : m_groups) {
            if (obj == g->tabBar()) {
                hitGroup = g;
                break;
            }
        }
        if (hitGroup && event->type() == QEvent::Wheel) {
            auto *wheel = static_cast<QWheelEvent *>(event);
            int delta = wheel->angleDelta().y();
            if (delta == 0)
                delta = wheel->angleDelta().x();
            if (delta == 0)
                return false;

            Qt::ArrowType wanted = (delta > 0) ? Qt::LeftArrow : Qt::RightArrow;
            for (QToolButton *btn : hitGroup->tabBar()->findChildren<QToolButton *>()) {
                if (btn->arrowType() == wanted && btn->isEnabled()) {
                    btn->click();
                    break;
                }
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::openFile(const QModelIndex &index)
{
    if (m_fileModel->isDir(index))
        return;

    loadFile(m_fileModel->filePath(index));
}

void MainWindow::openFileDialog()
{
    QString path = QFileDialog::getOpenFileName(this, "Open File", QDir::currentPath());
    if (!path.isEmpty())
        loadFile(path);
}

void MainWindow::saveFile()
{
    saveTab(m_activeGroup->currentIndex());
}

void MainWindow::saveTab(int index)
{
    if (index < 0)
        return;

    QString path = tabFilePath(index);
    if (path.isEmpty())
        return;

    auto *editor = qobject_cast<CodeEditor *>(m_activeGroup->widget(index));
    if (!editor || !editor->document()->isModified())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << editor->toPlainText();
    editor->document()->setModified(false);
}

void MainWindow::saveAll()
{
    EditorGroup *prev = m_activeGroup;
    for (EditorGroup *g : m_groups) {
        m_activeGroup = g;
        for (int i = 0; i < g->count(); ++i)
            saveTab(i);
    }
    m_activeGroup = prev;
}

void MainWindow::onEditorModified()
{
    m_autoSaveTimer->start();
    if (m_mdPreview->isVisible()
        && isMarkdownFile(QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix()))
        m_mdRenderTimer->start();
}

void MainWindow::onEditorZoom(int delta)
{
    int currentSize = 11;
    if (auto *ed = qobject_cast<CodeEditor *>(m_activeGroup->currentWidget())) {
        int p = ed->font().pointSize();
        if (p > 0)
            currentSize = p;
    }
    int size = qBound(6, currentSize + delta, 40);
    m_settings->setValueInt("editor_font_size", size);

    for (EditorGroup *g : m_groups) {
        for (int i = 0; i < g->count(); ++i) {
            if (auto *ed = qobject_cast<CodeEditor *>(g->widget(i))) {
                QFont f = ed->font();
                f.setPointSize(size);
                ed->setFont(f);
            }
        }
    }

    statusBar()->showMessage(QString("Font size: %1 pt").arg(size), 2000);
}

void MainWindow::onTabChanged(int index)
{
    // Save all before switching
    saveAll();

    if (index < 0) {
        setWindowTitle("tide");
        m_searchBar->setEditor(nullptr);
        m_pathLabel->hide();
        return;
    }
    setWindowTitle("tide - " + m_activeGroup->tabText(index));
    m_searchBar->setEditor(currentEditor());

    QString filePath = tabFilePath(index);
    QDir root(QDir::currentPath());
    QString relative = root.relativeFilePath(filePath);
    m_pathLabel->setText(relative.replace("/", "  >  "));
    m_pathLabel->show();

    applyMarkdownMode();
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate)
        saveAll();

    // Double-Shift detection for symbol search
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Shift && keyEvent->modifiers() == Qt::ShiftModifier) {
            if (m_shiftWasReleased && m_lastShiftPress.isValid() && m_lastShiftPress.elapsed() < 400) {
                m_shiftWasReleased = false;
                showSymbolSearch();
                return true;
            }
            m_lastShiftPress.start();
            m_shiftWasReleased = false;
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Shift)
            m_shiftWasReleased = true;
    }

    return QMainWindow::event(event);
}

void MainWindow::showSearch()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    m_searchBar->setEditor(editor);
    m_searchBar->activate();
}

void MainWindow::showFileSearch()
{
    FileSearchDialog dialog(QDir::currentPath(), this);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFile().isEmpty())
        loadFile(dialog.selectedFile());
}

void MainWindow::showSymbolSearch()
{
    SymbolSearchDialog dialog(QDir::currentPath(), this);
    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFile().isEmpty())
        loadFile(dialog.selectedFile(), dialog.selectedLine() - 1); // LSP uses 0-based lines
}

void MainWindow::closeTab(int index)
{
    saveTab(index);
    QWidget *w = m_activeGroup->widget(index);
    QTextDocument *doc = nullptr;
    if (auto *e = qobject_cast<CodeEditor *>(w))
        doc = e->document();

    m_activeGroup->removeTab(index);

    // If no other editor still references this doc, delete it.
    if (doc) {
        bool stillUsed = false;
        for (EditorGroup *g : m_groups) {
            for (int i = 0; i < g->count(); ++i) {
                auto *e = qobject_cast<CodeEditor *>(g->widget(i));
                if (e && e->document() == doc) {
                    stillUsed = true;
                    break;
                }
            }
            if (stillUsed) break;
        }
        if (!stillUsed)
            doc->deleteLater();
    }

    if (m_activeGroup->count() == 0 && m_groups.size() > 1)
        closeSplit();
}

void MainWindow::showTabContextMenu(const QPoint &pos)
{
    QTabBar *bar = m_activeGroup->tabBar();
    int index = bar->tabAt(pos);
    if (index < 0)
        return;

    QString path = tabFilePath(index);
    if (path.isEmpty())
        return;

    QMenu menu(this);
    QAction *copyAction = menu.addAction("Copy Path");
    QAction *moveAction = nullptr;
    QAction *showAction = nullptr;
    if (m_groups.size() > 1) {
        moveAction = menu.addAction("Move to Other Split");
        showAction = menu.addAction("Show in Other Split");
    }
    QAction *chosen = menu.exec(bar->mapToGlobal(pos));
    if (chosen == copyAction) {
        QApplication::clipboard()->setText(path);
    } else if (moveAction && chosen == moveAction) {
        EditorGroup *src = m_activeGroup;
        EditorGroup *dst = nullptr;
        for (EditorGroup *g : m_groups) {
            if (g != src) { dst = g; break; }
        }
        if (!dst)
            return;
        QWidget *w = src->widget(index);
        if (!w)
            return;
        QString label = src->tabText(index);
        src->tabBar()->removeTab(index);
        src->pageStack()->removeWidget(w);
        int newIdx = dst->addTab(w, label);
        if (m_activeGroup) m_activeGroup->setActiveLook(false);
        m_activeGroup = dst;
        dst->setActiveLook(true);
        dst->setCurrentIndex(newIdx);
    } else if (showAction && chosen == showAction) {
        EditorGroup *src = m_activeGroup;
        EditorGroup *dst = nullptr;
        for (EditorGroup *g : m_groups) {
            if (g != src) { dst = g; break; }
        }
        if (!dst)
            return;
        auto *srcEditor = qobject_cast<CodeEditor *>(src->widget(index));
        if (!srcEditor)
            return;
        QTextDocument *doc = srcEditor->document();
        // Reparent doc to MainWindow so it survives if either editor is closed.
        doc->setParent(this);

        auto *editor = new CodeEditor;
        editor->setDocument(doc);
        editor->setProperty("filePath", path);
        QFont f = editor->font();
        f.setPointSize(m_settings->valueInt("editor_font_size", f.pointSize()));
        editor->setFont(f);
        connect(editor, &CodeEditor::textChanged, this, &MainWindow::onEditorModified);
        connect(editor, &CodeEditor::zoomRequested, this, &MainWindow::onEditorZoom);

        QString name = QFileInfo(path).fileName();
        int newIdx = dst->addTab(editor, name);
        if (m_activeGroup) m_activeGroup->setActiveLook(false);
        m_activeGroup = dst;
        dst->setActiveLook(true);
        dst->setCurrentIndex(newIdx);
    }
}

void MainWindow::gotoDefinition()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    QString path = tabFilePath(m_activeGroup->currentIndex());
    if (path.isEmpty())
        return;

    LspClient *lsp = lspForFile(path);
    if (!lsp) {
        statusBar()->showMessage("No language server for this file type", 3000);
        return;
    }
    if (!lsp->isRunning()) {
        statusBar()->showMessage("Language server not running", 3000);
        return;
    }

    // Save first so LSP sees latest content
    saveFile();
    lsp->didChange(path, editor->toPlainText());

    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber();
    int column = cursor.columnNumber();

    QTextCursor wordCursor(cursor);
    wordCursor.select(QTextCursor::WordUnderCursor);
    QString word = wordCursor.selectedText();

    lsp->gotoDefinition(path, line, column, [this, word](const QVector<LspLocation> &locations) {
        if (!locations.isEmpty()) {
            pushCurrentLocation();
            m_forwardStack.clear();
            const LspLocation &loc = locations.first();
            navigateTo(loc.filePath, loc.line);
            return;
        }

        // Fallback: regex symbol scan for the word under the cursor
        if (word.isEmpty()) {
            statusBar()->showMessage("No definition found", 3000);
            return;
        }

        SymbolSearchDialog dialog(QDir::currentPath(), this);
        auto matches = dialog.exactMatches(word);

        if (matches.size() == 1) {
            pushCurrentLocation();
            m_forwardStack.clear();
            navigateTo(matches.first().fullPath, matches.first().line - 1);
            statusBar()->showMessage("Found via symbol index (LSP returned no result)", 3000);
            return;
        }

        if (matches.size() > 1) {
            dialog.setInitialQuery(word);
            if (dialog.exec() == QDialog::Accepted && !dialog.selectedFile().isEmpty()) {
                pushCurrentLocation();
                m_forwardStack.clear();
                navigateTo(dialog.selectedFile(), dialog.selectedLine() - 1);
            }
            return;
        }

        statusBar()->showMessage("No definition found", 3000);
    });
}

void MainWindow::pushCurrentLocation()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QString path = tabFilePath(m_activeGroup->currentIndex());
    if (path.isEmpty())
        return;
    m_backStack.push({path, editor->textCursor().blockNumber()});
}

void MainWindow::navigateTo(const QString &path, int line)
{
    m_navigating = true;
    loadFile(path, line);
    m_navigating = false;
}

void MainWindow::navigateBack()
{
    if (m_backStack.isEmpty())
        return;

    auto *editor = currentEditor();
    QString curPath = tabFilePath(m_activeGroup->currentIndex());
    int curLine = editor ? editor->textCursor().blockNumber() : 0;
    m_forwardStack.push({curPath, curLine});

    NavLocation loc = m_backStack.pop();
    navigateTo(loc.filePath, loc.line);
}

void MainWindow::navigateForward()
{
    if (m_forwardStack.isEmpty())
        return;

    auto *editor = currentEditor();
    QString curPath = tabFilePath(m_activeGroup->currentIndex());
    int curLine = editor ? editor->textCursor().blockNumber() : 0;
    m_backStack.push({curPath, curLine});

    NavLocation loc = m_forwardStack.pop();
    navigateTo(loc.filePath, loc.line);
}

void MainWindow::loadFile(const QString &path, int line)
{
    // If already open in some group, switch focus to it
    // If file is open in active group, switch to that tab
    if (int idx = m_activeGroup->indexOfPath(path); idx >= 0) {
        m_activeGroup->setCurrentIndex(idx);
        if (line >= 0) {
            auto *editor = currentEditor();
            QTextBlock block = editor->document()->findBlockByNumber(line);
            QTextCursor cursor(block);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        }
        return;
    }
    // Otherwise, if open in a different group, switch focus there
    if (EditorGroup *g = findGroupForPath(path)) {
        if (m_activeGroup) m_activeGroup->setActiveLook(false);
        m_activeGroup = g;
        m_activeGroup->setActiveLook(true);
        int idx = g->indexOfPath(path);
        g->setCurrentIndex(idx);
        if (line >= 0) {
            auto *editor = currentEditor();
            QTextBlock block = editor->document()->findBlockByNumber(line);
            QTextCursor cursor(block);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        }
        return;
    }

    // Prefer an empty group as the target — so the next open after
    // Split Right lands in the new (empty) pane regardless of which
    // group currently has visual focus.
    for (EditorGroup *g : m_groups) {
        if (g->count() == 0) {
            if (m_activeGroup != g) {
                if (m_activeGroup) m_activeGroup->setActiveLook(false);
                m_activeGroup = g;
                g->setActiveLook(true);
            }
            break;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString content = in.readAll();

    auto *editor = new CodeEditor;
    QFont f = editor->font();
    f.setPointSize(m_settings->valueInt("editor_font_size", f.pointSize()));
    editor->setFont(f);
    auto *doc = new QTextDocument(this);
    doc->setDocumentLayout(new QPlainTextDocumentLayout(doc));
    doc->setDefaultFont(f);
    doc->setPlainText(content);
    editor->setDocument(doc);
    editor->setFont(f);
    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    connect(editor, &CodeEditor::textChanged, this, &MainWindow::onEditorModified);
    connect(editor, &CodeEditor::zoomRequested, this, &MainWindow::onEditorZoom);

    QString suffix = QFileInfo(path).suffix();
    QString fileName = QFileInfo(path).fileName();
    if (isMakefile(fileName, suffix)) {
        new MakefileHighlighter(editor->document());
    } else if (isCppFile(suffix)) {
        new CppHighlighter(editor->document());
        m_cppLsp->didOpen(path, content, "cpp");
        QTimer::singleShot(1000, this, [this, path, editor]() {
            requestSemanticHighlight(path, editor);
        });
    } else if (isJsonFile(suffix)) {
        new JsonHighlighter(editor->document());
    } else if (isMarkdownFile(suffix)) {
        new MarkdownHighlighter(editor->document());
    } else if (isYamlFile(suffix)) {
        new YamlHighlighter(editor->document());
    } else if (isShellFile(suffix) || (suffix.isEmpty() && isShellByShebang(content))) {
        new ShellHighlighter(editor->document());
    } else if (isPythonFile(suffix)) {
        new PythonHighlighter(editor->document());
        ensurePythonLsp();
        if (m_pyLsp && m_pyLsp->isRunning()) {
            m_pyLsp->didOpen(path, content, "python");
            QTimer::singleShot(1000, this, [this, path, editor]() {
                requestSemanticHighlight(path, editor);
            });
        }
    }

    QString name = QFileInfo(path).fileName();
    int index = m_activeGroup->addTab(editor, name);
    // m_openFiles dropped: walk groups for lookups via findGroupForPath()
    m_activeGroup->setCurrentIndex(index);

    if (line >= 0) {
        QTextBlock block = editor->document()->findBlockByNumber(line);
        QTextCursor cursor(block);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    }
}

CodeEditor *MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor *>(m_activeGroup->currentWidget());
}

void MainWindow::setupGroup(EditorGroup *group)
{
    connect(group->tabBar(), &QTabBar::currentChanged, this, [this, group](int idx) {
        if (m_activeGroup != group) {
            if (m_activeGroup) m_activeGroup->setActiveLook(false);
            m_activeGroup = group;
            m_activeGroup->setActiveLook(true);
        }
        onTabChanged(idx);
    });
    connect(group->tabBar(), &QTabBar::tabCloseRequested, this, [this, group](int idx) {
        if (m_activeGroup != group) {
            if (m_activeGroup) m_activeGroup->setActiveLook(false);
            m_activeGroup = group;
            m_activeGroup->setActiveLook(true);
        }
        closeTab(idx);
    });
    group->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(group->tabBar(), &QWidget::customContextMenuRequested, this,
            [this, group](const QPoint &pos) {
        if (m_activeGroup != group) {
            if (m_activeGroup) m_activeGroup->setActiveLook(false);
            m_activeGroup = group;
            m_activeGroup->setActiveLook(true);
        }
        showTabContextMenu(pos);
    });
    group->tabBar()->installEventFilter(this);
    connect(group, &EditorGroup::activated, this, [this, group]() {
        if (m_activeGroup != group) {
            if (m_activeGroup) m_activeGroup->setActiveLook(false);
            m_activeGroup = group;
            m_activeGroup->setActiveLook(true);
        }
    });
}

void MainWindow::splitRight()
{
    if (m_groups.size() >= 2)
        return;
    auto *group = new EditorGroup;
    setupGroup(group);
    m_groups.append(group);
    m_groupSplitter->addWidget(group);
    if (m_activeGroup) m_activeGroup->setActiveLook(false);
    m_activeGroup = group;
    group->setActiveLook(true);
    for (EditorGroup *g : m_groups)
        g->setUnderlineVisible(true);

    // Force 50/50 split (deferred so width is finalised by the layout)
    QTimer::singleShot(0, this, [this]() {
        int total = m_groupSplitter->width();
        if (total > 0)
            m_groupSplitter->setSizes({total / 2, total / 2});
    });
    updateSplitActions();
}

void MainWindow::closeSplit()
{
    if (m_groups.size() <= 1)
        return;
    EditorGroup *toClose = m_groups.last();
    EditorGroup *survivor = m_groups.first();

    // Close all tabs in toClose
    while (toClose->count() > 0)
        toClose->removeTab(0);
    m_groups.removeOne(toClose);
    toClose->setParent(nullptr);
    toClose->deleteLater();
    m_activeGroup = survivor;
    survivor->setActiveLook(true);
    survivor->setUnderlineVisible(false);
    updateSplitActions();
}

void MainWindow::updateSplitActions()
{
    if (m_splitRightAct) m_splitRightAct->setEnabled(m_groups.size() < 2);
    if (m_closeSplitAct) m_closeSplitAct->setEnabled(m_groups.size() > 1);
}

EditorGroup *MainWindow::findGroupForPath(const QString &path) const
{
    for (EditorGroup *g : m_groups) {
        if (g->indexOfPath(path) >= 0)
            return g;
    }
    return nullptr;
}

QString MainWindow::tabFilePath(int index) const
{
    QWidget *widget = m_activeGroup->widget(index);
    if (!widget)
        return {};
    return widget->property("filePath").toString();
}

void MainWindow::requestSemanticHighlight(const QString &path, CodeEditor *editor)
{
    LspClient *lsp = lspForFile(path);
    if (!lsp || !lsp->isRunning())
        return;

    lsp->requestSemanticTokens(path, [editor](const QVector<SemanticToken> &tokens) {
        if (!editor)
            return;

        // Map token types to Darcula colors
        static const QMap<QString, QColor> colorMap = {
            {"keyword",       QColor(0xCC, 0x78, 0x32)},
            {"type",          QColor(0xB3, 0x89, 0xC5)},
            {"class",         QColor(0xB3, 0x89, 0xC5)},
            {"enum",          QColor(0xB3, 0x89, 0xC5)},
            {"interface",     QColor(0xB3, 0x89, 0xC5)},
            {"struct",        QColor(0xB3, 0x89, 0xC5)},
            {"typeParameter", QColor(0xB3, 0x89, 0xC5)},
            {"parameter",     QColor(0xA9, 0xB7, 0xC6)},
            {"variable",      QColor(0xA9, 0xB7, 0xC6)},
            {"property",      QColor(0x98, 0x76, 0xAA)},
            {"function",      QColor(0xFF, 0xC6, 0x6D)},
            {"method",        QColor(0xFF, 0xC6, 0x6D)},
            {"macro",         QColor(0x98, 0x76, 0xAA)},
            {"namespace",     QColor(0xA9, 0xB7, 0xC6)},
            {"comment",       QColor(0x80, 0x80, 0x80)},
            {"string",        QColor(0x6A, 0x87, 0x59)},
            {"number",        QColor(0x68, 0x97, 0xBB)},
            {"operator",      QColor(0xA9, 0xB7, 0xC6)},
            {"decorator",     QColor(0xBB, 0xB5, 0x29)},
            {"enumMember",    QColor(0x98, 0x76, 0xAA)},
        };

        QTextDocument *doc = editor->document();
        QList<QTextEdit::ExtraSelection> selections;

        for (const SemanticToken &tok : tokens) {
            auto it = colorMap.find(tok.tokenType);
            if (it == colorMap.end())
                continue;

            QTextBlock block = doc->findBlockByNumber(tok.line);
            if (!block.isValid())
                continue;

            QTextCursor cursor(block);
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, tok.column);
            cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, tok.length);

            QTextEdit::ExtraSelection sel;
            sel.cursor = cursor;
            sel.format.setForeground(it.value());

            // Function/method declarations are blue; calls stay yellow
            if ((tok.tokenType == "function" || tok.tokenType == "method")
                && (tok.modifiers.contains("declaration")
                    || tok.modifiers.contains("definition")))
                sel.format.setForeground(QColor(0x56, 0xA8, 0xF5));

            // Bold for keywords
            if (tok.tokenType == "keyword")
                sel.format.setFontWeight(QFont::Bold);

            // Italic for self/cls parameters with defaultLibrary modifier
            if (tok.modifiers.contains("defaultLibrary"))
                sel.format.setFontItalic(true);

            selections.append(sel);
        }

        editor->setExtraSelections(selections);
    });
}

void MainWindow::ensurePythonLsp()
{
    if (m_pyLsp && m_pyLsp->isRunning())
        return;
    if (m_pyLspPromptShown)
        return;

    QString root = QDir::currentPath();
    QString lspBin = findJediLsp(root, m_settings);

    if (!lspBin.isEmpty()) {
        m_pyLsp = new LspClient(lspBin, {}, root, this);
        QStringList pythonPaths = findPythonPath(root);
        if (!pythonPaths.isEmpty())
            m_pyLsp->setEnvironment({"PYTHONPATH=" + pythonPaths.join(":")});
        m_pyLsp->start();
        return;
    }

    // If a venv already exists in the project, just offer to install into it
    QString existingVenv = findExistingVenv(root, m_settings);
    if (!existingVenv.isEmpty()) {
        QMessageBox box(this);
        box.setWindowTitle("Install jedi-language-server?");
        box.setText("Install jedi-language-server into\n" + existingVenv + " ?");
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::Yes);
        int answer = box.exec();
        if (answer == QMessageBox::Yes) {
            m_pyLspPromptShown = true;
            auto *proc = new QProcess(this);
            proc->setWorkingDirectory(root);
            connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [this, proc, existingVenv, root](int exitCode, QProcess::ExitStatus) {
                proc->deleteLater();
                if (exitCode != 0)
                    return;

                QString lspBinPath = existingVenv + "/bin/jedi-language-server";
                m_pyLsp = new LspClient(lspBinPath, {}, root, this);
                QStringList pythonPaths = findPythonPath(root);
                if (!pythonPaths.isEmpty())
                    m_pyLsp->setEnvironment({"PYTHONPATH=" + pythonPaths.join(":")});
                m_pyLsp->start();

                for (EditorGroup *g : m_groups) {
                    for (int i = 0; i < g->count(); ++i) {
                        QWidget *w = g->widget(i);
                        QString p = w ? w->property("filePath").toString() : QString();
                        if (isPythonFile(QFileInfo(p).suffix())) {
                            auto *ed = qobject_cast<CodeEditor *>(w);
                            if (ed)
                                m_pyLsp->didOpen(p, ed->toPlainText(), "python");
                        }
                    }
                }
            });
            proc->start(existingVenv + "/bin/pip", {"install", "jedi-language-server"});
            return;
        }
        // No → fall through to full dialog (Create / Browse / Skip)
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Python Language Server");
    msgBox.setText("No Python language server (jedi-language-server) was found.\n\n"
                   "You can create a new .venv with jedi-language-server installed,\n"
                   "browse for an existing virtual environment, or\n"
                   "skip to use syntax highlighting only.");
    msgBox.setMinimumWidth(500);

    QAbstractButton *createBtn = msgBox.addButton("  Create .venv  ", QMessageBox::ActionRole);
    QAbstractButton *browseBtn = msgBox.addButton("  Browse...  ", QMessageBox::ActionRole);
    QAbstractButton *skipBtn = msgBox.addButton("  Skip  ", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == skipBtn) {
        m_pyLspPromptShown = true;
        return;
    }

    if (msgBox.clickedButton() == createBtn) {
        m_pyLspPromptShown = true;
        // Run the entire setup in the background
        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(root);

        // Chain: create venv, then install pylsp, then start LSP
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, root](int exitCode, QProcess::ExitStatus) {
            QString program = proc->program();

            if (program == "python3" && proc->arguments().contains("--version")) {
                if (exitCode != 0) {
                    proc->deleteLater();
                    return;
                }
                proc->start("python3", {"-m", "venv", ".venv"});

            } else if (program == "python3" && proc->arguments().contains("venv")) {
                if (exitCode != 0) {
                    proc->deleteLater();
                    return;
                }
                proc->start(root + "/.venv/bin/pip", {"install", "jedi-language-server"});

            } else {
                proc->deleteLater();
                if (exitCode != 0)
                    return;

                QString lspBinPath = root + "/.venv/bin/jedi-language-server";
                m_pyLsp = new LspClient(lspBinPath, {}, root, this);
                QStringList pythonPaths = findPythonPath(root);
                if (!pythonPaths.isEmpty())
                    m_pyLsp->setEnvironment({"PYTHONPATH=" + pythonPaths.join(":")});
                m_pyLsp->start();

                // Send didOpen for any already-open Python files
                for (EditorGroup *g : m_groups) {
                    for (int i = 0; i < g->count(); ++i) {
                        QWidget *w = g->widget(i);
                        QString path = w ? w->property("filePath").toString() : QString();
                        if (isPythonFile(QFileInfo(path).suffix())) {
                            auto *editor = qobject_cast<CodeEditor *>(w);
                            if (editor)
                                m_pyLsp->didOpen(path, editor->toPlainText(), "python");
                        }
                    }
                }
            }
        });

        proc->start("python3", {"--version"});

    } else if (msgBox.clickedButton() == browseBtn) {
        QFileDialog dialog(this, "Select Python venv directory", root);
        dialog.setFileMode(QFileDialog::Directory);
        dialog.setOption(QFileDialog::ShowDirsOnly, true);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setFilter(QDir::AllDirs | QDir::Hidden | QDir::NoDotAndDotDot);
        if (!dialog.exec())
            return;
        QString dir = dialog.selectedFiles().first();
        if (dir.isEmpty())
            return;

        QString path = dir + "/bin/jedi-language-server";
        if (!QFile::exists(path)) {
            QMessageBox::warning(this, "Not found",
                "No jedi-language-server found at:\n" + path + "\n\n"
                "Make sure jedi-language-server is installed in that venv.");
            return;
        }

        m_pyLspPromptShown = true;
        m_settings->setValue("python_venv_path", dir);
        m_pyLsp = new LspClient(path, {}, root, this);
        QStringList pythonPaths = findPythonPath(root);
        if (!pythonPaths.isEmpty())
            m_pyLsp->setEnvironment({"PYTHONPATH=" + pythonPaths.join(":")});
        m_pyLsp->start();
    }
}

LspClient *MainWindow::lspForFile(const QString &path) const
{
    QString suffix = QFileInfo(path).suffix();
    if (isCppFile(suffix))
        return m_cppLsp;
    if (isPythonFile(suffix))
        return m_pyLsp;
    return nullptr;
}

bool MainWindow::isCppFile(const QString &suffix)
{
    return suffix == "cpp" || suffix == "cxx" || suffix == "cc" ||
           suffix == "h" || suffix == "hpp" || suffix == "hxx" || suffix == "c";
}

bool MainWindow::isPythonFile(const QString &suffix)
{
    return suffix == "py" || suffix == "pyw" || suffix == "pyi";
}

bool MainWindow::isJsonFile(const QString &suffix)
{
    return suffix == "json";
}

bool MainWindow::isYamlFile(const QString &suffix)
{
    return suffix == "yml" || suffix == "yaml";
}

bool MainWindow::isShellFile(const QString &suffix)
{
    return suffix == "sh" || suffix == "bash" || suffix == "zsh";
}

bool MainWindow::isMakefile(const QString &fileName, const QString &suffix)
{
    return fileName.compare("Makefile", Qt::CaseInsensitive) == 0
        || fileName.compare("GNUmakefile", Qt::CaseInsensitive) == 0
        || suffix == "mk";
}

bool MainWindow::isMarkdownFile(const QString &suffix)
{
    return suffix == "md" || suffix == "markdown";
}

void MainWindow::setMarkdownMode(const QString &mode)
{
    m_settings->setValue("markdown_view_mode", mode);
    applyMarkdownMode();
}

void MainWindow::applyMarkdownMode()
{
    QString suffix = QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix();
    bool isMd = isMarkdownFile(suffix);

    m_mdButtonsContainer->setVisible(isMd);

    auto returnBorrowedTabBar = [this]() {
        if (m_previewBorrowedFrom) {
            QTabBar *bar = m_previewBorrowedFrom->tabBar();
            qobject_cast<QBoxLayout *>(m_previewTabHolder->layout())
                ->removeWidget(bar);
            m_previewBorrowedFrom->attachTabBar();
            m_previewBorrowedFrom = nullptr;
        }
        m_previewTabHolder->hide();
        m_groupSplitter->show();
    };

    if (!isMd) {
        m_mdPreview->hide();
        for (EditorGroup *g : m_groups) g->setContentVisible(true);
        returnBorrowedTabBar();
        return;
    }

    QString mode = m_settings->value("markdown_view_mode", "source");
    m_mdSourceBtn->setChecked(mode == "source");
    m_mdSplitBtn->setChecked(mode == "split");
    m_mdPreviewBtn->setChecked(mode == "preview");
    m_mdSourceAct->setChecked(mode == "source");
    m_mdSplitAct->setChecked(mode == "split");
    m_mdPreviewAct->setChecked(mode == "preview");

    if (mode == "source") {
        m_mdPreview->hide();
        for (EditorGroup *g : m_groups) g->setContentVisible(true);
        returnBorrowedTabBar();
    } else if (mode == "preview") {
        // Move active group's tab bar above the editor splitter, then
        // hide the source pane entirely so preview takes full width.
        if (m_previewBorrowedFrom != m_activeGroup) {
            returnBorrowedTabBar();
            m_activeGroup->detachTabBar();
            qobject_cast<QBoxLayout *>(m_previewTabHolder->layout())
                ->addWidget(m_activeGroup->tabBar());
            m_previewBorrowedFrom = m_activeGroup;
        }
        m_previewTabHolder->show();
        m_groupSplitter->hide();
        m_mdPreview->show();
        renderMarkdownPreview();
    } else {
        m_mdPreview->show();
        for (EditorGroup *g : m_groups) g->setContentVisible(true);
        returnBorrowedTabBar();
        renderMarkdownPreview();
    }
}

void MainWindow::revealCurrentFileInTree()
{
    QString path = tabFilePath(m_activeGroup->currentIndex());
    if (path.isEmpty())
        return;

    QModelIndex idx = m_fileModel->index(path);
    if (!idx.isValid())
        return;

    // Expand all ancestors top-down so lazy-loaded children are populated
    QList<QModelIndex> chain;
    for (QModelIndex p = idx.parent(); p.isValid(); p = p.parent())
        chain.prepend(p);
    for (const QModelIndex &p : chain)
        m_treeView->expand(p);

    m_treeView->setCurrentIndex(idx);
    m_treeView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
}

void MainWindow::renderMarkdownPreview()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QTextDocument *doc = m_mdPreview->document();
    doc->setMarkdown(editor->toPlainText());

    // Qt's markdown importer sets HTML cellspacing/border per cell, which
    // CSS border-collapse can't fully undo. Rewrite each table's format
    // directly so we get a clean single-line grid.
    QList<QTextFrame *> stack;
    stack.append(doc->rootFrame());
    while (!stack.isEmpty()) {
        QTextFrame *frame = stack.takeLast();
        const auto kids = frame->childFrames();
        for (QTextFrame *child : kids) {
            if (auto *table = qobject_cast<QTextTable *>(child)) {
                QTextTableFormat fmt = table->format();
                fmt.setBorder(1);
                fmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
                fmt.setBorderBrush(QBrush(QColor(0x3C, 0x3F, 0x41)));
                fmt.setCellSpacing(0);
                fmt.setCellPadding(8);
                fmt.setBorderCollapse(true);
                table->setFormat(fmt);
            }
            stack.append(child);
        }
    }
}
