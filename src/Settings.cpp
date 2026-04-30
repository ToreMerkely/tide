#include "Settings.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

Settings::Settings(const QString &rootPath)
    : m_configDir(rootPath + "/.tide")
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

QStringList Settings::valueList(const QString &key) const
{
    QStringList result;
    if (!m_data.contains(key))
        return result;
    for (const auto &v : m_data[key].toArray())
        result.append(v.toString());
    return result;
}

void Settings::setValueList(const QString &key, const QStringList &value)
{
    QJsonArray arr;
    for (const QString &s : value)
        arr.append(s);
    m_data[key] = arr;
    save();
}

int Settings::valueInt(const QString &key, int defaultValue) const
{
    if (m_data.contains(key))
        return m_data[key].toInt(defaultValue);
    return defaultValue;
}

void Settings::setValueInt(const QString &key, int value)
{
    m_data[key] = value;
    save();
}

QJsonObject Settings::valueObject(const QString &key) const
{
    if (m_data.contains(key))
        return m_data[key].toObject();
    return {};
}

void Settings::setValueObject(const QString &key, const QJsonObject &value)
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
