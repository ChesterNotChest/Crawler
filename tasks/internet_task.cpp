#include "internet_task.h"
#include <iostream>

InternetTask::InternetTask() {
    // 构造函数，未来可以在这里初始化配置
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchJobData(int pageNo, int pageSize, int recruitType) {
    std::cout << "[InternetTask] 开始爬取第 " << pageNo << " 页数据，每页 " << pageSize << " 条，招聘类型 " << recruitType << "..." << std::endl;
    
    // 调用爬虫主函数
    auto result = job_crawler_main(pageNo, pageSize, recruitType);
    
    std::cout << "[InternetTask] 爬取完成，获得 " << result.first.size() << " 条职位数据" << std::endl;
    
    return result;
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::fetchJobDataMultiPage(
    int startPage, int endPage, int pageSize, int recruitType) {
    
    std::cout << "[InternetTask] 开始批量爬取，页码范围: " << startPage << " - " << endPage << "，招聘类型 " << recruitType << std::endl;
    
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
    
    std::cout << "[InternetTask] 批量爬取完成，总计 " << allJobs.size() << " 条职位数据" << std::endl;
    
    return {allJobs, mergedMapping};
}
