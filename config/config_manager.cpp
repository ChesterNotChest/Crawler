#include "config_manager.h"
#include <QFile>
#include <QJsonParseError>
#include <QDebug>

QJsonObject ConfigManager::s_config;
bool ConfigManager::s_loaded = false;

bool ConfigManager::loadConfig(const QString& configPath) {
    QFile configFile(configPath);
    
    if (!configFile.exists()) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << configPath;
        qDebug() << "[提示] 请创建config.json配置文件";
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
    
    qDebug() << "[ConfigManager] 配置加载成功:" << configPath;
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

QString ConfigManager::getZhipinCity() {
    if (!s_loaded) {
        loadConfig();
    }
    
    if (s_config.contains("zhipin") && s_config["zhipin"].isObject()) {
        QJsonObject zhipin = s_config["zhipin"].toObject();
        if (zhipin.contains("city")) {
            return zhipin["city"].toString();
        }
    }
    
    return "101010100"; // 默认北京
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
