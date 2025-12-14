#include "crawler_task.h"
#include <QDebug>

CrawlerTask::CrawlerTask(SQLInterface *sqlInterface)
    : m_internetTask(),
      m_sqlTask(sqlInterface) {
    qDebug() << "[CrawlerTask] 初始化完成";
}

int CrawlerTask::crawlAndStore(int pageNo, int pageSize) {
    qDebug() << "[CrawlerTask] 开始爬取并存储第" << pageNo << "页数据...";
    
    // 步骤1: 调用InternetTask爬取数据
    auto [jobs, mapping] = m_internetTask.fetchJobData(pageNo, pageSize);
    
    if (jobs.empty()) {
        qDebug() << "[CrawlerTask] 警告: 未爬取到任何数据";
        return 0;
    }
    
    // 步骤2: 调用SqlTask存储数据
    qDebug() << "[CrawlerTask] 爬取完成，开始存储" << jobs.size() << "条数据...";
    int successCount = m_sqlTask.storeJobDataBatch(jobs);
    
    qDebug() << "[CrawlerTask] 存储完成，成功" << successCount << "条";
    return successCount;
}

int CrawlerTask::crawlAndStoreMultiPage(int startPage, int endPage, int pageSize) {
    qDebug() << "[CrawlerTask] 开始批量爬取并存储，页码范围:" << startPage << "-" << endPage;
    
    // 步骤1: 调用InternetTask批量爬取
    auto [jobs, mapping] = m_internetTask.fetchJobDataMultiPage(startPage, endPage, pageSize);
    
    if (jobs.empty()) {
        qDebug() << "[CrawlerTask] 警告: 未爬取到任何数据";
        return 0;
    }
    
    // 步骤2: 调用SqlTask批量存储
    qDebug() << "[CrawlerTask] 批量爬取完成，开始存储" << jobs.size() << "条数据...";
    int successCount = m_sqlTask.storeJobDataBatch(jobs);
    
    qDebug() << "[CrawlerTask] 批量存储完成，成功" << successCount << "条";
    return successCount;
}

std::pair<std::vector<JobInfo>, MappingData> CrawlerTask::crawlOnly(int pageNo, int pageSize) {
    qDebug() << "[CrawlerTask] 仅爬取模式：第" << pageNo << "页";
    return m_internetTask.fetchJobData(pageNo, pageSize);
}

int CrawlerTask::storeOnly(const std::vector<JobInfo>& jobs) {
    qDebug() << "[CrawlerTask] 仅存储模式：" << jobs.size() << "条数据";
    return m_sqlTask.storeJobDataBatch(jobs);
}
