#include "test.h"
#include "tasks/crawler_task.h"
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
    
    // ========== 第1步: 爬取第1页获取totalPage ==========
    qDebug() << "\n[Step 1] Crawl page 1 to get pagination info";
    auto [firstPageJobs, firstPageMapping] = crawlerTask.crawlOnly(1, 20);
    
    if (firstPageJobs.empty()) {
        qDebug() << "❌ Failed to crawl first page";
        sql.disconnect();
        return;
    }
    
    int totalPages = firstPageMapping.totalPage;
    int currentPage = firstPageMapping.currentPage;
    
    qDebug() << "✓ First page crawled:" << firstPageJobs.size() << "jobs";
    qDebug() << "  - Current page:" << currentPage;
    qDebug() << "  - Total pages:" << totalPages;
    
    // ========== 第2步: 存储第1页数据 ==========
    qDebug() << "\n[Step 2] Store first page data";
    int totalStored = crawlerTask.storeOnly(firstPageJobs);
    qDebug() << "✓ Stored" << totalStored << "jobs from page 1";
    
    // ========== 第3步: 批量爬取剩余页面 ==========
    if (totalPages > 1) {
        qDebug() << "\n[Step 3] Crawl and store remaining pages (2 to" << totalPages << ")";
        int countRemaining = crawlerTask.crawlAndStoreMultiPage(2, totalPages, 20);
        totalStored += countRemaining;
        qDebug() << "✓ Stored" << countRemaining << "jobs from pages 2-" << totalPages;
    }
    
    // ========== 第4步: 验证最终结果 ==========
    qDebug() << "\n[Step 4] Verify final result";
    SqlTask sqlTask(&sql);
    auto allJobs = sqlTask.queryAllJobs();
    qDebug() << "✓ Total records in database:" << allJobs.size();
    
    qDebug() << "  - Expected to store:" << (firstPageJobs.size() + totalStored - firstPageJobs.size());
    qDebug() << "  - Actually stored:" << allJobs.size();
    
    if (!allJobs.empty()) {
        qDebug() << "\n✅ CrawlerTask integration test completed successfully!\n";
    } else {
        qDebug() << "\n⚠️  Warning: No jobs stored in database\n";
    }
    
    sql.disconnect();
}
