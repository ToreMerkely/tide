#include "MainWindow.h"
#include <QMenuBar>
#include <QApplication>
#include <QSplitter>
#include <QTreeView>
#include <QPlainTextEdit>
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

    auto *splitter = new QSplitter;
    splitter->addWidget(m_treeView);
    splitter->addWidget(m_tabWidget);
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
        return;
    }
    setWindowTitle("sild - " + m_tabWidget->tabText(index));
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

void MainWindow::loadFile(const QString &path)
{
    // If already open, switch to that tab
    if (m_openFiles.contains(path)) {
        m_tabWidget->setCurrentIndex(m_openFiles[path]);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);

    auto *editor = new QPlainTextEdit;
    editor->setPlainText(in.readAll());
    editor->setProperty("filePath", path);

    QString name = QFileInfo(path).fileName();
    int index = m_tabWidget->addTab(editor, name);
    m_openFiles[path] = index;
    m_tabWidget->setCurrentIndex(index);
}

QPlainTextEdit *MainWindow::currentEditor() const
{
    return qobject_cast<QPlainTextEdit *>(m_tabWidget->currentWidget());
}

QString MainWindow::tabFilePath(int index) const
{
    QWidget *widget = m_tabWidget->widget(index);
    if (!widget)
        return {};
    return widget->property("filePath").toString();
}
