#include "MermaidRenderer.h"
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace {
// mmdc reports failures as a multi-line node stack trace; the "Error:" line
// carries the whole story and the rest is noise in a status bar.
QString firstError(const QByteArray &stderrText)
{
    const QStringList lines = QString::fromUtf8(stderrText).split('\n');
    for (const QString &line : lines) {
        const QString t = line.trimmed();
        if (t.startsWith("Error:"))
            return t.mid(6).trimmed();
    }
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty())
            return line.trimmed();
    }
    return "mmdc produced no output";
}
}

MermaidRenderer::MermaidRenderer(const QString &cliOverride,
                                 const QString &puppeteerConfig,
                                 QObject *parent)
    : QObject(parent)
    , m_puppeteerConfig(puppeteerConfig)
{
    m_cli = cliOverride.isEmpty() ? QStandardPaths::findExecutable("mmdc") : cliOverride;
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                 + "/mermaid";
    QDir().mkpath(m_cacheDir);
    if (m_puppeteerConfig.isEmpty())
        m_puppeteerConfig = defaultPuppeteerConfig();
}

// mermaid-cli drives a headless Chrome through puppeteer-core, which insists on
// the exact browser build it was packaged against - a build the local puppeteer
// cache usually does not have. An installed Chrome renders the same diagrams, so
// point mmdc at it unless the user configured something else.
QString MermaidRenderer::defaultPuppeteerConfig() const
{
    static const QStringList candidates{"google-chrome", "google-chrome-stable",
                                        "chromium", "chromium-browser"};
    QString chrome;
    for (const QString &c : candidates) {
        chrome = QStandardPaths::findExecutable(c);
        if (!chrome.isEmpty())
            break;
    }
    if (chrome.isEmpty())
        return {};

    const QString path = m_cacheDir + "/puppeteer.json";
    const QByteArray json = QJsonDocument(QJsonObject{{"executablePath", chrome}})
                                .toJson(QJsonDocument::Compact);
    QFile f(path);
    if (f.open(QIODevice::ReadOnly) && f.readAll() == json)
        return path;                  // already current
    f.close();
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    f.write(json);
    return path;
}

QString MermaidRenderer::cachePathFor(const QString &diagram) const
{
    QByteArray hash = QCryptographicHash::hash(diagram.toUtf8(),
                                               QCryptographicHash::Sha1).toHex();
    return m_cacheDir + "/" + QString::fromLatin1(hash) + ".png";
}

QString MermaidRenderer::cacheUrlPrefix() const
{
    return QUrl::fromLocalFile(m_cacheDir).toString() + "/";
}

QString MermaidRenderer::substitute(const QString &markdown)
{
    const QStringList lines = markdown.split('\n');
    QStringList out;
    out.reserve(lines.size());

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines[i].trimmed();
        const bool opensMermaid = (trimmed.startsWith("```") || trimmed.startsWith("~~~"))
                                  && trimmed.mid(3).trimmed().compare("mermaid",
                                                                      Qt::CaseInsensitive) == 0;
        if (!opensMermaid) {
            out.append(lines[i]);
            continue;
        }

        const QString fence = trimmed.left(3);
        QStringList body;
        int j = i + 1;
        for (; j < lines.size(); ++j) {
            if (lines[j].trimmed().startsWith(fence))
                break;
        }
        if (j >= lines.size()) {          // unterminated fence: leave it alone
            out.append(lines[i]);
            continue;
        }
        body = lines.mid(i + 1, j - i - 1);

        const QString diagram = body.join('\n');
        const QString png = cachePathFor(diagram);
        if (QFile::exists(png)) {
            // Blank lines keep the image its own block: an image glued to the
            // surrounding text would be laid out inline.
            out.append(QString());
            out.append(QString("![mermaid](%1)").arg(QUrl::fromLocalFile(png).toString()));
            out.append(QString());
        } else {
            startRender(diagram, png);
            out.append(lines.mid(i, j - i + 1));   // keep the source fence
        }
        i = j;
    }

    return out.join('\n');
}

void MermaidRenderer::startRender(const QString &diagram, const QString &pngPath)
{
    if (m_inFlight.contains(pngPath) || m_failed.contains(pngPath))
        return;
    if (m_cli.isEmpty()) {
        m_failed.insert(pngPath);
        emit renderFailed("mermaid-cli (mmdc) not found - install it, or set "
                          "\"mermaid_cli\" in .tide/config.json");
        return;
    }

    const QString input = pngPath + ".mmd";
    QFile in(input);
    if (!in.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    in.write(diagram.toUtf8());
    in.close();

    m_inFlight.insert(pngPath);
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this,
            [this, proc, input, pngPath](int code, QProcess::ExitStatus status) {
        m_inFlight.remove(pngPath);
        QFile::remove(input);
        const QByteArray err = proc->readAllStandardError();
        proc->deleteLater();
        const bool ok = status == QProcess::NormalExit && code == 0
                        && QFile::exists(pngPath);
        if (ok) {
            emit rendered();
            return;
        }
        // Remember the failure, or every preview refresh would relaunch a
        // render that already told us it cannot succeed.
        m_failed.insert(pngPath);
        emit renderFailed("Mermaid render failed: " + firstError(err));
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, input, pngPath]() {
        if (!m_inFlight.contains(pngPath))
            return;
        m_inFlight.remove(pngPath);
        m_failed.insert(pngPath);
        QFile::remove(input);
        proc->deleteLater();
        emit renderFailed("Mermaid render failed: could not run " + m_cli);
    });

    QStringList args{"-i", input, "-o", pngPath,
                     "-t", "dark", "-b", "transparent", "-w", "800"};
    if (!m_puppeteerConfig.isEmpty())
        args << "-p" << m_puppeteerConfig;

    proc->setProgram(m_cli);
    proc->setArguments(args);
    proc->setStandardOutputFile(QProcess::nullDevice());
    proc->start();
}
