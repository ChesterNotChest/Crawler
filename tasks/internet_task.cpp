#include "internet_task.h"
#include <QDebug>
#include <algorithm>

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

std::pair<std::vector<JobInfo>, MappingData> InternetTask::crawlBySource(
    const std::string& sourceCode, int pageNo, int pageSize, int recruitType) {
    
    qDebug() << "[InternetTask] 按来源爬取:" << sourceCode.c_str() << ", 页码:" << pageNo;
    
    if (sourceCode == "nowcode") {
        return NowcodeCrawler::crawlNowcode(pageNo, pageSize, recruitType);
    } else if (sourceCode == "zhipin") {
        // BOSS直聘使用北京城市代码 101010100
        return ZhipinCrawler::crawlZhipin(pageNo, pageSize, "101010100");
    } else {
        qDebug() << "[错误] 未知的数据源:" << sourceCode.c_str();
        return {{}, {}};
    }
}

std::pair<std::vector<JobInfo>, MappingData> InternetTask::crawlAll(int pageNo, int pageSize) {
    qDebug() << "[InternetTask] 开始爬取所有数据源...";
    
    std::vector<JobInfo> allJobs;
    MappingData mergedMapping;
    
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
    
    // 合并BOSS直聘数据
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
