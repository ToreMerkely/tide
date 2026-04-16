#include "Settings.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>

Settings::Settings(const QString &rootPath)
    : m_configDir(rootPath + "/.sild")
    , m_configFile(m_configDir + "/config.json")
{
    QDir().mkpath(m_configDir);
    load();
}

QString Settings::value(const QString &key, const QString &defaultValue) const
{
    if (m_data.contains(key))
        return m_data[key].toString();
    return defaultValue;
}

void Settings::setValue(const QString &key, const QString &value)
{
    m_data[key] = value;
    save();
}

QString Settings::configDir() const
{
    return m_configDir;
}

void Settings::load()
{
    QFile file(m_configFile);
    if (!file.open(QIODevice::ReadOnly))
        return;
    m_data = QJsonDocument::fromJson(file.readAll()).object();
}

void Settings::save()
{
    QFile file(m_configFile);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(m_data).toJson());
}
