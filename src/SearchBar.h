#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QPlainTextEdit;

class SearchBar : public QWidget {
    Q_OBJECT

public:
    explicit SearchBar(QWidget *parent = nullptr);

    void setEditor(QPlainTextEdit *editor);
    void activate(bool withReplace = false);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void close();

private slots:
    void onTextChanged(const QString &text);
    void onDocumentChanged();
    void findNext();
    void findPrevious();
    void replaceCurrent();
    void replaceAll();

private:
    bool collectMatches();
    void highlightMatches();
    void goToMatch(int index);
    void setReplaceVisible(bool visible);

    QLineEdit *m_input;
    QLineEdit *m_replaceInput = nullptr;
    QWidget *m_replaceRow = nullptr;
    QLabel *m_matchLabel;
    QPushButton *m_caseBtn = nullptr;
    QPushButton *m_regexBtn = nullptr;
    QPlainTextEdit *m_editor = nullptr;

    struct Match {
        int start;
        int length;
    };
    QVector<Match> m_matches;
    int m_currentMatch = -1;
    bool m_inReplace = false;
};

#endif
