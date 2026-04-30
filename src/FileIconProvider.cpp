#include "FileIconProvider.h"
#include <QFileInfo>
#include <QIcon>

QIcon FileIconProvider::icon(IconType type) const
{
    switch (type) {
    case Folder:
        return QIcon(":/icons/icons/folder.svg");
    case File:
        return QIcon(":/icons/icons/text.svg");
    default:
        return QAbstractFileIconProvider::icon(type);
    }
}

QIcon FileIconProvider::icon(const QFileInfo &info) const
{
    if (info.isDir())
        return QIcon(":/icons/icons/folder.svg");

    const QString name = info.fileName();
    const QString suffix = info.suffix().toLower();

    // Filename-based matches first
    if (name.compare("CMakeLists.txt", Qt::CaseInsensitive) == 0
        || suffix == "cmake")
        return QIcon(":/icons/icons/cmake.svg");
    if (name.compare("Makefile", Qt::CaseInsensitive) == 0
        || name.compare("GNUmakefile", Qt::CaseInsensitive) == 0
        || suffix == "mk")
        return QIcon(":/icons/icons/makefile.svg");
    if (name.startsWith(".git"))
        return QIcon(":/icons/icons/git.svg");

    // Extension-based
    if (suffix == "py" || suffix == "pyw" || suffix == "pyi")
        return QIcon(":/icons/icons/python.svg");
    if (suffix == "cpp" || suffix == "cxx" || suffix == "cc")
        return QIcon(":/icons/icons/cpp.svg");
    if (suffix == "h" || suffix == "hpp" || suffix == "hxx")
        return QIcon(":/icons/icons/header.svg");
    if (suffix == "c")
        return QIcon(":/icons/icons/c.svg");
    if (suffix == "json")
        return QIcon(":/icons/icons/json.svg");
    if (suffix == "md" || suffix == "markdown")
        return QIcon(":/icons/icons/markdown.svg");
    if (suffix == "yml" || suffix == "yaml")
        return QIcon(":/icons/icons/yaml.svg");
    if (suffix == "sh" || suffix == "bash" || suffix == "zsh")
        return QIcon(":/icons/icons/shell.svg");

    return QIcon(":/icons/icons/text.svg");
}
