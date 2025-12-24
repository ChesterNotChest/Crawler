#include "crawler_task.h"
#include <QDebug>
#include <QThread>
#include <algorithm>

CrawlerTask::CrawlerTask(SQLInterface *sqlInterface)
    : m_internetTask(),
      m_sqlTask(sqlInterface) {
    qDebug() << "[CrawlerTask] 初始化完成";
}




int CrawlerTask::crawlAll(int maxPagesPerSource, int pageSize) {
    qDebug() << "[CrawlerTask] crawlAll 启动，maxPagesPerSource=" << maxPagesPerSource << " pageSize=" << pageSize;

    int totalStored = 0;

    // 数据源顺序：nowcode -> zhipin
    // std::vector<std::string> sources = {"nowcode", "zhipin", "chinahr"};
    std::vector<std::string> sources = {"chinahr"};
    // 控制是否在页间暂停，以及哪些来源需要暂停（可按需修改）
    std::vector<std::string> pauseSources = { "zhipin" };
    for (const auto& src : sources) {
        qDebug() << "[CrawlerTask] 开始来源:" << src.c_str();
        m_internetTask.updateCookieBySource(src);

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
        while (true) {
            // 达到用户指定的最大页数上限则停止
            if (maxPagesPerSource > 0 && page > maxPagesPerSource) {
                qDebug() << "[CrawlerTask] 达到最大页数上限，停止来源:" << src.c_str();
                break;
            }

            qDebug() << "[CrawlerTask] 来源" << src.c_str() << "城市" << currentCity.c_str()
                     << "类型" << currentRecruitType << "第" << page << "页开始抓取...";
            auto [jobs, mapping] = m_internetTask.fetchBySource(src, page, pageSize, currentRecruitType, currentCity);

            // 任意来源遇到反爬码37则更新cookie并重试一次
            if (mapping.last_api_code == 37) {
                qDebug() << "[CrawlerTask] 检测到 反爬码 37，尝试更新 cookie 并重试此页...";
                bool ok = m_internetTask.updateCookieBySource(src);
                if (ok) {
                    qDebug() << "[CrawlerTask] cookie 更新成功，重试第" << page << "页...";
                    std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize, 1, currentCity);
                } else {
                    qDebug() << "[CrawlerTask] cookie 更新失败";
                }
            }

            if (!jobs.empty()) {
                int sourceId = (src == "nowcode") ? 1 : 2;
                int stored = m_sqlTask.storeJobDataBatchWithSource(jobs, sourceId);
                totalStored += stored;
                qDebug() << "[CrawlerTask] 来源" << src.c_str() << "第" << page << "页存储" << stored << "条";
            } else {
                qDebug() << "[CrawlerTask] 第" << page << "页无数据";
            }

            // 只有当 doPause 为 true 且当前来源在 pauseSources 列表中时才等待
            if (std::find(pauseSources.begin(), pauseSources.end(), src) != pauseSources.end()) {
                QThread::sleep(3);
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
        }
    }

    qDebug() << "[CrawlerTask] crawlAll 完成，总计存储:" << totalStored;
    return totalStored;
}
