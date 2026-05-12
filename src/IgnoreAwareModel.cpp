#include "IgnoreAwareModel.h"
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

void IgnoreAwareModel::setIgnoredPaths(const QSet<QString> &absolutePaths)
{
    m_ignoreRoots = absolutePaths;
    emit layoutChanged();
}

bool IgnoreAwareModel::isIgnoreRoot(const QString &absolutePath) const
{
    return m_ignoreRoots.contains(absolutePath);
}

bool IgnoreAwareModel::isIgnored(const QString &absolutePath) const
{
    if (m_ignoreRoots.isEmpty())
        return false;
    if (m_ignoreRoots.contains(absolutePath))
        return true;
    for (const QString &root : m_ignoreRoots) {
        if (absolutePath.startsWith(root + "/"))
            return true;
    }
    return false;
}

QVariant IgnoreAwareModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::ForegroundRole) {
        if (isIgnored(filePath(index)))
            return QColor("#6B6B6B");
    } else if (role == Qt::DecorationRole) {
        if (isIgnoreRoot(filePath(index))) {
            QVariant base = QFileSystemModel::data(index, role);
            QIcon icon = base.value<QIcon>();
            if (!icon.isNull()) {
                QPixmap pix = icon.pixmap(16, 16);
                QPixmap faded(pix.size());
                faded.fill(Qt::transparent);
                QPainter p(&faded);
                p.setOpacity(0.4);
                p.drawPixmap(0, 0, pix);
                p.end();
                return QIcon(faded);
            }
        }
    }
    return QFileSystemModel::data(index, role);
}
