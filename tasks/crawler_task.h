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
     * @brief 爬取并存储单页数据
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @return 成功存储的数量
     */
    int crawlAndStore(int pageNo, int pageSize);
    
    /**
     * @brief 爬取并存储多页数据
     * @param startPage 起始页
     * @param endPage 结束页
     * @param pageSize 每页数量
     * @return 成功存储的数量
     */
    int crawlAndStoreMultiPage(int startPage, int endPage, int pageSize);
    
    /**
     * @brief 仅爬取数据（不存储）
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @return JobInfo列表和映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> crawlOnly(int pageNo, int pageSize);
    
    /**
     * @brief 仅存储数据（不爬取）
     * @param jobs 已爬取的JobInfo列表
     * @return 成功存储的数量
     */
    int storeOnly(const std::vector<JobInfo>& jobs);
    
private:
    InternetTask m_internetTask;  // 网络爬虫任务
    SqlTask m_sqlTask;            // SQL存储任务
};

#endif // CRAWLER_TASK_H
