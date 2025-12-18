#ifndef INTERNET_TASK_H
#define INTERNET_TASK_H

#include <vector>
#include "network/job_crawler.h"

/**
 * @brief InternetTask - 负责网络爬虫操作
 * 封装爬虫调用过程，提供统一的网络数据获取接口
 */
class InternetTask {
public:
    InternetTask();
    
    /**
     * @brief 从网络爬取职位数据
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招，默认=1)
     * @return 爬取到的JobInfo列表和映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> fetchJobData(int pageNo, int pageSize, int recruitType = DEFAULT_RECRUIT_TYPE);
    
    /**
     * @brief 批量爬取多页数据
     * @param startPage 起始页
     * @param endPage 结束页
     * @param pageSize 每页数量
     * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招，默认=1)
     * @return 所有页的JobInfo列表和合并的映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> fetchJobDataMultiPage(
        int startPage, int endPage, int pageSize, int recruitType = DEFAULT_RECRUIT_TYPE);
    
private:
    // 爬虫配置参数可以在这里管理
    static constexpr int DEFAULT_PAGE_SIZE = 10;
    static constexpr int DEFAULT_RECRUIT_TYPE = 1; // 校招
};

#endif // INTERNET_TASK_H
