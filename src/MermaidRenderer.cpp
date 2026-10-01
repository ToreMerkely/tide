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

// Shown in the preview in place of the diagram, so a missing or broken mmdc
// says what to do about it instead of leaving an unexplained code block.
QString failureNote(const QString &reason)
{
    return "> \u26A0\uFE0F **Mermaid not rendered:** " + reason;
}

// The mmdc flags that decide what the PNG looks like. Part of the cache key,
// so changing them re-renders rather than serving an image made by the old
// ones.
const QStringList kRenderArgs{"-t", "dark", "-b", "transparent", "-w", "800"};

// mermaid scales a diagram to the width it is rendered in - fonts and all - so
// anything wider than the render viewport arrives shrunk to illegible. The flag
// is per diagram type, so name every type that honours it and let each diagram
// come out at its intrinsic size instead.
QByteArray mermaidConfigJson()
{
    static const QStringList types{
        "flowchart", "sequence", "gantt", "class", "state", "er", "pie",
        "journey", "requirement", "gitGraph", "c4", "mindmap", "timeline",
        "quadrantChart", "xyChart", "sankey", "block", "packet", "architecture",
        "radar", "treemap"};
    QJsonObject config;
    for (const QString &type : types)
        config.insert(type, QJsonObject{{"useMaxWidth", false}});
    return QJsonDocument(config).toJson(QJsonDocument::Compact);
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
    m_mermaidConfig = writeMermaidConfig();
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

QString MermaidRenderer::writeMermaidConfig() const
{
    const QString path = m_cacheDir + "/mermaid.json";
    const QByteArray json = mermaidConfigJson();
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
    QByteArray hash = QCryptographicHash::hash(
        diagram.toUtf8() + '\n' + kRenderArgs.join(' ').toUtf8()
            + '\n' + mermaidConfigJson(),
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
            if (m_failed.contains(png)) {
                out.append(QString());
                out.append(failureNote(m_failed.value(png)));
                out.append(QString());
            }
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
        const QString reason =
            "mermaid-cli (mmdc) not found - install it with "
            "`npm i -g @mermaid-js/mermaid-cli` or `brew install mermaid-cli`, "
            "or set `mermaid_cli` in .tide/config.json";
        m_failed.insert(pngPath, reason);
        emit renderFailed(reason);
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
        const QString reason = firstError(err);
        m_failed.insert(pngPath, reason);
        emit renderFailed("Mermaid render failed: " + reason);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, input, pngPath]() {
        if (!m_inFlight.contains(pngPath))
            return;
        m_inFlight.remove(pngPath);
        const QString reason = "could not run " + m_cli;
        m_failed.insert(pngPath, reason);
        QFile::remove(input);
        proc->deleteLater();
        emit renderFailed("Mermaid render failed: " + reason);
    });

    QStringList args{"-i", input, "-o", pngPath};
    args << kRenderArgs;
    if (!m_puppeteerConfig.isEmpty())
        args << "-p" << m_puppeteerConfig;
    if (!m_mermaidConfig.isEmpty())
        args << "-c" << m_mermaidConfig;

    proc->setProgram(m_cli);
    proc->setArguments(args);
    proc->setStandardOutputFile(QProcess::nullDevice());
    proc->start();
}
