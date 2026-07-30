#ifndef SEARCHBAR_H
#define SEARCHBAR_H

#include <QPointer>
#include <QTextEdit>
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
    // Read-only target (the markdown preview); replace is unavailable on it.
    void setBrowser(QTextEdit *browser);
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

    // The target is either an editable QPlainTextEdit or a read-only
    // QTextEdit; the two share no base class exposing the document/cursor
    // API, so route every access through these.
    void setTarget(QPlainTextEdit *editor, QTextEdit *browser);
    bool hasTarget() const { return m_editor || m_browser; }
    bool targetIsReadOnly() const { return m_browser != nullptr; }
    QTextDocument *targetDocument() const;
    QTextCursor targetCursor() const;
    void setTargetCursor(const QTextCursor &cursor);
    void setTargetExtraSelections(const QList<QTextEdit::ExtraSelection> &selections);
    void centerTargetCursor();
    void focusTarget();

    QLineEdit *m_input;
    QLineEdit *m_replaceInput = nullptr;
    QWidget *m_replaceRow = nullptr;
    QLabel *m_matchLabel;
    QPushButton *m_caseBtn = nullptr;
    QPushButton *m_regexBtn = nullptr;
    QPointer<QPlainTextEdit> m_editor;
    QPointer<QTextEdit> m_browser;

    struct Match {
        int start;
        int length;
    };
    QVector<Match> m_matches;
    int m_currentMatch = -1;
    bool m_inReplace = false;
};

#endif
