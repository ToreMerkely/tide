#include "MainWindow.h"
#include "CppHighlighter.h"
#include "PythonHighlighter.h"
#include "LspClient.h"
#include "SearchBar.h"
#include "Settings.h"
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
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTimer>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QProgressDialog>

static QString findPyright(const QString &rootPath, Settings *settings)
{
    // Check saved setting first
    QString saved = settings->value("python_venv_path");
    if (!saved.isEmpty()) {
        QString path = saved + "/bin/pyright-langserver";
        if (QFile::exists(path))
            return path;
    }

    // Look in .venv
    QString venvPath = rootPath + "/.venv/bin/pyright-langserver";
    if (QFile::exists(venvPath))
        return venvPath;

    // Try common venv names
    for (const QString &dir : {"venv", ".env", "env"}) {
        QString path = rootPath + "/" + dir + "/bin/pyright-langserver";
        if (QFile::exists(path))
            return path;
    }

    // Check system PATH
    QProcess which;
    which.start("which", {"pyright-langserver"});
    if (which.waitForFinished(2000) && which.exitCode() == 0)
        return "pyright-langserver";

    return {};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("sild");
    resize(1024, 768);

    m_settings = new Settings(QDir::currentPath());

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
        ensurePythonLsp();
        if (m_pyLsp && m_pyLsp->isRunning())
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
    QString pyright = findPyright(root, m_settings);

    if (!pyright.isEmpty()) {
        m_pyLsp = new LspClient(pyright, {"--stdio"}, root, this);
        m_pyLsp->start();
        return;
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Python Language Server");
    msgBox.setText("No Python language server (pyright) was found.\n\n"
                   "You can create a new .venv with pyright installed,\n"
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
                proc->start(root + "/.venv/bin/pip", {"install", "pyright"});

            } else {
                proc->deleteLater();
                if (exitCode != 0)
                    return;

                QString pyrightPath = root + "/.venv/bin/pyright-langserver";
                m_pyLsp = new LspClient(pyrightPath, {"--stdio"}, root, this);
                m_pyLsp->start();

                // Send didOpen for any already-open Python files
                for (int i = 0; i < m_tabWidget->count(); ++i) {
                    QString path = tabFilePath(i);
                    if (isPythonFile(QFileInfo(path).suffix())) {
                        auto *editor = qobject_cast<CodeEditor *>(m_tabWidget->widget(i));
                        if (editor)
                            m_pyLsp->didOpen(path, editor->toPlainText(), "python");
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

        QString path = dir + "/bin/pyright-langserver";
        if (!QFile::exists(path)) {
            QMessageBox::warning(this, "Not found",
                "No pyright-langserver found at:\n" + path + "\n\n"
                "Make sure pyright is installed in that venv.");
            return;
        }

        m_pyLspPromptShown = true;
        m_settings->setValue("python_venv_path", dir);
        m_pyLsp = new LspClient(path, {"--stdio"}, root, this);
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
