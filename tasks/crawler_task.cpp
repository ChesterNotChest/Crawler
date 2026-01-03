#include "crawler_task.h"
#include <QDebug>
#include <QThread>
#include <algorithm>
#include <sstream>
#include <QString>
#include <memory>
#include "network/webview2_browser_wrl.h"
#include "constants/network_types.h"
#include "ai_transfer_task.h"
#include <QJsonObject>
#include "config/config_manager.h"


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

void CrawlerTask::setSubProgressCallback(std::function<void(int, int)> callback) {
    m_subProgressCallback = callback;
}

void CrawlerTask::setSourceProgressCallback(std::function<void(size_t, double)> callback) {
    m_sourceProgressCallback = callback;
}

int CrawlerTask::crawlAll(const std::vector<std::string>& sources, const std::vector<int>& maxPagesPerSourceList, int pageSize) {
    qDebug() << "[CrawlerTask] crawlAll 启动，sources size=" << sources.size() << " maxPagesPerSourceList size=" << maxPagesPerSourceList.size() << " pageSize=" << pageSize;

    int totalStored = 0;
    // Read configuration flag to decide whether to call the vectorization endpoint.
    bool doVectorize = ConfigManager::getSaveAndVectorize(true);
    // per-source statistics
    std::vector<int> storedPerSource(sources.size(), 0);
    std::vector<int> pagesFetchedPerSource(sources.size(), 0);
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

        // compute per-source max pages (0 means no limit)
        int perSourceMax = 0;
        if (sourceIndex < maxPagesPerSourceList.size()) perSourceMax = maxPagesPerSourceList[sourceIndex];

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
        int pagesFetchedForSource = 0; // 累计已抓取页数（不随城市重置）
        int expectedPagesForSource = (perSourceMax > 0) ? perSourceMax : 0;
        bool isZhipin = (src == "zhipin");
        std::vector<std::string> seenCities;
        while (true) {
            if (m_isTerminated) break;
            while (m_isPaused) {
                QThread::sleep(1);  // 等待恢复
                if (m_isTerminated) break;
            }
            if (m_isTerminated) break;
            // 达到用户指定的最大页数上限则停止（按来源独立限制）
            if (perSourceMax > 0 && totalPage >= perSourceMax) {
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

            // For sources like zhipin that iterate cities, accumulate expected pages per city
            if (isZhipin && expectedPagesForSource == 0 && mapping.totalPage > 0) {
                // only add when seeing a new city
                bool already = false;
                for (const auto &c : seenCities) if (c == currentCity) { already = true; break; }
                if (!already) {
                    expectedPagesForSource += mapping.totalPage;
                    seenCities.push_back(currentCity);
                }
            }
            // decide effective expected pages for reporting
            int effectiveExpected = (expectedPagesForSource > 0) ? expectedPagesForSource : (perSourceMax > 0 ? perSourceMax : (mapping.totalPage > 0 ? mapping.totalPage : 10));

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
                        qDebug() << "[CrawlerTask] cookie 更新成功，稍作等待后重试第" << page << "页...";
                        // give the system and server a moment to accept the new cookie/session
                        QThread::msleep(2000);
                    if ((src == "wuyi" || src == "liepin") && m_sessionBrowser) {
                        std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize, m_sessionBrowser, currentRecruitType, currentCity);
                    } else {
                        std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize, currentRecruitType, currentCity);
                    }
                } else {
                    qDebug() << "[CrawlerTask] cookie 更新失败";
                }
            }

            bool pageSuccess = false;
            if (!jobs.empty()) {
                int sourceId = 0;
                auto it = SOURCE_ID_MAP.find(src);
                if (it != SOURCE_ID_MAP.end()) sourceId = it->second;

                // expected pages: prefer configured per-source max, then mapping.totalPage, else fallback to a reasonable default
                int expectedPages = (perSourceMax > 0) ? perSourceMax : (mapping.totalPage > 0 ? mapping.totalPage : 10);

                int storedCount = 0;
                // store jobs one by one to enable fine-grained progress updates
                for (size_t i = 0; i < jobs.size(); ++i) {
                    int res = m_sqlTask.storeJobDataWithSource(jobs[i], sourceId);
                    if (res >= 0) {
                        storedCount++;

                        // After storing, send this single job to Python backend for vectorization.
                        // Build a compact JSON object similar to AITransferTask::formatJobDataForAPI
                        QJsonObject jobObj;
                        jobObj["jobId"] = QString::number(res);
                        QString info;
                        info += QString::fromStdString(jobs[i].info_name) + "\n";
                        info += QString::fromStdString(jobs[i].company_name) + "\n";
                        info += QString::fromStdString(jobs[i].area_name) + "\n";
                        info += QString::number(jobs[i].salary_min) + "-" + QString::number(jobs[i].salary_max) + "\n";
                        info += QString::fromStdString(jobs[i].requirements);
                        jobObj["info"] = info;

                        // Optionally call blocking sender for vectorization based on config
                        if (doVectorize) {
                            bool ok = AITransferTask::sendSingleJobBlocking(jobObj);
                            if (!ok) qWarning() << "Vectorization request failed for job" << res;
                        }
                    }
                    // compute fractional progress: (pagesFetched + fractionWithinPage) / effectiveExpected
                    double fracWithinPage = (static_cast<double>(i) + 1.0) / static_cast<double>(jobs.size());
                    double fraction = (static_cast<double>(pagesFetchedForSource) + fracWithinPage) / static_cast<double>(effectiveExpected);
                    if (fraction > 1.0) fraction = 1.0;
                    if (m_sourceProgressCallback) m_sourceProgressCallback(sourceIndex, fraction);
                    // also update sub-progress callback for backward compatibility
                    if (m_subProgressCallback) {
                        // report current page being processed (pagesFetchedForSource + 1)
                        m_subProgressCallback(pagesFetchedForSource + 1, effectiveExpected);
                    }
                }

                // finished this page: consider success only when we got parsed jobs or mapping reports OK
                if (!jobs.empty() || mapping.last_api_code == 0) pageSuccess = true;

                if (pageSuccess) pagesFetchedForSource += 1;
                totalStored += storedCount;
                // accumulate per-source stored count
                storedPerSource[sourceIndex] += storedCount;
                qDebug() << "[CrawlerTask] 来源" << src.c_str() << "第" << page << "页存储" << storedCount << "条" << " pageSuccess=" << pageSuccess;

                // 进度回调 信息显示使用累计页数而非单城页码
                if (m_progressCallback) {
                    int displayPage = pagesFetchedForSource; // show cumulative pages fetched for source
                    int cumulativeStored = storedPerSource[sourceIndex];
                    std::string msg = "来源 " + src + " 已抓取 " + std::to_string(displayPage) + " 页，存储 " + std::to_string(storedCount) + " 条，来源累计存储 " + std::to_string(cumulativeStored) + " 条";
                    m_progressCallback(sourceIndex, sources.size(), msg);
                }
                // increment totalPage only on success (controls maxPagesPerSource)
                if (pageSuccess) {
                    totalPage += 1;
                    if (perSourceMax > 0 && totalPage >= perSourceMax) {
                        qDebug() << "[CrawlerTask] 达到 perSourceMax，停止来源:" << src.c_str();
                        // mark sub-progress complete
                        if (m_subProgressCallback) {
                            int expected = perSourceMax > 0 ? perSourceMax : (mapping.totalPage > 0 ? mapping.totalPage : page);
                            m_subProgressCallback(expected, expected);
                        }
                        break;
                    }
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
                    // If finished before reaching expected pages, mark sub-progress complete
                    if (m_subProgressCallback) {
                        int expected = perSourceMax > 0 ? perSourceMax : (mapping.totalPage > 0 ? mapping.totalPage : page);
                        m_subProgressCallback(expected, expected);
                    }
                    break;
                }
            }

            page++;
        }
        // record per-source pages fetched when this source finishes
        pagesFetchedPerSource[sourceIndex] = pagesFetchedForSource;
    }
    // After all sources finished, send a summary message listing per-source pages and stored counts and overall total
    if (m_progressCallback) {
        std::ostringstream ss;
        ss << "爬取完成，来源统计：\n";
        for (size_t i = 0; i < sources.size(); ++i) {
            ss << "- " << sources[i] << ": 爬取 " << pagesFetchedPerSource[i] << " 页，存储 " << storedPerSource[i] << " 条\n";
        }
        ss << "总计存储: " << totalStored << " 条";
        // also log summary to debug
        qDebug() << "[CrawlerTask] Summary:\n" << QString::fromStdString(ss.str());
        m_progressCallback(static_cast<int>(sources.size()), static_cast<int>(sources.size()), ss.str());
    }
    qDebug() << "[CrawlerTask] crawlAll 完成，总计存储:" << totalStored;
    return totalStored;
}

// 旧签名的包装器：将单个 maxPagesPerSource 拓展为列表并调用新实现
int CrawlerTask::crawlAll(const std::vector<std::string>& sources, int maxPagesPerSource, int pageSize) {
    std::vector<int> list(sources.size(), maxPagesPerSource);
    return crawlAll(sources, list, pageSize);
}
