#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>
#include <QStringList>
#include <QJsonObject>

class Settings {
public:
    explicit Settings(const QString &rootPath);

    QString value(const QString &key, const QString &defaultValue = {}) const;
    void setValue(const QString &key, const QString &value);

    QStringList valueList(const QString &key) const;
    void setValueList(const QString &key, const QStringList &value);

    int valueInt(const QString &key, int defaultValue = 0) const;
    void setValueInt(const QString &key, int value);

    QJsonObject valueObject(const QString &key) const;
    void setValueObject(const QString &key, const QJsonObject &value);

    QString configDir() const;

private:
    void load();
    void save();

    QString m_configDir;
    QString m_configFile;
    QJsonObject m_data;
};

#endif
