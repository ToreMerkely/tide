#include "FileSearchDialog.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QFileInfo>

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

    // Scan all files in project. Include hidden files (e.g. .gitignore) but
    // skip anything inside hidden directories (e.g. .git/) and the build dir.
    QDirIterator it(m_rootPath, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QDir root(m_rootPath);
    while (it.hasNext()) {
        it.next();
        QString relative = root.relativeFilePath(it.filePath());
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
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        return;
    }

    if (text.contains('*') || text.contains('?')) {
        // Wrap with implicit '*' on both ends so partial typing (e.g. "out*m")
        // shows progressive matches rather than nothing until the user reaches
        // a string that happens to end the basename.
        QString padded = text;
        if (!padded.startsWith('*')) padded.prepend('*');
        if (!padded.endsWith('*'))   padded.append('*');
        QRegularExpression re(
            QRegularExpression::wildcardToRegularExpression(padded),
            QRegularExpression::CaseInsensitiveOption);
        bool matchFullPath = text.contains('/');
        for (const QString &file : m_allFiles) {
            QString target = matchFullPath ? file : QFileInfo(file).fileName();
            if (re.match(target).hasMatch())
                m_list->addItem(file);
        }
        if (m_list->count() > 0)
            m_list->setCurrentRow(0);
        return;
    }

    // Plain query: rank substring matches (basename first, then path) above
    // subsequence (fuzzy) matches so e.g. "test_migr" surfaces files actually
    // named test_migr* before files where those letters merely happen to
    // appear in order.
    const QString lower = text.toLower();

    struct Scored { int score; QString path; };
    QList<Scored> scored;
    scored.reserve(m_allFiles.size());

    for (const QString &file : m_allFiles) {
        const QString baseLower = QFileInfo(file).fileName().toLower();
        const QString pathLower = file.toLower();

        int score = 0;
        if (baseLower.startsWith(lower))
            score = 5;
        else if (baseLower.contains(lower))
            score = 4;
        else if (pathLower.contains(lower))
            score = 3;
        else {
            // Subsequence (fuzzy)
            int qi = 0;
            for (int fi = 0; fi < baseLower.size() && qi < lower.size(); ++fi) {
                if (baseLower[fi] == lower[qi]) ++qi;
            }
            if (qi == lower.size()) {
                score = 2;
            } else {
                qi = 0;
                for (int fi = 0; fi < pathLower.size() && qi < lower.size(); ++fi) {
                    if (pathLower[fi] == lower[qi]) ++qi;
                }
                if (qi == lower.size())
                    score = 1;
            }
        }
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
