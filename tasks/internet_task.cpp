#include "internet_task.h"
#include <QDebug>
#include <algorithm>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDate>

InternetTask::InternetTask() {
    // 构造函数，未来可以在这里初始化配置
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchJobData(int pageNo, int pageSize, int recruitType) {
    qDebug() << "[InternetTask] 开始爬取第" << pageNo << "页数据，每页" << pageSize << "条，招聘类型" << recruitType << "...";
    
    // 直接调用牛客网爬虫（向后兼容）
    auto result = NowcodeCrawler::crawlNowcode(pageNo, pageSize, recruitType);
    
    qDebug() << "[InternetTask] 爬取完成，获得" << result.first.size() << "条职位数据";
    
    return result;
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchBySource(
    const std::string& sourceCode, int pageNo, int pageSize, int recruitType, const std::string& city) {
    
    qDebug() << "[InternetTask] 按来源爬取:" << sourceCode.c_str() << ", 页码:" << pageNo;
    
    if (sourceCode == "nowcode") {
        return NowcodeCrawler::crawlNowcode(pageNo, pageSize, recruitType);
    } else if (sourceCode == "zhipin") {
        // BOSS直聘：优先使用传入的 city 参数（若为空则使用默认城市代码）
        const std::string useCity = city.empty() ? std::string("100010000") : city;
        return ZhipinCrawler::crawlZhipin(pageNo, pageSize, useCity);
        } else if (sourceCode == "chinahr") {
            // Chinahr 使用默认 localId=1，可在后续扩展为参数
            return ChinahrCrawler::crawlChinahr(pageNo, pageSize, "1");
        } else if (sourceCode == "liepin") {
            // 使用 WebView2 注入脚本捕获 liepin 后台API响应；当前不会做parser，先捕获原始JSON并生成映射建议供人工确认
            return LiepinCrawler::crawlLiepin(pageNo, pageSize, city);
        } else if (sourceCode == "wuyi") {
            return WuyiCrawler::crawlWuyi(pageNo, pageSize, city);
    } else {
        qDebug() << "[错误] 未知的数据源:" << sourceCode.c_str();
        return {{}, {}};
    }
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchBySource(
    const std::string& sourceCode, int pageNo, int pageSize, WebView2BrowserWRL* browser,
    int recruitType, const std::string& city) {

    qDebug() << "[InternetTask] (browser) 按来源爬取:" << sourceCode.c_str() << ", 页码:" << pageNo;
    if (sourceCode == "wuyi") {
        return WuyiCrawler::crawlWuyi(pageNo, pageSize, city, browser);
    } else {
        qDebug() << "[InternetTask] (browser) 未实现的带浏览器的来源:" << sourceCode.c_str();
        return {{}, {}};
    }
}


// 更新 cookie 并写入 config.json
bool InternetTask::updateCookieBySource(const std::string& sourceCode, int timeoutMs) {
    qDebug() << "[InternetTask] 开始更新 cookie for source:" << sourceCode.c_str();

    QString homeUrl;
    QString listUrl;
    if (sourceCode == "zhipin") {
        homeUrl = QLatin1String("https://www.zhipin.com/");
        listUrl = QLatin1String("https://www.zhipin.com/web/geek/jobs?city=101010100");
    } else if (sourceCode == "nowcode") {
        // 牛客网不需要cookie，但保留接口
        qDebug() << "[InternetTask] nowcode 不需要cookie更新";
        return true;
    } else {
        qDebug() << "[InternetTask] 未知源，无法更新cookie:" << sourceCode.c_str();
        return false;
    }

    WebView2BrowserWRL browser;
    QEventLoop loop;
    bool got = false;
    QString finalCookies;

    QObject::connect(&browser, &WebView2BrowserWRL::cookieFetched, [&](const QString& cookies){
        qDebug() << "[InternetTask] WebView2 返回 cookie 长度:" << cookies.length();
        got = true;
        finalCookies = cookies;
        if (loop.isRunning()) loop.quit();
    });
    QObject::connect(&browser, &WebView2BrowserWRL::navigationFailed, [&](const QString& reason){
        qDebug() << "[InternetTask] WebView2 导航失败:" << reason;
        if (loop.isRunning()) loop.quit();
    });

    // 先访问首页，再访问职位列表页以触发完整cookie链
    browser.fetchCookies(homeUrl);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();

    // 如果首页未取得cookie，再试列表页
    if (!got) {
        qDebug() << "[InternetTask] 首页未获取到cookie，尝试列表页";
        browser.fetchCookies(listUrl);
        QEventLoop loop2;
        QTimer::singleShot(timeoutMs, &loop2, &QEventLoop::quit);
        loop2.exec();
    }

    // 如果没有cookie则返回false
    if (!got || finalCookies.isEmpty()) {
        qDebug() << "[InternetTask] 未能获取cookie";
        return false;
    }

    // 使用 ConfigManager 统一解析后的 config.json 路径并写入
    QString configPath = ConfigManager::getConfigFilePath();
    qDebug() << "[InternetTask] Writing config.json to:" << configPath;

    QFile f(configPath);
    if (!f.exists()) {
        qDebug() << "[InternetTask] config.json 不存在，尝试先通过 ConfigManager 创建默认配置文件";
        // 如果没有现成的配置，尝试加载以初始化路径
        ConfigManager::loadConfig(configPath);
    }

    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << "[InternetTask] 无法打开config.json读取:" << configPath;
        return false;
    }
    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "[InternetTask] config.json 格式错误";
        return false;
    }
    QJsonObject root = doc.object();
    QJsonObject sourceObj = root.value(QString::fromStdString(sourceCode)).toObject();
    sourceObj["cookie"] = QJsonValue(finalCookies);
    sourceObj["updateTime"] = QJsonValue(QDate::currentDate().toString(Qt::ISODate));
    root[QString::fromStdString(sourceCode)] = sourceObj;

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "[InternetTask] 无法打开config.json写入:" << configPath;
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

    // 更新 ConfigManager 内存副本（可选）
    if (ConfigManager::isLoaded()) {
        ConfigManager::loadConfig(configPath); // reload to update in-memory state
    }

    qDebug() << "[InternetTask] cookie 已写入 config.json";
    return true;
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::crawlAll(int pageNo, int pageSize) {
    qDebug() << "[InternetTask] 开始爬取所有数据源...";
    
    std::vector<JobInfo> allJobs;
    MappingData mergedMapping;
    // 1) 优先尝试 Liepin（同步）
    bool liepinInitialFailed = false;
    auto [liepinJobs, liepinMapping] = fetchBySource("liepin", pageNo, pageSize, InternetTask::DEFAULT_RECRUIT_TYPE);
    if (liepinJobs.empty()) {
        qDebug() << "[InternetTask] Liepin 初次尝试未返回数据，稍后重试";
        liepinInitialFailed = true;
    } else {
        allJobs.insert(allJobs.end(), liepinJobs.begin(), liepinJobs.end());
        mergedMapping.type_list.insert(
            mergedMapping.type_list.end(),
            liepinMapping.type_list.begin(),
            liepinMapping.type_list.end()
        );
        mergedMapping.area_list.insert(
            mergedMapping.area_list.end(),
            liepinMapping.area_list.begin(),
            liepinMapping.area_list.end()
        );
        mergedMapping.salary_level_list.insert(
            mergedMapping.salary_level_list.end(),
            liepinMapping.salary_level_list.begin(),
            liepinMapping.salary_level_list.end()
        );
    }

    // 2) 爬取其他数据源（作为回退和补充）
    // 爬取牛客网（校招、实习、社招）
    std::vector<int> recruitTypes = {1, 2, 3};
    for (int recruitType : recruitTypes) {
        auto [jobs, mapping] = NowcodeCrawler::crawlNowcode(pageNo, pageSize, recruitType);
        allJobs.insert(allJobs.end(), jobs.begin(), jobs.end());

        // 合并映射数据
        mergedMapping.type_list.insert(
            mergedMapping.type_list.end(),
            mapping.type_list.begin(),
            mapping.type_list.end()
        );
        mergedMapping.area_list.insert(
            mergedMapping.area_list.end(),
            mapping.area_list.begin(),
            mapping.area_list.end()
        );
        mergedMapping.salary_level_list.insert(
            mergedMapping.salary_level_list.end(),
            mapping.salary_level_list.begin(),
            mapping.salary_level_list.end()
        );
    }

    // 爬取BOSS直聘
    auto [zhipinJobs, zhipinMapping] = ZhipinCrawler::crawlZhipin(pageNo, pageSize);
    allJobs.insert(allJobs.end(), zhipinJobs.begin(), zhipinJobs.end());
    mergedMapping.type_list.insert(
        mergedMapping.type_list.end(),
        zhipinMapping.type_list.begin(),
        zhipinMapping.type_list.end()
    );
    mergedMapping.area_list.insert(
        mergedMapping.area_list.end(),
        zhipinMapping.area_list.begin(),
        zhipinMapping.area_list.end()
    );
    mergedMapping.salary_level_list.insert(
        mergedMapping.salary_level_list.end(),
        zhipinMapping.salary_level_list.begin(),
        zhipinMapping.salary_level_list.end()
    );

    // 3) 如果 Liepin 初次失败，则在其他来源爬取完成后重试一次 Liepin
    if (liepinInitialFailed) {
        qDebug() << "[InternetTask] 对 Liepin 进行一次重试";
        auto [liepinRetryJobs, liepinRetryMapping] = fetchBySource("liepin", pageNo, pageSize, InternetTask::DEFAULT_RECRUIT_TYPE);
        if (liepinRetryJobs.empty()) {
            qDebug() << "[InternetTask] Liepin 重试仍然失败，加入永久 hault 列表:" << pageNo;
            // 记录永久挂起的页码
            halted_pages_["liepin"].push_back(pageNo);
        } else {
            qDebug() << "[InternetTask] Liepin 重试成功，合并结果";
            allJobs.insert(allJobs.end(), liepinRetryJobs.begin(), liepinRetryJobs.end());
            mergedMapping.type_list.insert(
                mergedMapping.type_list.end(),
                liepinRetryMapping.type_list.begin(),
                liepinRetryMapping.type_list.end()
            );
            mergedMapping.area_list.insert(
                mergedMapping.area_list.end(),
                liepinRetryMapping.area_list.begin(),
                liepinRetryMapping.area_list.end()
            );
            mergedMapping.salary_level_list.insert(
                mergedMapping.salary_level_list.end(),
                liepinRetryMapping.salary_level_list.begin(),
                liepinRetryMapping.salary_level_list.end()
            );
        }
    }

    qDebug() << "[InternetTask] 所有数据源爬取完成，总计" << allJobs.size() << "条职位数据";
    
    return {allJobs, mergedMapping};
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchJobDataMultiPage(
    int startPage, int endPage, int pageSize, int recruitType) {
    
    qDebug() << "[InternetTask] 开始批量爬取，页码范围:" << startPage << "-" << endPage << "，招聘类型" << recruitType;
    
    std::vector<JobInfo> allJobs;
    MappingData mergedMapping;
    
    for (int page = startPage; page <= endPage; ++page) {
        auto [jobs, mapping] = fetchJobData(page, pageSize, recruitType);
        
        // 合并JobInfo
        allJobs.insert(allJobs.end(), jobs.begin(), jobs.end());
        
        // 合并MappingData
        mergedMapping.type_list.insert(
            mergedMapping.type_list.end(),
            mapping.type_list.begin(),
            mapping.type_list.end()
        );
        mergedMapping.area_list.insert(
            mergedMapping.area_list.end(),
            mapping.area_list.begin(),
            mapping.area_list.end()
        );
        mergedMapping.salary_level_list.insert(
            mergedMapping.salary_level_list.end(),
            mapping.salary_level_list.begin(),
            mapping.salary_level_list.end()
        );
    }
    
    qDebug() << "[InternetTask] 批量爬取完成，总计" << allJobs.size() << "条职位数据";
    
    return {allJobs, mergedMapping};
}
