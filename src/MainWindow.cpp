#include "MainWindow.h"
#include "CppHighlighter.h"
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

    // Start LSP
    m_lsp = new LspClient(QDir::currentPath(), this);
    m_lsp->start();
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
    int index = m_tabWidget->currentIndex();
    if (index < 0)
        return;

    QString path = tabFilePath(index);
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << currentEditor()->toPlainText();
}

void MainWindow::onTabChanged(int index)
{
    if (index < 0) {
        setWindowTitle("sild");
        m_searchBar->setEditor(nullptr);
        return;
    }
    setWindowTitle("sild - " + m_tabWidget->tabText(index));
    m_searchBar->setEditor(currentEditor());
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

    QTextCursor cursor = editor->textCursor();
    int line = cursor.blockNumber();
    int column = cursor.columnNumber();

    // Send current content to clangd so it has the latest
    m_lsp->didChange(path, editor->toPlainText());

    m_lsp->gotoDefinition(path, line, column, [this](const QVector<LspLocation> &locations) {
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

    QString suffix = QFileInfo(path).suffix();
    if (suffix == "cpp" || suffix == "cxx" || suffix == "cc" ||
        suffix == "h" || suffix == "hpp" || suffix == "hxx" || suffix == "c") {
        new CppHighlighter(editor->document());
        m_lsp->didOpen(path, content);
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
