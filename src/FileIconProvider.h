#ifndef FILEICONPROVIDER_H
#define FILEICONPROVIDER_H

#include <QAbstractFileIconProvider>

class FileIconProvider : public QAbstractFileIconProvider {
public:
    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo &info) const override;
};

#endif
