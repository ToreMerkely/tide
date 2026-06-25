#include "SymbolSearchDialog.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QKeyEvent>
#include <QRegularExpression>

SymbolSearchDialog::SymbolSearchDialog(const QString &rootPath, QWidget *parent)
    : QDialog(parent)
    , m_rootPath(rootPath)
{
    setWindowTitle("Go to Symbol");
    setMinimumSize(600, 400);

    m_input = new QLineEdit;
    m_input->setPlaceholderText("Search for symbol...");
    m_input->installEventFilter(this);

    m_list = new QListWidget;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addWidget(m_input);
    layout->addWidget(m_list);

    connect(m_input, &QLineEdit::textChanged, this, &SymbolSearchDialog::onTextChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &SymbolSearchDialog::onItemDoubleClicked);

    scanSymbols();

    m_input->setFocus();
}

QString SymbolSearchDialog::selectedFile() const
{
    return m_selectedFile;
}

int SymbolSearchDialog::selectedLine() const
{
    return m_selectedLine;
}

void SymbolSearchDialog::scanSymbols()
{
    // Patterns for symbol definitions by language
    static const QRegularExpression pyClass("^class\\s+(\\w+)");
    static const QRegularExpression pyFunc("^\\s*def\\s+(\\w+)");
    static const QRegularExpression pyAssign("^([A-Z][A-Z_0-9]+)\\s*=");

    static const QRegularExpression cppClass("^\\s*(?:class|struct|enum)\\s+(\\w+)");
    static const QRegularExpression cppFunc("^\\s*(?:[\\w:*&<>]+\\s+)+([\\w~]+)\\s*\\(");
    static const QRegularExpression cppDefine("^\\s*#define\\s+(\\w+)");

    QDir root(m_rootPath);
    QDirIterator it(m_rootPath, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QString relative = root.relativeFilePath(it.filePath());
        if (relative.startsWith(".") || relative.startsWith("build/") ||
            relative.contains("/__pycache__/") || relative.startsWith("__pycache__/"))
            continue;

        QString suffix = it.fileInfo().suffix();
        bool isPython = (suffix == "py");
        bool isCpp = (suffix == "cpp" || suffix == "cxx" || suffix == "cc" ||
                      suffix == "h" || suffix == "hpp" || suffix == "hxx" || suffix == "c");
        if (!isPython && !isCpp)
            continue;

        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QTextStream in(&file);
        int lineNum = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            ++lineNum;

            if (isPython) {
                auto m = pyClass.match(line);
                if (m.hasMatch()) {
                    m_allSymbols.append({m.captured(1), relative, it.filePath(), lineNum, "class"});
                    continue;
                }
                m = pyFunc.match(line);
                if (m.hasMatch()) {
                    m_allSymbols.append({m.captured(1), relative, it.filePath(), lineNum, "function"});
                    continue;
                }
                m = pyAssign.match(line);
                if (m.hasMatch()) {
                    m_allSymbols.append({m.captured(1), relative, it.filePath(), lineNum, "constant"});
                    continue;
                }
            }

            if (isCpp) {
                auto m = cppDefine.match(line);
                if (m.hasMatch()) {
                    m_allSymbols.append({m.captured(1), relative, it.filePath(), lineNum, "macro"});
                    continue;
                }
                m = cppClass.match(line);
                if (m.hasMatch()) {
                    m_allSymbols.append({m.captured(1), relative, it.filePath(), lineNum, "class"});
                    continue;
                }
                m = cppFunc.match(line);
                if (m.hasMatch()) {
                    QString name = m.captured(1);
                    // Skip keywords that look like functions
                    if (name != "if" && name != "while" && name != "for" &&
                        name != "switch" && name != "return" && name != "catch")
                        m_allSymbols.append({name, relative, it.filePath(), lineNum, "function"});
                    continue;
                }
            }
        }
    }
}

QVector<SymbolSearchDialog::Match>
SymbolSearchDialog::exactMatches(const QString &name) const
{
    QVector<Match> result;
    for (const Symbol &sym : m_allSymbols) {
        if (sym.name == name)
            result.append({sym.fullPath, sym.line});
    }
    return result;
}

void SymbolSearchDialog::setInitialQuery(const QString &query)
{
    m_input->setText(query);
}

void SymbolSearchDialog::onTextChanged(const QString &text)
{
    m_list->clear();

    if (text.isEmpty())
        return;

    QString lower = text.toLower();

    struct Match {
        int score;
        const Symbol *sym;
    };
    QVector<Match> matches;

    for (const Symbol &sym : m_allSymbols) {
        QString nameLower = sym.name.toLower();

        // Fuzzy match: all query chars appear in order
        int qi = 0;
        for (int fi = 0; fi < nameLower.size() && qi < lower.size(); ++fi) {
            if (nameLower[fi] == lower[qi])
                ++qi;
        }
        if (qi < lower.size())
            continue;

        // Score: lower is better
        int score;
        if (nameLower == lower) {
            score = 0;   // Exact match
        } else if (nameLower.startsWith(lower)) {
            score = 1;   // Prefix match
        } else if (nameLower.contains(lower)) {
            score = 2;   // Substring match
        } else {
            // Fuzzy — score by length difference (shorter names = closer match)
            score = 3 + (nameLower.size() - lower.size());
        }

        matches.append({score, &sym});

        if (matches.size() >= 500)
            break;
    }

    std::sort(matches.begin(), matches.end(), [](const Match &a, const Match &b) {
        if (a.score != b.score)
            return a.score < b.score;
        return a.sym->name.size() < b.sym->name.size();
    });

    int count = std::min(200, (int)matches.size());
    for (int i = 0; i < count; ++i) {
        const Symbol &sym = *matches[i].sym;
        QString label = sym.name + "  (" + sym.kind + ")  —  " +
                        sym.file + ":" + QString::number(sym.line);
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, sym.fullPath);
        item->setData(Qt::UserRole + 1, sym.line);
        m_list->addItem(item);
    }

    if (m_list->count() > 0)
        m_list->setCurrentRow(0);
}

void SymbolSearchDialog::onItemDoubleClicked()
{
    auto *item = m_list->currentItem();
    if (item) {
        m_selectedFile = item->data(Qt::UserRole).toString();
        m_selectedLine = item->data(Qt::UserRole + 1).toInt();
        accept();
    }
}

bool SymbolSearchDialog::eventFilter(QObject *obj, QEvent *event)
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
                m_selectedFile = item->data(Qt::UserRole).toString();
                m_selectedLine = item->data(Qt::UserRole + 1).toInt();
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
