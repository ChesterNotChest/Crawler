#include "crawler_task.h"
#include <QDebug>

CrawlerTask::CrawlerTask(SQLInterface *sqlInterface)
    : m_internetTask(),
      m_sqlTask(sqlInterface) {
    qDebug() << "[CrawlerTask] 初始化完成";
}

int CrawlerTask::crawlAndStore(int pageNo, int pageSize) {
    qDebug() << "[CrawlerTask] 开始爬取并存储第" << pageNo << "页数据（校招/实习/社招）...";
    
    int totalSuccess = 0;
    const char* typeNames[] = {"校招", "实习", "社招"};
    
    // 遍历三种招聘类型: 1=校招, 2=实习, 3=社招
    for (int recruitType = 1; recruitType <= 3; ++recruitType) {
        qDebug() << "[CrawlerTask] --> 正在爬取" << typeNames[recruitType-1] << "(recruitType=" << recruitType << ")...";
        
        auto [jobs, mapping] = m_internetTask.fetchJobData(pageNo, pageSize, recruitType);
        
        if (!jobs.empty()) {
            int count = m_sqlTask.storeJobDataBatch(jobs);
            totalSuccess += count;
            qDebug() << "[CrawlerTask] --> " << typeNames[recruitType-1] << "存储成功" << count << "条";
        } else {
            qDebug() << "[CrawlerTask] --> " << typeNames[recruitType-1] << "未爬取到数据";
        }
    }
    
    qDebug() << "[CrawlerTask] 第" << pageNo << "页所有类型存储完成，总计" << totalSuccess << "条";
    return totalSuccess;
}

int CrawlerTask::crawlAndStoreMultiPage(int startPage, int endPage, int pageSize) {
    qDebug() << "[CrawlerTask] 开始批量爬取并存储，页码范围:" << startPage << "-" << endPage << "（校招/实习/社招）...";
    
    int totalSuccess = 0;
    const char* typeNames[] = {"校招", "实习", "社招"};
    
    // 遍历三种招聘类型: 1=校招, 2=实习, 3=社招
    for (int recruitType = 1; recruitType <= 3; ++recruitType) {
        qDebug() << "[CrawlerTask] --> 正在批量爬取" << typeNames[recruitType-1] << "(recruitType=" << recruitType << ")...";
        
        auto [jobs, mapping] = m_internetTask.fetchJobDataMultiPage(startPage, endPage, pageSize, recruitType);
        
        if (!jobs.empty()) {
            int count = m_sqlTask.storeJobDataBatch(jobs);
            totalSuccess += count;
            qDebug() << "[CrawlerTask] --> " << typeNames[recruitType-1] << "批量存储成功" << count << "条";
        } else {
            qDebug() << "[CrawlerTask] --> " << typeNames[recruitType-1] << "未爬取到数据";
        }
    }
    
    qDebug() << "[CrawlerTask] 批量存储完成，总计" << totalSuccess << "条";
    return totalSuccess;
}
