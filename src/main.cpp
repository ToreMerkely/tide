#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("tide");
    app.setApplicationVersion(TIDE_VERSION);
    app.setWindowIcon(QIcon(":/tide.svg"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "tide - a lightweight Qt6 code editor.\n\n"
        "The project tree is rooted at the working directory. Given a\n"
        "directory, tide roots itself there instead; given a file, it opens\n"
        "that file in a tab.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("path", "File or directory to open.", "[path]");
    parser.process(app);

    // A directory argument has to be applied before MainWindow is built: the
    // project tree, settings, git branch and LSP roots all derive from the
    // working directory. A file argument is opened after the session restore.
    QString fileToOpen;
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        QFileInfo info(positional.first());
        if (!info.exists()) {
            QTextStream(stderr) << "tide: " << positional.first()
                                << ": no such file or directory\n";
            return 1;
        }
        if (info.isDir())
            QDir::setCurrent(info.absoluteFilePath());
        else
            fileToOpen = info.absoluteFilePath();
    }
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x2b, 0x2d, 0x30));
    palette.setColor(QPalette::WindowText, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Base, QColor(0x1e, 0x1f, 0x22));
    palette.setColor(QPalette::AlternateBase, QColor(0x26, 0x27, 0x29));
    palette.setColor(QPalette::Text, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Button, QColor(0x2b, 0x2d, 0x30));
    palette.setColor(QPalette::ButtonText, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Highlight, QColor(0x21, 0x42, 0x83));
    palette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    palette.setColor(QPalette::ToolTipBase, QColor(0x4b, 0x4b, 0x4b));
    palette.setColor(QPalette::ToolTipText, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Link, QColor(0x58, 0x9d, 0xf6));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x68, 0x68, 0x68));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x68, 0x68, 0x68));
    app.setPalette(palette);

    MainWindow window;
    if (!fileToOpen.isEmpty())
        window.openPath(fileToOpen);
    window.show();

    return app.exec();
}
