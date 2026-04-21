#include "FileSearchDialog.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QKeyEvent>

FileSearchDialog::FileSearchDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent)
    , m_rootPath(rootPath)
{
    setWindowTitle("Go to File");
    setMinimumSize(500, 400);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Search for file...");
    m_input->installEventFilter(this);

    m_list = new QListWidget;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(m_input);
    layout->addWidget(m_list);

    connect(m_input, &QLineEdit::textChanged, this, &FileSearchDialog::onTextChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &FileSearchDialog::onItemDoubleClicked);

    // Scan all files in project
    QDirIterator it(m_rootPath, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QDir root(m_rootPath);
    while (it.hasNext()) {
        it.next();
        QString relative = root.relativeFilePath(it.filePath());
        // Skip hidden dirs and build dir
        if (relative.startsWith(".") || relative.startsWith("build/"))
            continue;
        m_allFiles.append(relative);
    }
    m_allFiles.sort(Qt::CaseInsensitive);

    // Show all files initially
    m_list->addItems(m_allFiles);
    if (m_list->count() > 0)
        m_list->setCurrentRow(0);

    m_input->setFocus();
}

QString FileSearchDialog::selectedFile() const
{
    return m_selectedFile;
}

void FileSearchDialog::onTextChanged(const QString &text)
{
    m_list->clear();

    if (text.isEmpty()) {
        m_list->addItems(m_allFiles);
    } else {
        QString lower = text.toLower();
        for (const QString &file : m_allFiles) {
            // Fuzzy match: all characters of the query appear in order
            QString fileLower = file.toLower();
            int qi = 0;
            for (int fi = 0; fi < fileLower.size() && qi < lower.size(); ++fi) {
                if (fileLower[fi] == lower[qi])
                    ++qi;
            }
            if (qi == lower.size())
                m_list->addItem(file);
        }
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void FileSearchDialog::onItemDoubleClicked()
{
    auto *item = m_list->currentItem();
    if (item) {
        m_selectedFile = m_rootPath + "/" + item->text();
        accept();
    }
}

bool FileSearchDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Down) {
            int row = m_list->currentRow();
            if (row < m_list->count() - 1)
                m_list->setCurrentRow(row + 1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up) {
            int row = m_list->currentRow();
            if (row > 0)
                m_list->setCurrentRow(row - 1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            auto *item = m_list->currentItem();
            if (item) {
                m_selectedFile = m_rootPath + "/" + item->text();
                accept();
            }
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
