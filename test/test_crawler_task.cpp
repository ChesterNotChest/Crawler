#include "test.h"
#include "tasks/internet_task.h"
#include "tasks/sql_task.h"
#include "db/sqlinterface.h"
#include <QDebug>

/**
 * @brief CrawlerTask集成测试
 * 功能：直接爬取2种来源（牛客网和BOSS直聘）并存入数据库
 */
void test_crawler_task_integration() {
    qDebug() << "\n========== 多数据源集成测试 ==========\n";
    
    // ========== 1. 初始化数据库 ==========
    SQLInterface sql;
    if (!sql.connectSqlite("crawler.db")) {
        qDebug() << "❌ 数据库连接失败";
        return;
    }
    
    // 创建表结构（包括Source表）
    if (!sql.createAllTables()) {
        qDebug() << "❌ 创建表失败";
        sql.disconnect();
        return;
    }
    qDebug() << "✓ 数据库初始化完成";

    
    // ========== 2. 查询数据源ID ==========
    auto nowcodeSource = sql.querySourceByCode("nowcode");
    auto zhipinSource = sql.querySourceByCode("zhipin");
    
    if (nowcodeSource.sourceId <= 0 || zhipinSource.sourceId <= 0) {
        qDebug() << "❌ 数据源未正确初始化";
        sql.disconnect();
        return;
    }
    
    qDebug() << "✓ 数据源ID - 牛客网:" << nowcodeSource.sourceId << ", BOSS直聘:" << zhipinSource.sourceId;
    
    // ========== 3. 创建任务对象 ==========
    InternetTask internetTask;
    SqlTask sqlTask(&sql);
    
    int totalStored = 0;
    
    // ========== 4. 爬取并存储牛客网数据 ==========
    qDebug() << "\n[1/2] 开始爬取牛客网数据...";
    
    // 爬取牛客网第1页，校招类型
    auto [nowcodeJobs, nowcodeMapping] = internetTask.fetchBySource("nowcode", 1, 10, 1);
    
    if (!nowcodeJobs.empty()) {
        int count = sqlTask.storeJobDataBatchWithSource(nowcodeJobs, nowcodeSource.sourceId);
        totalStored += count;
        qDebug() << "✓ 牛客网数据存储完成，成功" << count << "条";
        
        // 显示第一条数据样本
        if (count > 0) {
            qDebug() << "  样本 - ID:" << nowcodeJobs[0].info_id
                     << ", 职位:" << QString::fromStdString(nowcodeJobs[0].info_name)
                     << ", 公司:" << QString::fromStdString(nowcodeJobs[0].company_name);
        }
    } else {
        qDebug() << "⚠️  牛客网未爬取到数据";
    }
    
    // ========== 5. 爬取并存储BOSS直聘数据 ==========
    qDebug() << "\n[2/2] 开始爬取BOSS直聘数据...";
    
    // 爬取BOSS直聘第1页
    auto [zhipinJobs, zhipinMapping] = internetTask.fetchBySource("zhipin", 1, 10);
    
    if (!zhipinJobs.empty()) {
        int count = sqlTask.storeJobDataBatchWithSource(zhipinJobs, zhipinSource.sourceId);
        totalStored += count;
        qDebug() << "✓ BOSS直聘数据存储完成，成功" << count << "条";
        
        // 显示第一条数据样本
        if (count > 0) {
            qDebug() << "  样本 - ID:" << zhipinJobs[0].info_id
                     << ", 职位:" << QString::fromStdString(zhipinJobs[0].info_name)
                     << ", 公司:" << QString::fromStdString(zhipinJobs[0].company_name);
        }
    } else {
        qDebug() << "⚠️  BOSS直聘未爬取到数据";
    }
    
    // ========== 6. 验证数据库记录 ==========
    qDebug() << "\n[验证] 检查数据库记录...";
    auto allJobs = sqlTask.queryAllJobs();
    qDebug() << "✓ 数据库中总记录数:" << allJobs.size();
    qDebug() << "✓ 本次成功存储:" << totalStored << "条";
    
    // 统计各数据源的记录数
    int nowcodeCount = 0, zhipinCount = 0;
    for (const auto& job : allJobs) {
        if (job.sourceId == nowcodeSource.sourceId) nowcodeCount++;
        else if (job.sourceId == zhipinSource.sourceId) zhipinCount++;
    }
    
    qDebug() << "✓ 牛客网记录:" << nowcodeCount << "条";
    qDebug() << "✓ BOSS直聘记录:" << zhipinCount << "条";
    
    // ========== 7. 测试结果 ==========
    if (totalStored > 0) {
        qDebug() << "\n✅ 多数据源集成测试成功完成！";
        qDebug() << "   共爬取并存储了" << totalStored << "条职位数据（来自2个数据源）\n";
    } else {
        qDebug() << "\n⚠️  警告：未成功存储任何数据\n";
    }
    
    sql.disconnect();
}
