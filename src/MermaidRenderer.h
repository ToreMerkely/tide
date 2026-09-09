#ifndef MERMAIDRENDERER_H
#define MERMAIDRENDERER_H

#include <QObject>
#include <QSet>
#include <QString>

// Renders ```mermaid fences in markdown to PNGs via the mermaid-cli (mmdc),
// so the QTextBrowser preview - which cannot run JavaScript - can show them
// as images.
class MermaidRenderer : public QObject {
    Q_OBJECT

public:
    explicit MermaidRenderer(const QString &cliOverride = {},
                             const QString &puppeteerConfig = {},
                             QObject *parent = nullptr);

    // Replaces every mermaid fence that has a cached PNG with an image link
    // and starts a background render for the rest, returning markdown that is
    // otherwise untouched.
    QString substitute(const QString &markdown);

    // URL prefix shared by every image substitute() emits.
    QString cacheUrlPrefix() const;

signals:
    // A queued diagram finished rendering: the caller should substitute again.
    void rendered();
    // mmdc could not produce the diagram, or is not installed at all.
    void renderFailed(const QString &message);

private:
    QString cachePathFor(const QString &diagram) const;
    QString defaultPuppeteerConfig() const;
    QString writeMermaidConfig() const;
    void startRender(const QString &diagram, const QString &pngPath);

    QString m_cli;          // resolved mmdc executable, empty when unavailable
    QString m_puppeteerConfig;
    QString m_mermaidConfig;
    QString m_cacheDir;
    QSet<QString> m_inFlight;   // png paths currently being rendered
    QSet<QString> m_failed;     // png paths mmdc could not produce
};

#endif
