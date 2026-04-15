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

    m_editor = new QPlainTextEdit;

    auto *splitter = new QSplitter;
    splitter->addWidget(m_treeView);
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    setCentralWidget(splitter);

    connect(m_treeView, &QTreeView::clicked, this, &MainWindow::openFile);
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
    if (m_currentFilePath.isEmpty())
        return;

    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << m_editor->toPlainText();
}

void MainWindow::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    m_currentFilePath = path;
    setWindowTitle("sild - " + QFileInfo(path).fileName());
}
