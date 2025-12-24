#ifndef CRAWLER_TASK_H
#define CRAWLER_TASK_H

#include "internet_task.h"
#include "sql_task.h"
#include <vector>

/**
 * @brief CrawlerTask - 总任务协调器
 * 协调网络爬取(InternetTask)和数据存储(SqlTask)
 * 提供完整的"爬取→存储"一站式服务
 */
class CrawlerTask {
public:
    /**
     * @brief 构造函数
     * @param sqlInterface SQL接口指针
     */
    CrawlerTask(SQLInterface *sqlInterface);
    
    

    
    /**
     * @brief 按来源爬取所有数据（两层循环），会调用internet_task.updateCookieBySource并在失败时重试
     * @param maxPagesPerSource 每个来源的最大页数上限（避免无限循环），0 表示无限制
     * @param pageSize 每页大小
     */
    int crawlAll(int maxPagesPerSource = 0, int pageSize = 15);
    
private:
    InternetTask m_internetTask;  // 网络爬虫任务
    SqlTask m_sqlTask;            // SQL存储任务
};

#endif // CRAWLER_TASK_H
