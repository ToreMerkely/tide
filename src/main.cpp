#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(0x3c, 0x3f, 0x41));
    palette.setColor(QPalette::WindowText, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Base, QColor(0x2b, 0x2b, 0x2b));
    palette.setColor(QPalette::AlternateBase, QColor(0x32, 0x32, 0x32));
    palette.setColor(QPalette::Text, QColor(0xa9, 0xb7, 0xc6));
    palette.setColor(QPalette::Button, QColor(0x3c, 0x3f, 0x41));
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
    window.show();

    return app.exec();
}
