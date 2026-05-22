#include "EditorGroup.h"
#include "SearchBar.h"
#include "CodeEditor.h"
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QEvent>
#include <QPainter>

void ActiveUnderlineTabBar::setGroupActive(bool active)
{
    if (m_groupActive == active)
        return;
    m_groupActive = active;
    update();
}

void ActiveUnderlineTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    int idx = currentIndex();
    if (idx < 0)
        return;
    QRect r = tabRect(idx);
    QPainter p(this);
    const int thickness = 3;
    int y = height() - thickness;
    QColor c = m_groupActive ? QColor(0x56, 0xA8, 0xF5)
                             : QColor(0x60, 0x63, 0x66);
    p.fillRect(r.x(), y, r.width(), thickness, c);
}

EditorGroup::EditorGroup(QWidget *parent)
    : QWidget(parent)
{
    m_tabBar = new ActiveUnderlineTabBar;
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setDrawBase(false);
    m_tabBar->setMinimumHeight(28);
    m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_pageStack = new QStackedWidget;

    m_searchBar = new SearchBar;

    m_underline = nullptr;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_searchBar);
    layout->addWidget(m_pageStack, 1);

    connect(m_tabBar, &QTabBar::currentChanged, m_pageStack, &QStackedWidget::setCurrentIndex);
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int idx) {
        m_searchBar->setEditor(qobject_cast<QPlainTextEdit *>(
            idx >= 0 ? m_pageStack->widget(idx) : nullptr));
    });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *w = m_pageStack->widget(from);
        if (!w)
            return;
        m_pageStack->removeWidget(w);
        m_pageStack->insertWidget(to, w);
    });

    m_tabBar->installEventFilter(this);
    m_pageStack->installEventFilter(this);
}

int EditorGroup::count() const { return m_tabBar->count(); }
int EditorGroup::currentIndex() const { return m_tabBar->currentIndex(); }
void EditorGroup::setCurrentIndex(int i) { m_tabBar->setCurrentIndex(i); }
QWidget *EditorGroup::currentWidget() const { return m_pageStack->currentWidget(); }
QWidget *EditorGroup::widget(int i) const { return m_pageStack->widget(i); }
QString EditorGroup::tabText(int i) const { return m_tabBar->tabText(i); }

int EditorGroup::addTab(QWidget *page, const QString &label)
{
    m_pageStack->addWidget(page);
    int idx = m_tabBar->addTab(label);
    if (auto *ed = qobject_cast<CodeEditor *>(page)) {
        connect(ed, &CodeEditor::escapePressed, this, [this]() {
            if (m_searchBar->isVisible())
                m_searchBar->close();
        });
    }
    return idx;
}

void EditorGroup::removeTab(int index)
{
    QWidget *w = m_pageStack->widget(index);
    m_tabBar->removeTab(index);
    if (w) {
        m_pageStack->removeWidget(w);
        delete w;
    }
}

int EditorGroup::indexOfPath(const QString &path) const
{
    for (int i = 0; i < m_pageStack->count(); ++i) {
        QWidget *w = m_pageStack->widget(i);
        if (w && w->property("filePath").toString() == path)
            return i;
    }
    return -1;
}

void EditorGroup::activateSearch()
{
    m_searchBar->setEditor(qobject_cast<QPlainTextEdit *>(currentWidget()));
    m_searchBar->activate();
}

void EditorGroup::activateSearchReplace()
{
    m_searchBar->setEditor(qobject_cast<QPlainTextEdit *>(currentWidget()));
    m_searchBar->activate(true);
}

void EditorGroup::setActiveLook(bool active)
{
    if (auto *bar = qobject_cast<ActiveUnderlineTabBar *>(m_tabBar))
        bar->setGroupActive(active);
}

void EditorGroup::setUnderlineVisible(bool /*visible*/)
{
    // No-op: the inactive state is now shown by the tab-underline color
    // instead of a separate strip widget.
}

void EditorGroup::setContentVisible(bool visible)
{
    m_pageStack->setVisible(visible);
}

void EditorGroup::detachTabBar()
{
    if (auto *box = qobject_cast<QBoxLayout *>(layout()))
        box->removeWidget(m_tabBar);
}

void EditorGroup::attachTabBar()
{
    if (auto *box = qobject_cast<QBoxLayout *>(layout()))
        box->insertWidget(0, m_tabBar);
}

bool EditorGroup::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FocusIn ||
        (obj == m_tabBar && event->type() == QEvent::MouseButtonPress)) {
        emit activated();
    }
    return QWidget::eventFilter(obj, event);
}
