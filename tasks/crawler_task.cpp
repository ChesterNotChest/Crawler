#include "crawler_task.h"
#include <QDebug>
#include <QThread>

CrawlerTask::CrawlerTask(SQLInterface *sqlInterface)
    : m_internetTask(),
      m_sqlTask(sqlInterface) {
    qDebug() << "[CrawlerTask] 初始化完成";
}




int CrawlerTask::crawlAll(int maxPagesPerSource, int pageSize) {
    qDebug() << "[CrawlerTask] crawlAll 启动，maxPagesPerSource=" << maxPagesPerSource << " pageSize=" << pageSize;

    int totalStored = 0;
    // 数据源顺序：nowcode -> zhipin
    //std::vector<std::string> sources = {"nowcode", "zhipin"};
    std::vector<std::string> sources = { "zhipin"};
    for (const auto& src : sources) {
        qDebug() << "[CrawlerTask] 开始来源:" << src.c_str();
        m_internetTask.updateCookieBySource(src);
        int page = 1;
        int consecutiveFailures = 0;
        while (true) {
            // 终止条件 1 (可选上限): 达到用户指定的最大页数上限则停止（对牛客/其他来源通用）
            if (maxPagesPerSource > 0 && page > maxPagesPerSource) {
                qDebug() << "[CrawlerTask] 达到最大页数上限，停止来源:" << src.c_str();
                break;
            }

            qDebug() << "[CrawlerTask] 来源" << src.c_str() << "第" << page << "页开始抓取...";
            auto [jobs, mapping] = m_internetTask.fetchBySource(src, page, pageSize);

            // 如果返回了反爬错误码（统一为37），只在此情况下尝试更新cookie并重试一次
            if (src == "zhipin" && mapping.last_api_code == 37) {
                qDebug() << "[CrawlerTask] 检测到 zhipin 反爬码 37，尝试更新 cookie 并重试此页...";
                bool ok = m_internetTask.updateCookieBySource(src);
                if (ok) {
                    qDebug() << "[CrawlerTask] cookie 更新成功，重试第" << page << "页...";
                    std::tie(jobs, mapping) = m_internetTask.fetchBySource(src, page, pageSize);
                } else {
                    qDebug() << "[CrawlerTask] cookie 更新失败，计入一次失败";
                }
            }

            if (jobs.empty()) {
                qDebug() << "[CrawlerTask] 第" << page << "页无数据，计入失败";
                consecutiveFailures++;
                // 牛客终止条件：连续三次未获取到数据则认为无更多可抓，停止来源
                // （此条为牛客/通用来源的重要终止逻辑）
                if (consecutiveFailures >= 3) {
                    qDebug() << "[CrawlerTask] 连续失败3次，停止此来源";
                    break;
                }
            } else {
                consecutiveFailures = 0;
                int sourceId = (src == "nowcode") ? 1 : 2;
                int stored = m_sqlTask.storeJobDataBatchWithSource(jobs, sourceId);
                totalStored += stored;
                qDebug() << "[CrawlerTask] 来源" << src.c_str() << "第" << page << "页存储" << stored << "条";
            }

            // 页间等待3秒
            QThread::sleep(3);

            // 对于 zhipin：仅当返回的 zpData.hasMore 为 false 时结束；否则继续
            if (src == "zhipin") {
                if (!mapping.has_more) {
                    qDebug() << "[CrawlerTask] zhipin 返回 hasMore=false，结束来源抓取";
                    break;
                }
            }

            page++;
        }
    }

    qDebug() << "[CrawlerTask] crawlAll 完成，总计存储:" << totalStored;
    return totalStored;
}
