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
    void setEnvironment(const QStringList &env);
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
    // The server failed to start or exited before answering `initialize`.
    void failed(const QString &stderrText);

private slots:
    void onReadyRead();

private:
    void sendRequest(const QString &method, const QJsonObject &params,
                     std::function<void(const QJsonObject &)> callback = nullptr);
    void sendNotification(const QString &method, const QJsonObject &params);
    void sendMessage(const QJsonObject &msg);
    void handleMessage(const QJsonObject &msg);
    void reportFailure();

    QProcess *m_process;
    QString m_command;
    QStringList m_args;
    QString m_rootPath;
    int m_nextId = 1;
    QByteArray m_buffer;
    QMap<int, std::function<void(const QJsonObject &)>> m_callbacks;
    QMap<QString, int> m_fileVersions;
    QStringList m_tokenTypes;
    QStringList m_tokenModifiers;
    bool m_initialized = false;
    bool m_failureReported = false;
};

#endif
