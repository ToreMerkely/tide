#include "LspClient.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QFileInfo>

LspClient::LspClient(const QString &command, const QStringList &args,
                     const QString &rootPath, QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_command(command)
    , m_args(args)
    , m_rootPath(rootPath)
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LspClient::onReadyRead);
}

void LspClient::setEnvironment(const QStringList &env)
{
    QProcessEnvironment procEnv = QProcessEnvironment::systemEnvironment();
    for (const QString &e : env) {
        int eq = e.indexOf('=');
        if (eq > 0)
            procEnv.insert(e.left(eq), e.mid(eq + 1));
    }
    m_process->setProcessEnvironment(procEnv);
}

LspClient::~LspClient()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void LspClient::start()
{
    m_process->start(m_command, m_args);
    if (!m_process->waitForStarted(3000))
        return;

    QJsonObject semanticTokensCaps;
    semanticTokensCaps["dynamicRegistration"] = false;
    semanticTokensCaps["requests"] = QJsonObject{
        {"range", false},
        {"full", QJsonObject{{"delta", false}}}
    };
    semanticTokensCaps["tokenTypes"] = QJsonArray::fromStringList({
        "namespace", "type", "class", "enum", "interface",
        "struct", "typeParameter", "parameter", "variable", "property",
        "enumMember", "event", "function", "method", "macro",
        "keyword", "modifier", "comment", "string", "number",
        "regexp", "operator", "decorator"
    });
    semanticTokensCaps["tokenModifiers"] = QJsonArray::fromStringList({
        "declaration", "definition", "readonly", "static",
        "deprecated", "abstract", "async", "modification",
        "documentation", "defaultLibrary"
    });
    semanticTokensCaps["formats"] = QJsonArray{"relative"};
    semanticTokensCaps["overlappingTokenSupport"] = false;
    semanticTokensCaps["multilineTokenSupport"] = false;

    QJsonObject caps;
    caps["textDocument"] = QJsonObject{
        {"definition", QJsonObject{{"dynamicRegistration", false}}},
        {"semanticTokens", semanticTokensCaps}
    };
    caps["general"] = QJsonObject{};

    QString rootUri = QUrl::fromLocalFile(m_rootPath).toString();

    QJsonArray workspaceFolders;
    workspaceFolders.append(QJsonObject{
        {"uri", rootUri},
        {"name", QFileInfo(m_rootPath).fileName()}
    });

    QJsonObject params;
    params["processId"] = (int)QCoreApplication::applicationPid();
    params["rootUri"] = rootUri;
    params["rootPath"] = m_rootPath;
    params["workspaceFolders"] = workspaceFolders;
    params["capabilities"] = caps;

    sendRequest("initialize", params, [this](const QJsonObject &response) {
        // Capture semantic token legend
        QJsonObject result = response["result"].toObject();
        QJsonObject serverCaps = result["capabilities"].toObject();
        QJsonObject semTokens = serverCaps["semanticTokensProvider"].toObject();
        QJsonObject legend = semTokens["legend"].toObject();

        m_tokenTypes.clear();
        for (const auto &v : legend["tokenTypes"].toArray())
            m_tokenTypes.append(v.toString());

        m_tokenModifiers.clear();
        for (const auto &v : legend["tokenModifiers"].toArray())
            m_tokenModifiers.append(v.toString());

        sendNotification("initialized", QJsonObject{});
        emit ready();
    });
}

bool LspClient::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

void LspClient::didOpen(const QString &filePath, const QString &content, const QString &languageId)
{
    if (!isRunning())
        return;

    m_fileVersions[filePath] = 1;

    QJsonObject textDoc;
    textDoc["uri"] = QUrl::fromLocalFile(filePath).toString();
    textDoc["languageId"] = languageId;
    textDoc["version"] = 1;
    textDoc["text"] = content;

    sendNotification("textDocument/didOpen", QJsonObject{{"textDocument", textDoc}});
}

void LspClient::didChange(const QString &filePath, const QString &content)
{
    if (!isRunning())
        return;

    int version = ++m_fileVersions[filePath];

    QJsonObject textDoc;
    textDoc["uri"] = QUrl::fromLocalFile(filePath).toString();
    textDoc["version"] = version;

    QJsonArray changes;
    changes.append(QJsonObject{{"text", content}});

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["contentChanges"] = changes;

    sendNotification("textDocument/didChange", params);
}

void LspClient::gotoDefinition(const QString &filePath, int line, int column,
                                std::function<void(const QVector<LspLocation> &)> callback)
{
    if (!isRunning()) {
        if (callback)
            callback({});
        return;
    }

    QJsonObject params;
    params["textDocument"] = QJsonObject{
        {"uri", QUrl::fromLocalFile(filePath).toString()}
    };
    params["position"] = QJsonObject{
        {"line", line},
        {"character", column}
    };

    sendRequest("textDocument/definition", params, [callback](const QJsonObject &response) {
        QVector<LspLocation> locations;
        QJsonValue result = response["result"];

        auto parseLocation = [](const QJsonObject &loc) -> LspLocation {
            QString uri = loc["uri"].toString();
            QString path = QUrl(uri).toLocalFile();
            QJsonObject range = loc["range"].toObject();
            QJsonObject start = range["start"].toObject();
            return {path, start["line"].toInt(), start["character"].toInt()};
        };

        if (result.isArray()) {
            for (const auto &val : result.toArray())
                locations.append(parseLocation(val.toObject()));
        } else if (result.isObject()) {
            locations.append(parseLocation(result.toObject()));
        }

        if (callback)
            callback(locations);
    });
}

void LspClient::requestSemanticTokens(const QString &filePath,
                                       std::function<void(const QVector<SemanticToken> &)> callback)
{
    if (!isRunning() || m_tokenTypes.isEmpty()) {
        if (callback)
            callback({});
        return;
    }

    QJsonObject params;
    params["textDocument"] = QJsonObject{
        {"uri", QUrl::fromLocalFile(filePath).toString()}
    };

    sendRequest("textDocument/semanticTokens/full", params,
                [this, callback](const QJsonObject &response) {
        QVector<SemanticToken> tokens;
        QJsonArray data = response["result"].toObject()["data"].toArray();

        int line = 0;
        int col = 0;

        for (int i = 0; i + 4 < data.size(); i += 5) {
            int deltaLine = data[i].toInt();
            int deltaStart = data[i + 1].toInt();
            int length = data[i + 2].toInt();
            int typeIndex = data[i + 3].toInt();
            int modBits = data[i + 4].toInt();

            if (deltaLine > 0) {
                line += deltaLine;
                col = deltaStart;
            } else {
                col += deltaStart;
            }

            QString type;
            if (typeIndex >= 0 && typeIndex < m_tokenTypes.size())
                type = m_tokenTypes[typeIndex];

            QStringList mods;
            for (int bit = 0; bit < m_tokenModifiers.size(); ++bit) {
                if (modBits & (1 << bit))
                    mods.append(m_tokenModifiers[bit]);
            }

            tokens.append({line, col, length, type, mods});
        }

        if (callback)
            callback(tokens);
    });
}

void LspClient::sendRequest(const QString &method, const QJsonObject &params,
                             std::function<void(const QJsonObject &)> callback)
{
    int id = m_nextId++;
    if (callback)
        m_callbacks[id] = callback;

    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    msg["params"] = params;

    sendMessage(msg);
}

void LspClient::sendNotification(const QString &method, const QJsonObject &params)
{
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    msg["params"] = params;

    sendMessage(msg);
}

void LspClient::sendMessage(const QJsonObject &msg)
{
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray header = "Content-Length: " + QByteArray::number(data.size()) + "\r\n\r\n";
    m_process->write(header + data);
}

void LspClient::onReadyRead()
{
    m_buffer += m_process->readAllStandardOutput();

    while (true) {
        int headerEnd = m_buffer.indexOf("\r\n\r\n");
        if (headerEnd == -1)
            break;

        // Parse Content-Length
        int contentLength = -1;
        QString header = QString::fromUtf8(m_buffer.left(headerEnd));
        for (const QString &line : header.split("\r\n")) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
                contentLength = line.mid(15).trimmed().toInt();
                break;
            }
        }

        if (contentLength < 0)
            break;

        int messageStart = headerEnd + 4;
        if (m_buffer.size() < messageStart + contentLength)
            break;

        QByteArray jsonData = m_buffer.mid(messageStart, contentLength);
        m_buffer = m_buffer.mid(messageStart + contentLength);

        QJsonObject msg = QJsonDocument::fromJson(jsonData).object();
        handleMessage(msg);
    }
}

void LspClient::handleMessage(const QJsonObject &msg)
{
    if (msg.contains("id") && !msg.contains("method")) {
        // This is a response
        int id = msg["id"].toInt();
        if (m_callbacks.contains(id)) {
            m_callbacks[id](msg);
            m_callbacks.remove(id);
        }
    }
}
