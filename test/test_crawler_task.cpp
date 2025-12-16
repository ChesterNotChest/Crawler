#include "test.h"
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"
#include <QDebug>

/**
 * @brief CrawlerTask集成测试
 * 测试Crawler侧的完整工作流: InternetTask(爬取) → SqlTask(存储)
 */
void test_crawler_task_integration() {
    qDebug() << "\n========== CrawlerTask 集成测试 ==========\n";
    
    // 初始化数据库
    SQLInterface sql;
    if (!sql.connectSqlite("crawler.db")) {
        qDebug() << "❌ 数据库连接失败";
        return;
    }
    
    // 创建表结构
    sql.createAllTables();
    qDebug() << "✓ 数据库初始化完成";
    
    // 创建CrawlerTask实例
    CrawlerTask crawlerTask(&sql);
    
    // ========== 测试场景1: 爬取并存储单页 ==========
    qDebug() << "\n[场景1] 爬取并存储单页数据";
    int count1 = crawlerTask.crawlAndStore(1, 5);
    qDebug() << "结果: 成功存储" << count1 << "条数据\n";
    
    // ========== 测试场景2: 爬取并存储多页 ==========
    qDebug() << "\n[场景2] 批量爬取并存储多页数据";
    int count2 = crawlerTask.crawlAndStoreMultiPage(2, 3, 20);
    qDebug() << "结果: 成功存储" << count2 << "条数据\n";
    
    // ========== 测试场景3: 仅爬取模式 ==========
    qDebug() << "\n[场景3] 仅爬取数据（不存储）";
    auto [jobs, mapping] = crawlerTask.crawlOnly(4, 3);
    qDebug() << "结果: 爬取到" << jobs.size() << "条数据";
    qDebug() << "  - 类型映射:" << mapping.type_list.size();
    qDebug() << "  - 地区映射:" << mapping.area_list.size();
    qDebug() << "  - 薪资档次:" << mapping.salary_level_list.size();
    
    // ========== 测试场景4: 仅存储模式 ==========
    qDebug() << "\n[场景4] 仅存储模式（使用场景3爬取的数据）";
    int count4 = crawlerTask.storeOnly(jobs);
    qDebug() << "结果: 成功存储" << count4 << "条数据\n";
    
    // ========== 验证最终结果 ==========
    qDebug() << "\n[验证] 查询数据库中的所有数据";
    SqlTask sqlTask(&sql);
    auto allJobs = sqlTask.queryAllJobs();
    qDebug() << "数据库中共有" << allJobs.size() << "条职位记录";
    
    sql.disconnect();
    qDebug() << "\n✅ CrawlerTask 集成测试完成!\n";
}
