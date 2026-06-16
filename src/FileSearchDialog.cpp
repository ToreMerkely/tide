#include "FileSearchDialog.h"
#include <QLineEdit>
#include <QListWidget>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QFileInfo>
#include <QTimer>
#include <QApplication>

FileSearchDialog::FileSearchDialog(const QString &rootPath,
                                   const QSet<QString> &ignoredAbsolutePaths,
                                   const QString &initialQuery,
                                   QWidget *parent)
    : QDialog(parent)
    , m_rootPath(rootPath)
    , m_ignoredAbsolute(ignoredAbsolutePaths)
{
    setWindowTitle("Go to File");
    setMinimumSize(500, 400);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Search for file...");
    m_input->installEventFilter(this);
    if (!initialQuery.isEmpty()) {
        m_input->setText(initialQuery);
        m_input->selectAll();
    }

    m_list = new QListWidget;
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->addItem("Scanning project...");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(m_input);
    layout->addWidget(m_list);

    connect(m_input, &QLineEdit::textChanged, this, &FileSearchDialog::onTextChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &FileSearchDialog::onItemDoubleClicked);

    m_input->setFocus();

    // Defer the project scan so the dialog appears immediately.
    QTimer::singleShot(0, this, &FileSearchDialog::populate);
}

void FileSearchDialog::populate()
{
    QDirIterator it(m_rootPath, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QDir root(m_rootPath);
    int seen = 0;
    while (it.hasNext()) {
        it.next();
        QString absolute = it.filePath();
        QString relative = root.relativeFilePath(absolute);
        if (relative.startsWith("build/"))
            continue;
        QString dirPath = QFileInfo(relative).path();
        bool inHiddenDir = false;
        if (!dirPath.isEmpty() && dirPath != ".") {
            const QStringList parts = dirPath.split('/', Qt::SkipEmptyParts);
            for (const QString &p : parts) {
                if (p.startsWith('.')) { inHiddenDir = true; break; }
            }
        }
        if (inHiddenDir)
            continue;
        bool inIgnored = false;
        for (const QString &ig : m_ignoredAbsolute) {
            if (absolute == ig || absolute.startsWith(ig + "/")) {
                inIgnored = true;
                break;
            }
        }
        if (inIgnored)
            continue;
        m_allFiles.append(relative);
        if (++seen % 1000 == 0)
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    m_allFiles.sort(Qt::CaseInsensitive);
    m_populated = true;
    onTextChanged(m_input->text());
}

QStringList FileSearchDialog::selectedFiles() const
{
    return m_selectedFiles;
}

void FileSearchDialog::onTextChanged(const QString &text)
{
    m_list->clear();

    if (!m_populated) {
        m_list->addItem("Scanning project...");
        return;
    }

    // Strip a leading "./" (or repeated "././") so a path-style query like
    // "./bin/foo" matches the relative paths we store ("bin/foo").
    QString query = text;
    while (query.startsWith("./"))
        query.remove(0, 2);

    if (query.isEmpty()) {
        m_list->addItems(m_allFiles);
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        return;
    }

    if (query.contains('*') || query.contains('?')) {
        // Wrap with implicit '*' on both ends so partial typing (e.g. "out*m")
        // shows progressive matches rather than nothing until the user reaches
        // a string that happens to end the basename.
        QString padded = query;
        if (!padded.startsWith('*')) padded.prepend('*');
        if (!padded.endsWith('*'))   padded.append('*');
        QRegularExpression re(
            QRegularExpression::wildcardToRegularExpression(padded),
            QRegularExpression::CaseInsensitiveOption);
        bool matchFullPath = query.contains('/');
        for (const QString &file : m_allFiles) {
            QString target = matchFullPath ? file : QFileInfo(file).fileName();
            if (re.match(target).hasMatch())
                m_list->addItem(file);
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        return;
    }

    // Plain query: substring match only, ranked by where the hit lands
    // (basename-prefix > basename-anywhere > path-anywhere). Use globs (*, ?)
    // for looser matching.
    const QString lower = query.toLower();

    struct Scored { int score; QString path; };
    QList<Scored> scored;
    scored.reserve(m_allFiles.size());

    for (const QString &file : m_allFiles) {
        const QString baseLower = QFileInfo(file).fileName().toLower();
        const QString pathLower = file.toLower();

        int score = 0;
        if (baseLower.startsWith(lower))
            score = 3;
        else if (baseLower.contains(lower))
            score = 2;
        else if (pathLower.contains(lower))
            score = 1;

        if (score > 0)
            scored.append({score, file});
    }

    std::sort(scored.begin(), scored.end(), [](const Scored &a, const Scored &b) {
        if (a.score != b.score) return a.score > b.score;
        return a.path.compare(b.path, Qt::CaseInsensitive) < 0;
    });

    const int cap = 500;
    const int n = qMin(cap, int(scored.size()));
    for (int i = 0; i < n; ++i)
        m_list->addItem(scored[i].path);

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void FileSearchDialog::onItemDoubleClicked()
{
    auto *item = m_list->currentItem();
    if (item) {
        m_selectedFiles = {m_rootPath + "/" + item->text()};
        accept();
    }
}

void FileSearchDialog::acceptSelection()
{
    // Collect every selected row in list order; fall back to the current row.
    QList<int> rows;
    const auto items = m_list->selectedItems();
    for (QListWidgetItem *item : items)
        rows.append(m_list->row(item));
    std::sort(rows.begin(), rows.end());

    m_selectedFiles.clear();
    for (int row : rows)
        m_selectedFiles.append(m_rootPath + "/" + m_list->item(row)->text());

    if (m_selectedFiles.isEmpty()) {
        if (auto *cur = m_list->currentItem())
            m_selectedFiles.append(m_rootPath + "/" + cur->text());
    }

    if (!m_selectedFiles.isEmpty())
        accept();
}

bool FileSearchDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_input && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        const bool shift = keyEvent->modifiers() & Qt::ShiftModifier;
        if (keyEvent->key() == Qt::Key_Down) {
            int row = m_list->currentRow();
            if (row < m_list->count() - 1)
                // Shift extends the selection downward; a plain Down collapses
                // back to a single selection.
                m_list->setCurrentRow(row + 1, shift ? QItemSelectionModel::Select
                                                     : QItemSelectionModel::ClearAndSelect);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up) {
            int row = m_list->currentRow();
            if (row > 0)
                m_list->setCurrentRow(row - 1, shift ? QItemSelectionModel::Select
                                                     : QItemSelectionModel::ClearAndSelect);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            acceptSelection();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            reject();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
