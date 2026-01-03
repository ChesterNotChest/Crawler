#include "config_manager.h"
#include <QFile>
#include <QJsonParseError>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>

QJsonObject ConfigManager::s_config;
bool ConfigManager::s_loaded = false;
QString ConfigManager::s_configPath;

bool ConfigManager::loadConfig(const QString& configPath) {
    QString resolved = configPath;
    if (configPath == "config.json") {
        resolved = getConfigFilePath(configPath);
    }
    QFile configFile(resolved);
    
    if (!configFile.exists()) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << resolved;
        qDebug() << "[提示] 请创建config.json配置文件于项目根目录或" << resolved;
        return false;
    }
    
    if (!configFile.open(QIODevice::ReadOnly)) {
        qDebug() << "[ConfigManager] 无法打开配置文件:" << configFile.errorString();
        return false;
    }
    
    QByteArray data = configFile.readAll();
    configFile.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "[ConfigManager] JSON解析失败:" << parseError.errorString();
        return false;
    }
    
    if (!doc.isObject()) {
        qDebug() << "[ConfigManager] 配置文件格式错误，根节点必须是对象";
        return false;
    }
    
    s_config = doc.object();
    s_loaded = true;
    s_configPath = resolved;

    qDebug() << "[ConfigManager] 配置加载成功:" << s_configPath;
    return true;
}

QString ConfigManager::getConfigFilePath(const QString& hint) {
    // 从可执行目录向上搜索包含 CMakeLists.txt 的目录，认为该目录为项目根
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 12; ++depth) {
        QString candidateCMake = dir.filePath("CMakeLists.txt");
        if (QFile::exists(candidateCMake)) {
            QString cfg = dir.filePath("config.json");
            return cfg;
        }
        if (!dir.cdUp()) break;
    }

    // 未找到项目根，尝试使用 hint 相对于可执行目录的位置
    QString fallback = QCoreApplication::applicationDirPath() + QLatin1String("/") + hint;
    return fallback;
}

bool ConfigManager::saveConfig() {
    QString path = s_configPath;
    if (path.isEmpty()) {
        path = getConfigFilePath();
        s_configPath = path;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "[ConfigManager] 无法打开config.json写入:" << path << ", 错误:" << f.errorString();
        return false;
    }
    f.write(QJsonDocument(s_config).toJson(QJsonDocument::Indented));
    f.close();
    qDebug() << "[ConfigManager] 配置已保存到:" << path;
    return true;
}

QString ConfigManager::getZhipinCookie() {
    if (!s_loaded) {
        qDebug() << "[ConfigManager] 警告: 配置未加载，尝试自动加载...";
        loadConfig();
    }
    
    if (s_config.contains("zhipin") && s_config["zhipin"].isObject()) {
        QJsonObject zhipin = s_config["zhipin"].toObject();
        if (zhipin.contains("cookie")) {
            QString cookie = zhipin["cookie"].toString();
            if (!cookie.isEmpty()) {
                qDebug() << "[ConfigManager] 已加载BOSS直聘Cookie (长度:" << cookie.length() << ")";
                return cookie;
            }
        }
    }
    
    qDebug() << "[ConfigManager] 警告: 未找到zhipin.cookie配置";
    return QString();
}


QString ConfigManager::getNowcodeCookie() {
    if (!s_loaded) {
        loadConfig();
    }
    
    if (s_config.contains("nowcode") && s_config["nowcode"].isObject()) {
        QJsonObject nowcode = s_config["nowcode"].toObject();
        if (nowcode.contains("cookie")) {
            return nowcode["cookie"].toString();
        }
    }
    
    return QString();
}

bool ConfigManager::isLoaded() {
    return s_loaded;
}

bool ConfigManager::getSaveAndVectorize(bool defaultValue) {
    if (!s_loaded) {
        loadConfig();
    }
    if (s_config.contains("saveAndVectorize")) {
        const auto v = s_config.value("saveAndVectorize");
        if (v.isBool()) return v.toBool();
        // accept numeric/string truthy forms
        if (v.isDouble()) return v.toDouble() != 0.0;
        if (v.isString()) {
            QString s = v.toString().toLower();
            return (s == "1" || s == "true" || s == "yes" || s == "on");
        }
    }
    return defaultValue;
}

QJsonObject ConfigManager::getEmailSenderConfig() {
    if (!s_loaded) loadConfig();
    if (s_config.contains("email") && s_config["email"].isObject()) {
        QJsonObject email = s_config["email"].toObject();
        if (email.contains("sender") && email["sender"].isObject()) {
            return email["sender"].toObject();
        }
    }
    return QJsonObject();
}

QString ConfigManager::getEmailReceiver() {
    if (!s_loaded) loadConfig();
    if (s_config.contains("email") && s_config["email"].isObject()) {
        QJsonObject email = s_config["email"].toObject();
        if (email.contains("receiver")) return email["receiver"].toString();
    }
    return QString();
}

bool ConfigManager::getSendAlert(bool defaultValue) {
    if (!s_loaded) loadConfig();
    if (s_config.contains("email") && s_config["email"].isObject()) {
        QJsonObject email = s_config["email"].toObject();
        if (email.contains("sendAlert")) {
            const auto v = email.value("sendAlert");
            if (v.isBool()) return v.toBool();
            if (v.isDouble()) return v.toDouble() != 0.0;
            if (v.isString()) {
                QString s = v.toString().toLower();
                return (s == "1" || s == "true" || s == "yes" || s == "on");
            }
        }
    }
    return defaultValue;
}
