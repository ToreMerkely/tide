#ifndef IGNOREAWAREMODEL_H
#define IGNOREAWAREMODEL_H

#include <QFileSystemModel>
#include <QSet>

class IgnoreAwareModel : public QFileSystemModel {
    Q_OBJECT
public:
    using QFileSystemModel::QFileSystemModel;
    void setIgnoredPaths(const QSet<QString> &absolutePaths);
    bool isIgnored(const QString &absolutePath) const;
    bool isIgnoreRoot(const QString &absolutePath) const;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    QSet<QString> m_ignoreRoots;
};

#endif
