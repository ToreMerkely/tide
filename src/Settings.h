#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QJsonObject>

class Settings {
public:
    explicit Settings(const QString &rootPath);

    QString value(const QString &key, const QString &defaultValue = {}) const;
    void setValue(const QString &key, const QString &value);

    QString configDir() const;

private:
    void load();
    void save();

    QString m_configDir;
    QString m_configFile;
    QJsonObject m_data;
};

#endif
