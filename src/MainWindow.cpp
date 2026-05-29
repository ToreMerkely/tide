#include "MainWindow.h"
#include "CppHighlighter.h"
#include "PythonHighlighter.h"
#include "JsonHighlighter.h"
#include "YamlHighlighter.h"
#include "ShellHighlighter.h"
#include "MakefileHighlighter.h"
#include "MarkdownHighlighter.h"
#include "HtmlHighlighter.h"
#include "CssHighlighter.h"
#include "JsHighlighter.h"
#include "ConfigHighlighter.h"
#include "GherkinHighlighter.h"
#include "DockerfileHighlighter.h"
#include "TerraformHighlighter.h"
#include "GoHighlighter.h"
#include "EditorGroup.h"
#include "LspClient.h"
#include "SearchBar.h"
#include "Settings.h"
#include "FileSearchDialog.h"
#include "SymbolSearchDialog.h"
#include "FileIconProvider.h"
#include "IgnoreAwareModel.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QSplitter>
#include <QTreeView>
#include "CodeEditor.h"
#include "ImageViewer.h"
#include "PdfViewer.h"
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
#include <QFileSystemWatcher>
#include <QTextBrowser>
#include <QActionGroup>
#include <QTextTable>
#include <QTextFrame>
#include <QDesktopServices>
#include <QUrl>
#include <QAbstractTextDocumentLayout>
#include <QInputDialog>

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

static QString findGopls(const QString &rootPath, Settings *settings)
{
    Q_UNUSED(rootPath);
    QString saved = settings->value("gopls_path");
    if (!saved.isEmpty() && QFile::exists(saved))
        return saved;

    QProcess which;
    which.start("which", {"gopls"});
    if (which.waitForFinished(2000) && which.exitCode() == 0)
        return "gopls";

    QString home = qEnvironmentVariable("HOME");
    QString gopath = qEnvironmentVariable("GOPATH");
    QString gobin = qEnvironmentVariable("GOBIN");
    QStringList candidates;
    if (!gobin.isEmpty())
        candidates << gobin + "/gopls";
    if (!gopath.isEmpty())
        candidates << gopath + "/bin/gopls";
    if (!home.isEmpty())
        candidates << home + "/go/bin/gopls";
    for (const QString &c : candidates) {
        if (QFile::exists(c))
            return c;
    }
    return {};
}

// Walk up from `path` looking for a directory containing go.mod; returns
// the directory of the closest go.mod or an empty string if none found.
static QString findGoModRoot(const QString &startPath)
{
    QFileInfo info(startPath);
    QDir dir = info.isDir() ? QDir(info.absoluteFilePath()) : info.absoluteDir();
    while (true) {
        if (QFile::exists(dir.filePath("go.mod")))
            return dir.absolutePath();
        if (!dir.cdUp())
            return {};
    }
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

static QString readGitBranch(const QString &repoRoot)
{
    QFile head(repoRoot + "/.git/HEAD");
    if (!head.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QString line = QTextStream(&head).readLine().trimmed();
    if (line.startsWith("ref: refs/heads/"))
        return line.mid(QStringLiteral("ref: refs/heads/").size());
    if (line.size() >= 7)
        return line.left(7); // detached HEAD: short hash
    return {};
}

QString MainWindow::projectTitlePrefix()
{
    QString cwd = QDir::currentPath();
    QDir d(cwd);
    QString last = d.dirName();
    QString locale = last.isEmpty() ? cwd : last;
    if (!last.isEmpty() && d.cdUp()) {
        QString parent = d.dirName();
        if (!parent.isEmpty())
            locale = parent + "/" + last;
    }
    QString branch = readGitBranch(cwd);
    if (!branch.isEmpty()) {
        QString tag = branch;
        int pr = m_prNumberByBranch.value(branch, 0);
        if (pr > 0)
            tag += " #" + QString::number(pr);
        locale += " [" + tag + "]";
    }
    return locale;
}

void MainWindow::queryPrForBranch(const QString &branch)
{
    if (branch.isEmpty())
        return;
    if (m_prQueryInFlight == branch)
        return;
    m_prQueryInFlight = branch;

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(QDir::currentPath());
    proc->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, branch](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_prQueryInFlight == branch)
            m_prQueryInFlight.clear();
        int number = -1;
        if (exitCode == 0) {
            QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
            bool ok = false;
            int parsed = out.toInt(&ok);
            if (ok && parsed > 0)
                number = parsed;
        }
        int prev = m_prNumberByBranch.value(branch, 0);
        m_prNumberByBranch.insert(branch, number);
        if (prev != number && readGitBranch(QDir::currentPath()) == branch)
            updateWindowTitle();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, branch](QProcess::ProcessError) {
        proc->deleteLater();
        if (m_prQueryInFlight == branch)
            m_prQueryInFlight.clear();
        m_prNumberByBranch.insert(branch, -1);
    });
    proc->start("gh", {"pr", "list", "--state", "open", "--head", branch,
                       "--json", "number", "--jq", ".[0].number"});
}

void MainWindow::updateWindowTitle()
{
    QString prefix = projectTitlePrefix();
    int idx = m_activeGroup ? m_activeGroup->currentIndex() : -1;
    if (idx < 0)
        setWindowTitle(prefix);
    else
        setWindowTitle(prefix + " — " + m_activeGroup->tabText(idx));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(projectTitlePrefix());
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

    m_fileModel = new IgnoreAwareModel(this);
    m_fileModel->setIconProvider(new FileIconProvider());
    m_fileModel->setFilter(m_fileModel->filter() | QDir::Hidden);
    m_fileModel->setRootPath(QDir::currentPath());
    loadIgnoredFromSettings();

    m_treeView = new QTreeView;
    m_treeView->setModel(m_fileModel);
    m_treeView->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_treeView->hideColumn(1); // size
    m_treeView->hideColumn(2); // type
    m_treeView->hideColumn(3); // date modified
    m_treeView->header()->hide();
    {
        QFont tf = m_treeView->font();
        int saved = m_settings->valueInt("tree_font_size", 11);
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


    m_pathLabel = new QLabel;
    m_pathLabel->setContentsMargins(6, 2, 6, 2);
    m_pathLabel->setStyleSheet("color: #808080; font-size: 11px;");
    m_pathLabel->hide();

    m_mdPreview = new QTextBrowser;
    m_mdPreview->setOpenExternalLinks(false);
    m_mdPreview->setOpenLinks(false);
    connect(m_mdPreview, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
        const QString scheme = url.scheme().toLower();
        if (scheme != "http" && scheme != "https" && scheme != "mailto")
            return;
        // QDesktopServices::openUrl inherits stderr, so browser chatter
        // ("Opening in existing browser session.") leaks into our terminal.
        // Launch xdg-open with nulled stdio instead.
        QProcess proc;
        proc.setProgram("xdg-open");
        proc.setArguments({url.toString()});
        proc.setStandardOutputFile(QProcess::nullDevice());
        proc.setStandardErrorFile(QProcess::nullDevice());
        if (!proc.startDetached())
            QDesktopServices::openUrl(url);
    });
    connect(m_mdPreview->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::syncEditorFromPreview);
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
    m_treeView->installEventFilter(this);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &MainWindow::showTreeContextMenu);

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

    auto *replaceShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_R), this);
    connect(replaceShortcut, &QShortcut::activated, this, &MainWindow::showSearchReplace);

    auto *fileSearchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N), this);
    connect(fileSearchShortcut, &QShortcut::activated, this, &MainWindow::showFileSearch);

    auto *gotoLineShortcut = new QShortcut(QKeySequence(Qt::Key_F7), this);
    connect(gotoLineShortcut, &QShortcut::activated, this, &MainWindow::gotoLine);

    auto *zoomIn1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
    auto *zoomIn2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
    auto *zoomOut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
    connect(zoomIn1, &QShortcut::activated, this, [this]() { onEditorZoom(+1); });
    connect(zoomIn2, &QShortcut::activated, this, [this]() { onEditorZoom(+1); });
    connect(zoomOut, &QShortcut::activated, this, [this]() { onEditorZoom(-1); });

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

    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &MainWindow::onFileChangedOnDisk);

    {
        QString headPath = QDir::currentPath() + "/.git/HEAD";
        if (QFile::exists(headPath)) {
            auto *gitWatcher = new QFileSystemWatcher(this);
            gitWatcher->addPath(headPath);
            connect(gitWatcher, &QFileSystemWatcher::fileChanged,
                    this, [this, gitWatcher, headPath](const QString &) {
                // Some tools replace .git/HEAD on write; re-add the watch.
                if (!gitWatcher->files().contains(headPath)
                    && QFile::exists(headPath))
                    gitWatcher->addPath(headPath);
                updateWindowTitle();
                QString b = readGitBranch(QDir::currentPath());
                if (!b.isEmpty() && !m_prNumberByBranch.contains(b))
                    queryPrForBranch(b);
            });
            QString b = readGitBranch(QDir::currentPath());
            if (!b.isEmpty())
                queryPrForBranch(b);
        }
    }

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
    if (obj == m_treeView && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            QModelIndex idx = m_treeView->currentIndex();
            if (idx.isValid()) {
                if (m_fileModel->isDir(idx)) {
                    m_treeView->setExpanded(idx, !m_treeView->isExpanded(idx));
                } else {
                    openFile(idx);
                }
                return true;
            }
        }
    }

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

void MainWindow::onFileChangedOnDisk(const QString &path)
{
    // Some editors replace the file on save; the watcher may stop watching
    // it. Re-add (deferred so the FS settles).
    QTimer::singleShot(0, this, [this, path]() {
        if (m_fileWatcher && !m_fileWatcher->files().contains(path)
            && QFile::exists(path)) {
            m_fileWatcher->addPath(path);
        }
    });

    if (!QFile::exists(path)) {
        // Could be a true delete, or an atomic-replace save mid-rename.
        // Re-check after a short delay; if still gone, close the tabs.
        QTimer::singleShot(250, this, [this, path]() {
            if (QFile::exists(path))
                return;
            QList<EditorGroup *> emptied;
            QSet<QTextDocument *> docsTouched;
            for (EditorGroup *g : m_groups) {
                int idx = g->indexOfPath(path);
                if (idx < 0) continue;
                if (auto *e = qobject_cast<CodeEditor *>(g->widget(idx)))
                    docsTouched.insert(e->document());
                g->removeTab(idx);
                if (g->count() == 0)
                    emptied.append(g);
            }
            if (m_fileWatcher && m_fileWatcher->files().contains(path))
                m_fileWatcher->removePath(path);
            for (QTextDocument *doc : docsTouched) {
                bool stillUsed = false;
                for (EditorGroup *g : m_groups) {
                    for (int i = 0; i < g->count() && !stillUsed; ++i) {
                        auto *e = qobject_cast<CodeEditor *>(g->widget(i));
                        if (e && e->document() == doc)
                            stillUsed = true;
                    }
                    if (stillUsed) break;
                }
                if (!stillUsed)
                    doc->deleteLater();
            }
            for (EditorGroup *g : emptied) {
                if (m_groups.size() > 1)
                    removeGroup(g);
            }
            statusBar()->showMessage(
                QString("Deleted on disk: %1").arg(QFileInfo(path).fileName()),
                3000);
        });
        return;
    }

    // Collect all editors in any group that have this path open
    struct State { CodeEditor *editor; int line; int col; int scroll; };
    QList<State> states;
    for (EditorGroup *g : m_groups) {
        int idx = g->indexOfPath(path);
        if (idx < 0) continue;
        if (auto *e = qobject_cast<CodeEditor *>(g->widget(idx))) {
            states.append({e,
                           e->textCursor().blockNumber(),
                           e->textCursor().columnNumber(),
                           e->verticalScrollBar()->value()});
        }
    }
    if (states.isEmpty())
        return;

    // Read disk content
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QString diskContent = QTextStream(&f).readAll();

    QTextDocument *doc = states.first().editor->document();
    if (doc->toPlainText() == diskContent)
        return; // matches what we already have (probably our own save)

    if (doc->isModified()) {
        QString name = QFileInfo(path).fileName();
        auto answer = QMessageBox::question(this,
            "File Changed on Disk",
            QString("%1 has been modified outside the editor.\n\n"
                    "You have unsaved changes. Reload from disk and discard them?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    doc->setPlainText(diskContent);
    doc->setModified(false);

    for (const State &s : states) {
        QTextBlock block = s.editor->document()->findBlockByNumber(s.line);
        if (block.isValid()) {
            QTextCursor c(block);
            c.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                           qMin(s.col, block.length() - 1));
            s.editor->setTextCursor(c);
        }
        int scroll = s.scroll;
        CodeEditor *e = s.editor;
        QTimer::singleShot(0, e, [e, scroll]() {
            e->verticalScrollBar()->setValue(scroll);
        });
    }
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
        updateWindowTitle();
        m_pathLabel->hide();
    } else {
        updateWindowTitle();

        QString filePath = tabFilePath(index);
        QDir root(QDir::currentPath());
        QString relative = root.relativeFilePath(filePath);
        m_pathLabel->setText(relative.replace("/", "  >  "));
        m_pathLabel->show();
    }

    applyMarkdownMode();
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate)
        saveAll();

    if (event->type() == QEvent::WindowActivate) {
        QString b = readGitBranch(QDir::currentPath());
        if (!b.isEmpty())
            queryPrForBranch(b);
    }

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
    QString suffix = QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix();
    if (isMarkdownFile(suffix) && m_settings->value("markdown_view_mode") == "preview")
        setMarkdownMode("split");
    if (m_activeGroup)
        m_activeGroup->activateSearch();
}

void MainWindow::showSearchReplace()
{
    auto *editor = currentEditor();
    if (!editor)
        return;
    QString suffix = QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix();
    if (isMarkdownFile(suffix) && m_settings->value("markdown_view_mode") == "preview")
        setMarkdownMode("split");
    if (m_activeGroup)
        m_activeGroup->activateSearchReplace();
}

void MainWindow::showFileSearch()
{
    FileSearchDialog dialog(QDir::currentPath(), m_ignoredAbsolute, this);
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
    QString closingPath = tabFilePath(index);
    QWidget *w = m_activeGroup->widget(index);
    QTextDocument *doc = nullptr;
    if (auto *e = qobject_cast<CodeEditor *>(w))
        doc = e->document();

    m_activeGroup->removeTab(index);

    if (!closingPath.isEmpty() && !findGroupForPath(closingPath)
        && m_fileWatcher && m_fileWatcher->files().contains(closingPath))
        m_fileWatcher->removePath(closingPath);

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
        removeGroup(m_activeGroup);
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
    QAction *moveAction = menu.addAction("Move to Other Split");
    QAction *showAction = menu.addAction("Show in Other Split");
    QAction *chosen = menu.exec(bar->mapToGlobal(pos));
    if (chosen == nullptr)
        return;

    auto otherGroupOf = [this](EditorGroup *src) -> EditorGroup * {
        for (EditorGroup *g : m_groups)
            if (g != src) return g;
        return nullptr;
    };

    if (chosen == copyAction) {
        QApplication::clipboard()->setText(path);
    } else if (chosen == moveAction) {
        EditorGroup *src = m_activeGroup;
        if (m_groups.size() == 1)
            splitRight();
        EditorGroup *dst = otherGroupOf(src);
        if (!dst)
            return;
        int srcIdx = src->indexOfPath(path);
        QWidget *w = (srcIdx >= 0) ? src->widget(srcIdx) : nullptr;
        if (!w)
            return;
        QString label = src->tabText(srcIdx);
        src->tabBar()->removeTab(srcIdx);
        src->pageStack()->removeWidget(w);
        int newIdx = dst->addTab(w, label);
        if (m_activeGroup) m_activeGroup->setActiveLook(false);
        m_activeGroup = dst;
        dst->setActiveLook(true);
        dst->setCurrentIndex(newIdx);
    } else if (chosen == showAction) {
        EditorGroup *src = m_activeGroup;
        int srcIdx = src->indexOfPath(path);
        auto *srcEditor = qobject_cast<CodeEditor *>(
            srcIdx >= 0 ? src->widget(srcIdx) : nullptr);
        if (!srcEditor)
            return;
        QTextDocument *doc = srcEditor->document();
        doc->setParent(this);

        if (m_groups.size() == 1)
            splitRight();
        EditorGroup *dst = otherGroupOf(src);
        if (!dst)
            return;

        auto *editor = new CodeEditor;
        editor->setDocument(doc);
        editor->setProperty("filePath", path);
        QFont f = editor->font();
        f.setPointSize(m_settings->valueInt("editor_font_size", 12));
        editor->setFont(f);
        connect(editor, &CodeEditor::textChanged, this, &MainWindow::onEditorModified);
        connect(editor, &CodeEditor::zoomRequested, this, &MainWindow::onEditorZoom);
        connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
                this, &MainWindow::syncPreviewFromEditor);
        connect(editor, &CodeEditor::cursorPositionChanged,
                this, &MainWindow::syncPreviewFromEditor);

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

void MainWindow::gotoLine()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    const int total = editor->document()->blockCount();
    const int current = editor->textCursor().blockNumber() + 1;
    bool ok = false;
    int line = QInputDialog::getInt(this, "Go to Line",
                                    QString("Line number (1 - %1):").arg(total),
                                    current, 1, total, 1, &ok);
    if (!ok)
        return;

    pushCurrentLocation();
    m_forwardStack.clear();

    QTextCursor cursor(editor->document()->findBlockByNumber(line - 1));
    editor->setTextCursor(cursor);
    editor->centerCursor();
    editor->setFocus();
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

    if (isImageFile(QFileInfo(path).suffix())) {
        auto *viewer = new ImageViewer;
        if (!viewer->load(path)) {
            delete viewer;
            statusBar()->showMessage(
                QString("Cannot display image: %1").arg(QFileInfo(path).fileName()),
                3000);
            return;
        }
        viewer->setProperty("filePath", path);
        QString name = QFileInfo(path).fileName();
        int index = m_activeGroup->addTab(viewer, name);
        m_activeGroup->setCurrentIndex(index);
        if (m_fileWatcher && !m_fileWatcher->files().contains(path))
            m_fileWatcher->addPath(path);
        return;
    }

    if (isPdfFile(QFileInfo(path).suffix())) {
        auto *viewer = new PdfViewer;
        if (!viewer->load(path)) {
            delete viewer;
            statusBar()->showMessage(
                QString("Cannot display PDF: %1").arg(QFileInfo(path).fileName()),
                3000);
            return;
        }
        viewer->setProperty("filePath", path);
        QString name = QFileInfo(path).fileName();
        int index = m_activeGroup->addTab(viewer, name);
        m_activeGroup->setCurrentIndex(index);
        if (m_fileWatcher && !m_fileWatcher->files().contains(path))
            m_fileWatcher->addPath(path);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString content = in.readAll();

    auto *editor = new CodeEditor;
    QFont f = editor->font();
    f.setPointSize(m_settings->valueInt("editor_font_size", 12));
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
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MainWindow::syncPreviewFromEditor);
    connect(editor, &CodeEditor::cursorPositionChanged,
            this, &MainWindow::syncPreviewFromEditor);

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
    } else if (isHtmlFile(suffix)) {
        new HtmlHighlighter(editor->document());
    } else if (isCssFile(suffix)) {
        new CssHighlighter(editor->document());
    } else if (isTsFile(suffix)) {
        new JsHighlighter(editor->document(), JsHighlighter::TypeScript);
    } else if (isJsFile(suffix)) {
        new JsHighlighter(editor->document(), JsHighlighter::JavaScript);
    } else if (isXmlFile(suffix)) {
        new HtmlHighlighter(editor->document());
    } else if (isDockerfile(fileName, suffix)) {
        new DockerfileHighlighter(editor->document());
    } else if (isTerraformFile(suffix)) {
        new TerraformHighlighter(editor->document());
    } else if (isGoFile(fileName, suffix)) {
        new GoHighlighter(editor->document());
        ensureGoLsp();
        if (m_goLsp && m_goLsp->isRunning()) {
            m_goLsp->didOpen(path, content, "go");
            QTimer::singleShot(1000, this, [this, path, editor]() {
                requestSemanticHighlight(path, editor);
            });
        }
    } else if (isGherkinFile(suffix)) {
        new GherkinHighlighter(editor->document());
    } else if (isConfigFile(fileName, suffix)) {
        new ConfigHighlighter(editor->document());
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

    if (m_fileWatcher && !m_fileWatcher->files().contains(path))
        m_fileWatcher->addPath(path);

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
    removeGroup(m_groups.last());
}

void MainWindow::removeGroup(EditorGroup *group)
{
    if (!group || m_groups.size() <= 1 || !m_groups.contains(group))
        return;

    // If this group's tab bar is currently on loan to the markdown preview
    // holder, return it before destruction so it gets cleaned up with the
    // group instead of orphaned in the holder.
    if (m_previewBorrowedFrom == group) {
        qobject_cast<QBoxLayout *>(m_previewTabHolder->layout())
            ->removeWidget(group->tabBar());
        group->attachTabBar();
        m_previewBorrowedFrom = nullptr;
        m_previewTabHolder->hide();
        m_groupSplitter->show();
    }

    EditorGroup *survivor = nullptr;
    for (EditorGroup *g : m_groups) {
        if (g != group) { survivor = g; break; }
    }

    while (group->count() > 0)
        group->removeTab(0);

    m_groups.removeOne(group);
    group->setParent(nullptr);
    group->deleteLater();

    if (survivor) {
        m_activeGroup = survivor;
        survivor->setActiveLook(true);
        survivor->setUnderlineVisible(false);
        // Make sure md mode / preview state / path bar all reflect the
        // survivor's active tab, since no currentChanged signal fired.
        onTabChanged(survivor->currentIndex());
    }
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

void MainWindow::ensureGoLsp()
{
    if (m_goLsp && m_goLsp->isRunning())
        return;
    if (m_goLspPromptShown)
        return;

    QString rootPath = QDir::currentPath();
    auto startWith = [this, rootPath](const QString &binPath) {
        // Use the nearest go.mod ancestor as the workspace root if there is
        // one; otherwise fall back to the application's working directory.
        QString workRoot = findGoModRoot(rootPath);
        if (workRoot.isEmpty())
            workRoot = rootPath;
        m_goLsp = new LspClient(binPath, {}, workRoot, this);
        m_goLsp->start();
    };

    QString gopls = findGopls(rootPath, m_settings);
    if (!gopls.isEmpty()) {
        startWith(gopls);
        return;
    }

    QString predicted;
    {
        QString gobin = qEnvironmentVariable("GOBIN");
        QString gopath = qEnvironmentVariable("GOPATH");
        QString home = qEnvironmentVariable("HOME");
        if (!gobin.isEmpty())
            predicted = gobin + "/gopls";
        else if (!gopath.isEmpty())
            predicted = gopath + "/bin/gopls";
        else if (!home.isEmpty())
            predicted = home + "/go/bin/gopls";
    }

    QMessageBox box(this);
    box.setWindowTitle("Go Language Server");
    QString msg = "No Go language server (gopls) was found.\n\n"
                  "You can install it with `go install golang.org/x/tools/gopls@latest`,\n"
                  "browse for an existing binary, or skip and use highlighting only.";
    if (!predicted.isEmpty())
        msg += QString("\n\nInstall location: %1").arg(predicted);
    box.setText(msg);
    box.setMinimumWidth(520);
    QAbstractButton *installBtn = box.addButton("  Install  ", QMessageBox::ActionRole);
    QAbstractButton *browseBtn  = box.addButton("  Browse...  ", QMessageBox::ActionRole);
    QAbstractButton *skipBtn    = box.addButton("  Skip  ",   QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == skipBtn) {
        m_goLspPromptShown = true;
        return;
    }

    if (box.clickedButton() == installBtn) {
        m_goLspPromptShown = true;
        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(rootPath);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, startWith](int exitCode, QProcess::ExitStatus) {
            proc->deleteLater();
            if (exitCode != 0) {
                statusBar()->showMessage("gopls install failed (is `go` on PATH?)", 5000);
                return;
            }
            QString gopls = findGopls(QDir::currentPath(), m_settings);
            if (gopls.isEmpty()) {
                statusBar()->showMessage("gopls installed but binary not found", 5000);
                return;
            }
            startWith(gopls);
            // Send didOpen for any already-open Go files
            for (EditorGroup *g : m_groups) {
                for (int i = 0; i < g->count(); ++i) {
                    QWidget *w = g->widget(i);
                    QString p = w ? w->property("filePath").toString() : QString();
                    if (QFileInfo(p).suffix() == "go") {
                        auto *ed = qobject_cast<CodeEditor *>(w);
                        if (ed)
                            m_goLsp->didOpen(p, ed->toPlainText(), "go");
                    }
                }
            }
        });
        proc->start("go", {"install", "golang.org/x/tools/gopls@latest"});
        return;
    }

    if (box.clickedButton() == browseBtn) {
        QString file = QFileDialog::getOpenFileName(
            this, "Select gopls binary",
            qEnvironmentVariable("HOME") + "/go/bin");
        if (file.isEmpty())
            return;
        if (!QFileInfo(file).isExecutable()) {
            QMessageBox::warning(this, "Not executable",
                "The selected file is not executable.");
            return;
        }
        m_goLspPromptShown = true;
        m_settings->setValue("gopls_path", file);
        startWith(file);
    }
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
        QString pip = dir + "/bin/pip";
        auto startLsp = [this, path, dir, root]() {
            m_pyLspPromptShown = true;
            m_settings->setValue("python_venv_path", dir);
            m_pyLsp = new LspClient(path, {}, root, this);
            QStringList pythonPaths = findPythonPath(root);
            if (!pythonPaths.isEmpty())
                m_pyLsp->setEnvironment({"PYTHONPATH=" + pythonPaths.join(":")});
            m_pyLsp->start();
            for (EditorGroup *g : m_groups) {
                for (int i = 0; i < g->count(); ++i) {
                    QWidget *w = g->widget(i);
                    QString fp = w ? w->property("filePath").toString() : QString();
                    if (isPythonFile(QFileInfo(fp).suffix())) {
                        auto *editor = qobject_cast<CodeEditor *>(w);
                        if (editor)
                            m_pyLsp->didOpen(fp, editor->toPlainText(), "python");
                    }
                }
            }
        };

        if (QFile::exists(path)) {
            startLsp();
            return;
        }

        if (!QFile::exists(pip)) {
            QMessageBox::warning(this, "Not a venv",
                "Could not find `pip` at:\n" + pip + "\n\n"
                "This doesn't look like a Python virtual environment.");
            return;
        }

        QMessageBox install(this);
        install.setWindowTitle("Install jedi-language-server?");
        install.setText("jedi-language-server is not installed in this venv.\n\n"
                        "Install it now with:\n  " + pip + " install jedi-language-server");
        QAbstractButton *yes = install.addButton("  Install  ", QMessageBox::AcceptRole);
        install.addButton("  Cancel  ", QMessageBox::RejectRole);
        install.exec();
        if (install.clickedButton() != yes)
            return;

        m_pyLspPromptShown = true;
        auto *proc = new QProcess(this);
        proc->setWorkingDirectory(root);
        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, path, startLsp](int exitCode, QProcess::ExitStatus) {
            proc->deleteLater();
            if (exitCode != 0 || !QFile::exists(path)) {
                statusBar()->showMessage("jedi-language-server install failed", 5000);
                return;
            }
            startLsp();
        });
        proc->start(pip, {"install", "jedi-language-server"});
    }
}

LspClient *MainWindow::lspForFile(const QString &path) const
{
    QString suffix = QFileInfo(path).suffix();
    if (isCppFile(suffix))
        return m_cppLsp;
    if (isPythonFile(suffix))
        return m_pyLsp;
    if (suffix == "go")
        return m_goLsp;
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

bool MainWindow::isHtmlFile(const QString &suffix)
{
    return suffix == "html" || suffix == "htm" || suffix == "xhtml";
}

bool MainWindow::isCssFile(const QString &suffix)
{
    return suffix == "css" || suffix == "scss" || suffix == "sass" || suffix == "less";
}

bool MainWindow::isJsFile(const QString &suffix)
{
    return suffix == "js" || suffix == "mjs" || suffix == "cjs" || suffix == "jsx";
}

bool MainWindow::isTsFile(const QString &suffix)
{
    return suffix == "ts" || suffix == "tsx";
}

bool MainWindow::isXmlFile(const QString &suffix)
{
    return suffix == "xml" || suffix == "svg" || suffix == "xsd"
        || suffix == "xsl" || suffix == "plist";
}

bool MainWindow::isConfigFile(const QString &fileName, const QString &suffix)
{
    if (suffix == "env" || suffix == "toml" || suffix == "ini"
        || suffix == "cfg" || suffix == "conf" || suffix == "properties")
        return true;
    if (fileName.startsWith(".env"))
        return true;
    if (fileName.compare(".gitignore", Qt::CaseInsensitive) == 0
        || fileName.compare(".gitattributes", Qt::CaseInsensitive) == 0
        || fileName.compare(".dockerignore", Qt::CaseInsensitive) == 0
        || fileName.compare(".editorconfig", Qt::CaseInsensitive) == 0
        || fileName.compare("go.mod", Qt::CaseInsensitive) == 0
        || fileName.compare("go.sum", Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

bool MainWindow::isGherkinFile(const QString &suffix)
{
    return suffix == "feature";
}

bool MainWindow::isDockerfile(const QString &fileName, const QString &suffix)
{
    return fileName.compare("Dockerfile", Qt::CaseInsensitive) == 0
        || fileName.startsWith("Dockerfile.", Qt::CaseInsensitive)
        || fileName.endsWith(".Dockerfile", Qt::CaseInsensitive)
        || suffix.compare("dockerfile", Qt::CaseInsensitive) == 0;
}

bool MainWindow::isTerraformFile(const QString &suffix)
{
    return suffix == "tf" || suffix == "tfvars" || suffix == "hcl";
}

bool MainWindow::isGoFile(const QString &fileName, const QString &suffix)
{
    Q_UNUSED(fileName);
    return suffix == "go";
}

bool MainWindow::isImageFile(const QString &suffix)
{
    QString s = suffix.toLower();
    return s == "png" || s == "svg" || s == "jpg" || s == "jpeg"
        || s == "gif" || s == "bmp" || s == "webp";
}

bool MainWindow::isPdfFile(const QString &suffix)
{
    return suffix.compare("pdf", Qt::CaseInsensitive) == 0;
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

void MainWindow::showTreeContextMenu(const QPoint &pos)
{
    QModelIndex idx = m_treeView->indexAt(pos);
    if (!idx.isValid())
        return;
    QString path = m_fileModel->filePath(idx);
    bool isDir = m_fileModel->isDir(idx);

    QMenu menu(this);
    QAction *openAct = isDir ? nullptr : menu.addAction("Open");
    QAction *splitAct = isDir ? nullptr : menu.addAction("Open in Other Split");
    QAction *copyAct = menu.addAction("Copy Path");
    QAction *ignoreAct = nullptr;
    if (isDir) {
        menu.addSeparator();
        ignoreAct = menu.addAction(m_ignoredAbsolute.contains(path)
                                       ? "Stop Ignoring"
                                       : "Ignore Directory");
    }

    QAction *chosen = menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    if (chosen == nullptr)
        return;
    if (chosen == openAct)
        loadFile(path);
    else if (chosen == splitAct)
        openInOtherSplit(path);
    else if (chosen == copyAct)
        QApplication::clipboard()->setText(path);
    else if (chosen == ignoreAct)
        toggleIgnored(path);
}

void MainWindow::loadIgnoredFromSettings()
{
    m_ignoredAbsolute.clear();
    QString root = QDir::currentPath();
    QDir rootDir(root);
    for (const QString &rel : m_settings->valueList("ignored_dirs"))
        m_ignoredAbsolute.insert(QDir::cleanPath(rootDir.absoluteFilePath(rel)));
    if (m_fileModel)
        m_fileModel->setIgnoredPaths(m_ignoredAbsolute);
}

void MainWindow::saveIgnoredToSettings()
{
    QString root = QDir::currentPath();
    QDir rootDir(root);
    QStringList rels;
    for (const QString &abs : m_ignoredAbsolute) {
        QString rel = rootDir.relativeFilePath(abs);
        if (rel.startsWith("../") || rel == ".." || rel.isEmpty())
            continue;
        rels.append(rel);
    }
    rels.sort();
    m_settings->setValueList("ignored_dirs", rels);
}

void MainWindow::toggleIgnored(const QString &absolutePath)
{
    QString clean = QDir::cleanPath(absolutePath);
    if (m_ignoredAbsolute.contains(clean))
        m_ignoredAbsolute.remove(clean);
    else
        m_ignoredAbsolute.insert(clean);
    m_fileModel->setIgnoredPaths(m_ignoredAbsolute);
    saveIgnoredToSettings();
}

void MainWindow::openInOtherSplit(const QString &path)
{
    if (m_groups.size() == 1) {
        splitRight();
        // After splitRight, the new group is active and empty.
    } else {
        for (EditorGroup *g : m_groups) {
            if (g != m_activeGroup) {
                if (m_activeGroup) m_activeGroup->setActiveLook(false);
                m_activeGroup = g;
                g->setActiveLook(true);
                break;
            }
        }
    }
    loadFile(path);
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

void MainWindow::rebuildMarkdownScrollMap()
{
    m_mdScrollAnchors.clear();
    auto *editor = currentEditor();
    if (!editor) return;

    // Collect source heading lines.
    QList<int> srcHeadingLines;
    const QString src = editor->toPlainText();
    int line = 0;
    int pos = 0;
    bool inFence = false;
    while (pos <= src.size()) {
        int eol = src.indexOf('\n', pos);
        if (eol == -1) eol = src.size();
        QString s = src.mid(pos, eol - pos);
        QString stripped = s.trimmed();
        if (stripped.startsWith("```") || stripped.startsWith("~~~"))
            inFence = !inFence;
        else if (!inFence && s.startsWith('#')) {
            int i = 0;
            while (i < s.size() && s[i] == '#') i++;
            if (i >= 1 && i <= 6 && (i == s.size() || s[i] == ' '))
                srcHeadingLines.append(line);
        }
        if (eol == src.size()) break;
        pos = eol + 1;
        line++;
    }

    // Collect rendered heading Y positions.
    QTextDocument *doc = m_mdPreview->document();
    auto *layout = doc->documentLayout();
    QList<int> prevHeadingY;
    for (QTextBlock b = doc->begin(); b != doc->end(); b = b.next()) {
        if (b.blockFormat().headingLevel() > 0)
            prevHeadingY.append(int(layout->blockBoundingRect(b).top()));
    }

    int n = qMin(srcHeadingLines.size(), prevHeadingY.size());
    for (int i = 0; i < n; ++i)
        m_mdScrollAnchors.append({srcHeadingLines[i], prevHeadingY[i]});
}

void MainWindow::syncPreviewFromEditor()
{
    if (m_syncingScroll) return;
    if (m_settings->value("markdown_view_mode") != "split") return;
    if (!m_activeGroup) return;
    QString suffix = QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix();
    if (!isMarkdownFile(suffix)) return;
    auto *editor = currentEditor();
    if (!editor) return;
    QScrollBar *pb = m_mdPreview->verticalScrollBar();
    if (pb->maximum() <= 0) return;

    int srcLine = editor->cursorForPosition(QPoint(0, 0)).block().blockNumber();
    int targetY;
    if (m_mdScrollAnchors.isEmpty()) {
        QScrollBar *eb = editor->verticalScrollBar();
        double frac = eb->maximum() > 0 ? double(eb->value()) / eb->maximum() : 0.0;
        targetY = int(frac * pb->maximum());
    } else {
        int before = -1, after = -1;
        for (int i = 0; i < m_mdScrollAnchors.size(); ++i) {
            if (m_mdScrollAnchors[i].sourceLine <= srcLine) before = i;
            else { after = i; break; }
        }
        if (before == -1 && after != -1) {
            int span = m_mdScrollAnchors[after].sourceLine;
            double f = span > 0 ? double(srcLine) / span : 0.0;
            targetY = int(f * m_mdScrollAnchors[after].previewY);
        } else if (after == -1) {
            int lastSrc = m_mdScrollAnchors[before].sourceLine;
            int lastY = m_mdScrollAnchors[before].previewY;
            int srcEnd = editor->document()->blockCount();
            int yEnd = pb->maximum() + pb->pageStep();
            int span = srcEnd - lastSrc;
            double f = span > 0 ? double(srcLine - lastSrc) / span : 0.0;
            targetY = lastY + int(f * (yEnd - lastY));
        } else {
            int srcSpan = m_mdScrollAnchors[after].sourceLine - m_mdScrollAnchors[before].sourceLine;
            int ySpan = m_mdScrollAnchors[after].previewY - m_mdScrollAnchors[before].previewY;
            double f = srcSpan > 0 ? double(srcLine - m_mdScrollAnchors[before].sourceLine) / srcSpan : 0.0;
            targetY = m_mdScrollAnchors[before].previewY + int(f * ySpan);
        }
    }
    m_syncingScroll = true;
    pb->setValue(qBound(0, targetY, pb->maximum()));
    m_syncingScroll = false;
}

void MainWindow::syncEditorFromPreview()
{
    if (m_syncingScroll) return;
    if (m_settings->value("markdown_view_mode") != "split") return;
    if (!m_activeGroup) return;
    QString suffix = QFileInfo(tabFilePath(m_activeGroup->currentIndex())).suffix();
    if (!isMarkdownFile(suffix)) return;
    auto *editor = currentEditor();
    if (!editor) return;
    QScrollBar *eb = editor->verticalScrollBar();
    QScrollBar *pb = m_mdPreview->verticalScrollBar();
    if (eb->maximum() <= 0) return;

    int prevY = pb->value();
    int targetLine;
    if (m_mdScrollAnchors.isEmpty()) {
        double frac = pb->maximum() > 0 ? double(prevY) / pb->maximum() : 0.0;
        targetLine = int(frac * editor->document()->blockCount());
    } else {
        int before = -1, after = -1;
        for (int i = 0; i < m_mdScrollAnchors.size(); ++i) {
            if (m_mdScrollAnchors[i].previewY <= prevY) before = i;
            else { after = i; break; }
        }
        if (before == -1 && after != -1) {
            double f = m_mdScrollAnchors[after].previewY > 0
                ? double(prevY) / m_mdScrollAnchors[after].previewY : 0.0;
            targetLine = int(f * m_mdScrollAnchors[after].sourceLine);
        } else if (after == -1) {
            int lastSrc = m_mdScrollAnchors[before].sourceLine;
            int lastY = m_mdScrollAnchors[before].previewY;
            int srcEnd = editor->document()->blockCount();
            int yEnd = pb->maximum() + pb->pageStep();
            int ySpan = yEnd - lastY;
            double f = ySpan > 0 ? double(prevY - lastY) / ySpan : 0.0;
            targetLine = lastSrc + int(f * (srcEnd - lastSrc));
        } else {
            int ySpan = m_mdScrollAnchors[after].previewY - m_mdScrollAnchors[before].previewY;
            int srcSpan = m_mdScrollAnchors[after].sourceLine - m_mdScrollAnchors[before].sourceLine;
            double f = ySpan > 0 ? double(prevY - m_mdScrollAnchors[before].previewY) / ySpan : 0.0;
            targetLine = m_mdScrollAnchors[before].sourceLine + int(f * srcSpan);
        }
    }
    m_syncingScroll = true;
    eb->setValue(qBound(0, targetLine, eb->maximum()));
    m_syncingScroll = false;
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

    rebuildMarkdownScrollMap();
}
