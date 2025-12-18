#include "test.h"
#include "tasks/crawler_task.h"
#include "tasks/internet_task.h"
#include "db/sqlinterface.h"
#include <QDebug>

/**
 * @brief CrawlerTask集成测试
 * 单一集成测试：自动根据totalPage爬取并存储所有页面数据
 */
void test_crawler_task_integration() {
    qDebug() << "\n========== CrawlerTask 集成测试 ==========\n";
    
    // 初始化数据库
    SQLInterface sql;
    if (!sql.connectSqlite("crawler.db")) {
        qDebug() << "❌ Database connection failed";
        return;
    }
    
    // 创建表结构
    sql.createAllTables();
    qDebug() << "✓ Database initialized";
    
    // 创建CrawlerTask实例
    CrawlerTask crawlerTask(&sql);
    
    // ========== 爬取并存储所有页面（自动处理校招/实习/社招） ==========
    qDebug() << "\n[Integration Test] Crawl and store all pages (all types)";
    
    // 先爬取第1页获取totalPage信息（通过InternetTask直接获取）
    InternetTask internetTask;
    auto [firstPageJobs, firstPageMapping] = internetTask.fetchJobData(1, 20, 1); // 用校招类型获取分页
    
    if (firstPageJobs.empty()) {
        qDebug() << "❌ Failed to get pagination info";
        sql.disconnect();
        return;
    }
    
    int totalPages = firstPageMapping.totalPage;
    qDebug() << "✓ Total pages:" << totalPages;
    
    // 爬取并存储所有页面的所有类型
    int totalStored = crawlerTask.crawlAndStoreMultiPage(1, totalPages, 20);
    qDebug() << "✓ Crawled and stored all pages, total:" << totalStored << "records";
    
    // ========== 验证最终结果 ==========
    qDebug() << "\n[Validation] Verify database records";
    SqlTask sqlTask(&sql);
    auto allJobs = sqlTask.queryAllJobs();
    qDebug() << "✓ Total records in database:" << allJobs.size();
    
    if (!allJobs.empty()) {
        qDebug() << "\n✅ CrawlerTask integration test completed successfully!\n";
    } else {
        qDebug() << "\n⚠️  Warning: No jobs stored in database\n";
    }
    
    sql.disconnect();
}
