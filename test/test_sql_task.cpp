#include "test.h"
#include "tasks/sql_task.h"
#include "db/sqlinterface.h"
#include <QDebug>
#include <QDateTime>

/**
 * @brief SqlTask单元测试
 * 测试范围: 数据转换和存储功能（不涉及网络爬取）
 * 测试场景:
 *   1. 单条数据存储
 *   2. 批量数据存储
 *   3. 类型转换验证
 *   4. 数据库查询验证
 */
void test_sql_task_unit() {
    qDebug() << "\n========== SqlTask 单元测试 ==========\n";
    
    // 初始化数据库
    SQLInterface sql;
    if (!sql.connectSqlite("crawler.db")) {
        qDebug() << "❌ 数据库连接失败";
        return;
    }
    
    sql.createAllTables();
    qDebug() << "✓ 数据库初始化完成\n";
    
    SqlTask sqlTask(&sql);
    
    // ========== 场景1: 单条Mock数据存储 ==========
    qDebug() << "[场景1] 单条数据存储测试";
    
    ::JobInfo mockJob1;
    mockJob1.info_id = 9001;
    mockJob1.info_name = "C++ 开发工程师 (单元测试)";
    mockJob1.type_id = 1;  // 校招
    mockJob1.area_id = 0;  // 使用城市名称插入
    mockJob1.area_name = "北京";
    mockJob1.company_name = "UnitTest 公司A";
    mockJob1.company_id = 201;
    mockJob1.salary_min = 20.0;
    mockJob1.salary_max = 35.0;
    mockJob1.requirements = "熟悉C++11/17, Qt框架, 数据库设计";
    mockJob1.create_time = "2025-12-15 10:00:00";
    mockJob1.update_time = "2025-12-15 10:00:00";
    mockJob1.hr_last_login = "2025-12-15 09:00:00";
    mockJob1.tag_ids = {1, 2, 3};
    
    int jobId1 = sqlTask.storeJobData(mockJob1);
    qDebug() << "✓ 存储结果: jobId =" << jobId1;
    
    if (jobId1 > 0) {
        qDebug() << "  - 数据存储成功";
        qDebug() << "  - 依赖数据自动创建 (Company, City, Tags)";
        qDebug() << "  - JobTagMapping 关联已建立";
    } else {
        qDebug() << "❌ 数据存储失败";
    }
    
    // ========== 场景2: 批量Mock数据存储 ==========
    qDebug() << "\n[场景2] 批量数据存储测试";
    
    std::vector<::JobInfo> mockJobs;
    
    // Mock Job 2
    ::JobInfo mockJob2;
    mockJob2.info_id = 9002;
    mockJob2.info_name = "Python 后端开发";
    mockJob2.type_id = 2;  // 实习
    mockJob2.area_id = 0;  // 使用城市名称插入
    mockJob2.area_name = "上海";
    mockJob2.company_name = "UnitTest 公司B";
    mockJob2.company_id = 202;
    mockJob2.salary_min = 8.0;
    mockJob2.salary_max = 12.0;
    mockJob2.requirements = "Python, Django/Flask, MySQL";
    mockJob2.create_time = "2025-12-15 11:00:00";
    mockJob2.update_time = "2025-12-15 11:00:00";
    mockJob2.hr_last_login = "2025-12-15 10:30:00";
    mockJob2.tag_ids = {4, 5};
    mockJobs.push_back(mockJob2);
    
    // Mock Job 3
    ::JobInfo mockJob3;
    mockJob3.info_id = 9003;
    mockJob3.info_name = "前端开发工程师";
    mockJob3.type_id = 1;  // 校招
    mockJob3.area_id = 0;  // 使用城市名称插入
    mockJob3.area_name = "深圳";
    mockJob3.company_name = "UnitTest 公司C";
    mockJob3.company_id = 203;
    mockJob3.salary_min = 15.0;
    mockJob3.salary_max = 25.0;
    mockJob3.requirements = "React/Vue, TypeScript, 前端工程化";
    mockJob3.create_time = "2025-12-15 12:00:00";
    mockJob3.update_time = "2025-12-15 12:00:00";
    mockJob3.hr_last_login = "2025-12-15 11:45:00";
    mockJob3.tag_ids = {6};
    mockJobs.push_back(mockJob3);
    
    int successCount = sqlTask.storeJobDataBatch(mockJobs);
    qDebug() << "✓ 批量存储结果:" << successCount << "/" << mockJobs.size() << "条成功";
    
    // ========== 场景3: 类型转换验证 ==========
    qDebug() << "\n[场景3] 数据类型转换验证";
    
    auto allJobs = sqlTask.queryAllJobs();
    qDebug() << "✓ 数据库中共有" << allJobs.size() << "条记录";
    
    if (!allJobs.empty()) {
        const auto& firstJob = allJobs[0];
        qDebug() << "\n✓ 转换后数据样本 (SQLNS::JobInfo):";
        qDebug() << "  - jobId (int):" << firstJob.jobId;
        qDebug() << "  - jobName (QString):" << firstJob.jobName;
        qDebug() << "  - salaryMin (double):" << firstJob.salaryMin;
        qDebug() << "  - salarySlabId (int):" << firstJob.salarySlabId;
        qDebug() << "  - tagIds (QVector<int>):" << firstJob.tagIds;
    }
    
    // ========== 场景4: 基础SQL操作测试 ==========
    qDebug() << "\n[场景4] 基础SQL操作测试";
    
    // 测试insertCompany (一般ID)
    int companyId = sqlTask.insertCompany(301, "UnitTest科技");
    qDebug() << "✓ insertCompany (一般ID):" << (companyId > 0 ? "成功" : "失败");
    
    // 测试insertCity (自增ID)
    int cityId = sqlTask.insertCity("杭州");
    qDebug() << "✓ insertCity (自增ID): cityId =" << cityId;
    
    // 测试insertTag (自增ID)
    int tagId = sqlTask.insertTag("单元测试");
    qDebug() << "✓ insertTag (自增ID): tagId =" << tagId;

    // === 验证部分 ===
    qDebug() << "\n[场景5] 数据一致性验证";
    
    bool consistent = true;
    for (const auto& dbJob : allJobs) {
        // 验证必填字段
        if (dbJob.jobId <= 0 || dbJob.jobName.isEmpty()) {
            qDebug() << "❌ 数据不一致: jobId =" << dbJob.jobId;
            consistent = false;
        }
        
        // 验证薪资档次ID是否合理 (0-6, 0表示无薪资)
        if (dbJob.salarySlabId < 0 || dbJob.salarySlabId > 6) {
            qDebug() << "❌ 薪资档次ID异常:" << dbJob.salarySlabId;
            consistent = false;
        }
    }
    
    if (consistent) {
        qDebug() << "✓ 数据一致性检查通过";
    }
    
    sql.disconnect();
    qDebug() << "\n✅ SqlTask 单元测试完成!\n";
}
