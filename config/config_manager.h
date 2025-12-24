#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>

/**
 * @brief 配置文件管理类
 * 
 * 管理config.json配置文件，包括Cookie等敏感信息
 */
class ConfigManager {
public:
    /**
     * @brief 加载配置文件
     * @param configPath 配置文件路径（默认: config.json）
     * @return 是否加载成功
     */
    static bool loadConfig(const QString& configPath = "config.json");
    /**
     * @brief 返回解析后的 config.json 路径（优先在包含 CMakeLists.txt 的项目根目录）
     */
    static QString getConfigFilePath(const QString& hint = "config.json");

    /**
     * @brief 将内存中的配置写回到磁盘（使用 getConfigFilePath() 决定位置）
     */
    static bool saveConfig();
    
    /**
     * @brief 获取BOSS直聘Cookie
     * @return Cookie字符串
     */
    static QString getZhipinCookie();
    
    /**
     * @brief 获取牛客网配置（预留）
     */
    static QString getNowcodeCookie();
    
    /**
     * @brief 检查配置是否已加载
     */
    static bool isLoaded();

private:
    static QJsonObject s_config;
    static bool s_loaded;
    static QString s_configPath;
};

#endif // CONFIG_MANAGER_H
