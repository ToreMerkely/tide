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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("sild");
    resize(1024, 768);

    QMenu *fileMenu = menuBar()->addMenu("&File");
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
    m_editor->setReadOnly(true);

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

    QString path = m_fileModel->filePath(index);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    m_editor->setPlainText(in.readAll());
    setWindowTitle("sild - " + m_fileModel->fileName(index));
}
