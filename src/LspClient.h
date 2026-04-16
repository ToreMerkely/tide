#ifndef LSPCLIENT_H
#define LSPCLIENT_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QStringList>
#include <functional>

struct LspLocation {
    QString filePath;
    int line;
    int column;
};

struct SemanticToken {
    int line;
    int column;
    int length;
    QString tokenType;
    QStringList modifiers;
};

class LspClient : public QObject {
    Q_OBJECT

public:
    LspClient(const QString &command, const QStringList &args,
              const QString &rootPath, QObject *parent = nullptr);
    ~LspClient();

    void start();
    bool isRunning() const;
    void didOpen(const QString &filePath, const QString &content, const QString &languageId);
    void didChange(const QString &filePath, const QString &content);
    void gotoDefinition(const QString &filePath, int line, int column,
                        std::function<void(const QVector<LspLocation> &)> callback);
    void requestSemanticTokens(const QString &filePath,
                               std::function<void(const QVector<SemanticToken> &)> callback);

signals:
    void ready();

private slots:
    void onReadyRead();

private:
    void sendRequest(const QString &method, const QJsonObject &params,
                     std::function<void(const QJsonObject &)> callback = nullptr);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendMessage(const QJsonObject &msg);
    void handleMessage(const QJsonObject &msg);

    QProcess *m_process;
    QString m_command;
    QStringList m_args;
    QString m_rootPath;
    int m_nextId = 1;
    QByteArray m_buffer;
    QMap<int, std::function<void(const QJsonObject &)>> m_callbacks;
    QStringList m_tokenTypes;
    QStringList m_tokenModifiers;
};

#endif
