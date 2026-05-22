#ifndef EDITORGROUP_H
#define EDITORGROUP_H

#include <QWidget>
#include <QTabBar>

class QStackedWidget;
class SearchBar;

class ActiveUnderlineTabBar : public QTabBar {
    Q_OBJECT
public:
    using QTabBar::QTabBar;
    void setGroupActive(bool active);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    bool m_groupActive = true;
};

class EditorGroup : public QWidget {
    Q_OBJECT

public:
    explicit EditorGroup(QWidget *parent = nullptr);

    QTabBar *tabBar() const { return m_tabBar; }
    QStackedWidget *pageStack() const { return m_pageStack; }

    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int i);
    QWidget *currentWidget() const;
    QWidget *widget(int i) const;
    QString tabText(int i) const;
    int addTab(QWidget *page, const QString &label);
    void removeTab(int index);

    int indexOfPath(const QString &path) const;
    SearchBar *searchBar() const { return m_searchBar; }
    void activateSearch();
    void activateSearchReplace();
    void setActiveLook(bool active);
    void setUnderlineVisible(bool visible);
    void setContentVisible(bool visible);
    void detachTabBar();
    void attachTabBar();

signals:
    void activated();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTabBar *m_tabBar;
    QStackedWidget *m_pageStack;
    SearchBar *m_searchBar;
    QWidget *m_underline;
};

#endif
