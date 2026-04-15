#include "MainWindow.h"
#include "CppHighlighter.h"
#include "PythonHighlighter.h"
#include "LspClient.h"
#include "SearchBar.h"
#include <QMenuBar>
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
#include <QTabWidget>
#include <QTabBar>
#include <QShortcut>
#include <QTextBlock>
#include <QVBoxLayout>
#include <QTimer>

static QString findPylsp(const QString &rootPath)
{
    // Look in .venv first
    QString venvPath = rootPath + "/.venv/bin/pylsp";
    if (QFile::exists(venvPath))
        return venvPath;

    // Try common venv names
    for (const QString &dir : {"venv", ".env", "env"}) {
        QString path = rootPath + "/" + dir + "/bin/pylsp";
        if (QFile::exists(path))
            return path;
    }

    // Fall back to system
    return "pylsp";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("sild");
    resize(1024, 768);

    QMenu *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Open...", QKeySequence::Open, this, &MainWindow::openFileDialog);
    fileMenu->addAction("&Save", QKeySequence::Save, this, &MainWindow::saveFile);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", QKeySequence::Quit, QApplication::instance(), &QApplication::quit);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setRootPath(QDir::currentPath());

    m_treeView = new QTreeView;
    m_treeView->setModel(m_fileModel);
    m_treeView->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_treeView->hideColumn(1); // size
    m_treeView->hideColumn(2); // type
    m_treeView->hideColumn(3); // date modified
    m_treeView->header()->hide();

    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);

    m_searchBar = new SearchBar;

    auto *rightPane = new QWidget;
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(m_searchBar);
    rightLayout->addWidget(m_tabWidget);

    auto *splitter = new QSplitter;
    splitter->addWidget(m_treeView);
    splitter->addWidget(rightPane);
    splitter->setSizes({200, 824});
    setCentralWidget(splitter);

    connect(m_treeView, &QTreeView::doubleClicked, this, &MainWindow::openFile);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);

    auto *prevTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), this);
    connect(prevTab, &QShortcut::activated, this, [this]() {
        int count = m_tabWidget->count();
        if (count > 1)
            m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() - 1 + count) % count);
    });

    auto *nextTab = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), this);
    connect(nextTab, &QShortcut::activated, this, [this]() {
        int count = m_tabWidget->count();
        if (count > 1)
            m_tabWidget->setCurrentIndex((m_tabWidget->currentIndex() + 1) % count);
    });

    auto *gotoDef = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this);
    connect(gotoDef, &QShortcut::activated, this, &MainWindow::gotoDefinition);

    auto *findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(findShortcut, &QShortcut::activated, this, &MainWindow::showSearch);

    // Start LSP servers
    QString root = QDir::currentPath();

    m_cppLsp = new LspClient("clangd", {"--compile-commands-dir=" + root}, root, this);
    m_cppLsp->start();

    m_pyLsp = new LspClient(findPylsp(root), {}, root, this);
    m_pyLsp->start();

    // Auto-save on idle
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(3000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::saveAll);
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
    saveTab(m_tabWidget->currentIndex());
}

void MainWindow::saveTab(int index)
{
    if (index < 0)
        return;

    QString path = tabFilePath(index);
    if (path.isEmpty())
        return;

    auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(index));
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
    for (int i = 0; i < m_tabWidget->count(); ++i)
        saveTab(i);
}

void MainWindow::onEditorModified()
{
    m_autoSaveTimer->start();
}

void MainWindow::onTabChanged(int index)
{
    // Save all before switching
    saveAll();

    if (index < 0) {
        setWindowTitle("sild");
        m_searchBar->setEditor(nullptr);
        return;
    }
    setWindowTitle("sild - " + m_tabWidget->tabText(index));
    m_searchBar->setEditor(currentEditor());
}

bool MainWindow::event(QEvent *event)
{
    if (event->type() == QEvent::WindowDeactivate)
        saveAll();
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

void MainWindow::closeTab(int index)
{
    saveTab(index);
    QString path = tabFilePath(index);
    m_openFiles.remove(path);

    QWidget *widget = m_tabWidget->widget(index);
    m_tabWidget->removeTab(index);
    delete widget;

    // Rebuild index map since tab indices shift after removal
    m_openFiles.clear();
    for (int i = 0; i < m_tabWidget->count(); ++i)
        m_openFiles[tabFilePath(i)] = i;
}

void MainWindow::gotoDefinition()
{
    auto *editor = currentEditor();
    if (!editor)
        return;

    QString path = tabFilePath(m_tabWidget->currentIndex());
    if (path.isEmpty())
        return;

    LspClient *lsp = lspForFile(path);
    if (!lsp || !lsp->isRunning())
        return;

    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber();
    int column = cursor.columnNumber();

    lsp->didChange(path, editor->toPlainText());

    lsp->gotoDefinition(path, line, column, [this](const QVector<LspLocation> &locations) {
        if (locations.isEmpty())
            return;
        const LspLocation &loc = locations.first();
        loadFile(loc.filePath, loc.line);
    });
}

void MainWindow::loadFile(const QString &path, int line)
{
    // If already open, switch to that tab
    if (m_openFiles.contains(path)) {
        m_tabWidget->setCurrentIndex(m_openFiles[path]);
        if (line >= 0) {
            auto *editor = currentEditor();
            QTextBlock block = editor->document()->findBlockByNumber(line);
            QTextCursor cursor(block);
            editor->setTextCursor(cursor);
            editor->centerCursor();
        }
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString content = in.readAll();

    auto *editor = new CodeEditor;
    editor->setPlainText(content);
    editor->setProperty("filePath", path);
    editor->document()->setModified(false);
    connect(editor, &CodeEditor::textChanged, this, &MainWindow::onEditorModified);

    QString suffix = QFileInfo(path).suffix();
    if (isCppFile(suffix)) {
        new CppHighlighter(editor->document());
        m_cppLsp->didOpen(path, content, "cpp");
    } else if (isPythonFile(suffix)) {
        new PythonHighlighter(editor->document());
        m_pyLsp->didOpen(path, content, "python");
    }

    QString name = QFileInfo(path).fileName();
    int index = m_tabWidget->addTab(editor, name);
    m_openFiles[path] = index;
    m_tabWidget->setCurrentIndex(index);

    if (line >= 0) {
        QTextBlock block = editor->document()->findBlockByNumber(line);
        QTextCursor cursor(block);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    }
}

CodeEditor *MainWindow::currentEditor() const
{
    return qobject_cast<CodeEditor *>(m_tabWidget->currentWidget());
}

QString MainWindow::tabFilePath(int index) const
{
    QWidget *widget = m_tabWidget->widget(index);
    if (!widget)
        return {};
    return widget->property("filePath").toString();
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
