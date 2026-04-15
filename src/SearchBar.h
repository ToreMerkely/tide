#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>

class QLineEdit;
class QLabel;
class QPlainTextEdit;

class SearchBar : public QWidget {
    Q_OBJECT

public:
    explicit SearchBar(QWidget *parent = nullptr);

    void setEditor(QPlainTextEdit *editor);
    void activate();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onTextChanged(const QString &text);
    void findNext();
    void findPrevious();
    void close();

private:
    void highlightMatches();
    void goToMatch(int index);

    QLineEdit *m_input;
    QLabel *m_matchLabel;
    QPlainTextEdit *m_editor = nullptr;

    struct Match {
        int start;
        int length;
    };
    QVector<Match> m_matches;
    int m_currentMatch = -1;
};

#endif
