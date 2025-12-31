#include "crawler_task.h"
#include <QDebug>
#include <QThread>
#include <algorithm>
#include <QString>
#include <memory>
#include "network/webview2_browser_wrl.h"
#include "constants/network_types.h"


CrawlerTask::CrawlerTask(SQLInterface *sqlInterface, WebView2BrowserWRL *sessionBrowser)
        : m_sqlInterface(sqlInterface),
            m_internetTask(),
            m_sqlTask(sqlInterface),
            m_isPaused(false),
            m_isTerminated(false),
            m_sessionBrowser(sessionBrowser) {
        qDebug() << "[CrawlerTask] 初始化完成";
}

void CrawlerTask::pause() {
    m_isPaused = true;
}

void CrawlerTask::resume() {
    m_isPaused = false;
}

void CrawlerTask::terminate() {
    m_isTerminated = true;
}

bool CrawlerTask::isPaused() const {
    return m_isPaused;
}

bool CrawlerTask::isTerminated() const {
    return m_isTerminated;
}

void CrawlerTask::setProgressCallback(std::function<void(int, int, const std::string&)> callback) {
    m_progressCallback = callback;
}

int CrawlerTask::crawlAll(const std::vector<std::string>& sources, int maxPagesPerSource, int pageSize) {
    qDebug() << "[CrawlerTask] crawlAll 启动，sources size=" << sources.size() << " maxPagesPerSource=" << maxPagesPerSource << " pageSize=" << pageSize;

    int totalStored = 0;
    m_isPaused = false;
    m_isTerminated = false;

    // 控制是否在页间暂停，以及哪些来源需要暂停（可按需修改）
    std::vector<std::string> pauseSources = { "zhipin", "liepin", "wuyi" };
    for (size_t sourceIndex = 0; sourceIndex < sources.size(); ++sourceIndex) {
        if (m_isTerminated) break;
        const auto& src = sources[sourceIndex];
        if (m_progressCallback) {
            m_progressCallback(sourceIndex, sources.size(), "开始处理来源: " + src);
        }
        qDebug() << "[CrawlerTask] 开始来源:" << src.c_str();
        // 对于需要浏览器会话的来源（zhipin/liepin/wuyi），不要在子线程直接调用 updateCookieBySource（会创建 QWidget）
        if (src != "zhipin" && src != "wuyi" && src != "liepin") {
            m_internetTask.updateCookieBySource(src);
        }

        // sourceId mapping is provided by constants/network_types.h

        // For session-based sources like wuyi, 使用主线程创建的 m_sessionBrowser
        if (src == "wuyi" && m_sessionBrowser) {
            QMetaObject::invokeMethod(m_sessionBrowser, "enableRequestCapture", Qt::QueuedConnection, Q_ARG(bool, true));
            // Navigate to the list page to initialize session (city may be empty)
            QString initUrl = QLatin1String("https://we.51job.com/pc/search");
            QMetaObject::invokeMethod(m_sessionBrowser, "fetchCookies", Qt::QueuedConnection, Q_ARG(QString, initUrl));
            // give the page a moment to initialize before the first capture
            QThread::sleep(1);
        }

        // 若为 nowcode，则初始化 recruitType 列表（1-3）并使用索引切换
        size_t recruitIndex = 0;
        int currentRecruitType = 1;
        if (src == "nowcode") {
            recruitIndex = 0;
            currentRecruitType = static_cast<int>(recruitIndex) + 1; // 1,2,3
        }

        // 若为 zhipin，则初始化当前城市为列表首项（可为空）
        size_t cityIndex = 0;
        std::string currentCity = "";
        if (src == "zhipin" && !ZHIPIN_CITY_LIST.empty()) currentCity = ZHIPIN_CITY_LIST[0];


        int page = 1;
        int totalPage = 0;  // 总页数计数，用于zhipin跨城市合计
        while (true) {
            if (m_isTerminated) break;
            while (m_isPaused) {
                QThread::sleep(1);  // 等待恢复
                if (m_isTerminated) break;
            }
            if (m_isTerminated) break;
            // 达到用户指定的最大页数上限则停止
            if (maxPagesPerSource > 0 && totalPage >= maxPagesPerSource) {
                qDebug() << "[CrawlerTask] 达到最大页数上限，停止来源:" << src.c_str();
                break;
            }

            qDebug() << "[CrawlerTask] 来源" << src.c_str() << "城市" << currentCity.c_str()
                     << "类型" << currentRecruitType << "第" << page << "页开始抓取...";
            std::pair<std::vector<JobInfo>, MappingData> res;
            if ((src == "wuyi" || src == "liepin") && m_sessionBrowser) {
                res = m_internetTask.fetchBySource(src, page, pageSize, m_sessionBrowser, currentRecruitType, currentCity);
            } else {
                res = m_internetTask.fetchBySource(src, page, pageSize, currentRecruitType, currentCity);
            }
            auto [jobs, mapping] = res;

            // 任意来源遇到反爬码37则更新cookie并重试一次
            if (mapping.last_api_code == 37) {
                qDebug() << "[CrawlerTask] 检测到 反爬码 37，尝试更新 cookie 并重试此页...";
                bool ok = false;
                // 如果来源需要浏览器会话并且我们有主线程创建的 session browser，则使用它来更新cookie
                if ((src == "wuyi" || src == "liepin" || src == "zhipin") && m_sessionBrowser) {
                    ok = m_internetTask.updateCookieBySource(src, m_sessionBrowser);
                } else {
                    ok = m_internetTask.updateCookieBySource(src);
                }
                if (ok) {
                    qDebug() << "[CrawlerTask] cookie 更新成功，重试第" << page << "页...";
                    if ((src == "wuyi" || src == "liepin") && m_sessionBrowser) {
                        std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize, m_sessionBrowser, currentRecruitType, currentCity);
                    } else {
                        std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize, currentRecruitType, currentCity);
                    }
                } else {
                    qDebug() << "[CrawlerTask] cookie 更新失败";
                }
            }

            if (!jobs.empty()) {
                int sourceId = 0;
                auto it = SOURCE_ID_MAP.find(src);
                if (it != SOURCE_ID_MAP.end()) sourceId = it->second;
                int stored = m_sqlTask.storeJobDataBatchWithSource(jobs, sourceId);
                totalStored += stored;
                qDebug() << "[CrawlerTask] 来源" << src.c_str() << "第" << page << "页存储" << stored << "条";

                // 进度回调
                if (m_progressCallback) {
                    // For zhipin show totalPage (overall) when available; otherwise fallback to current page
                    int displayPage = page;
                    if (src == "zhipin" && mapping.totalPage > 0) displayPage = mapping.totalPage;
                    std::string msg = "来源 " + src + " 第 " + std::to_string(displayPage) + " 页存储 " + std::to_string(stored) + " 条";
                    m_progressCallback(sourceIndex, sources.size(), msg);
                }
            } else {
                qDebug() << "[CrawlerTask] 第" << page << "页无数据";
            }

            // 只有当 doPause 为 true 且当前来源在 pauseSources 列表中时才等待
            if (std::find(pauseSources.begin(), pauseSources.end(), src) != pauseSources.end()) {
                QThread::sleep(3);
            }

            // For wuyi, trigger the in-page next button after processing this page
            if (src == "wuyi") {
                // If has_more is false, do not click next
                if (mapping.has_more) {
                    qDebug() << "[CrawlerTask] wuyi: clicking next page button in session browser";
                    if (m_sessionBrowser) QMetaObject::invokeMethod(m_sessionBrowser, "clickNext", Qt::QueuedConnection);
                    // small pause to allow page to fire requests
                    QThread::sleep(1);
                } else {
                    qDebug() << "[CrawlerTask] wuyi: no more pages according to mapping.has_more";
                }
            }

            // 终止条件统一由 has_more 控制；当 has_more == false 时：
            // - 若是 zhipin 且还有未遍历的城市，则切换到下一个城市并从 page=1 继续
            // - 否则结束该来源
            if (!mapping.has_more) {
                if (src == "zhipin" && cityIndex + 1 < ZHIPIN_CITY_LIST.size()) {
                    cityIndex++;
                    currentCity = ZHIPIN_CITY_LIST[cityIndex];
                    page = 1;
                    qDebug() << "[CrawlerTask] 切换到 zhipin 下一个城市:" << currentCity.c_str();
                    continue;
                } else if (src == "nowcode" && recruitIndex + 1 < 3) {
                    // 切换到下一个 recruitType (1->2->3)
                    recruitIndex++;
                    currentRecruitType = static_cast<int>(recruitIndex) + 1;
                    page = 1;
                    qDebug() << "[CrawlerTask] 切换到 nowcode 下一个 recruitType:" << currentRecruitType;
                    continue;
                } else {
                    qDebug() << "[CrawlerTask] 来源" << src.c_str() << "无更多数据，结束来源抓取";
                    break;
                }
            }

            page++;
            totalPage++;
        }
    }

    qDebug() << "[CrawlerTask] crawlAll 完成，总计存储:" << totalStored;
    return totalStored;
}
